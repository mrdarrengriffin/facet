// Radio: the Facet radio (docs: Components > Pickers > Radio, @facet/ui pickers.ts radio(on), CSS .f-radio), on the
// contract (renderer/CONTRACT.md, "contract": 2): a ring that shows which one option of a group is chosen. It does not
// decide anything itself: the Picker row (and the Bottom sheet's list) owns the choice and sets the radio; on its own it
// is a small tap target for the playground.
//   ring     radio (layout:radio 24 / 16, round, a stroke:edge ring) in text-dim, no fill. Docs: line-hi, which is
//            1.8-2.0:1 on bg; an unchosen radio is a control boundary (ui 3:1), so text-dim, like the List row's chevron
//   on       ring + dot in on-mark (accent), the dot radio-dot (layout:radio-dot 8) centred: (24 - 8) / 2 and
//            (16 - 8) / 2 are both on the grid
//   switch   Motion toggle (fade-switch: the ring colour and the dot fade, nothing moves)
// State = LVGL CHECKED (st::ON) on the ring, trickled to the dot, so an owner sets it with lv_obj_add_state /
// remove_state and the look follows. Disabled (st::OFF): ring and dot disabled-ink. n/a: pressed (the owner's row
// shows the press), unavailable, loading, unknown, empty, error. Themed through the live palette (no part: the theme
// engine has no radio part yet).
// Tap (standalone only; a Picker row makes it non-clickable and owns the tap): a real CLICKED selects it (a radio is
// never un-chosen by tapping it): event "tap" (detail "on") + sound tap; disabled = "blocked" + error on finger down.
// Hit area: at least touch:min (fx::hit). Update path: on / disabled fade in place. Bleed: none.
// Contrast pairs: tokens/colour.json section "radio".
#include "fw.h"
#include "fx.h"

namespace {
constexpr lv_state_t ST_ON = fw::st::ON, ST_OFF = fw::st::OFF;
void fades(lv_obj_t *o) {  // the Motion toggle fade on every state the part changes in
  for (lv_state_t st : {(lv_state_t) 0, ST_ON, ST_OFF, (lv_state_t) (ST_ON | ST_OFF)}) fx::use(o, "fade-switch", st);
}
}  // namespace

FW_COMPONENT(radio, "Radio", "components", R"({
  "about": "A ring that shows which one option of a group is chosen; the picker row around it owns the choice.",
  "contract": 2,
  "parts": [],
  "min": ["layout:radio"],
  "frame": [160, 96],
  "props": [
    {"name": "on", "about": "Whether this option is the chosen one; the ring fills in.", "type": "bool", "default": true},
    {"name": "state", "about": "default can be chosen; disabled is greyed out.", "type": "select", "options": ["default", "disabled"], "default": "default"}
  ],
  "variants": [
    {"label": "off", "props": {"on": false}},
    {"label": "on", "props": {}},
    {"label": "off · disabled", "props": {"on": false, "state": "disabled"}},
    {"label": "on · disabled", "props": {"state": "disabled"}}
  ]
})") {
  lv_obj_t *r = lv_obj_create(parent);
  lv_obj_remove_style_all(r);
  lv_obj_add_flag(r, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_STATE_TRICKLE));
  lv_obj_remove_flag(r, LV_OBJ_FLAG_SCROLLABLE);
  fx::name(r, "radio");
  fx::use(r, "border:text-dim");  // docs line-hi fails ui 3:1 (see top)
  fx::use(r, "border:on-mark", ST_ON);
  fx::use(r, "border:disabled-ink", ST_OFF);
  fx::use(r, "border:disabled-ink", ST_ON | ST_OFF);
  // the ring's size and edge width last at each state: a border:<c> token draws a hairline
  for (lv_state_t st : {(lv_state_t) 0, ST_ON, ST_OFF, (lv_state_t) (ST_ON | ST_OFF)}) fx::use(r, "radio", st);
  fades(r);

  lv_obj_t *k = fx::box(r, "free");  // the chosen dot
  fx::use(k, "radio-dot");
  fx::use(k, "transp");
  fx::use(k, "bg:on-mark");
  fx::use(k, "opaque", ST_ON);
  fx::use(k, "bg:disabled-ink", ST_ON | ST_OFF);
  fades(k);

  const bool off = !strcmp(p["state"] | "default", "disabled");
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
  fx::hit(r);
}

// Update path: on / off -> the state's own transitions (ring colour + dot, Motion toggle), default <-> disabled ->
// fw::st::set_off fades in place. Silent (no tap).
FW_UPDATE(radio) {
  fw::st::set_state(root, ST_ON, p["on"] | true, false);  // LVGL's transitions animate the dot + ring
  const bool off = !strcmp(p["state"] | "default", "disabled");
  if (off != lv_obj_has_state(root, ST_OFF)) fw::st::set_off(root, off);
  return true;
}
