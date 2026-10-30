// Arc: the Facet arc (docs: Components > Controls > Arc, @facet/ui controls.ts arc(), LVGL lv_arc): a value shown as
// an arc over 270 degrees from 135 (open at the bottom), with a track, a round-capped indicator and an optional knob.
// Built with the core value arc fw::value_arc (atoms.h "Dials and rings"), the one the Dial and the Thermostat dial
// use, so every arc in Facet has the same track, indicator, caps and knob.
//   box        size x size (dp, snapped to the grid; docs control 64-320)
//   track      stroke::edge (2) line, square ends (docs 2 / line)
//   indicator  stroke::indicator (6 / 4) st::on_fill (accent), round caps (docs 6 / accent); 0 = nothing drawn
//   knob       optional, lay::dial_knob (28 / 20) accent with a stroke::knob ring in the canvas colour (docs 26 / 4)
// A read-out, not a control (docs: no touch): no events, no sound. Disabled (st::OFF look): indicator text-mute, knob
// text-dim (the Dial's disabled roles). Update path: value sweeps there (Motion value, 400 ms ease-out; reduced motion
// = instant), size / knob / state refill in place (snap). Contrast pairs: tokens/colour.json section "arc".
#include "fw.h"

namespace {
constexpr float START = 135.0f, RANGE = 270.0f;
constexpr int32_t STEPS = 1000;  // 0.1 % steps so a sweep glides
struct Arc {
  lv_obj_t *arc = nullptr;
  int32_t shown = 0;
};
Arc *of(lv_obj_t *o) { return (Arc *) lv_obj_get_user_data(o); }
int32_t target_of(fw::Props p) { return (int32_t) std::lround(LV_CLAMP(0.0, (double) (p["value"] | 62.0), 100.0) * STEPS / 100); }
std::string sig_of(fw::Props p) {
  char b[64];
  snprintf(b, sizeof b, "%d|%d|%s", (int) (p["size"] | 160), (int) (p["knob"] | true), p["state"] | "default");
  return b;
}
void show(void *o, int32_t v) {
  auto *d = of((lv_obj_t *) o);
  d->shown = v;
  if (d->arc) lv_arc_set_value(d->arc, v);
}

void fill(lv_obj_t *s, fw::Props p) {
  const fw::Theme &t = *fw::T;
  auto *d = of(s);
  const bool off = !strcmp(p["state"] | "default", "disabled");
  const int size = fw::px((float) (p["size"] | 160));
  lv_obj_remove_style_all(s);
  lv_obj_remove_flag(s, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_size(s, size, size);
  fw::ArcLook k;
  k.start = START;
  k.range = RANGE;
  k.steps = STEPS;
  k.track_w = fw::stroke::edge;
  k.track_c = t.line;
  k.ind_w = fw::stroke::indicator;
  k.ind_c = off ? fw::st::disabled_ink(t) : fw::st::on_fill(t);
  if (p["knob"] | true) {
    k.knob = fw::lay::dial_knob;
    k.knob_c = off ? fw::st::disabled_knob(t) : t.accent;
    k.knob_ring = fw::stroke::knob;
    k.knob_ring_c = t.bg;
  }
  // the knob is wider than the indicator it rides on: inset the arc by the difference so the knob stays in the box
  // (2026-10-22 audit: at 64 the knob was cut off at the box edge)
  d->arc = fw::value_arc(s, size - (k.knob > k.ind_w ? (k.knob - k.ind_w + 7) / 8 * 8 : 0), k);  // even steps of 4 each side: centred on the grid
  lv_obj_center(d->arc);
  show(s, target_of(p));
  if (off) lv_obj_add_state(s, fw::st::OFF);
  fw::shape(s, sig_of(p));
}
}  // namespace

FW_COMPONENT(arc, "Arc", "components", R"({
  "about": "A value as an arc with a track, a round-capped indicator and an optional knob: the arc every dial uses.",
  "frame": [336, 336],
  "props": [
    {"name": "value", "about": "How far round the arc is filled, from 0 (empty) to 100 (full).", "type": "range", "min": 0, "max": 100, "step": 1, "default": 62},
    {"name": "size", "about": "How wide the arc is across, in pixels.", "type": "range", "min": 64, "max": 320, "step": 16, "default": 160},
    {"name": "knob", "about": "Shows a round knob at the end of the filled part, the handle you drag on a dial.", "type": "bool", "default": true},
    {"name": "state", "debug": true, "about": "default draws the arc in the accent colour; disabled greys it out to show it cannot be changed.", "type": "select", "options": ["default", "disabled"], "default": "default"}
  ],
  "variants": [
    {"label": "62 · knob", "props": {}},
    {"label": "100", "props": {"value": 100}},
    {"label": "0", "props": {"value": 0}},
    {"label": "no knob · 30", "props": {"knob": false, "value": 30}},
    {"label": "small · 64", "props": {"size": 64, "value": 75}},
    {"label": "disabled", "props": {"state": "disabled"}}
  ]
})") {
  auto *d = new Arc;
  lv_obj_t *s = lv_obj_create(parent);
  lv_obj_set_user_data(s, d);
  lv_obj_add_event_cb(s, [](lv_event_t *e) { delete (Arc *) lv_event_get_user_data(e); }, LV_EVENT_DELETE, d);
  fill(s, p);
}

FW_UPDATE(arc) {
  auto *d = of(root);
  if (!d) return false;
  if (!fw::shape(root, sig_of(p))) {
    const int32_t was = d->shown;
    fw::refill(root);
    fill(root, p);
    show(root, was);  // keep what was shown; the value sweeps from there below
  }
  const int32_t to = target_of(p);
  if (to != d->shown || lv_anim_get(root, (lv_anim_exec_xcb_t) show)) fw::animate(root, fw::st::SWEEP, {show, d->shown, to, fw::mo::VALUE});
  return true;
}
