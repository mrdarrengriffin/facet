// Progressive JPEG for the panel's picture decode (fw_imgdec.h detail::jpeg calls it when TJpgDec says "progressive"):
// a small decoder of its own, from the whole file in memory (the scans of a progressive file each cover the whole
// picture, so it cannot be decoded as it downloads). Huffman, 8-bit, 1 (grey) or 3 (YCbCr) components, any sampling.
// Memory: a progressive file is only a picture after its last scan, so its DCT coefficients are held for every block.
// Shrunk while decoding as the baseline path (1/2, 1/4, 1/8: the same choice, the smallest step that still covers every
// view): a block at 1/2^sc needs only its lowest (8 >> sc)^2 frequencies, so only those are kept (int16: 64, 16, 4 or
// 1), plus a 64-bit mask of which coefficients are not zero (a refining scan reads a correction bit for every
// coefficient that already is). A 640 x 640 4:2:0 cover: 1.3 MB whole, 384 KB at 1/2, 154 KB at 1/4. The block is then inverse-transformed at its output size (an N x N IDCT on the N x N lowest
// frequencies, N = 8 >> sc), its chroma placed by its sampling (nearest) and converted to RGB, one MCU row at a time,
// into every view's Sink. No ESP-IDF calls: the host ASan test (firmware/tests/imgdec) runs this same code.
#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace fw::imgdec::detail {

namespace prog {

constexpr uint8_t ZZ[64] = {0,  1,  8,  16, 9,  2,  3,  10, 17, 24, 32, 25, 18, 11, 4,  5,  12, 19, 26, 33, 40, 48,
                            41, 34, 27, 20, 13, 6,  7,  14, 21, 28, 35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23,
                            30, 37, 44, 51, 58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63};  // zigzag -> row * 8 + col
// the coefficients kept per block at 1/2^sc: those with row, col < 8 >> sc (Dec::slot maps zigzag -> kept, -1 = not)
constexpr int KEEP[4] = {64, 16, 4, 1};

struct Huff {
  bool ok = false;
  uint8_t val[256];
  int maxcode[18], valptr[17], mincode[17];
  uint8_t look_len[256], look_val[256];  // the first 8 bits -> length (0 = longer) and value
  void build(const uint8_t *bits, const uint8_t *vals, int nv) {
    memcpy(val, vals, (size_t) nv);
    int code = 0, k = 0;
    memset(look_len, 0, sizeof look_len);
    for (int l = 1; l <= 16; l++) {
      valptr[l] = k, mincode[l] = code;
      for (int i = 0; i < bits[l - 1]; i++, k++, code++)
        if (l <= 8)
          for (int j = 0; j < (1 << (8 - l)); j++) look_len[(code << (8 - l)) | j] = (uint8_t) l, look_val[(code << (8 - l)) | j] = val[k];
      maxcode[l] = bits[l - 1] ? code - 1 : -1;
      code <<= 1;
    }
    maxcode[17] = 0x7fffffff;
    ok = true;
  }
};

struct Comp {
  int id = 0, h = 1, v = 1, tq = 0;
  int bw = 0, bh = 0;      // blocks held (the MCU grid's, padded)
  int cw = 0, ch = 0;      // blocks of the component itself (a non-interleaved scan's)
  int td = 0, ta = 0, pred = 0;
  int16_t *co = nullptr;   // bw * bh * keep
  uint64_t *nz = nullptr;  // bw * bh: zigzag k not zero
};

struct Dec {
  const uint8_t *b;
  size_t n, at = 0;
  uint16_t q[4][64];
  bool have_q[4] = {false, false, false, false};
  Huff dc[4], ac[4];
  Comp c[3];
  int nc = 0, W = 0, H = 0, hmax = 1, vmax = 1, mcux = 0, mcuy = 0, keep = 64, restart = 0;
  int8_t slot[64];  // zigzag k -> its place in a block's kept coefficients, -1 = not kept (above the output's N)
  void keep_for(int N) {
    keep = 0;
    for (int z = 0; z < 64; z++) slot[z] = (ZZ[z] >> 3) < N && (ZZ[z] & 7) < N ? (int8_t) keep++ : (int8_t) -1;
  }
  // the bit reader over the entropy-coded bytes (0xFF00 = 0xFF; a marker stops it)
  uint32_t acc = 0;
  int nb = 0;
  bool marker = false, cut = false;
  int eobrun = 0;
  std::string err;
  int byte() {
    if (at >= n) { cut = true; return 0; }
    return b[at++];
  }
  void fill() {
    while (nb <= 24) {
      int x = 0;
      if (!marker) {
        if (at >= n) { cut = true; marker = true; }
        else if (b[at] == 0xFF) {
          if (at + 1 < n && b[at + 1] == 0x00) x = 0xFF, at += 2;
          else marker = true;  // a marker: zeros from here (the scan ends at it)
        } else x = b[at++];
      }
      acc |= (uint32_t) x << (24 - nb);
      nb += 8;
    }
  }
  int bits(int k) {
    if (!k) return 0;
    fill();
    const int r = (int) (acc >> (32 - k));
    acc <<= k, nb -= k;
    return r;
  }
  int bit() { return bits(1); }
  static int extend(int v, int s) { return v < (1 << (s - 1)) ? v - (1 << s) + 1 : v; }
  int decode(const Huff &t) {
    fill();
    const int top = (int) (acc >> 24), l0 = t.look_len[top];
    if (l0) { acc <<= l0, nb -= l0; return t.look_val[top]; }
    int code = (int) (acc >> 23), l = 9;  // 9 bits
    while (l <= 16 && code > t.maxcode[l]) code = (int) (acc >> (31 - l)), l++;
    if (l > 16) { err = "broken picture (JPEG)"; return 0; }
    acc <<= l, nb -= l;
    return t.val[t.valptr[l] + code - t.mincode[l]];
  }
  void reset_bits() { acc = 0, nb = 0, marker = false; }
  // after a restart interval: the RSTn marker, then fresh predictions
  void restart_at() {
    reset_bits();
    while (at + 1 < n && !(b[at] == 0xFF && b[at + 1] != 0x00 && b[at + 1] != 0xFF)) at++;  // to the next marker, no further
    if (at + 1 < n && b[at + 1] >= 0xD0 && b[at + 1] <= 0xD7) at += 2;
    for (int i = 0; i < nc; i++) c[i].pred = 0;
    eobrun = 0;
  }
  int16_t *blk(Comp &k, int bx, int by, uint64_t **m) {
    const size_t i = (size_t) by * k.bw + bx;
    *m = k.nz + i;
    return k.co + i * keep;
  }
  // one block of a scan (G.1.2): DC first / refine, AC first / refine
  void block(Comp &k, int bx, int by, int ss, int se, int ah, int al) {
    uint64_t *m;
    int16_t *co = blk(k, bx, by, &m);
    if (ss == 0) {  // DC (a DC scan has se 0)
      if (!ah) {
        const int s = decode(dc[k.td]);
        if (s > 15) { err = "broken picture (JPEG)"; return; }
        k.pred += s ? extend(bits(s), s) : 0;
        co[0] = (int16_t) (k.pred * (1 << al));
      } else if (bit()) co[0] = (int16_t) (co[0] | (1 << al));
      return;
    }
    const Huff &t = ac[k.ta];
    if (!ah) {  // AC first
      if (eobrun) { eobrun--; return; }
      for (int z = ss; z <= se; z++) {
        const int rs = decode(t), r = rs >> 4, s = rs & 15;
        if (!s) {
          if (r < 15) { eobrun = (1 << r) - 1 + (r ? bits(r) : 0); break; }
          z += 15;
          continue;
        }
        z += r;
        if (z > 63) { err = "broken picture (JPEG)"; return; }
        const int v = extend(bits(s), s) * (1 << al);
        if (slot[z] >= 0) co[slot[z]] = (int16_t) v;
        *m |= 1ull << z;
      }
      return;
    }
    // AC refine (libjpeg's decode_mcu_AC_refine)
    const int p1 = 1 << al, m1 = -1 * (1 << al);
    int z = ss;
    auto refine = [&](int zi) {  // a coefficient already not zero: its correction bit
      const int si = slot[zi];
      if (bit() && si >= 0 && (co[si] & p1) == 0) co[si] = (int16_t) (co[si] + (co[si] >= 0 ? p1 : m1));
    };
    if (!eobrun) {
      for (; z <= se; z++) {
        const int rs = decode(t);
        int r = rs >> 4, s = rs & 15;
        if (s) {
          s = bit() ? p1 : m1;
        } else if (r != 15) {
          eobrun = (1 << r) + (r ? bits(r) : 0);
          break;
        }
        do {
          if (*m >> z & 1) refine(z);
          else {
            if (--r < 0) break;
          }
          z++;
        } while (z <= se);
        if (s && z <= 63) {
          if (slot[z] >= 0) co[slot[z]] = (int16_t) s;
          *m |= 1ull << z;
        }
        if (!err.empty()) return;
      }
    }
    if (eobrun) {
      for (; z <= se; z++) if (*m >> z & 1) refine(z);
      eobrun--;
    }
  }
  bool scan(const int *sel, int ns, int ss, int se, int ah, int al) {
    reset_bits();
    eobrun = 0;
    for (int i = 0; i < nc; i++) c[i].pred = 0;
    if (ss == 0 && se != 0) { err = "broken picture (JPEG)"; return false; }
    if (ss > se || se > 63 || (ss > 0 && ns != 1)) { err = "broken picture (JPEG)"; return false; }
    for (int i = 0; i < ns; i++) {
      Comp &k = c[sel[i]];
      if (ss == 0 && !ah && !dc[k.td].ok) { err = "broken picture (JPEG: no table)"; return false; }
      if (ss > 0 && !ac[k.ta].ok) { err = "broken picture (JPEG: no table)"; return false; }
    }
    int todo = restart;
    auto tick = [&]() {
      if (!restart) return;
      if (--todo == 0) { restart_at(); todo = restart; }
    };
    if (ns == 1) {  // non-interleaved: the component's own blocks
      Comp &k = c[sel[0]];
      for (int by = 0; by < k.ch; by++)
        for (int bx = 0; bx < k.cw; bx++) {
          block(k, bx, by, ss, se, ah, al);
          if (!err.empty()) return false;
          tick();
        }
    } else {
      for (int my = 0; my < mcuy; my++)
        for (int mx = 0; mx < mcux; mx++) {
          for (int i = 0; i < ns; i++) {
            Comp &k = c[sel[i]];
            for (int yy = 0; yy < k.v; yy++)
              for (int xx = 0; xx < k.h; xx++) {
                block(k, mx * k.h + xx, my * k.v + yy, ss, se, ah, al);
                if (!err.empty()) return false;
              }
          }
          tick();
        }
    }
    // to the next marker (the scan's padding)
    while (at + 1 < n && !(b[at] == 0xFF && b[at + 1] != 0x00 && !(b[at + 1] >= 0xD0 && b[at + 1] <= 0xD7))) at++;
    return true;
  }
};

// the header up to the first scan: size, components, tables. false = r.err
inline bool header(Dec &d, std::string &err) {
  if (d.n < 4 || d.b[0] != 0xFF || d.b[1] != 0xD8) { err = "not a picture"; return false; }
  d.at = 2;
  for (;;) {
    while (d.at < d.n && d.b[d.at] != 0xFF) d.at++;
    while (d.at < d.n && d.b[d.at] == 0xFF) d.at++;
    if (d.at + 2 >= d.n) { err = "broken picture (JPEG: cut short)"; return false; }
    const int mk = d.b[d.at++];
    if (mk == 0xD9) { err = "broken picture (JPEG)"; return false; }
    const size_t len = (size_t) d.b[d.at] << 8 | d.b[d.at + 1], end = d.at + len;
    if (len < 2 || end > d.n) { err = "broken picture (JPEG: cut short)"; return false; }
    const uint8_t *p = d.b + d.at + 2;
    if (mk == 0xC2) {
      if (len < 8 || p[0] != 8) { err = "JPEG type isn't supported on the panel"; return false; }
      d.H = p[1] << 8 | p[2], d.W = p[3] << 8 | p[4], d.nc = p[5];
      if (!d.W || !d.H || (d.nc != 1 && d.nc != 3) || len < 8 + 3u * d.nc) { err = "JPEG type isn't supported on the panel"; return false; }
      for (int i = 0; i < d.nc; i++) {
        Comp &k = d.c[i];
        k.id = p[6 + i * 3], k.h = p[7 + i * 3] >> 4, k.v = p[7 + i * 3] & 15, k.tq = p[8 + i * 3] & 3;
        if (k.h < 1 || k.h > 4 || k.v < 1 || k.v > 4) { err = "broken picture (JPEG)"; return false; }
        d.hmax = std::max(d.hmax, k.h), d.vmax = std::max(d.vmax, k.v);
      }
      d.at = end;
      return true;
    }
    if (mk == 0xC0 || mk == 0xC1 || (mk >= 0xC3 && mk <= 0xCF && mk != 0xC4 && mk != 0xC8 && mk != 0xCC)) {
      err = "JPEG type isn't supported on the panel";  // not progressive Huffman (arithmetic, lossless, ...)
      return false;
    }
    d.at = end;
  }
}

// the tables and scans after the frame header, to the end. false = d.err
inline bool body(Dec &d) {
  for (;;) {
    while (d.at < d.n && d.b[d.at] != 0xFF) d.at++;
    while (d.at < d.n && d.b[d.at] == 0xFF) d.at++;
    if (d.at >= d.n) { d.err = "broken picture (JPEG: cut short)"; return false; }
    const int mk = d.b[d.at++];
    if (mk == 0xD9) return true;
    if (mk >= 0xD0 && mk <= 0xD7) continue;
    if (d.at + 2 > d.n) { d.err = "broken picture (JPEG: cut short)"; return false; }
    const size_t len = (size_t) d.b[d.at] << 8 | d.b[d.at + 1], end = d.at + len;
    if (len < 2 || end > d.n) { d.err = "broken picture (JPEG: cut short)"; return false; }
    const uint8_t *p = d.b + d.at + 2, *e = d.b + end;
    if (mk == 0xDB) {
      while (p < e) {
        const int pq = p[0] >> 4, tq = p[0] & 3;
        p++;
        if (p + (pq ? 128 : 64) > e) { d.err = "broken picture (JPEG)"; return false; }
        for (int i = 0; i < 64; i++) d.q[tq][i] = pq ? (uint16_t) (p[i * 2] << 8 | p[i * 2 + 1]) : p[i];
        d.have_q[tq] = true;
        p += pq ? 128 : 64;
      }
    } else if (mk == 0xC4) {
      while (p + 17 <= e) {
        const int tc = p[0] >> 4, th = p[0] & 3;
        int nv = 0;
        for (int i = 0; i < 16; i++) nv += p[1 + i];
        if (nv > 256 || p + 17 + nv > e) { d.err = "broken picture (JPEG)"; return false; }
        (tc ? d.ac[th] : d.dc[th]).build(p + 1, p + 17, nv);
        p += 17 + nv;
      }
    } else if (mk == 0xDD) {
      if (len >= 4) d.restart = p[0] << 8 | p[1];
    } else if (mk == 0xDA) {
      const int ns = p[0];
      if (ns < 1 || ns > d.nc || len < 6 + 2u * ns) { d.err = "broken picture (JPEG)"; return false; }
      int sel[3];
      for (int i = 0; i < ns; i++) {
        sel[i] = -1;
        for (int j = 0; j < d.nc; j++) if (d.c[j].id == p[1 + i * 2]) sel[i] = j;
        if (sel[i] < 0) { d.err = "broken picture (JPEG)"; return false; }
        d.c[sel[i]].td = p[2 + i * 2] >> 4 & 3, d.c[sel[i]].ta = p[2 + i * 2] & 3;
      }
      const uint8_t *s = p + 1 + ns * 2;
      d.at = end;
      if (!d.scan(sel, ns, s[0], s[1], s[2] >> 4, s[2] & 15)) return false;
      if (d.cut) { d.err = "broken picture (JPEG: cut short)"; return false; }
      continue;
    }
    d.at = end;
  }
}

// N x N inverse DCT of block co (its kept coefficients, slot) with quant q, into out (stride os), level-shifted, clamped
inline void idct(const int16_t *co, const int8_t *slot, const uint16_t *q, int N, const float *cs, uint8_t *out, int os) {
  float F[8][8] = {}, t[8][8];
  for (int z = 0; z < 64; z++)
    if (slot[z] >= 0) F[ZZ[z] >> 3][ZZ[z] & 7] = (float) co[slot[z]] * (float) q[z];  // DQT is stored in zigzag order too
  // cs[x * 8 + u] = C(u) cos((2x + 1) u pi / 2N)
  for (int r = 0; r < N; r++)
    for (int x = 0; x < N; x++) {
      float s = 0;
      for (int u = 0; u < N; u++) s += F[r][u] * cs[x * 8 + u];
      t[r][x] = s;
    }
  for (int y = 0; y < N; y++)
    for (int x = 0; x < N; x++) {
      float s = 0;
      for (int v = 0; v < N; v++) s += t[v][x] * cs[y * 8 + v];
      const int px = (int) lrintf(s * 0.25f + 128.0f);
      out[y * os + x] = (uint8_t) (px < 0 ? 0 : px > 255 ? 255 : px);
    }
}

}  // namespace prog

// a progressive JPEG in b / n -> every box (r.px); the same choice of step and memory fallback as detail::jpeg
inline bool jpeg_progressive(const uint8_t *b, size_t n, long max_px, size_t mem_max, const std::vector<Box> &boxes, Result &r) {
  using namespace prog;
  void *dm = FW_IMGDEC_ALLOC(sizeof(Dec));  // ~10 KB of tables: picture memory, not the task's stack
  if (!dm) { r.err = "out of memory"; return false; }
  struct Gone { void *m; ~Gone() { ((Dec *) m)->~Dec(); FW_IMGDEC_FREE(m); } } gone{new (dm) Dec{}};
  Dec &d = *(Dec *) dm;
  d.b = b, d.n = n;
  if (!header(d, r.err)) return false;
  d.mcux = (d.W + 8 * d.hmax - 1) / (8 * d.hmax), d.mcuy = (d.H + 8 * d.vmax - 1) / (8 * d.vmax);
  size_t blocks = 0;
  for (int i = 0; i < d.nc; i++) {
    Comp &k = d.c[i];
    k.bw = d.mcux * k.h, k.bh = d.mcuy * k.v;
    k.cw = ((d.W * k.h + d.hmax - 1) / d.hmax + 7) / 8, k.ch = ((d.H * k.v + d.vmax - 1) / d.vmax + 7) / 8;
    if (d.nc == 1) k.bw = k.cw, k.bh = k.ch, d.mcux = k.cw, d.mcuy = k.ch;  // grey: one block an MCU
    blocks += (size_t) k.bw * k.bh;
  }
  if (d.nc == 1) d.hmax = d.vmax = d.c[0].h = d.c[0].v = 1;
  const auto dim = [&](int sc, int &w, int &h) { w = std::max(1, d.W >> sc), h = std::max(1, d.H >> sc); };
  constexpr int SC_MAX = 3;
  int lo = 0, w, h;
  for (;; lo++) {
    if (lo > SC_MAX) { r.err = "too big"; return false; }
    dim(lo, w, h);
    if ((long long) w * h <= max_px) break;
  }
  std::unique_ptr<fw::img::Sink[]> k;
  int sc = lo, ow = 0, oh = 0;
  size_t need = 0;
  const auto band_of = [&](int s) { return (size_t) d.mcux * d.hmax * (8 >> s) * d.vmax * (8 >> s); };  // one MCU row, a component
  for (int step = 0, side = 0;; step++) {
    sc = lo;
    while (sc < SC_MAX) {
      dim(sc + 1, w, h);
      bool covers = true;
      for (const Box &bx : boxes) {
        int bw, bh;
        fw::img::stored(bx, w, h, side, bw, bh);
        covers &= bx.s != fw::img::pixel && bw <= w && bh <= h;
      }
      if (!covers) break;
      sc++;
    }
    // over the room: fewer coefficients first (a coarser step, LVGL scales it up smoothly), then smaller views
    for (;; sc++) {
      dim(sc, ow, oh);
      if (!plan(boxes, ow, oh, side, false, k, need, r)) return false;
      need += sizeof(Dec) + blocks * (KEEP[sc] * 2 + 8) + band_of(sc) * d.nc + (size_t) ow * 4;
      if (!step && r.need_full == 0) r.need_full = need;
      if (need <= mem_max || sc >= SC_MAX || sc >= 2) break;  // never below 1/4 for the room: 1/8 is a blur
    }
    r.side = side;
    if (need <= mem_max || step == FIT_STEPS) break;
    side = biggest(boxes, ow, oh, side) * 2 / 3;
  }
  r.sw = ow, r.sh = oh, r.sc = sc, r.need = need;
  if (need > mem_max) { r.err = too_big(need, mem_max); return false; }
  d.keep_for(8 >> sc);
  bool ok = true;
  for (int i = 0; i < d.nc && ok; i++) {
    Comp &c = d.c[i];
    const size_t nbk = (size_t) c.bw * c.bh;
    c.co = (int16_t *) FW_IMGDEC_ALLOC(nbk * d.keep * 2);
    c.nz = (uint64_t *) FW_IMGDEC_ALLOC(nbk * 8);
    ok = c.co && c.nz;
    if (ok) memset(c.co, 0, nbk * d.keep * 2), memset(c.nz, 0, nbk * 8);
  }
  const size_t bandsz = band_of(sc);
  uint8_t *band[3] = {nullptr, nullptr, nullptr}, *row = nullptr;
  for (int i = 0; i < d.nc && ok; i++) ok = (band[i] = (uint8_t *) FW_IMGDEC_ALLOC(bandsz)) != nullptr;
  if (ok) ok = (row = (uint8_t *) FW_IMGDEC_ALLOC((size_t) ow * 4)) != nullptr;
  auto drop = [&]() {
    for (int i = 0; i < d.nc; i++) {
      if (d.c[i].co) FW_IMGDEC_FREE(d.c[i].co);
      if (d.c[i].nz) FW_IMGDEC_FREE(d.c[i].nz);
      if (band[i]) FW_IMGDEC_FREE(band[i]);
      d.c[i].co = nullptr, d.c[i].nz = nullptr, band[i] = nullptr;
    }
    if (row) FW_IMGDEC_FREE(row);
    row = nullptr;
  };
  if (!ok || !begin(k, boxes.size(), r)) { drop(); r.err = "out of memory"; return false; }
  d.at = 2;  // the tables before the frame header too
  if (!body(d)) { drop(); r.err = d.err; release(r); return false; }
  for (int i = 0; i < d.nc; i++)
    if (!d.have_q[d.c[i].tq]) { drop(); r.err = "broken picture (JPEG: no table)"; return false; }
  // the output: one MCU row at a time, each component's blocks at N x N into its band, then the picture's rows
  const int N = 8 >> sc;
  float cs[64];
  for (int x = 0; x < N; x++)
    for (int u = 0; u < 8; u++) cs[x * 8 + u] = u < N ? (u ? 1.0f : 0.70710678f) * cosf((2 * x + 1) * u * 3.14159265f / (2 * N)) : 0;
  const int mh = d.vmax * N;  // the picture rows of one MCU row
  for (int my = 0; my < d.mcuy; my++) {
    for (int i = 0; i < d.nc; i++) {
      Comp &c = d.c[i];
      const int stride = c.bw * N;
      for (int yy = 0; yy < c.v; yy++)
        for (int bx = 0; bx < c.bw; bx++) {
          const int by = my * c.v + yy;
          const int16_t *co = c.co + ((size_t) by * c.bw + bx) * d.keep;
          idct(co, d.slot, d.q[c.tq], N, cs, band[i] + (size_t) yy * N * stride + bx * N, stride);
        }
    }
    for (int yy = 0; yy < mh; yy++) {
      const int y = my * mh + yy;
      if (y >= oh) break;
      for (int x = 0; x < ow; x++) {
        uint8_t *o = row + (size_t) x * 4;
        if (d.nc == 1) {
          const uint8_t g = band[0][(size_t) yy * d.c[0].bw * N + x];
          o[0] = o[1] = o[2] = g;
        } else {
          int s[3];
          for (int i = 0; i < 3; i++) {
            const Comp &c = d.c[i];
            s[i] = band[i][(size_t) (yy * c.v / d.vmax) * c.bw * N + x * c.h / d.hmax];
          }
          const float Y = (float) s[0], cb = (float) s[1] - 128.0f, cr = (float) s[2] - 128.0f;
          const int R = (int) lrintf(Y + 1.402f * cr), G = (int) lrintf(Y - 0.344136f * cb - 0.714136f * cr), B = (int) lrintf(Y + 1.772f * cb);
          o[0] = (uint8_t) (R < 0 ? 0 : R > 255 ? 255 : R);
          o[1] = (uint8_t) (G < 0 ? 0 : G > 255 ? 255 : G);
          o[2] = (uint8_t) (B < 0 ? 0 : B > 255 ? 255 : B);
        }
        o[3] = 255;
      }
      feed(k, boxes.size(), row);
    }
  }
  drop();
  if (!finish(k, boxes.size(), r)) { r.err = "broken picture (JPEG: cut short)"; return false; }
  return true;
}

}  // namespace fw::imgdec::detail
