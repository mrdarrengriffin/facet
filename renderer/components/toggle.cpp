// Toggle: the Facet switch (docs: @facet/ui toggleSwitch(), settings rows; CSS .f-toggle), on the contract
// (renderer/CONTRACT.md, "contract": 2). A pill track with a round knob, inset from the edge by the same amount all
// round; on = accent track, knob on the right.
//   size     a fixed touch control from tokens (C32): layout:toggle-w x touch:min (80 x 48 / 56 x 32), knob
//            layout:toggle-knob (32 / 24) inset space 2 (8 / 4), travel = the knob (core/fx_core.cpp "toggle*")
//   minimum  layout:toggle-w x touch:min. Steps: one
//   off  line track, text-dim knob          on  on-fill track, on-label knob, moved right by the travel
// A theme's switch part with shape "check" (Win98) makes it a classic checkbox: the track and knob go, the core draws
// a square the switch's height at its right end (the part's face + edges) with a tick when on; the tap area is unchanged.
// The docs define no pressed look (the knob moving IS the feedback) and no visible label (a labelled row = List row).
// States (a token per state, C17; LVGL CHECKED = on, fw::st::OFF = disabled):
//   off · disabled  hollow track (hair border disabled-line, no fill), disabled-knob knob (the inset less the hair)
//   on · disabled   disabled-accent track, disabled-on-fill knob
//   unavailable     the word Unavailable (body, unavailable-note) in place of the switch, touch:min tall
// Tap: the knob slides + the colours fade = Motion pattern "toggle" as a style transition (reduced motion = instant),
// emits "toggle" (detail on / off) and plays toggle-on / toggle-off; disabled / unavailable: "blocked" + error on
// finger down. Bleed: none. Contrast pairs: tokens/colour.json section "toggle".
#include "fw.h"
#include "fx.h"

namespace {
constexpr lv_state_t ST_OFF = fw::st::OFF, ST_ON = fw::st::ON;  // disabled (LVGL sends no events to LV_STATE_DISABLED)

struct Toggle { bool on = false, unavail = false; };
Toggle *of(lv_obj_t *t) { return (Toggle *) lv_obj_get_user_data(t); }

void fill(lv_obj_t *t, fw::Props p) {
  auto *d = of(t);
  d->on = p["on"] | false;
  d->unavail = !strcmp(p["state"] | "default", "unavailable");
  const bool disabled = !strcmp(p["state"] | "default", "disabled");
  lv_obj_add_flag(t, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_STATE_TRICKLE));
  lv_obj_remove_flag(t, LV_OBJ_FLAG_SCROLLABLE);
  if (d->unavail) {  // states.json unavailable: the word in place of the switch; a tap = blocked
    fx::use(t, "word-slot");
    fx::fit_grid(fx::text(t, fw::st::UNAVAILABLE_TEXT, "type:body", "fg:unavailable-note"));
    lv_obj_add_state(t, ST_OFF);
    return;
  }
  const bool slide = fw::mo::duration(t, fw::st::SWITCH, fw::mo::MOVE) > 0;  // reduced motion: no transition
  lv_obj_t *kn = fx::box(t, "free");
  if (d->on) fx::state(t, ST_ON, true);  // the states before the styles: building never slides (C34)
  if (disabled) fx::state(t, ST_OFF, true);
  fx::use(t, "toggle");
  fx::use(t, "bg:line");
  fx::use(t, "bg:on-fill", ST_ON);
  fx::use(t, "bg-none", ST_OFF);                 // off · disabled: hollow
  fx::use(t, "border:disabled-line", ST_OFF);
  fx::use(t, "toggle-off-pad", ST_OFF);
  fx::use(t, "bg:disabled-accent", ST_OFF | ST_ON);
  fx::use(t, "border:disabled-accent", ST_OFF | ST_ON);
  if (slide) { fx::use(t, "toggle-slide"); fx::use(t, "toggle-slide", ST_ON); }
  fx::use(t, "part:track");  // the theme's track (C46), after the track's own state tokens
  fx::use(t, "part:switch");  // the theme's switch shape (C46): pill = this; check = a checkbox the core draws

  fx::use(kn, "toggle-knob");
  fx::use(kn, "bg:text-dim");
  fx::use(kn, "bg:on-label", ST_ON);
  fx::use(kn, "toggle-knob-on", ST_ON);
  fx::use(kn, "bg:disabled-knob", ST_OFF);
  fx::use(kn, "bg:disabled-on-fill", ST_OFF | ST_ON);
  if (slide) { fx::use(kn, "toggle-slide"); fx::use(kn, "toggle-slide", ST_ON); }
  fx::use(kn, "part:switch");  // a checkbox has no knob
}
}  // namespace

FW_COMPONENT(toggle, "Toggle", "components", R"({
  "about": "An on/off switch.",
  "contract": 2,
  "parts": ["track", "switch"],
  "min": ["layout:toggle-w", "touch:min"],
  "frame": [160, 96],
  "props": [
    {"name": "on", "about": "Whether the switch is on.", "type": "bool", "default": false},
    {"name": "state", "about": "default can be tapped; disabled cannot be changed; unavailable shows the device cannot be reached.", "type": "select", "options": ["default", "disabled", "unavailable"], "default": "default"}
  ],
  "variants": [
    {"label": "off", "props": {}},
    {"label": "on", "props": {"on": true}},
    {"label": "off · disabled", "props": {"state": "disabled"}},
    {"label": "on · disabled", "props": {"on": true, "state": "disabled"}},
    {"label": "unavailable", "props": {"state": "unavailable"}}
  ]
})") {
  auto *d = new Toggle;
  lv_obj_t *t = lv_obj_create(parent);
  lv_obj_remove_style_all(t);
  fx::name(t, "toggle");
  lv_obj_set_user_data(t, d);
  fill(t, p);
  lv_obj_add_event_cb(t, [](lv_event_t *e) { delete (Toggle *) lv_event_get_user_data(e); }, LV_EVENT_DELETE, d);
  fw::st::on_tap(t, [](lv_event_t *e) {  // blocked on finger down, a real tap on CLICKED (states.json)
    if (!fw::st::tap_now(e)) return;
    auto *o = (lv_obj_t *) lv_event_get_current_target(e);
    auto *d = of(o);
    if (lv_obj_has_state(o, ST_OFF)) { fw::st::blocked(o, d->unavail ? "unavailable" : d->on ? "on" : "off"); return; }
    d->on = !d->on;
    fx::state(o, ST_ON, d->on);  // the style transition slides the knob and fades the colours
    fw::emit(o, fw::st::SWITCH_EVENT, d->on ? "on" : "off");
    fw::sound(o, d->on ? fw::st::ON_SOUND : fw::st::OFF_SOUND);
  }, nullptr);
}

// Update path: on -> the knob slides (the same transition, silent), default <-> disabled -> fw::st::set_off fades in
// place; to or from unavailable = the contents again in the same root (fw::refill, snap) and they fade in. Silent.
FW_UPDATE(toggle) {
  auto *d = of(root);
  if (!d) return false;
  const bool unavail = !strcmp(p["state"] | "default", "unavailable");
  if (unavail || d->unavail) {
    if (unavail && d->unavail) return true;
    fw::refill(root);
    fill(root, p);
    fw::st::fade_in_children(root);
    return true;
  }
  const bool on = p["on"] | false, off = !strcmp(p["state"] | "default", "disabled");
  if (on != d->on) { d->on = on; fx::state(root, ST_ON, on); }
  if (off != lv_obj_has_state(root, ST_OFF)) fw::st::set_off(root, off);
  return true;
}
