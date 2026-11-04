// The picture scaler (core/images.h): source rows in, top to bottom, a view's pixels out, in the format it is drawn
// in (RGB565, + an A8 plane only when something is see-through: a rounded corner, a transparent PNG), dithered to 565
// (pixel art: nearest, never dithered), with the view's round corners and shade. One pass, no full-size copy: it
// holds only the few source rows the next output row needs (a ring) and the output itself.
//   The WASM host feeds it the browser's decoded RGBA rows (images.cpp put); the panel's decoder (facet
//   firmware/fw_imgdec.h) feeds it each row as the JPEG or PNG decode produces it, on its own task, straight into the
//   view's pixels (images.cpp put(src, pixels) adopts them, no copy). No LVGL calls, so it runs off the LVGL task and
//   in the host tests.
// Stored size: a view keeps its box's pixels, except that a box over the board's max_side (budget(): Guition 320) keeps
// max_side px on its long side and is scaled up by LVGL when drawn (fx::Pen::image): 320 x 320 RGB565 = 200 KB where
// 480 x 480 would be 450 KB.
// Memory: every buffer through FW_IMG_MALLOC (the panel: PSRAM), freed with FW_IMG_FREE (free_pixels()).
#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#ifndef FW_IMG_MALLOC
#ifdef ESP_PLATFORM  // the panel: PSRAM (pictures never belong in internal RAM)
#include "esp_heap_caps.h"
#define FW_IMG_MALLOC(n) heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
#define FW_IMG_REALLOC(p, n) heap_caps_realloc(p, n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
#define FW_IMG_FREE(p) heap_caps_free(p)
#else
#define FW_IMG_MALLOC(n) malloc(n)
#define FW_IMG_REALLOC(p, n) realloc(p, n)
#define FW_IMG_FREE(p) free(p)
#endif
#endif

namespace fw::img {
enum Scale : uint8_t { cover, contain, pixel };
constexpr float SHADE_MIN = 0.35f;  // the shade's darkest factor (white -> #595959)

// a view's box: what its pixels are made for
struct Box {
  int w = 0, h = 0, r = 0, shade = 0;
  Scale s = cover;
  bool nearest = false;
  bool operator==(const Box &o) const { return w == o.w && h == o.h && r == o.r && shade == o.shade && s == o.s && nearest == o.nearest; }
};
// a view's finished pixels: w x h RGB565, then (alpha) w x h A8. size = the bytes held (FW_IMG_MALLOC)
struct Pixels {
  Box box;
  int w = 0, h = 0;
  bool alpha = false;
  uint8_t *data = nullptr;
  size_t size = 0;
};
inline void free_pixels(Pixels &p) {
  if (p.data) FW_IMG_FREE(p.data);
  p.data = nullptr, p.size = 0;
}

// the view's size for a sw x sh source: cover = the box; contain = the source fitted inside; pixel = a whole-number
// zoom (or shrink). Then capped to max_side (0 = none) on the long side, pixel art excepted
inline void stored(const Box &v, int sw, int sh, int max_side, int &ow, int &oh) {
  if (v.s == cover) ow = v.w, oh = v.h;
  else if (v.s == contain) {
    const double k = std::min((double) v.w / sw, (double) v.h / sh);
    ow = std::max(1, (int) std::lround(sw * k));
    oh = std::max(1, (int) std::lround(sh * k));
  } else if (sw <= v.w && sh <= v.h) {
    const int k = std::max(1, std::min(v.w / sw, v.h / sh));
    ow = sw * k, oh = sh * k;
  } else {
    const int d = (int) std::ceil(std::max((double) sw / v.w, (double) sh / v.h));
    ow = std::max(1, sw / d), oh = std::max(1, sh / d);
  }
  if (v.s != pixel && max_side > 0 && std::max(ow, oh) > max_side) {
    const double f = (double) max_side / std::max(ow, oh);
    ow = std::max(1, (int) std::lround(ow * f));
    oh = std::max(1, (int) std::lround(oh * f));
  }
}

class Sink {
 public:
  Sink() = default;
  ~Sink() { drop(); }
  Sink(const Sink &) = delete;
  Sink &operator=(const Sink &) = delete;

  // sw x sh source rows into box b (max_side: budget()); may_alpha = room for an A8 plane (a source that may be
  // see-through, or a round corner). Works out the size, the taps and the rows to hold; false = out of memory
  bool plan(const Box &b, int sw, int sh, int max_side, bool may_alpha) {
    drop();
    if (sw < 1 || sh < 1 || b.w < 1 || b.h < 1) return false;
    box_ = b, sw_ = sw, sh_ = sh;
    stored(b, sw, sh, max_side, ow_, oh_);
    int uw, uh;  // uncapped: the corner and the shade shrink with the pixels
    stored(b, sw, sh, 0, uw, uh);
    const double f = std::min(1.0, (double) ow_ / uw);
    r_ = (int) std::lround(b.r * f), shade_ = (int) std::lround(b.shade * f);
    alpha_ = may_alpha || r_ > 0;
    double k = 1, x0 = 0, y0 = 0;
    if (b.s == cover) {
      k = std::max((double) ow_ / sw, (double) oh_ / sh);
      x0 = (sw - ow_ / k) / 2, y0 = (sh - oh_ / k) / 2;
    } else if (b.s == contain) k = (double) ow_ / sw;
    xs_ = (Tap *) FW_IMG_MALLOC(sizeof(Tap) * (size_t) (ow_ + oh_));
    if (!xs_) return false;
    ys_ = xs_ + ow_;
    taps(xs_, ow_, sw, k, x0);
    taps(ys_, oh_, sh, k, y0);
    ring_n_ = 1;
    for (int y = 0; y < oh_; y++) ring_n_ = std::max(ring_n_, last(y) - ys_[y].a + 1);
    return true;
  }
  int w() const { return ow_; }
  int h() const { return oh_; }
  // the bytes begin() takes, on top of plan()'s taps (taps() are counted too: the decode's whole reserve)
  size_t need() const { return px() * (alpha_ ? 3 : 2) + (size_t) ring_n_ * sw_ * 4 + sizeof(Tap) * (size_t) (ow_ + oh_); }
  bool begin() {
    if (!xs_) return false;
    out_ = (uint8_t *) FW_IMG_MALLOC(px() * (alpha_ ? 3 : 2));
    ring_ = (uint8_t *) FW_IMG_MALLOC((size_t) ring_n_ * sw_ * 4);
    if (!out_ || !ring_) { drop(); return false; }
    in_ = 0, y_ = 0, see_ = false;
    return true;
  }
  // the next source row: sw px of R, G, B, A (rows past the last are ignored)
  void row(const uint8_t *rgba) {
    if (!out_ || in_ >= sh_) return;
    memcpy(ring_ + (size_t) (in_ % ring_n_) * sw_ * 4, rgba, (size_t) sw_ * 4);
    while (y_ < oh_ && last(y_) <= in_) emit(y_++);
    in_++;
  }
  // every row came: out = the pixels (RGB565 alone when nothing came out see-through); false = rows missing
  bool finish(Pixels &out) {
    if (!out_ || y_ < oh_) { drop(); return false; }
    out = Pixels{};
    out.box = box_, out.w = ow_, out.h = oh_, out.alpha = see_;
    out.size = px() * (alpha_ ? 3 : 2);
    if (alpha_ && !see_) {  // the A8 plane isn't needed: given back (a shrink in place)
      if (void *p = FW_IMG_REALLOC(out_, px() * 2)) out_ = (uint8_t *) p, out.size = px() * 2;
    }
    out.data = out_;
    out_ = nullptr;
    drop();
    return true;
  }

 private:
  // one output column (or row): its source taps. Box (down): the source span [a, b); bilinear (up): a, b and b's weight
  // w (0-256); nearest: a alone (w = 0, b = a). Worked out once per column and row, so the pixel loop is integer only
  // (the panels' FPUs are single precision: double maths there is software, ~1 s for a full screen)
  struct Tap { int a, b, w; bool box; };
  static constexpr uint8_t BAYER[16] = {0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};
  void taps(Tap *t, int on, int sn, double k, double o) const {
    const bool pix = box_.s == pixel, near = box_.nearest;
    for (int x = 0; x < on; x++) {
      Tap &q = t[x];
      q.box = false, q.w = 0;
      if (pix) q.a = q.b = std::min(sn - 1, x * sn / on);
      else if (near && k >= 1) q.a = q.b = std::clamp((int) std::floor(o + (x + 0.5) / k), 0, sn - 1);
      else if (k >= 1) {
        const double f = std::clamp(o + (x + 0.5) / k - 0.5, 0.0, sn - 1.0);
        q.a = (int) f, q.b = std::min(q.a + 1, sn - 1), q.w = (int) std::lround((f - q.a) * 256);
      } else {
        const int a = (int) std::floor(o + x / k), b = std::max(a + 1, (int) std::ceil(o + (x + 1) / k));
        q.a = std::clamp(a, 0, sn - 1), q.b = std::clamp(b, q.a + 1, sn), q.box = true;
      }
    }
  }
  int last(int y) const { return ys_[y].box ? ys_[y].b - 1 : ys_[y].b; }  // the last source row output row y reads
  const uint8_t *src(int sy) const { return ring_ + (size_t) (sy % ring_n_) * sw_ * 4; }
  size_t px() const { return (size_t) ow_ * oh_; }
  void drop() {
    if (xs_) FW_IMG_FREE(xs_);
    if (out_) FW_IMG_FREE(out_);
    if (ring_) FW_IMG_FREE(ring_);
    xs_ = ys_ = nullptr, out_ = ring_ = nullptr;
  }
  void emit(int y) {
    const Tap &ty = ys_[y];
    const bool pix = box_.s == pixel, near = box_.nearest;
    const int ow = ow_, oh = oh_, rad = std::min(r_, std::min(ow, oh) / 2);
    uint16_t *col = (uint16_t *) out_ + (size_t) y * ow;
    uint8_t *al = alpha_ ? out_ + px() * 2 + (size_t) y * ow : nullptr;
    int shade = 256;  // the shade: darker towards the bottom edge (images.h), so light text reads over it
    if (shade_ > 0 && y >= oh - shade_) {
      const float t = std::min(1.0f, (y - (oh - shade_) + 0.5f) / (shade_ / 2.0f));
      shade = (int) std::lround(256 * (1 - (1 - SHADE_MIN) * t * t * (3 - 2 * t)));
    }
    const bool yc = y < rad || y >= oh - rad;
    for (int x = 0; x < ow; x++) {
      const Tap &tx = xs_[x];
      uint32_t a = 0, r = 0, g = 0, b = 0, n = 1;  // premultiplied sums: R = r / a
      if (tx.box) {
        n = 0;
        for (int sy = ty.a; sy < ty.b; sy++) {
          const uint8_t *p = src(sy) + (size_t) tx.a * 4;
          for (int sx = tx.a; sx < tx.b; sx++, p += 4, n++) a += p[3], r += p[0] * p[3], g += p[1] * p[3], b += p[2] * p[3];
        }
      } else {  // bilinear (nearest: one tap at weight 65536): weights in 1/65536
        const int wx[2] = {256 - tx.w, tx.w}, wy[2] = {256 - ty.w, ty.w}, sx[2] = {tx.a, tx.b}, sy[2] = {ty.a, ty.b};
        for (int q = 0; q < 4; q++) {
          const uint32_t w = (uint32_t) (wx[q & 1] * wy[q >> 1]);
          if (!w) continue;
          const uint8_t *p = src(sy[q >> 1]) + (size_t) sx[q & 1] * 4;
          const uint32_t pa = p[3] * w;
          a += pa, r += (p[0] * pa) >> 8, g += (p[1] * pa) >> 8, b += (p[2] * pa) >> 8;
        }
        n = 65536;
        if (a) r = (uint32_t) (((uint64_t) r << 8) / a), g = (uint32_t) (((uint64_t) g << 8) / a), b = (uint32_t) (((uint64_t) b << 8) / a);
      }
      int R, G, Bc, A;
      if (tx.box) {
        R = a ? (int) (r / a) : 0, G = a ? (int) (g / a) : 0, Bc = a ? (int) (b / a) : 0;
        A = n ? (int) ((a + n / 2) / n) : 0;
      } else {
        R = (int) r, G = (int) g, Bc = (int) b;
        A = (int) ((a + 32768) >> 16);
      }
      if (rad > 0 && yc && (x < rad || x >= ow - rad)) {  // rounded corner: anti-aliased coverage of the corner circle
        const float cx = x < rad ? rad : ow - rad, cy = y < rad ? rad : oh - rad;
        const float d = std::sqrt((x + 0.5f - cx) * (x + 0.5f - cx) + (y + 0.5f - cy) * (y + 0.5f - cy));
        A = (int) std::lround(A * std::clamp(rad - d + 0.5f, 0.0f, 1.0f));
      }
      if (shade < 256) R = R * shade >> 8, G = G * shade >> 8, Bc = Bc * shade >> 8;
      if (!pix && !near) {  // ordered dither to 565 (photos band otherwise); pixel art stays exact
        const int t = BAYER[(x & 3) + (y & 3) * 4];
        R = std::min(255, R + t / 2); G = std::min(255, G + t / 4); Bc = std::min(255, Bc + t / 2);
      }
      col[x] = (uint16_t) (((R >> 3) << 11) | ((G >> 2) << 5) | (Bc >> 3));
      if (al) al[x] = (uint8_t) std::clamp(A, 0, 255), see_ |= A < 255;
    }
  }

  Box box_;
  int sw_ = 0, sh_ = 0, ow_ = 0, oh_ = 0, r_ = 0, shade_ = 0, ring_n_ = 1, in_ = 0, y_ = 0;
  bool alpha_ = false, see_ = false;
  Tap *xs_ = nullptr, *ys_ = nullptr;
  uint8_t *out_ = nullptr, *ring_ = nullptr;
};
}  // namespace fw::img
