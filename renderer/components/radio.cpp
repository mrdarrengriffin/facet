// Radio: the Facet radio (docs: Components > Pickers > Radio, @facet/ui pickers.ts radio(on), CSS .f-radio): a ring
// that shows which one option of a group is chosen. It does not decide anything itself: the Picker row (and the
// Bottom sheet's list) owns the choice and sets the radio; on its own it is a small tap target for the playground.
//   box      lay::radio (24 / 16; docs 24 at both densities, 480 = x 2/3 so it matches the row icon, ic::icon 16),
//            round, a stroke::edge ring (2 px) in text-dim, no fill. Docs: line-hi, which is 1.8-2.0:1 on bg in both
//            themes; an unchosen radio is a control boundary (ui 3:1), so text-dim, like the List row's chevron
//   on       ring + dot = st::on_mark (accent), dot lay::radio_dot (8 at both: docs size dot 8 at 720; docs 1x 6 is
//            off the grid), centred: (24 - 8) / 2 = 8 and (16 - 8) / 2 = 4, both on the grid
//   switch   Motion toggle (states.json on.motion = st::SWITCH) as style transitions on the ring colour and the dot
//            opacity: colour / opacity only, nothing moves (the docs radio switches instantly)
// State = LVGL CHECKED (st::ON) on the ring, trickled to the dot, so an owner sets it with lv_obj_add_state /
// remove_state and the look follows. Disabled (st::OFF, Button rule): ring and dot text-mute (st::disabled_ink).
// Tap (standalone only; a Picker row makes it non-clickable and owns the tap): a real CLICKED selects it (a radio is
// never un-chosen by tapping it): event "tap" (detail "on") + sound tap; disabled = "blocked" + error on finger down.
// Hit area: touch::min (ext click area around the drawn ring). Contrast pairs: tokens/colour.json section "radio".
#include "fw.h"

namespace {
constexpr lv_state_t ST_ON = fw::st::ON, ST_OFF = fw::st::OFF;

const lv_style_prop_t RING_PROPS[] = {LV_STYLE_BORDER_COLOR, LV_STYLE_PROP_INV};
const lv_style_prop_t DOT_PROPS[] = {LV_STYLE_BG_OPA, LV_STYLE_PROP_INV};
const lv_style_transition_dsc_t *tr(bool dot) {
  static lv_style_transition_dsc_t d[2];
  static bool ready = false;
  if (!ready) {  // states.json on: Motion toggle
    fw::mo::transition(&d[0], fw::st::SWITCH, RING_PROPS);
    fw::mo::transition(&d[1], fw::st::SWITCH, DOT_PROPS);
    ready = true;
  }
  return &d[dot];
}
}  // namespace

FW_COMPONENT(radio, "Radio", "components", R"({
  "about": "A ring that shows which one option of a group is chosen; the picker row around it owns the choice.",
  "frame": [160, 96],
  "props": [
    {"name": "on", "type": "bool", "default": true},
    {"name": "state", "type": "select", "options": ["default", "disabled"], "default": "default"}
  ],
  "variants": [
    {"label": "off", "props": {"on": false}},
    {"label": "on", "props": {}},
    {"label": "off · disabled", "props": {"on": false, "state": "disabled"}},
    {"label": "on · disabled", "props": {"state": "disabled"}}
  ]
})") {
  const fw::Theme &t = *fw::T;
  const bool off = !strcmp(p["state"] | "default", "disabled");
  const int d = fw::lay::radio, dot = fw::lay::radio_dot;
  static_assert((fw::lay::radio - fw::lay::radio_dot) % 8 == 0, "the radio dot must centre on the 4 px grid");

  lv_obj_t *r = lv_obj_create(parent);
  lv_obj_remove_style_all(r);
  lv_obj_add_flag(r, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_STATE_TRICKLE));
  lv_obj_remove_flag(r, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(r, d, d);
  lv_obj_set_ext_click_area(r, std::max(0, (fw::touch::min - d) / 2));
  lv_obj_set_style_radius(r, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_border_width(r, fw::stroke::edge, 0);
  lv_obj_set_style_border_color(r, t.text_dim, 0);  // docs line-hi fails ui 3:1 (see top)
  lv_obj_set_style_border_color(r, fw::st::on_mark(t), ST_ON);
  lv_obj_set_style_border_color(r, fw::st::disabled_ink(t), ST_OFF);
  lv_obj_set_style_border_color(r, fw::st::disabled_ink(t), ST_ON | ST_OFF);
  lv_obj_set_style_transition(r, tr(false), 0);
  lv_obj_set_style_transition(r, tr(false), ST_ON);

  lv_obj_t *k = lv_obj_create(r);
  lv_obj_remove_style_all(k);
  lv_obj_remove_flag(k, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_size(k, dot, dot);
  lv_obj_align(k, LV_ALIGN_CENTER, 0, 0);  // inside the border: (d - 2 edge - dot) / 2 + edge = (d - dot) / 2
  lv_obj_set_style_radius(k, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(k, fw::st::on_mark(t), 0);
  lv_obj_set_style_bg_color(k, fw::st::disabled_ink(t), ST_OFF);
  lv_obj_set_style_bg_opa(k, LV_OPA_TRANSP, 0);
  lv_obj_set_style_bg_opa(k, LV_OPA_COVER, ST_ON);
  lv_obj_set_style_transition(k, tr(true), 0);
  lv_obj_set_style_transition(k, tr(true), ST_ON);

  if (p["on"] | true) lv_obj_add_state(r, ST_ON);
  if (off) lv_obj_add_state(r, ST_OFF);

  fw::st::on_tap(r, [](lv_event_t *e) {  // blocked on finger down, a real tap on CLICKED (states.json)
    if (!fw::st::tap_now(e)) return;
    auto *o = (lv_obj_t *) lv_event_get_current_target(e);
    if (lv_obj_has_state(o, ST_OFF)) { fw::st::blocked(o, lv_obj_has_state(o, ST_ON) ? "on" : "off"); return; }
    lv_obj_add_state(o, ST_ON);
    fw::emit(o, fw::st::TAP_EVENT, "on");
    fw::sound(o, fw::st::TAP_SOUND);
  }, nullptr);
}
