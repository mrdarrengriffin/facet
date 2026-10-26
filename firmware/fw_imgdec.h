// Picture decode for the panel's own image fetch (fw_images.h; renderer core/images.h piece 2): the downloaded bytes ->
// RGBA8888 for fw::img::put(). No ESP-IDF calls, so the host ASan test (firmware/tests/imgdec) runs this same code.
//   PNG  = pngle (fw_pngle.c + its trimmed miniz inflate, MIT; the decoder ESPHome's online_image uses): streams the
//          rows, so memory = ~45 KB of decoder state (the 32 KB inflate window) + the RGBA out; size checked from the
//          header before the out is allocated. Palette, grey, 16-bit, alpha (tRNS) and interlaced PNGs
//   JPEG = TJpgDec R0.03 (fw_tjpgd.c, a ~4 KB work area, decodes block by block straight into the RGBA out): baseline
//          only (progressive = "not supported"); a photo with more pixels than max_px is shrunk while decoding by
//          1/2, 1/4 or 1/8 (the decoder's own descaling), so a camera-sized JPEG costs only the small result
//   anything else = "not a picture" (WebP, GIF, SVG, BMP named as such: "<type> isn't supported on the panel")
// Memory: every buffer through FW_IMGDEC_ALLOC (the panel: PSRAM). mem_max = the most the decode may hold at once
// (inflate + out); over = "too big", decided before any big allocation. Out-of-memory mid-decode = "out of memory".
#pragma once
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
#ifndef FW_IMGDEC_ALLOC
#ifdef ESP_PLATFORM  // the panel: PSRAM (pictures never belong in internal RAM)
#include "esp_heap_caps.h"
#define FW_IMGDEC_ALLOC(n) heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
#define FW_IMGDEC_FREE(p) heap_caps_free(p)
#else
#define FW_IMGDEC_ALLOC(n) malloc(n)
#define FW_IMGDEC_FREE(p) free(p)
#endif
#endif

namespace fw::imgdec {

struct Out {
  int w = 0, h = 0;
  uint8_t *rgba = nullptr;  // w * h * 4 (FW_IMGDEC_ALLOC), free with release()
  std::string err;          // the reason when decode() returns false (the words the component shows)
};
inline void release(Out &o) {
  if (o.rgba) FW_IMGDEC_FREE(o.rgba);
  o.rgba = nullptr;
}

// what the bytes are: png | jpeg | gif | webp | bmp | svg | "" (the hub proxy's sniff, apps/hub api/wasm/img.ts)
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
    uint8_t *nb = (uint8_t *) FW_IMGDEC_ALLOC(r);
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

struct Png {
  Out *o;
  long max_px;
  size_t mem_max;
  bool refused = false;  // too big / out of memory: o->err says which
  bool done = false;     // IEND reached: the whole picture came
};
inline void png_init(pngle_t *p, uint32_t w, uint32_t h) {
  Png *u = (Png *) pngle_get_user_data(p);
  // pngle's own state (~45 KB with the inflate window) + two scanlines + the RGBA out
  const unsigned long long need = (unsigned long long) w * h * 4 + 48 * 1024 + (unsigned long long) w * 16;
  if (!w || !h || (unsigned long long) w * h > (unsigned long long) u->max_px || need > u->mem_max) { u->o->err = "too big"; u->refused = true; return; }
  u->o->rgba = (uint8_t *) FW_IMGDEC_ALLOC((size_t) w * h * 4);
  if (!u->o->rgba) { u->o->err = "out of memory"; u->refused = true; return; }
  memset(u->o->rgba, 0, (size_t) w * h * 4);
  u->o->w = (int) w; u->o->h = (int) h;
}
inline void png_draw(pngle_t *p, uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint8_t rgba[4]) {
  Png *u = (Png *) pngle_get_user_data(p);
  Out &o = *u->o;
  if (!o.rgba) return;
  // one pixel, or an interlaced pass's block (later passes paint over it): clipped to the picture
  for (uint32_t yy = y; yy < y + h && yy < (uint32_t) o.h; yy++)
    for (uint32_t xx = x; xx < x + w && xx < (uint32_t) o.w; xx++) memcpy(o.rgba + ((size_t) yy * o.w + xx) * 4, rgba, 4);
}
inline bool png(const uint8_t *b, size_t n, long max_px, size_t mem_max, Out &o) {
  pngle_t *p = pngle_new();
  if (!p) { o.err = "out of memory"; return false; }
  Png u{&o, max_px, mem_max};
  pngle_set_user_data(p, &u);
  pngle_set_init_callback(p, png_init);
  pngle_set_draw_callback(p, png_draw);
  pngle_set_done_callback(p, [](pngle_t *q) { ((Png *) pngle_get_user_data(q))->done = true; });
  size_t at = 0;
  bool ok = true;
  while (at < n && !u.refused) {  // fed in 1 KB steps (as the bytes would come off the network)
    const size_t k = n - at < 1024 ? n - at : 1024;
    const int r = pngle_feed(p, b + at, k);
    if (r < 0) { ok = false; break; }
    if (r == 0) break;  // wants more than there is: truncated
    at += (size_t) r;
  }
  const bool whole = ok && !u.refused && u.done && o.rgba;
  if (!whole && !u.refused) o.err = std::string("broken picture (PNG: ") + (ok ? "cut short" : pngle_error(p)) + ")";
  pngle_destroy(p);
  if (!whole) { release(o); o.w = o.h = 0; return false; }
  return true;
}

struct Jpg {
  const uint8_t *b;
  size_t n, at;
  Out *o;
  int ow, oh;
};
inline size_t jin(JDEC *jd, uint8_t *buf, size_t len) {
  Jpg *j = (Jpg *) jd->device;
  const size_t k = len < j->n - j->at ? len : j->n - j->at;
  if (buf) memcpy(buf, j->b + j->at, k);
  j->at += k;
  return k;
}
inline int jout(JDEC *jd, void *bitmap, JRECT *r) {
  Jpg *j = (Jpg *) jd->device;
  const uint8_t *s = (const uint8_t *) bitmap;
  // the block as decoded (its edge may pass the picture's: clipped here)
  for (int y = r->top; y <= r->bottom; y++)
    for (int x = r->left; x <= r->right; x++, s += 3) {
      if (x >= j->ow || y >= j->oh) continue;
      uint8_t *d = j->o->rgba + ((size_t) y * j->ow + x) * 4;
      d[0] = s[2]; d[1] = s[1]; d[2] = s[0]; d[3] = 255;  // LVGL's TJpgDec writes B, G, R
    }
  return 1;
}
inline bool jpeg(const uint8_t *b, size_t n, long max_px, size_t mem_max, Out &o) {
  constexpr size_t WORK = 4096;  // TJpgDec's pool: tables + one MCU (JD_FASTDECODE 1 needs ~3.5 KB)
  void *work = FW_IMGDEC_ALLOC(WORK);
  if (!work) { o.err = "out of memory"; return false; }
  JDEC jd;
  Jpg j{b, n, 0, &o, 0, 0};
  JRESULT r = jd_prepare(&jd, jin, work, WORK, &j);
  if (r != JDR_OK) {
    FW_IMGDEC_FREE(work);
    o.err = r == JDR_FMT3 ? "JPEG type isn't supported on the panel (progressive)" : r == JDR_MEM1 ? "JPEG type isn't supported on the panel" : "broken picture (JPEG)";
    return false;
  }
  // shrink while decoding (1/1, 1/2, 1/4, 1/8) until the result fits max_px and mem_max
  int sc = 0;
  for (;; sc++) {
    if (sc > 3) { FW_IMGDEC_FREE(work); o.err = "too big"; return false; }
    const long w = (jd.width + (1 << sc) - 1) >> sc, h = (jd.height + (1 << sc) - 1) >> sc;
    if ((long long) w * h <= max_px && (unsigned long long) w * h * 4 + WORK <= mem_max) { j.ow = (int) w; j.oh = (int) h; break; }
  }
  o.rgba = (uint8_t *) FW_IMGDEC_ALLOC((size_t) j.ow * j.oh * 4);
  if (!o.rgba) { FW_IMGDEC_FREE(work); o.err = "out of memory"; return false; }
  memset(o.rgba, 0, (size_t) j.ow * j.oh * 4);
  o.w = j.ow; o.h = j.oh;
  r = jd_decomp(&jd, jout, (uint8_t) sc);
  FW_IMGDEC_FREE(work);
  if (r != JDR_OK && r != JDR_INP) {  // a truncated file (JDR_INP) still shows what came: like a browser
    release(o);
    o.w = o.h = 0;
    o.err = "broken picture (JPEG)";
    return false;
  }
  return true;
}
}  // namespace detail

// bytes -> o (RGBA8888). max_px = the most pixels the result may have (images.h budget().src_max_px), mem_max = the most
// memory the decode may hold at once. false = o.err says why ("too big", "not a picture", ...); o holds nothing then.
inline bool decode(const uint8_t *b, size_t n, long max_px, size_t mem_max, Out &o) {
  o = Out{};
  const char *t = sniff(b, n);
  if (!strcmp(t, "png")) return detail::png(b, n, max_px, mem_max, o);
  if (!strcmp(t, "jpeg")) return detail::jpeg(b, n, max_px, mem_max, o);
  if (*t) { o.err = std::string(t == std::string("svg") ? "SVG" : t == std::string("webp") ? "WebP" : t == std::string("gif") ? "GIF" : "BMP") + " isn't supported on the panel"; return false; }
  o.err = "not a picture";
  return false;
}

}  // namespace fw::imgdec
