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
//   off · disabled  hollow track (1 px line border, no fill), text-dim knob (every disabled knob, Darren 2026-10-21)
//   on · disabled   accent-dim track, text-dim knob (= primary Button disabled)
// Tap: knob slides + track colour fades = Motion pattern "toggle" (press 80 ms, ease-out; reduced motion = instant),
// emits "toggle" (detail = the new state, on / off; every switch emits toggle, Darren 2026-10-21) and plays toggle-on / toggle-off (docs: data-snd follows the state it goes to);
// disabled: emits "blocked" and plays error on finger down. Unavailable (states.json): the word "Unavailable" (body,
// text-dim) in place of the switch, touch::min tall, finger down = blocked + error. Contrast pairs: tokens/colour.json section "toggle".
#include "fw.h"

namespace {
constexpr lv_state_t ST_OFF = fw::st::OFF, ST_ON = fw::st::ON;  // disabled (LVGL sends no events to LV_STATE_DISABLED; see Button)

struct Toggle {
  bool on = false, unavail = false;
  int32_t shown = 0;  // 0 = off look, 255 = on look (mid-animation in between)
  int travel = 0;
};

// v 0..255: off -> on. Track line -> accent, knob text-dim -> on-accent, knob slides by the travel.
void show(void *o, int32_t v) {
  auto *t = (lv_obj_t *) o;
  auto *d = (Toggle *) lv_obj_get_user_data(t);
  lv_obj_t *k = lv_obj_get_child(t, 0);
  const uint8_t m = (uint8_t) LV_CLAMP(0, v, 255);
  lv_obj_set_style_bg_color(t, lv_color_mix(fw::st::on_fill(*fw::T), fw::T->line, m), 0);  // states.json on roles
  lv_obj_set_style_bg_color(k, lv_color_mix(fw::st::on_label(*fw::T), fw::T->text_dim, m), 0);
  lv_obj_set_style_translate_x(k, d->travel * m / 255, 0);
  d->shown = v;
}
}  // namespace

namespace {
void fill(lv_obj_t *t, fw::Props p) {
  auto *d = (Toggle *) lv_obj_get_user_data(t);
  d->on = p["on"] | false;
  d->unavail = !strcmp(p["state"] | "default", "unavailable");
  const bool disabled = !strcmp(p["state"] | "default", "disabled");
  if (!strcmp(p["state"] | "default", "unavailable")) {  // states.json unavailable: the word in place of the switch
    lv_obj_t *u = t;
    lv_obj_remove_style_all(u);
    lv_obj_add_flag(u, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(u, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(u, LV_SIZE_CONTENT, fw::touch::min);
    lv_obj_t *l = fw::text(u, fw::ty::body, fw::st::UNAVAILABLE_TEXT, fw::st::unavailable_note(*fw::T));
    lv_obj_remove_flag(l, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_center(l);
    lv_obj_update_layout(l);
    lv_obj_set_width(u, (lv_obj_get_width(l) + fw::sp::GRID - 1) / fw::sp::GRID * fw::sp::GRID);  // on the grid
    lv_obj_add_state(u, ST_OFF);  // a tap = blocked "unavailable" (the root's tap handler)
    return;
  }

  const int h = fw::touch::min, in = fw::sp::s2, k = h - 2 * in;  // 48 / 8 / 32 at 720, 32 / 4 / 24 at 480
  const int w = 2 * k + 2 * in;
  lv_obj_remove_style_all(t);
  lv_obj_add_flag(t, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(t, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(t, w, h);
  lv_obj_set_style_radius(t, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(t, LV_OPA_COVER, 0);
  d->travel = w - k - 2 * in;

  lv_obj_t *kn = lv_obj_create(t);
  lv_obj_remove_style_all(kn);
  lv_obj_remove_flag(kn, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_size(kn, k, k);
  lv_obj_set_pos(kn, in, in);
  lv_obj_set_style_radius(kn, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(kn, LV_OPA_COVER, 0);

  // disabled looks = state styles (st::OFF, + st::ON when on), so an update moves between them in place
  // (fw::st::set_off fades the colours): off = hollow (1 px line border inside the box, the knob held in place by
  // x / y at OFF), on = accent-dim track + text-dim knob; every disabled knob text-dim (Darren 2026-10-21)
  lv_obj_set_style_bg_opa(t, LV_OPA_TRANSP, ST_OFF);
  lv_obj_set_style_border_width(t, fw::stroke::hair, ST_OFF);
  lv_obj_set_style_border_color(t, fw::st::disabled_line(*fw::T), ST_OFF);
  lv_obj_set_style_bg_opa(t, LV_OPA_COVER, ST_OFF | ST_ON);
  lv_obj_set_style_bg_color(t, fw::st::disabled_accent(*fw::T), ST_OFF | ST_ON);  // states.json disabled roles
  lv_obj_set_style_border_color(t, fw::st::disabled_accent(*fw::T), ST_OFF | ST_ON);
  lv_obj_set_style_bg_color(kn, fw::st::disabled_knob(*fw::T), ST_OFF);
  lv_obj_set_style_bg_color(kn, fw::st::disabled_on_fill(*fw::T), ST_OFF | ST_ON);
  lv_obj_set_style_x(kn, in - fw::stroke::hair, ST_OFF);  // children start inside the border: the knob keeps its place
  lv_obj_set_style_y(kn, in - fw::stroke::hair, ST_OFF);

  show(t, d->on ? 255 : 0);
  if (d->on) fw::st::set_state(t, ST_ON, true, false);
  if (disabled) fw::st::set_off(t, true, false);


}
}  // namespace

FW_COMPONENT(toggle, "Toggle", "components", R"({
  "about": "An on/off switch.",
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
  lv_obj_set_user_data(t, d);
  fill(t, p);
  lv_obj_add_event_cb(t, [](lv_event_t *e) { delete (Toggle *) lv_event_get_user_data(e); }, LV_EVENT_DELETE, d);
  // a real tap (CLICKED: dropped after a scroll, like Button)
  fw::st::on_tap(t, [](lv_event_t *e) {  // blocked on finger down, a real tap on CLICKED (states.json)
    if (!fw::st::tap_now(e)) return;
    auto *o = (lv_obj_t *) lv_event_get_current_target(e);
    auto *d = (Toggle *) lv_obj_get_user_data(o);
    if (lv_obj_has_state(o, ST_OFF)) { fw::st::blocked(o, d->unavail ? "unavailable" : d->on ? "on" : "off"); return; }
    d->on = !d->on;
    fw::st::set_state(o, ST_ON, d->on, false);  // the knob slide below is the animation
    fw::emit(o, fw::st::SWITCH_EVENT, d->on ? "on" : "off");
    fw::sound(o, d->on ? fw::st::ON_SOUND : fw::st::OFF_SOUND);
    fw::animate(o, fw::st::SWITCH, {show, d->shown, d->on ? 255 : 0, fw::mo::MOVE});
  }, nullptr);
}

// Update path: on -> the knob slides (Motion toggle, silent: the slide plays no sound), default <-> disabled ->
// fw::st::set_off fades in place; to or from unavailable (the word in place of the switch) = the contents again in
// the same root (fw::refill, snap) and the knob / word fade in (fw::st::fade_in_children). Silent.
FW_UPDATE(toggle) {
  auto *d = (Toggle *) lv_obj_get_user_data(root);
  if (!d) return false;
  const bool unavail = !strcmp(p["state"] | "default", "unavailable");
  if (unavail || d->unavail) {
    if (unavail && d->unavail) return true;
    lv_anim_delete(root, (lv_anim_exec_xcb_t) show);
    fw::refill(root);
    fill(root, p);
    fw::st::fade_in_children(root);
    return true;
  }
  const bool on = p["on"] | false, off = !strcmp(p["state"] | "default", "disabled");
  if (on != d->on) {
    d->on = on;
    fw::st::set_state(root, ST_ON, on, false);
    fw::animate(root, fw::st::SWITCH, {show, d->shown, on ? 255 : 0, fw::mo::MOVE});
  }
  if (off != lv_obj_has_state(root, ST_OFF)) fw::st::set_off(root, off);
  return true;
}
