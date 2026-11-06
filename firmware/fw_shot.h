// GET /screenshot (fw_device.h "screenshot"): the panel framebuffer read back through LVGL's rotation, a band of rows
// at a time. Pure C++ (no ESP-IDF, no LVGL): tested on the host (fw_shot.test.cpp, renderer/check.sh fw-shot).
#pragma once
#include <cstddef>
#include <cstdint>

namespace fw::shot {

// LVGL (x, y) -> framebuffer index, for a panel pw x ph (its own orientation) and LVGL's rotation in degrees: the
// inverse of what the flush writes (esphome/board/rgb_flush.h, ESPHome's lvgl draw_buffer_)
inline size_t at(int x, int y, int pw, int ph, int rot) {
  switch (rot) {
    case 90: return (size_t) x * pw + (pw - 1 - y);
    case 180: return (size_t) (ph - 1 - y) * pw + (pw - 1 - x);
    case 270: return (size_t) (ph - 1 - x) * pw + y;
    default: return (size_t) y * pw + x;
  }
}
// LVGL's size for that panel and rotation
inline void size(int pw, int ph, int rot, int &w, int &h) {
  const bool side = rot == 90 || rot == 270;
  w = side ? ph : pw, h = side ? pw : ph;
}
// LVGL rows y0, y0 - 1, ..., y0 - n + 1 (a BMP goes bottom-up) into out (n x w pixels). A rotated read walks x
// outermost: each panel row then gives n contiguous pixels, so every cache line it loads is used.
inline void band(const uint16_t *fb, int pw, int ph, int rot, int y0, int n, uint16_t *out) {
  int w, h;
  size(pw, ph, rot, w, h);
  if (rot == 90 || rot == 270) {
    for (int x = 0; x < w; x++)
      for (int k = 0; k < n; k++) out[(size_t) k * w + x] = fb[at(x, y0 - k, pw, ph, rot)];
  } else {
    for (int k = 0; k < n; k++)
      for (int x = 0; x < w; x++) out[(size_t) k * w + x] = fb[at(x, y0 - k, pw, ph, rot)];
  }
}

}  // namespace fw::shot
