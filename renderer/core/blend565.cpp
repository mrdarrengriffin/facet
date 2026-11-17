// Grey-safe 16-bit blending (#39). The panels draw in RGB565: red and blue have 5 bits (steps of 8), green has 6
// (steps of 4). LVGL mixes two RGB565 colours (lv_color_16_16_mix: every fill, text edge and picture drawn at an
// opacity) with 32 levels and rounds each channel down, so between two near-greys green moves a step before red and
// blue do: a fading white card over the light canvas sat at (247,251,247), mint, and fading text turned greenish.
// Here the mix of two near-greys (each colour's channels within GREY of each other) is worked out in 8 bits and
// rounded to the RGB565 colour (each channel one step up or down) that keeps the true colour's tint (green against
// red, green against blue; never greener than it) at the brightness LVGL's mix gives; any other pair is mixed as LVGL does. The
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
  const int rl = r * 31 / 255, gl = g * 63 / 255, bl = b * 31 / 255;
  // the brightness LVGL's own mix gives (its rounding down keeps thin anti-aliased lines as dark as the contrast
  // rules measured them): only the tint changes
  const uint16_t lv = lvgl_mix(c1, c2, mix);
  const int lum = e5(lv >> 11) + e6((lv >> 5) & 63) + e5(lv & 31);
  int best = 1 << 30;
  uint16_t out = c2;
  // each channel: one step either side of the true colour, and LVGL's own level (a thin line's darkest edge)
  const int lr = lv >> 11, lg = (lv >> 5) & 63, lb = lv & 31;
  const int r0 = rl < lr ? rl : lr, r1 = rl + 1 > lr ? rl + 1 : lr, g0 = gl < lg ? gl : lg, g1 = gl + 1 > lg ? gl + 1 : lg;
  const int b0 = bl < lb ? bl : lb, b1 = bl + 1 > lb ? bl + 1 : lb;
  for (int R = r0; R <= r1 && R <= 31; R++)
    for (int G = g0; G <= g1 && G <= 63; G++)
      for (int B = b0; B <= b1 && B <= 31; B++) {
        const int er = e5(R), eg = e6(G), eb = e5(B);
        // the tint first (x3: green against red and against blue), never greener than the true colour (x30), then
        // the brightness
        const int over = (eg - (er > eb ? er : eb)) - (g - (r > b ? r : b));
        const int err = 3 * (abs((eg - er) - (g - r)) + abs((eg - eb) - (g - b))) + 30 * (over > 0 ? over : 0) + abs(er + eg + eb - lum);
        if (err < best) best = err, out = (uint16_t) (R << 11 | G << 5 | B);
      }
  return out;
}
