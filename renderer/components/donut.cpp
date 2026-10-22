// Donut: the Facet donut (docs: Components > Charts > Donut, @facet/ui charts.ts donut()): one arc per part, clockwise
// from 12 o'clock, ch::DONUT_GAP_DEG (3) between parts, ch::donut_t thick (16 / 12), over a 1 px line track at the
// ring's middle radius (docs: track optional, on by default). Parts here: the docs sample's price bands (cheap blue,
// normal band-normal, peak accent = the chart highlight); a zero part is left out, one part = a full ring with no gap.
//   arcs   one lv_arc each, the part drawn as the arc's track (square ends, no indicator / knob): LVGL's own smooth arc
//   motion the arcs fade in one after another when shown (docs play(): opacity only; Motion fade, each a Motion press
//          later), reduced = at once; a new set of values sweeps every part from its old angles to the new ones
//          (Motion value), silent
// A read-out: no touch, no sound. Update path: values sweep (above), size refills. Contrast pairs: section "donut".
#include "fw.h"
#include "chart.h"

namespace {
constexpr int N = 3;
struct Donut {
  lv_obj_t *arc[N] = {};
  float from[N][2] = {}, to[N][2] = {}, now[N][2] = {};
};
Donut *of(lv_obj_t *o) { return (Donut *) lv_obj_get_user_data(o); }
// the parts' [start, end] angles (LVGL degrees, may pass 360) for values v; a part of 0 = empty (end == start)
void angles(const float *v, float out[N][2]) {
  float total = 0;
  int live = 0;
  for (int i = 0; i < N; i++) total += std::max(0.0f, v[i]), live += v[i] > 0;
  const float GAP = live > 1 ? fw::ch::DONUT_GAP_DEG : 0;
  float a = 270;
  for (int i = 0; i < N; i++) {
    if (total <= 0 || v[i] <= 0) { out[i][0] = out[i][1] = a; continue; }
    const float sweep = v[i] / total * 360;
    out[i][0] = a + GAP / 2;
    out[i][1] = a + std::max(GAP / 2 + 0.5f, sweep - GAP / 2);
    a += sweep;
  }
}
void place(lv_obj_t *root) {
  auto *d = of(root);
  for (int i = 0; i < N; i++) {
    lv_obj_t *a = d->arc[i];
    const float s = d->now[i][0], e = d->now[i][1];
    if (e - s < 0.25f) { lv_obj_add_flag(a, LV_OBJ_FLAG_HIDDEN); continue; }
    lv_obj_remove_flag(a, LV_OBJ_FLAG_HIDDEN);
    // a whole turn (one part, no gap) as 0..360: start == end after the wrap would draw nothing (the "one part" bug)
    if (e - s >= 359.5f) lv_arc_set_bg_angles(a, 0, 360);
    else lv_arc_set_bg_angles(a, (lv_value_precise_t) std::fmod(s, 360.0f), (lv_value_precise_t) std::fmod(e, 360.0f));
  }
}
void sweep(void *o, int32_t k) {
  auto *d = of((lv_obj_t *) o);
  for (int i = 0; i < N; i++)
    for (int j = 0; j < 2; j++) d->now[i][j] = d->from[i][j] + (d->to[i][j] - d->from[i][j]) * k / 1024.0f;
  place((lv_obj_t *) o);
}
void set_opa(void *o, int32_t v) { lv_obj_set_style_opa((lv_obj_t *) o, (lv_opa_t) v, 0); }
void values(fw::Props p, float *v) { v[0] = p["a"] | 3.2f; v[1] = p["b"] | 4.1f; v[2] = p["c"] | 1.4f; }
void fill(lv_obj_t *root, fw::Props p, bool opening) {
  const fw::Theme &t = *fw::T;
  auto *d = of(root);
  const int S = fw::px((float) (p["size"] | 112)), T = fw::ch::donut_t;
  lv_obj_remove_style_all(root);
  lv_obj_remove_flag(root, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_size(root, S, S);
  lv_obj_t *tr = lv_obj_create(root);  // the track: a hairline circle at the ring's middle (docs r = C - t / 2)
  lv_obj_remove_style_all(tr);
  lv_obj_remove_flag(tr, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_size(tr, S - T, S - T);
  lv_obj_center(tr);
  lv_obj_set_style_radius(tr, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_border_width(tr, fw::stroke::hair, 0);
  lv_obj_set_style_border_color(tr, fw::ch::ink::grid(t), 0);
  const lv_color_t pal[N] = {t.blue, t.band_normal, fw::ch::ink::highlight(t)};
  float v[N];
  values(p, v);
  angles(v, d->now);
  for (int i = 0; i < N; i++) {
    fw::ArcLook k;
    k.track_w = T;
    k.track_c = pal[i];
    k.ind_w = 0;
    k.ind_c = pal[i];
    lv_obj_t *a = d->arc[i] = fw::value_arc(root, S, k);
    lv_obj_set_style_arc_opa(a, LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_obj_center(a);
    if (opening && !fw::mo::reduced(root)) {  // fade in one after another (opacity only)
      lv_obj_set_style_opa(a, LV_OPA_TRANSP, 0);
      fw::animate(a, fw::mo::fade, {set_opa, LV_OPA_TRANSP, LV_OPA_COVER, fw::mo::OPA, (uint32_t) (i + 1) * fw::mo::ms::press});
    }
  }
  place(root);
  fw::shape(root, std::to_string((int) (p["size"] | 112)));
}
}  // namespace

FW_COMPONENT(donut, "Donut", "components", R"({
  "about": "One arc per part, clockwise from 12 o'clock, with small gaps.",
  "frame": [176, 176],
  "props": [
    {"name": "a", "type": "range", "min": 0, "max": 10, "step": 0.1, "default": 3.2},
    {"name": "b", "type": "range", "min": 0, "max": 10, "step": 0.1, "default": 4.1},
    {"name": "c", "type": "range", "min": 0, "max": 10, "step": 0.1, "default": 1.4},
    {"name": "size", "type": "range", "min": 64, "max": 160, "step": 16, "default": 112}
  ],
  "opening": "Showing it fades the arcs in one after another.",
  "variants": [
    {"label": "three parts", "props": {}},
    {"label": "one part", "props": {"a": 0, "b": 5, "c": 0}},
    {"label": "mostly peak", "props": {"a": 1, "b": 1, "c": 8}},
    {"label": "small · 64", "props": {"size": 64}}
  ]
})") {
  auto *d = new Donut;
  lv_obj_t *root = lv_obj_create(parent);
  lv_obj_set_user_data(root, d);
  lv_obj_add_event_cb(root, [](lv_event_t *e) { delete (Donut *) lv_event_get_user_data(e); }, LV_EVENT_DELETE, d);
  fill(root, p, p["opening"] | true);
}

FW_UPDATE(donut) {
  auto *d = of(root);
  if (!d) return false;
  if (!fw::shape(root, std::to_string((int) (p["size"] | 112)))) {
    fw::refill(root);
    fill(root, p, false);
    return true;
  }
  float v[N];
  values(p, v);
  memcpy(d->from, d->now, sizeof d->from);
  angles(v, d->to);
  fw::animate(root, fw::st::SWEEP, {sweep, 0, 1024, fw::mo::VALUE});
  return true;
}
