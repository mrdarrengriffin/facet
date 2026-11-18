// Grey-safe 16-bit blending (#39). The panels draw in RGB565: red and blue have 5 bits (steps of 8), green has 6
// (steps of 4). LVGL mixes two RGB565 colours (lv_color_16_16_mix: every fill, text edge and picture drawn at an
// opacity) with 32 levels and rounds each channel down, so between two near-greys green moves a step before red and
// blue do: a fading white card over the light canvas sat at (247,251,247), mint, and fading text turned greenish.
// Here the mix of two near-greys (each colour's channels within GREY of each other) is worked out in 8 bits and
// rounded to RGB565: red and blue to their nearest level, green to the level that keeps the true colour's tint
// (green against red and against blue; never greener than it); any other pair is mixed as LVGL does. The
// build links it in place of LVGL's own (-Wl,--wrap=lv_color_16_16_mix: renderer/build.sh, tools/asan.sh,
// esphome/packages/workbench.yaml), so LVGL itself stays as released. Check: tests/functional/fade_tint.test.mjs.
#include <cstdint>
#include <cstdlib>

namespace {
constexpr int GREY = 24;  // a near-grey: its 8-bit channels at most this far apart (the themes' canvas, text and lines)
inline int e5(int v) { return (v << 3) | (v >> 2); }
inline int e6(int v) { return (v << 2) | (v >> 4); }
inline int div255(int x) { x += 128; return (x + (x >> 8)) >> 8; }  // x / 255, rounded (x <= 65280)
inline bool grey(int r, int g, int b) {
  const int hi = r > g ? (r > b ? r : b) : (g > b ? g : b), lo = r < g ? (r < b ? r : b) : (g < b ? g : b);
  return hi - lo <= GREY;
}
// LVGL 9.5's lv_color_16_16_mix (misc/lv_color.c), for every pair that is not two near-greys
inline uint16_t lvgl_mix(uint16_t c1, uint16_t c2, uint8_t mix) {
  const uint32_t m = ((uint32_t) mix + 4) >> 3;
  const uint32_t bg = (uint32_t) (c2 | ((uint32_t) c2 << 16)) & 0x7E0F81F, fg = (uint32_t) (c1 | ((uint32_t) c1 << 16)) & 0x7E0F81F;
  const uint32_t r = ((((fg - bg) * m) >> 5) + bg) & 0x7E0F81F;
  return (uint16_t) ((r >> 16) | r);
}
}  // namespace

extern "C" uint16_t __wrap_lv_color_16_16_mix(uint16_t c1, uint16_t c2, uint8_t mix) {
  if (mix == 255) return c1;
  if (mix == 0) return c2;
  if (c1 == c2) return c1;
  const int fr = e5(c1 >> 11), fg = e6((c1 >> 5) & 63), fb = e5(c1 & 31);
  const int br = e5(c2 >> 11), bgr = e6((c2 >> 5) & 63), bb = e5(c2 & 31);
  if (!grey(fr, fg, fb) || !grey(br, bgr, bb)) return lvgl_mix(c1, c2, mix);
  const int m = mix, n = 255 - m;
  const int r = div255(fr * m + br * n), g = div255(fg * m + bgr * n), b = div255(fb * m + bb * n);  // the true colour
  // red and blue rounded to their 5-bit level; green the 6-bit level near the true colour (one below, at, one above)
  // that keeps its tint against red and against blue (x3), never greener than it (x30), then nearest. Three tries, so
  // a full-screen fade or a long scroll of anti-aliased text costs little more than LVGL's own mix
  const int R = (r * 31 + 127) / 255, B = (b * 31 + 127) / 255, er = e5(R), eb = e5(B), hi = er > eb ? er : eb;
  const int gl = g * 63 / 255, tint = g - (r > b ? r : b);
  int best = 1 << 30, G = gl;
  for (int k = gl - 1; k <= gl + 1; k++) {
    if (k < 0 || k > 63) continue;
    const int eg = e6(k), over = (eg - hi) - tint;
    const int err = 3 * (abs((eg - er) - (g - r)) + abs((eg - eb) - (g - b))) + 30 * (over > 0 ? over : 0) + abs(eg - g);
    if (err < best) best = err, G = k;
  }
  const uint16_t out = (uint16_t) (R << 11 | G << 5 | B);
  return out;
}
