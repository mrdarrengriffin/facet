// Snapped drawing probe (WASM only, like core/theme_lab.cpp): fw_lab(-1, "pen_probe", json) from core/main.cpp draws
// the Pen's primitives (arc band, minor + major ticks, a knob on the arc, a dot, a thick line, a pixel icon) in a box
// at an odd screen offset, in the live theme, so tests/functional/theme_snap.test.mjs can check every pixel (C29:
// whole cells of stroke:pixel on the screen grid, the knob on the arc's cells, k cells across a thick line).
//   pen_probe {x, y, size, knob_deg, line_deg}  -> {cx, cy, half, r, knob: [x, y], cell}   pen_clear  removes it
#include <math.h>
#include <string>
#include <vector>
#include "fw.h"
#include "fx.h"

namespace fx {
int pen_cell();  // core/fx_core.cpp: the live theme's snapped cell (0 = not snapped)
namespace {
lv_obj_t *probe = nullptr, *icon_box = nullptr;
float knob_deg = 0, line_deg = 30;
int R = 0;
void draw(const Pen &p) {
  R = p.half * 7 / 10;  // room for the knob inside the box
  p.arc(R, 135, 405, "stroke:indicator", "fg:line", false, "arc");
  for (int d = 135; d <= 405; d += 15) p.tick((float) d, (d - 135) % 45 ? "layout:dial-tick" : "layout:dial-tick-major", "stroke:tick", "fg:blue", (d - 135) % 45 ? "tick" : "tick-major");
  int x, y;
  fx::at(knob_deg, R, &x, &y);
  p.knob(x, y, "layout:dial-knob", "fg:accent", "fg:bg", "stroke:knob");
  const float a = line_deg * 0.01745329f;
  p.line(0, 0, (int) lroundf(cosf(a) * R / 2), (int) lroundf(sinf(a) * R / 2), "stroke:indicator", "fg:text-dim");
  p.dot(0, 0, "stroke:indicator", "fg:text");
}
}  // namespace
std::vector<lv_obj_t *> hidden;  // the screen's other children, hidden while the probe is up
std::string pen_lab(lv_display_t *disp, const std::string &cmd, const std::string &arg) {
  if (probe) { lv_obj_delete(probe); probe = nullptr; icon_box = nullptr; }
  for (lv_obj_t *o : hidden) lv_obj_remove_flag(o, LV_OBJ_FLAG_HIDDEN);
  hidden.clear();
  if (cmd == "pen_clear") return "{}";
  if (cmd != "pen_probe") return "{\"error\":\"unknown pen command\"}";
  ArduinoJson::JsonDocument a;
  deserializeJson(a, arg);
  knob_deg = a["knob_deg"] | 0.0f; line_deg = a["line_deg"] | 30.0f;
  lv_obj_t *scr = lv_display_get_screen_active(disp);
  for (uint32_t i = 0; i < lv_obj_get_child_count(scr); i++) {
    lv_obj_t *o = lv_obj_get_child(scr, (int32_t) i);
    if (!lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN)) { lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN); hidden.push_back(o); }
  }
  probe = lv_obj_create(scr);  // an opaque backdrop in the theme's bg over the whole screen: only the probe is seen
  lv_obj_remove_style_all(probe);
  lv_obj_set_size(probe, lv_pct(100), lv_pct(100));
  lv_obj_set_style_bg_color(probe, fx::bg(disp), 0); lv_obj_set_style_bg_opa(probe, LV_OPA_COVER, 0);
  lv_obj_remove_flag(probe, LV_OBJ_FLAG_SCROLLABLE);
  const int sz = a["size"] | 300, px = a["x"] | 7, py = a["y"] | 5;
  lv_obj_t *d = fx::drawn(probe, draw, "free");
  lv_obj_set_pos(d, px, py);
  lv_obj_set_size(d, sz, sz);
  icon_box = fx::box(probe, "free");
  lv_obj_set_pos(icon_box, px + 3, py + sz + 2);
  fx::icon(icon_box, "screen-light", "icon", "fg:text");
  lv_obj_update_layout(scr);
  lv_obj_invalidate(scr);
  lv_area_t c;
  lv_obj_get_content_coords(d, &c);
  const int w = lv_area_get_width(&c), h = lv_area_get_height(&c), m = w < h ? w : h;
  char b[320];
  int kx, ky;
  R = fx::half(d) * 7 / 10;  // as draw() computes it
  fx::at(knob_deg, R, &kx, &ky);
  lv_area_t bx, ib;
  lv_obj_get_coords(d, &bx);
  lv_obj_get_coords(icon_box, &ib);
  snprintf(b, sizeof b, "{\"cx\":%d,\"cy\":%d,\"half\":%d,\"r\":%d,\"knob\":[%d,%d],\"cell\":%d,\"box\":[%d,%d,%d,%d],\"icon\":[%d,%d,%d,%d]}",
           (int) (c.x1 + w / 2), (int) (c.y1 + h / 2), (m - 1) / 2, R, kx, ky, pen_cell(), (int) bx.x1, (int) bx.y1, (int) bx.x2, (int) bx.y2,
           (int) ib.x1, (int) ib.y1, (int) ib.x2, (int) ib.y2);
  return b;
}
}  // namespace fx
