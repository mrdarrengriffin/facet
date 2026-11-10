// Picture decode for the panel's own image fetch (fw_images.h; renderer core/images.h): the downloaded bytes ->
// the pixels of every view that waits for them, decoded row by row straight into each view's box and format (renderer
// core/img_scale.h Sink: RGB565, + A8 only when see-through). No full-size copy of the picture is ever made. No ESP-IDF
// calls, so the host ASan tests (firmware/tests/imgdec, imgfetch) run this same code.
//   PNG  = pngle (fw_pngle.c + its trimmed miniz inflate, MIT; the decoder ESPHome's online_image uses): ~45 KB of
//          decoder state (the 32 KB inflate window) + one row; size checked from the header before anything big is
//          allocated. Palette, grey, 16-bit, alpha (tRNS) and interlaced PNGs (interlaced only: one full RGBA copy, the
//          passes paint over each other)
//   JPEG = TJpgDec R0.03 (fw_tjpgd.c, a ~4 KB work area, one band of MCU rows): baseline only (progressive = "not
//          supported"). Shrunk while decoding (1/2, 1/4: the decoder's own descaling) to the smallest step that still
//          covers every box it is for, never below (a smaller step blown up = blocky art). LVGL's copy of TJpgDec
//          left out the descale stage: fw_tjpgd.c has ChaN's back for 1/2 and 1/4 (without it every other row came out
//          shifted, the "interlaced" art). Never 1/8: its output stage is missing too, so a 1/8 decode is garbage
//   JPEG on a chip with a JPEG engine + PPA (the ESP32-P4): fw_imgdec_hw.h first (hw_jpeg), this path when it says no
//   anything else = "not a picture" (WebP, GIF, SVG, BMP named as such: "<type> isn't supported on the panel")
// A picture must be whole: a PNG without its end (IEND) or a JPEG that runs out of bytes = "broken picture (... cut
// short)", never half a picture.
// Size: every view at its own box size (img_scale.h stored; an area filter down, bilinear up, never nearest).
// Memory: mem_max = the most the decode may hold at once (outputs + rows + decoder). It is worked out from the header
// before any big allocation. Views that don't fit at their box size are kept smaller, the biggest view's side two thirds
// at a time, at most FIT_STEPS times (LVGL scales them up smoothly when drawn: fx::Pen::image); still over = "too big
// (needs N KB, M KB free)". Out-of-memory mid-way = "out of memory".
#pragma once
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
extern "C" {
#include "fw_tjpgd.h"
}
extern "C" {
#include "fw_pngle.h"
}
#include <memory>
#include <new>
#include <vector>
#include "img_scale.h"
#ifndef FW_IMGDEC_ALLOC
#ifdef ESP_PLATFORM  // the panel: PSRAM (pictures never belong in internal RAM)
#include "esp_heap_caps.h"
#include "img_arena.h"
#define FW_IMGDEC_ALLOC(n) fw::img::arena::alloc(n)  // the pictures' own memory first (img_arena.h)
#define FW_IMGDEC_FREE(p) fw::img::arena::free(p)
#define FW_IMGDEC_BODY_ALLOC(n) fw::img::arena::alloc_shared_first(n)  // the download: shared PSRAM first
#else
#define FW_IMGDEC_ALLOC(n) malloc(n)
#define FW_IMGDEC_FREE(p) free(p)
#endif
#endif
#ifndef FW_IMGDEC_BODY_ALLOC
#define FW_IMGDEC_BODY_ALLOC(n) FW_IMGDEC_ALLOC(n)
#endif

namespace fw::imgdec {

using fw::img::Box;
using fw::img::Pixels;
// a decode's result: one finished Pixels per box (in the boxes' order), or err
struct Result {
  std::vector<Pixels> px;
  std::string err;           // the reason when decode() returns false (the words the component shows)
  int sw = 0, sh = 0, sc = 0;  // the source as decoded (after a JPEG's 1/2^sc)
  int side = 0;              // 0 = every view at its box size; else the memory fallback's cap on the long side (fit)
  size_t need = 0;           // the bytes the decode held at most
  size_t need_full = 0;      // the bytes it would have held with every view at its box size (side = 0)
  size_t room = 0;           // mem_max: what it was allowed
  bool hw = false;           // decoded by the chip's JPEG engine (fw_imgdec_hw.h), else in software
};
// a board's hardware JPEG decode (the ESP32-P4: fw_imgdec_hw.h, installed by fw_images.h): tried first for every JPEG;
// false = the software path below decodes it (progressive, out of memory, any engine error). None = software only
using HwJpeg = bool (*)(const uint8_t *b, size_t n, long max_px, size_t mem_max, const std::vector<Box> &boxes, Result &r);
inline HwJpeg &hw_jpeg() { static HwJpeg f = nullptr; return f; }
inline void release(Result &r) {
  for (auto &p : r.px) fw::img::free_pixels(p);
  r.px.clear();
}

// what the bytes are: png | jpeg | gif | webp | bmp | svg | "" (the same sniff as the workbench's image fetch)
inline const char *sniff(const uint8_t *b, size_t n) {
  if (n < 12) return "";
  if (b[0] == 0x89 && !memcmp(b + 1, "PNG", 3)) return "png";
  if (b[0] == 0xff && b[1] == 0xd8) return "jpeg";
  if (!memcmp(b, "GIF", 3)) return "gif";
  if (!memcmp(b, "RIFF", 4) && !memcmp(b + 8, "WEBP", 4)) return "webp";
  if (b[0] == 'B' && b[1] == 'M') return "bmp";
  const size_t m = n < 256 ? n : 256;
  size_t i = 0;
  while (i < m && (b[i] == ' ' || b[i] == '\t' || b[i] == '\r' || b[i] == '\n')) i++;
  if (i + 5 < m && b[i] == '<' && (!memcmp(b + i + 1, "svg", 3) || !memcmp(b + i + 1, "?xml", 4)))
    for (size_t j = i; j + 4 <= n && j < 4096; j++)
      if (!memcmp(b + j, "<svg", 4)) return "svg";
  return "";
}

// the download, capped (images.h budget().body_max): grows in steps, refuses the byte past the cap ("too big (over N KB)")
struct Body {
  uint8_t *b = nullptr;
  size_t n = 0, cap = 0, room = 0;
  std::string err;
  explicit Body(size_t max) : cap(max) {}
  Body(const Body &) = delete;
  Body &operator=(const Body &) = delete;
  ~Body() { if (b) FW_IMGDEC_FREE(b); }
  void over() {
    char t[48];
    if (cap >= 1024 * 1024) snprintf(t, sizeof t, "too big (over %u MB)", (unsigned) (cap / (1024 * 1024)));
    else snprintf(t, sizeof t, "too big (over %u KB)", (unsigned) (cap / 1024));
    err = t;
  }
  // expect = Content-Length when the server sent one (-1 = unknown): over the cap = refused before reading anything
  bool expect(long long len) {
    if (len > (long long) cap) { over(); return false; }
    return len <= 0 || grow((size_t) len);
  }
  bool grow(size_t want) {
    if (want <= room) return true;
    size_t r = room ? room : 16 * 1024;
    while (r < want) r *= 2;
    if (r > cap) r = cap;
    uint8_t *nb = (uint8_t *) FW_IMGDEC_BODY_ALLOC(r);
    if (!nb) { err = "out of memory"; return false; }
    if (b) { memcpy(nb, b, n); FW_IMGDEC_FREE(b); }
    b = nb; room = r;
    return true;
  }
  bool add(const uint8_t *d, size_t k) {
    if (n + k > cap) { over(); return false; }
    if (!grow(n + k)) return false;
    memcpy(b + n, d, k);
    n += k;
    return true;
  }
};

namespace detail {

inline std::string too_big(size_t need, size_t room) {
  char t[64];
  snprintf(t, sizeof t, "too big (needs %u KB, %u KB free)", (unsigned) ((need + 1023) / 1024), (unsigned) (room / 1024));
  return t;
}
// the memory fallback: at most this many steps, each the biggest view's long side x 2 / 3 (480 -> 320 -> 213)
constexpr int FIT_STEPS = 2;
// the long side of the biggest view these boxes keep from a sw x sh source, capped at side (0 = their boxes)
inline int biggest(const std::vector<Box> &boxes, int sw, int sh, int side) {
  int m = 0;
  for (const Box &bx : boxes) {
    int ow, oh;
    fw::img::stored(bx, sw, sh, side, ow, oh);
    m = std::max(m, std::max(ow, oh));
  }
  return m;
}
// one Sink per box, planned for a sw x sh source; false = out of memory (r.err)
inline bool plan(const std::vector<Box> &boxes, int sw, int sh, int max_side, bool may_alpha, std::unique_ptr<fw::img::Sink[]> &k, size_t &need, Result &r) {
  k.reset(new (std::nothrow) fw::img::Sink[boxes.size()]);
  if (!k) { r.err = "out of memory"; return false; }
  need = 0;
  for (size_t i = 0; i < boxes.size(); i++) {
    if (!k[i].plan(boxes[i], sw, sh, max_side, may_alpha)) { r.err = "out of memory"; return false; }
    need += k[i].need();
  }
  return true;
}
inline bool begin(std::unique_ptr<fw::img::Sink[]> &k, size_t n, Result &r) {
  for (size_t i = 0; i < n; i++)
    if (!k[i].begin()) { r.err = "out of memory"; return false; }
  return true;
}
inline void feed(std::unique_ptr<fw::img::Sink[]> &k, size_t n, const uint8_t *row) {
  for (size_t i = 0; i < n; i++) k[i].row(row);
}
inline bool finish(std::unique_ptr<fw::img::Sink[]> &k, size_t n, Result &r) {
  r.px.resize(n);
  for (size_t i = 0; i < n; i++)
    if (!k[i].finish(r.px[i])) { release(r); return false; }
  return true;
}

struct Png {
  const std::vector<Box> *boxes;
  long max_px;
  size_t mem_max;
  Result *r;
  std::unique_ptr<fw::img::Sink[]> k;
  uint8_t *buf = nullptr;  // one row (or, interlaced, the whole picture)
  bool interlaced = false;
  uint32_t w = 0, h = 0;
  bool refused = false;  // too big / out of memory: r->err says which
  bool done = false;     // IEND reached: the whole picture came
};
inline void png_init(pngle_t *p, uint32_t w, uint32_t h) {
  Png *u = (Png *) pngle_get_user_data(p);
  if (!w || !h || (unsigned long long) w * h > (unsigned long long) u->max_px) { u->r->err = "too big"; u->refused = true; return; }
  const pngle_ihdr_t *ih = pngle_get_ihdr(p);
  u->interlaced = ih && ih->interlace;
  // an A8 plane only for a picture that can be see-through (an alpha channel or tRNS) or a round corner: an opaque PNG
  // costs 2 bytes a pixel like a JPEG and its screen-size view goes into a reserved slot (#186: every PNG planned 3
  // bytes a pixel outside the slots, so idle art from a PNG source needed 698 KB, its 213 px fallback 198 KB)
  const bool may_alpha = pngle_may_alpha(p);
  size_t need = 0;
  for (int step = 0, side = 0;; step++) {  // every view at its box size; smaller only when that doesn't fit (fit)
    if (!plan(*u->boxes, (int) w, (int) h, side, may_alpha, u->k, need, *u->r)) { u->refused = true; return; }
    // pngle's own state (~45 KB with the inflate window) + its two scanlines + our row (interlaced: the picture)
    need += 48 * 1024 + (size_t) w * 16 + (u->interlaced ? (size_t) w * h * 4 : (size_t) w * 4);
    u->r->need = need, u->r->side = side;
    if (!step) u->r->need_full = need;
    if (need <= u->mem_max || step == FIT_STEPS) break;
    side = biggest(*u->boxes, (int) w, (int) h, side) * 2 / 3;
  }
  if (need > u->mem_max) { u->r->err = too_big(need, u->mem_max); u->refused = true; return; }
  const size_t bytes = u->interlaced ? (size_t) w * h * 4 : (size_t) w * 4;
  u->buf = (uint8_t *) FW_IMGDEC_ALLOC(bytes);
  if (!u->buf || !begin(u->k, u->boxes->size(), *u->r)) { u->r->err = "out of memory"; u->refused = true; return; }
  memset(u->buf, 0, bytes);
  u->w = w, u->h = h, u->r->sw = (int) w, u->r->sh = (int) h;
}
inline void png_draw(pngle_t *p, uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint8_t rgba[4]) {
  Png *u = (Png *) pngle_get_user_data(p);
  if (!u->buf || u->refused) return;
  if (!u->interlaced) {  // every pixel in order, one at a time: a row is done at its last pixel
    if (x >= u->w || y >= u->h) return;
    memcpy(u->buf + (size_t) x * 4, rgba, 4);
    if (x == u->w - 1) feed(u->k, u->boxes->size(), u->buf);
    return;
  }
  // an interlaced pass's block (later passes paint over it): clipped to the picture
  for (uint32_t yy = y; yy < y + h && yy < u->h; yy++)
    for (uint32_t xx = x; xx < x + w && xx < u->w; xx++) memcpy(u->buf + ((size_t) yy * u->w + xx) * 4, rgba, 4);
}
inline bool png(const uint8_t *b, size_t n, long max_px, size_t mem_max, const std::vector<Box> &boxes, Result &r) {
  // the size from IHDR first (bytes 16-23): pngle allocates its scanlines from it before our init callback can refuse
  if (n >= 24 && !memcmp(b + 12, "IHDR", 4)) {
    const uint32_t w = (uint32_t) b[16] << 24 | b[17] << 16 | b[18] << 8 | b[19], h = (uint32_t) b[20] << 24 | b[21] << 16 | b[22] << 8 | b[23];
    if (!w || !h || w > (1u << 16) || h > (1u << 16) || (unsigned long long) w * h > (unsigned long long) max_px) { r.err = "too big"; return false; }
  }
  pngle_t *p = pngle_new();
  if (!p) { r.err = "out of memory"; return false; }
  Png u;
  u.boxes = &boxes, u.max_px = max_px, u.mem_max = mem_max, u.r = &r;
  pngle_set_user_data(p, &u);
  pngle_set_init_callback(p, png_init);
  pngle_set_draw_callback(p, png_draw);
  pngle_set_done_callback(p, [](pngle_t *q) { ((Png *) pngle_get_user_data(q))->done = true; });
  size_t at = 0;
  bool ok = true;
  while (at < n && !u.refused) {  // fed in 1 KB steps (as the bytes would come off the network)
    const size_t k = n - at < 1024 ? n - at : 1024;
    const int got = pngle_feed(p, b + at, k);
    if (got < 0) { ok = false; break; }
    if (got == 0) break;  // wants more than there is: truncated
    at += (size_t) got;
  }
  bool whole = ok && !u.refused && u.done && u.buf;
  if (whole && u.interlaced)
    for (uint32_t y = 0; y < u.h; y++) feed(u.k, boxes.size(), u.buf + (size_t) y * u.w * 4);
  if (whole && !finish(u.k, boxes.size(), r)) whole = false, r.err = "broken picture (PNG: cut short)";
  if (!whole && !u.refused && r.err.empty()) r.err = std::string("broken picture (PNG: ") + (ok ? "cut short" : pngle_error(p)) + ")";
  pngle_destroy(p);
  if (u.buf) FW_IMGDEC_FREE(u.buf);
  if (!whole) { release(r); return false; }
  return true;
}

struct Jpg {
  const uint8_t *b;
  size_t n, at;
  std::unique_ptr<fw::img::Sink[]> *k;
  size_t nk;
  int ow, oh;
  uint8_t *band;      // one band of MCU rows, RGBA
  int top = -1, bot = -1;  // the band's rows (picture rows)
  // a stream instead of b / n: more(ctx, buf, len) brings up to len bytes, 0 = the end (or a failure: its own words)
  size_t (*more)(void *, uint8_t *, size_t) = nullptr;
  void *ctx = nullptr;
};
inline size_t jin(JDEC *jd, uint8_t *buf, size_t len) {
  Jpg *j = (Jpg *) jd->device;
  if (j->more) {  // straight from the download: nothing held but TJpgDec's own input buffer
    uint8_t skip[256];
    size_t got = 0;
    while (got < len) {
      size_t want = len - got;
      if (!buf && want > sizeof skip) want = sizeof skip;
      const size_t k = j->more(j->ctx, buf ? buf + got : skip, want);
      if (!k) break;
      got += k;
    }
    return got;
  }
  const size_t k = len < j->n - j->at ? len : j->n - j->at;
  if (buf) memcpy(buf, j->b + j->at, k);
  j->at += k;
  return k;
}
inline void jflush(Jpg *j) {  // the band's rows, in order, into every box
  if (j->top < 0) return;
  for (int y = j->top; y <= j->bot && y < j->oh; y++) feed(*j->k, j->nk, j->band + (size_t) (y - j->top) * j->ow * 4);
  j->top = j->bot = -1;
}
inline int jout(JDEC *jd, void *bitmap, JRECT *r) {
  Jpg *j = (Jpg *) jd->device;
  if ((int) r->top != j->top) { jflush(j); j->top = r->top, j->bot = r->bottom; }  // the next MCU row: the last one is done
  const uint8_t *s = (const uint8_t *) bitmap;
  // the block as decoded (its edge may pass the picture's: clipped here)
  for (int y = r->top; y <= r->bottom; y++)
    for (int x = r->left; x <= r->right; x++, s += 3) {
      if (x >= j->ow || y >= j->oh) continue;
      uint8_t *d = j->band + ((size_t) (y - j->top) * j->ow + x) * 4;
      d[0] = s[2]; d[1] = s[1]; d[2] = s[0]; d[3] = 255;  // LVGL's TJpgDec writes B, G, R
    }
  return 1;
}
inline bool jpeg(const uint8_t *b, size_t n, long max_px, size_t mem_max, const std::vector<Box> &boxes, Result &r,
                 size_t (*more)(void *, uint8_t *, size_t) = nullptr, void *ctx = nullptr) {
  constexpr size_t WORK = 4096;  // TJpgDec's pool: tables + one MCU (JD_FASTDECODE 1 needs ~3.5 KB)
  void *work = FW_IMGDEC_ALLOC(WORK);
  if (!work) { r.err = "out of memory"; return false; }
  JDEC jd;
  std::unique_ptr<fw::img::Sink[]> k;
  Jpg j{b, n, 0, &k, boxes.size(), 0, 0, nullptr};
  j.more = more, j.ctx = ctx;
  JRESULT res = jd_prepare(&jd, jin, work, WORK, &j);
  if (res != JDR_OK) {
    FW_IMGDEC_FREE(work);
    r.err = res == JDR_FMT3 ? "JPEG type isn't supported on the panel (progressive)" : res == JDR_MEM1 ? "JPEG type isn't supported on the panel" : "broken picture (JPEG)";
    return false;
  }
  const auto dim = [&](int sc, int &w, int &h) { w = std::max(1, (int) jd.width >> sc), h = std::max(1, (int) jd.height >> sc); };
  // the fewest pixels within max_px ...
  constexpr int SC_MAX = 2;  // 1/4 at most (the 1/8 output stage is missing: see the top)
  int lo = 0, w, h;
  for (;; lo++) {
    if (lo > SC_MAX) { FW_IMGDEC_FREE(work); r.err = "too big"; return false; }
    dim(lo, w, h);
    if ((long long) w * h <= max_px) break;
  }
  // ... and the smallest step that still covers every view (its stored size: img_scale.h), never below it. Every view
  // at its box size; smaller (and so maybe a smaller step) only when that doesn't fit in mem_max (fit)
  int sc = lo, band_h = 1;
  size_t need = 0;
  for (int step = 0, side = 0;; step++) {
    sc = lo;
    while (sc < SC_MAX) {
      dim(sc + 1, w, h);
      bool covers = true;
      for (const Box &bx : boxes) {
        int ow, oh;
        fw::img::stored(bx, w, h, side, ow, oh);
        covers &= bx.s != fw::img::pixel && ow <= w && oh <= h;
      }
      if (!covers) break;
      sc++;
    }
    dim(sc, j.ow, j.oh);
    // opaque: an A8 plane only for a round corner (the Sink adds it)
    if (!plan(boxes, j.ow, j.oh, side, false, k, need, r)) { FW_IMGDEC_FREE(work); return false; }
    band_h = std::max(1, (jd.msy * 8) >> sc);
    need += WORK + (size_t) j.ow * band_h * 4;
    r.side = side;
    if (!step) r.need_full = need;
    if (need <= mem_max || step == FIT_STEPS) break;
    side = biggest(boxes, j.ow, j.oh, side) * 2 / 3;
  }
  r.sw = j.ow, r.sh = j.oh, r.sc = sc;
  r.need = need;
  if (need > mem_max) { FW_IMGDEC_FREE(work); r.err = too_big(need, mem_max); return false; }
  j.band = (uint8_t *) FW_IMGDEC_ALLOC((size_t) j.ow * band_h * 4);
  if (!j.band || !begin(k, boxes.size(), r)) {
    if (j.band) FW_IMGDEC_FREE(j.band);
    FW_IMGDEC_FREE(work);
    r.err = "out of memory";
    return false;
  }
  res = jd_decomp(&jd, jout, (uint8_t) sc);
  FW_IMGDEC_FREE(work);
  if (res == JDR_OK) jflush(&j);
  FW_IMGDEC_FREE(j.band);
  if (res != JDR_OK) { r.err = res == JDR_INP ? "broken picture (JPEG: cut short)" : "broken picture (JPEG)"; return false; }
  if (!finish(k, boxes.size(), r)) { r.err = "broken picture (JPEG: cut short)"; return false; }
  return true;
}
}  // namespace detail

// bytes -> the pixels of each box (r.px, in the boxes' order). max_px = the most pixels the source may have as decoded
// (images.h budget().src_max_px), mem_max = the most memory the decode may hold at once (views kept smaller when their
// box size doesn't fit: r.side).
// false = r.err says why ("too big", "not a picture", ...); r holds nothing then.
inline bool decode(const uint8_t *b, size_t n, long max_px, size_t mem_max, const std::vector<Box> &boxes, Result &r) {
  r = Result{};
  r.room = mem_max;
  if (boxes.empty()) { r.err = "left the screen"; return false; }
  const char *t = sniff(b, n);
  if (!strcmp(t, "png")) return detail::png(b, n, max_px, mem_max, boxes, r);
  if (!strcmp(t, "jpeg")) {
    if (hw_jpeg() && hw_jpeg()(b, n, max_px, mem_max, boxes, r)) return true;
    r = Result{};  // the hardware said no: the software path from a clean result
    r.room = mem_max;
    return detail::jpeg(b, n, max_px, mem_max, boxes, r);
  }
  if (*t) { r.err = std::string(t == std::string("svg") ? "SVG" : t == std::string("webp") ? "WebP" : t == std::string("gif") ? "GIF" : "BMP") + " isn't supported on the panel"; return false; }
  r.err = "not a picture";
  return false;
}

// a JPEG decoded while it downloads (the software decoder; the panel never holds the whole file: a 640 px cover at
// quality 100 is 300 KB, more than the 480 panel has in one block): more(ctx, buf, len) brings the bytes in order
inline bool decode_jpeg_stream(size_t (*more)(void *, uint8_t *, size_t), void *ctx, long max_px, size_t mem_max,
                               const std::vector<Box> &boxes, Result &r) {
  r = Result{};
  r.room = mem_max;
  if (boxes.empty()) { r.err = "left the screen"; return false; }
  return detail::jpeg(nullptr, 0, max_px, mem_max, boxes, r, more, ctx);
}

}  // namespace fw::imgdec
