// Waveshare ESP32-P4-86: the MIPI-DSI panel's framebuffer for GET /screenshot (firmware/fw_device.h "screenshot").
// ESPHome's mipi_dsi keeps its panel handle protected in a final class; it is reached through a pointer to member named
// in an explicit template instantiation (where access checks don't apply: legal C++, no layout assumptions). The framebuffer is in panel orientation,
// written by ESPHome's draw_buffer_ (rotated in software / PPA) through DMA2D, so the CPU cache may hold old lines:
// frame() invalidates them first. Nothing on the CPU writes the framebuffer (IDF's draw_bitmap copies by DMA2D, or by
// memcpy + write-back), so the invalidate never drops a dirty line.
#pragma once
#include "esp_cache.h"
#include "esp_lcd_mipi_dsi.h"
#include "esphome/components/lvgl/lvgl_esphome.h"
#include "esphome/components/mipi_dsi/mipi_dsi.h"

namespace panel {
namespace dsiframe {

template<typename Tag, typename Tag::type M> struct Reach {
  friend typename Tag::type member(Tag) { return M; }
};
struct HandleTag {
  using type = esp_lcd_panel_handle_t esphome::mipi_dsi::MipiDsi::*;
  friend type member(HandleTag);
};
struct DepthTag {
  using type = esphome::display::ColorBitness esphome::mipi_dsi::MipiDsi::*;
  friend type member(DepthTag);
};
template struct Reach<HandleTag, &esphome::mipi_dsi::MipiDsi::handle_>;
template struct Reach<DepthTag, &esphome::mipi_dsi::MipiDsi::color_depth_>;
struct Peek {
  static esp_lcd_panel_handle_t handle(esphome::mipi_dsi::MipiDsi *d) { return d->*member(HandleTag{}); }
  static bool rgb565(esphome::mipi_dsi::MipiDsi *d) { return d->*member(DepthTag{}) == esphome::display::COLOR_BITNESS_565; }
};

struct Ctx {
  esphome::lvgl::LvglComponent *lv{nullptr};
  uint16_t *fb{nullptr};
  int w{0}, h{0};
};
inline Ctx &ctx() {
  static Ctx c;
  return c;
}

// after the display's setup (on_boot): the framebuffer, if it is RGB565 (the screenshot reads 16-bit pixels)
inline void install(esphome::lvgl::LvglComponent *lv, esphome::mipi_dsi::MipiDsi *lcd) {
  void *fb = nullptr;
  if (lcd->is_failed() || !Peek::rgb565(lcd) ||
      esp_lcd_dpi_panel_get_frame_buffer(Peek::handle(lcd), 1, &fb) != ESP_OK || fb == nullptr) {
    ESP_LOGW("dsi_frame", "framebuffer not found: no screenshot");
    return;
  }
  Ctx &c = ctx();
  c.lv = lv;
  c.fb = static_cast<uint16_t *>(fb);
  c.w = lcd->get_width_internal();  // the panel's own size, not rotated
  c.h = lcd->get_height_internal();
}

}  // namespace dsiframe

// the screenshot's source (fw_device.h shot::frame_hook): framebuffer, panel size, LVGL's rotation in degrees.
// Called with LVGL's refresh held, so no DMA2D copy is running.
inline bool shot_frame(const uint16_t **fb, int *w, int *h, int *rot) {
  dsiframe::Ctx &c = dsiframe::ctx();
  if (!c.fb || !c.lv) return false;
  esp_cache_msync(c.fb, (size_t) c.w * c.h * 2, ESP_CACHE_MSYNC_FLAG_DIR_M2C | ESP_CACHE_MSYNC_FLAG_TYPE_DATA |
                                                    ESP_CACHE_MSYNC_FLAG_UNALIGNED);
  *fb = c.fb, *w = c.w, *h = c.h, *rot = (int) c.lv->get_rotation();
  return true;
}

}  // namespace panel
