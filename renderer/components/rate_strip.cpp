// Rate strip: the Facet rate strip (docs: Components > Charts > Rate strip, @facet/ui charts.ts rateStrip()): price bands
// across the day as a strip of runs (equal slots, one rounded run per stretch of the same band, a small gap between runs),
// with a tick at now. Bands always come with a legend or a label somewhere (docs: never colour alone).
//   size    width prop (dp), ch::strip_h tall (8 at both densities: docs 8; 4 at 480 was too thin to read)
//   runs    BAND_COLOUR (docs): cheap blue, normal band-normal, peak accent (the chart highlight); ch::bar_radius ends,
//           stroke::edge (2) between runs (docs 2)
//   now     a stroke::edge wide notch in the canvas colour (bg) at the middle of the now slot, the strip's height
//           (docs: a text-coloured tick, 1.6-2.8:1 on the light bands in dark; the notch meets each band at 3:1+)
// Drawn in one draw event (runs are fractional slots, placed by time not by the grid). Static: no touch, no sound.
// Update path: new bands / now redraw in place (one invalidate); width refills. Contrast pairs: section "rate_strip".
#include "fw.h"
#include "chart.h"

namespace {
const char *DEF = "cheap,cheap,cheap,cheap,cheap,cheap,normal,normal,normal,normal,normal,normal,normal,normal,normal,normal,peak,peak,peak,normal,normal,normal,cheap,cheap";
struct Strip { std::vector<std::string> bands; int now = -1; };
Strip *of(lv_obj_t *o) { return (Strip *) lv_obj_get_user_data(o); }
lv_color_t colour(const fw::Theme &t, const std::string &b) { return b == "peak" ? fw::ch::ink::highlight(t) : b == "cheap" ? t.blue : t.band_normal; }
void rect(lv_layer_t *l, int x1, int y1, int x2, int y2, lv_color_t c, int r) {
  if (x2 < x1) return;
  lv_draw_rect_dsc_t d;
  lv_draw_rect_dsc_init(&d);
  d.bg_color = c;
  d.radius = r;
  lv_area_t a = {x1, y1, x2, y2};
  lv_draw_rect(l, &d, &a);
}
void draw(lv_event_t *e) {
  auto *o = (lv_obj_t *) lv_event_get_current_target(e);
  const Strip *s = of(o);
  const fw::Theme &t = *fw::T;
  lv_layer_t *l = lv_event_get_layer(e);
  lv_area_t a;
  lv_obj_get_coords(o, &a);
  const int n = std::max<int>(1, s->bands.size()), W = lv_area_get_width(&a), g = fw::stroke::edge;
  const float slot = W / (float) n;
  for (int i = 0; i < (int) s->bands.size();) {
    int j = i;
    while (j + 1 < (int) s->bands.size() && s->bands[j + 1] == s->bands[i]) j++;
    const int x0 = a.x1 + (int) std::lround(i * slot) + (i ? g / 2 : 0);
    const int x1 = a.x1 + (int) std::lround((j + 1) * slot) - 1 - (j < (int) s->bands.size() - 1 ? g - g / 2 : 0);
    rect(l, x0, a.y1, x1, a.y2, colour(t, s->bands[i]), fw::ch::bar_radius);
    i = j + 1;
  }
  if (s->now >= 0 && s->now < n) {
    const int x = a.x1 + (int) std::lround((s->now + 0.5f) * slot) - g / 2;
    rect(l, x, a.y1, x + g - 1, a.y2, t.bg, 0);  // a notch in the canvas colour (docs: text, 1.6-2.8:1 on the light bands in dark)
  }
}
void apply(lv_obj_t *o, fw::Props p) {
  auto *s = of(o);
  s->bands = fw::chart::words(p, "bands", DEF);
  s->now = p["now"] | 14;
}
}  // namespace

FW_COMPONENT(rate_strip, "Rate strip", "components", R"({
  "about": "Price bands as a strip of runs, with a tick at now.",
  "frame": [448, 48],
  "props": [
    {"name": "bands", "type": "text", "default": "cheap,cheap,cheap,cheap,cheap,cheap,normal,normal,normal,normal,normal,normal,normal,normal,normal,normal,peak,peak,peak,normal,normal,normal,cheap,cheap"},
    {"name": "now", "type": "range", "min": -1, "max": 23, "step": 1, "default": 14},
    {"name": "width", "type": "range", "min": 160, "max": 440, "step": 8, "default": 320}
  ],
  "variants": [
    {"label": "a day · now 14:00", "props": {}},
    {"label": "no now", "props": {"now": -1}},
    {"label": "narrow", "props": {"width": 200, "now": 7}}
  ]
})") {
  auto *s = new Strip;
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_size(o, fw::px((float) (p["width"] | 320)), fw::ch::strip_h);
  lv_obj_set_user_data(o, s);
  lv_obj_add_event_cb(o, [](lv_event_t *e) { delete (Strip *) lv_event_get_user_data(e); }, LV_EVENT_DELETE, s);
  lv_obj_add_event_cb(o, draw, LV_EVENT_DRAW_MAIN, nullptr);
  apply(o, p);
}

FW_UPDATE(rate_strip) {
  if (!of(root)) return false;
  lv_obj_set_width(root, fw::px((float) (p["width"] | 320)));
  apply(root, p);
  lv_obj_invalidate(root);
  return true;
}
