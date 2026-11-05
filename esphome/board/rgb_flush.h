// Guition 4848S040: LVGL's flush writes straight into the RGB panel's PSRAM framebuffer, rotated row by row (#118).
//
// Why: the panel streams that framebuffer through two small internal bounce buffers, refilled by the LCD interrupt.
// When PSRAM is too busy the refill is late, the panel shows stale rows until the next VSYNC = a band that flickers
// or shifts, low on the screen. ESPHome's software rotation (Rotate screen 90 / 270) was the heaviest PSRAM user on
// a redraw: it wrote each LVGL chunk column by column into a 57.6 KB rotate buffer in PSRAM (every pixel a different
// cache line), then read it back and copied it into the framebuffer, so a full redraw crossed PSRAM ~4 times.
// Here the pixels go once: read from LVGL's internal draw buffer (any order is cheap there), written to the
// framebuffer as contiguous rows. Same pixels, same places as ESPHome's draw_buffer_ (lvgl_esphome.cpp) for every
// rotation; touch is untouched (LVGL's rotation setting still maps the touch points).
// The bounce-buffer ISR reads the framebuffer through the same data cache, so no cache sync is needed (ESP-IDF's
// own draw_bitmap skips it too when bounce buffers are on).
#pragma once
#include <cstring>
#include "esp_attr.h"
#include "esp_flash.h"
#include "esp_lcd_panel_rgb.h"
#include "esphome/core/log.h"
#include "esphome/components/lvgl/lvgl_esphome.h"
#include "esphome/components/mipi_rgb/mipi_rgb.h"

// the underrun counters' job bits (Facet firmware/fw_busy.h: FLUSH = 1 << 1, FLASH = 1 << 4), defined by
// components/mipi_rgb
extern "C" volatile uint32_t facet_busy;
extern "C" volatile uint32_t facet_busy_seen;

namespace panel {
namespace rgbflush {

constexpr uint32_t BUSY_FLUSH = 1u << 1, BUSY_FLASH = 1u << 4;
inline IRAM_ATTR void busy_on(uint32_t b) {
  __atomic_fetch_or(const_cast<uint32_t *>(&facet_busy), b, __ATOMIC_RELAXED);
  __atomic_fetch_or(const_cast<uint32_t *>(&facet_busy_seen), b, __ATOMIC_RELAXED);
}
inline IRAM_ATTR void busy_off(uint32_t b) { __atomic_fetch_and(const_cast<uint32_t *>(&facet_busy), ~b, __ATOMIC_RELAXED); }

// FLASH bit: the default flash chip's OS hooks, wrapped (start/end bracket every write, erase and read of the driver)
inline const esp_flash_os_functions_t *flash_orig = nullptr;
inline esp_flash_os_functions_t flash_ours{};
inline IRAM_ATTR esp_err_t flash_start(void *arg, uint32_t flags) {
  const esp_err_t r = flash_orig->start ? flash_orig->start(arg, flags) : ESP_OK;
  busy_on(BUSY_FLASH);
  return r;
}
inline IRAM_ATTR esp_err_t flash_end(void *arg) {
  busy_off(BUSY_FLASH);
  return flash_orig->end ? flash_orig->end(arg) : ESP_OK;
}
inline void watch_flash() {
  if (flash_orig || !esp_flash_default_chip || !esp_flash_default_chip->os_func) return;
  flash_orig = esp_flash_default_chip->os_func;
  flash_ours = *flash_orig;
  flash_ours.start = flash_start;
  flash_ours.end = flash_end;
  esp_flash_default_chip->os_func = &flash_ours;
}

struct Ctx {
  esphome::lvgl::LvglComponent *lv{nullptr};
  uint16_t *fb{nullptr};
  int w{0}, h{0};
};
inline Ctx &ctx() {
  static Ctx c;
  return c;
}

inline void flush(lv_display_t *disp, const lv_area_t *a, uint8_t *px) {
  Ctx &c = ctx();
  busy_on(BUSY_FLUSH);
  if (!c.lv->is_paused()) {
    const int aw = lv_area_get_width(a), ah = lv_area_get_height(a), W = c.w, H = c.h;
    const uint16_t *src = reinterpret_cast<const uint16_t *>(px);
    switch (c.lv->get_rotation()) {
      case esphome::display::DISPLAY_ROTATION_90_DEGREES:
        // LVGL (x, y) -> panel (W - 1 - y, x): LVGL column i = panel row x1 + i, read bottom-up
        for (int i = 0; i < aw; i++) {
          uint16_t *d = c.fb + (a->x1 + i) * W + (W - a->y1 - ah);
          const uint16_t *s = src + (ah - 1) * aw + i;
          for (int k = 0; k < ah; k++, s -= aw) d[k] = *s;
        }
        break;
      case esphome::display::DISPLAY_ROTATION_270_DEGREES:
        // LVGL (x, y) -> panel (y, H - 1 - x)
        for (int i = 0; i < aw; i++) {
          uint16_t *d = c.fb + (H - 1 - a->x1 - i) * W + a->y1;
          const uint16_t *s = src + i;
          for (int k = 0; k < ah; k++, s += aw) d[k] = *s;
        }
        break;
      case esphome::display::DISPLAY_ROTATION_180_DEGREES:
        // LVGL (x, y) -> panel (W - 1 - x, H - 1 - y)
        for (int j = 0; j < ah; j++) {
          uint16_t *d = c.fb + (H - 1 - a->y1 - j) * W + (W - a->x1 - aw);
          const uint16_t *s = src + j * aw + aw - 1;
          for (int k = 0; k < aw; k++) d[k] = *s--;
        }
        break;
      default:
        for (int j = 0; j < ah; j++) memcpy(c.fb + (a->y1 + j) * W + a->x1, src + j * aw, aw * 2);
        break;
    }
  }
  busy_off(BUSY_FLUSH);
  lv_display_flush_ready(disp);
}

// after LVGL's setup (on_boot priority below 600): replaces ESPHome's flush callback on this display
inline void install(esphome::lvgl::LvglComponent *lv, esphome::mipi_rgb::MipiRgb *lcd) {
  watch_flash();
  void *fb = nullptr;
  if (lcd->is_failed() || lv->get_disp() == nullptr ||
      esp_lcd_rgb_panel_get_frame_buffer(lcd->get_handle(), 1, &fb) != ESP_OK || fb == nullptr) {
    ESP_LOGW("rgb_flush", "framebuffer not found: ESPHome's flush stays");
    return;
  }
  Ctx &c = ctx();
  c.lv = lv;
  c.fb = static_cast<uint16_t *>(fb);
  c.w = lcd->get_width_internal();
  c.h = lcd->get_height_internal();
  lv_display_set_flush_cb(lv->get_disp(), flush);
}

}  // namespace rgbflush
}  // namespace panel
