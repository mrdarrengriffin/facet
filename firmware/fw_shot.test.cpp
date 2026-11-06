// host test for fw_shot.h: for every rotation, a picture drawn the way the flush draws it (ESPHome lvgl draw_buffer_ /
// esphome/board/rgb_flush.h, written here from their loops, area by area) reads back upright, band by band.
//   c++ -std=c++17 -Wall -Wextra -Werror firmware/fw_shot.test.cpp && ./a.out
#include "fw_shot.h"
#include <cstdio>
#include <vector>

static int fails = 0;
#define EXPECT(c, ...) do { if (!(c)) { fails++; printf("FAIL %s:%d ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

// rgb_flush.h flush(): an LVGL area (x1, y1, aw x ah, row-major src) into the panel framebuffer W x H
static void flush(uint16_t *fb, int W, int H, int rot, int x1, int y1, int aw, int ah, const uint16_t *src) {
  switch (rot) {
    case 90:
      for (int i = 0; i < aw; i++) {
        uint16_t *d = fb + (x1 + i) * W + (W - y1 - ah);
        const uint16_t *s = src + (ah - 1) * aw + i;
        for (int k = 0; k < ah; k++, s -= aw) d[k] = *s;
      }
      break;
    case 270:
      for (int i = 0; i < aw; i++) {
        uint16_t *d = fb + (H - 1 - x1 - i) * W + y1;
        const uint16_t *s = src + i;
        for (int k = 0; k < ah; k++, s += aw) d[k] = *s;
      }
      break;
    case 180:
      for (int j = 0; j < ah; j++) {
        uint16_t *d = fb + (H - 1 - y1 - j) * W + (W - x1 - aw);
        const uint16_t *s = src + j * aw + aw - 1;
        for (int k = 0; k < aw; k++) d[k] = *s--;
      }
      break;
    default:
      for (int j = 0; j < ah; j++)
        for (int k = 0; k < aw; k++) fb[(y1 + j) * W + x1 + k] = src[j * aw + k];
  }
}

static void run(int pw, int ph, int rot, int bandn) {
  int w, h;
  fw::shot::size(pw, ph, rot, w, h);
  // LVGL's picture: every pixel different
  std::vector<uint16_t> img((size_t) w * h);
  for (int y = 0; y < h; y++)
    for (int x = 0; x < w; x++) img[(size_t) y * w + x] = (uint16_t) (y * 131 + x * 7 + 1);
  // drawn in uneven areas (LVGL's partial buffers)
  std::vector<uint16_t> fb((size_t) pw * ph, 0);
  for (int y1 = 0; y1 < h; y1 += 7)
    for (int x1 = 0; x1 < w; x1 += 11) {
      const int aw = x1 + 11 <= w ? 11 : w - x1, ah = y1 + 7 <= h ? 7 : h - y1;
      std::vector<uint16_t> src((size_t) aw * ah);
      for (int j = 0; j < ah; j++)
        for (int i = 0; i < aw; i++) src[(size_t) j * aw + i] = img[(size_t) (y1 + j) * w + x1 + i];
      flush(fb.data(), pw, ph, rot, x1, y1, aw, ah, src.data());
    }
  for (size_t i = 0; i < fb.size(); i++) EXPECT(fb[i] != 0, "%dx%d rot %d: framebuffer pixel %zu not drawn", pw, ph, rot, i);
  // read back bottom-up in bands, as the BMP goes
  std::vector<uint16_t> out((size_t) w * bandn);
  int bad = 0;
  for (int y0 = h - 1; y0 >= 0; y0 -= bandn) {
    const int n = bandn < y0 + 1 ? bandn : y0 + 1;
    fw::shot::band(fb.data(), pw, ph, rot, y0, n, out.data());
    for (int k = 0; k < n; k++)
      for (int x = 0; x < w; x++)
        if (out[(size_t) k * w + x] != img[(size_t) (y0 - k) * w + x]) bad++;
  }
  EXPECT(bad == 0, "%dx%d rot %d band %d: %d pixels wrong", pw, ph, rot, bandn, bad);
}

int main() {
  for (int rot : {0, 90, 180, 270})
    for (int b : {1, 4, 16}) {
      run(48, 48, rot, b);  // square, like both panels
      run(40, 24, rot, b);  // not square: width and height must not swap
    }
  if (fails) { printf("%d failed\n", fails); return 1; }
  printf("fw_shot: ok\n");
  return 0;
}
