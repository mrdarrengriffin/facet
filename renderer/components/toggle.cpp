// Toggle: the Facet switch (docs: @facet/ui toggleSwitch(), settings rows; CSS .f-toggle). A pill track with a round
// knob, inset from the edge by the same amount all round; on = accent track, knob on the right.
//   720: 80 x 48 track, 32 knob 8 in, travel 32 (= the docs CSS exactly)
//   480: 56 x 32 track, 24 knob 4 in, travel 24 (Darren 2026-10-21 size study, "even spacing": docs x 2/3 = 53 x 32 /
//        21 knob / 5 in is off the 4 px grid and off-centre, so built
//        from the same tokens as 720 instead: height touch-min, inset space 2, knob = height - 2 x inset, width =
//        2 x knob + 2 x inset, so travel = knob at both densities)
//   off  line track, text-dim knob          on  accent track, on-accent knob, knob moved right by the travel
// The docs define no pressed look (the knob moving IS the feedback) and no visible label (aria-label only; a labelled
// row = List row / Settings row, later). Disabled is not in the docs' toggle; it follows Button's disabled rule:
//   off · disabled  hollow track (1 px line border, no fill), text-mute knob (text-mute on line would be < 3:1)
//   on · disabled   accent-dim track, text-dim knob (= primary Button disabled)
// Tap: knob slides + track colour fades = Motion pattern "toggle" (press 80 ms, ease-out; reduced motion = instant),
// emits "tap" (detail = the new state, on / off) and plays toggle-on / toggle-off (docs: data-snd follows the state it goes to);
// disabled: emits "blocked" and plays error. Contrast pairs: tokens/colour.json section "toggle".
#include "fw.h"

namespace {
constexpr lv_state_t ST_OFF = LV_STATE_USER_1;  // disabled (LVGL sends no events to LV_STATE_DISABLED; see Button)

struct Toggle {
  bool on = false;
  int32_t shown = 0;  // 0 = off look, 255 = on look (mid-animation in between)
  int travel = 0;
};

// v 0..255: off -> on. Track line -> accent, knob text-dim -> on-accent, knob slides by the travel.
void show(void *o, int32_t v) {
  auto *t = (lv_obj_t *) o;
  auto *d = (Toggle *) lv_obj_get_user_data(t);
  lv_obj_t *k = lv_obj_get_child(t, 0);
  const uint8_t m = (uint8_t) LV_CLAMP(0, v, 255);
  lv_obj_set_style_bg_color(t, lv_color_mix(fw::T->accent, fw::T->line, m), 0);
  lv_obj_set_style_bg_color(k, lv_color_mix(fw::T->on_accent, fw::T->text_dim, m), 0);
  lv_obj_set_style_translate_x(k, d->travel * m / 255, 0);
  d->shown = v;
}
}  // namespace

FW_COMPONENT(toggle, "Toggle", "components", R"({
  "frame": [160, 96],
  "props": [
    {"name": "on", "type": "bool", "default": false},
    {"name": "state", "type": "select", "options": ["default", "disabled"], "default": "default"}
  ],
  "variants": [
    {"label": "off", "props": {}},
    {"label": "on", "props": {"on": true}},
    {"label": "off · disabled", "props": {"state": "disabled"}},
    {"label": "on · disabled", "props": {"on": true, "state": "disabled"}}
  ]
})") {
  const bool disabled = !strcmp(p["state"] | "default", "disabled");
  auto *d = new Toggle;
  d->on = p["on"] | false;

  const int h = fw::touch::min, in = fw::sp::s2, k = h - 2 * in;  // 48 / 8 / 32 at 720, 32 / 4 / 24 at 480
  const int w = 2 * k + 2 * in;
  lv_obj_t *t = lv_obj_create(parent);
  lv_obj_remove_style_all(t);
  lv_obj_add_flag(t, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(t, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(t, w, h);
  lv_obj_set_style_radius(t, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(t, LV_OPA_COVER, 0);
  lv_obj_set_user_data(t, d);
  d->travel = w - k - 2 * in;

  lv_obj_t *kn = lv_obj_create(t);
  lv_obj_remove_style_all(kn);
  lv_obj_remove_flag(kn, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_size(kn, k, k);
  lv_obj_set_pos(kn, in, in);
  lv_obj_set_style_radius(kn, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(kn, LV_OPA_COVER, 0);

  show(t, d->on ? 255 : 0);
  if (disabled) {
    lv_obj_add_state(t, ST_OFF);
    if (d->on) {
      lv_obj_set_style_bg_color(t, fw::T->accent_dim, 0);
      lv_obj_set_style_bg_color(kn, fw::T->text_dim, 0);
    } else {  // hollow: the 1 px border sits inside the box, so the knob keeps its place
      lv_obj_set_style_bg_opa(t, LV_OPA_TRANSP, 0);
      lv_obj_set_style_border_width(t, fw::stroke::hair, 0);
      lv_obj_set_style_border_color(t, fw::T->line, 0);
      lv_obj_set_style_bg_color(kn, fw::T->text_mute, 0);
      lv_obj_set_pos(kn, in - fw::stroke::hair, in - fw::stroke::hair);  // children start inside the border
    }
  }

  lv_obj_add_event_cb(t, [](lv_event_t *e) { delete (Toggle *) lv_event_get_user_data(e); }, LV_EVENT_DELETE, d);
  // a real tap (CLICKED: dropped after a scroll, like Button)
  lv_obj_add_event_cb(t, [](lv_event_t *e) {
    auto *o = (lv_obj_t *) lv_event_get_current_target(e);
    auto *d = (Toggle *) lv_obj_get_user_data(o);
    if (lv_obj_has_state(o, ST_OFF)) { fw::emit(o, "blocked", d->on ? "on" : "off"); fw::sound(o, "error"); return; }
    d->on = !d->on;
    fw::emit(o, "tap", d->on ? "on" : "off");
    fw::sound(o, d->on ? "toggle-on" : "toggle-off");
    fw::animate(o, fw::mo::toggle, {show, d->shown, d->on ? 255 : 0, fw::mo::MOVE});
  }, LV_EVENT_CLICKED, nullptr);
}
