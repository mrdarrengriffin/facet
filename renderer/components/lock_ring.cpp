// Lock ring: the door lock's padlock in its ring, the state AND the control (the panel lab's Door lock hero, board
// facet-53, apps/hub panel/lock lockRing3; Art for the lock, scope "Art (big screen pictures)").
//   ring     lay::lock_ring square (= card-w, a Split's left column), rad::round, surface fill, stroke::edge border;
//            the confirm meter round it (fw::value_arc, stroke::indicator, accent, the full turn from 12 o'clock)
//   padlock  centred, sp::s2 high (lab lock.css: margin -96 on its 176 box), drawn in ONE object (its draw event:
//            one invalidation per change instead of ten; the "many moving things: one layer" rule): the shackle = a
//            half ring (lay::lock_bar, flat ends) on two legs, the body lay::lock_body_w x _h (rad::button) with the keyhole cut
//            in the ring's fill (a round top lay::lock_hole over a slot lay::lock_slot_w x _h). Locked: the legs go
//            lay::lock_overlap into the body. Unlocked: the shackle rises lay::lock_raise with only the LEFT leg still
//            in (the pin), the right leg free: open at a glance (lab lock.css). Parts at the padlock's own
//            proportions; the padlock's box holds the raised shackle, so nothing draws outside a box
//   hint     sp::s5 under the ring, type label, text-dim, centred: "hold to unlock", "keep holding", "tap to lock",
//            "tap to lock again" (jammed, error), "wait" (busy), nothing offline
// States (HA lock): locked = calm (text ink, line-hi edge); unlocked / open = accent edge + ink (not secure);
// jammed = loud (error edge + ink, the ring filled error at chip.tint over the surface, the error sound when it
// starts); locking / unlocking / opening = a quarter of the meter turns (Motion busy-turn, linear; reduced = still);
// unavailable = text-mute, inert. Pressed: the press-tint fill (states.json pressed tint, as Card; Motion press / release).
// Confirm step: unlocking needs a HOLD of Motion hold-confirm (1.2 s): the meter fills; let go early and it drains
// (Motion release) and nothing happens; full = sound confirm + event "lock" "unlock". Locking is the safe way round:
// a tap on an unlocked, open or jammed lock = event "lock" "lock" + sound tap. A short tap on a locked lock only
// says "hold to unlock". Busy / offline taps = blocked + error. After an action the ring shows the motor state
// (locking / unlocking) for up to 5 s until the state prop catches up; with no host answering it goes back.
// Feed: prop state (an HA lock entity's state; the spec binds it, sample "locked").
// Update path: a new state animates like a tap (the shackle slides, colours fade; silent); first build at rest.
// Contrast: section "lock_ring".
#include "fw.h"

namespace {
struct LR {
  lv_obj_t *ring = nullptr, *arc = nullptr, *pad = nullptr, *hint = nullptr;
  int lift = 0, pin_opa = 0;  // the shackle's rise (px) and the pin's opacity, animated; the pad draws them
  std::string state = "locked", held;
  uint32_t held_until = 0;
  bool raised = false, holding = false, done = false;
  lv_timer_t *expire = nullptr;
};
LR *of(lv_obj_t *o) { return (LR *) lv_obj_get_user_data(o); }
lv_obj_t *box(lv_obj_t *parent) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  return o;
}
bool busy(const std::string &s) { return s == "locking" || s == "unlocking" || s == "opening"; }
bool shut(const std::string &s) { return s == "locked" || s == "locking" || s == "jammed"; }  // jammed: bolt half-thrown
bool open_look(const std::string &s) { return s == "unlocked" || s == "open" || s == "unlocking" || s == "opening"; }
const std::string &shown(LR *c) {
  if (!c->held.empty() && (int32_t) (c->held_until - lv_tick_get()) > 0) return c->held;
  c->held.clear();
  return c->state;
}
const char *hint_of(LR *c) {
  const std::string &s = shown(c);
  if (s == "unavailable") return "";
  if (busy(s)) return "wait";
  if (s == "jammed") return "tap to lock again";
  if (s == "locked") return c->holding ? "keep holding" : "hold to unlock";
  return "tap to lock";
}
const lv_style_prop_t PRESS_PROPS[] = {LV_STYLE_BG_COLOR, LV_STYLE_PROP_INV};
const lv_style_transition_dsc_t *press_tr(bool pressed) {
  static lv_style_transition_dsc_t d[2];
  static bool ready = false;
  if (!ready) { fw::st::press_transitions(d, PRESS_PROPS); ready = true; }
  return &d[pressed];
}
void spin(LR *c, bool on) {
  lv_anim_delete(c->arc, nullptr);
  lv_arc_set_rotation(c->arc, 270);
  lv_arc_set_value(c->arc, on ? 250 : 0);
  if (!on || fw::mo::reduced(c->arc)) return;
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, c->arc);
  lv_anim_set_values(&a, 270, 270 + 360);
  lv_anim_set_duration(&a, fw::mo::ms::busy_turn);
  lv_anim_set_path_cb(&a, fw::mo::ease::linear);
  lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
  lv_anim_set_exec_cb(&a, [](void *o, int32_t v) { lv_arc_set_rotation((lv_obj_t *) o, v % 360); });
  lv_anim_start(&a);
}
void paint(LR *c, bool animate) {
  const fw::Theme &t = *fw::T;
  const std::string &s = shown(c);
  const bool off = s == "unavailable", jam = s == "jammed", op = open_look(s);
  const lv_color_t ink = off ? t.text_mute : jam ? t.error : op ? t.accent : t.text;
  const lv_color_t edge = off ? t.line : jam ? t.error : op ? t.accent : t.line_hi;
  const lv_color_t fill = jam ? lv_color_mix(t.error, t.surface, (lv_opa_t) lroundf(fw::tokens::comp::chip_tint * 255)) : t.surface;
  fw::st::restyle(c->ring, [&] {
    lv_obj_set_style_border_color(c->ring, edge, 0);
    lv_obj_set_style_bg_color(c->ring, fill, 0);
    lv_obj_set_style_bg_color(c->pad, ink, 0);       // the pad draws itself from these two (fade with restyle)
    lv_obj_set_style_border_color(c->pad, fill, 0);
  }, animate);
  fw::st::restyle(c->hint, [&] { lv_obj_set_style_text_color(c->hint, jam ? t.error : t.text_dim, 0); }, animate);
  fw::set_text(c->hint, fw::ty::label, hint_of(c));
  const bool up = !shut(s) && !off;
  if (up != c->raised) {
    c->raised = up;
    const int32_t to = up ? fw::lay::lock_raise : 0;
    if (animate) {
      fw::animate(c->pad, fw::mo::toggle, {[](void *o, int32_t v) { LR *c = of(lv_obj_get_parent(lv_obj_get_parent((lv_obj_t *) o))); c->lift = v; lv_obj_invalidate((lv_obj_t *) o); }, c->lift, to});
      c->pin_opa = up ? LV_OPA_COVER : LV_OPA_TRANSP;  // the pin follows the lift (drawn when the leg leaves the body)
    } else {
      c->lift = to, c->pin_opa = up ? LV_OPA_COVER : LV_OPA_TRANSP;
      lv_obj_invalidate(c->pad);
    }
  }
  if (!c->holding) {
    if (busy(s)) { if (!lv_anim_get(c->arc, nullptr)) spin(c, true); }
    else spin(c, false);
  }
}
// the padlock, drawn in one object (one invalidation, not ten): ink = the pad's bg colour, the keyhole = its border
// colour (styles, so fw::st::restyle fades them), the shackle raised by LR::lift
void draw_pad(lv_event_t *e) {
  auto *pad = (lv_obj_t *) lv_event_get_current_target(e);
  LR *c = of(lv_obj_get_parent(lv_obj_get_parent(pad)));
  if (!c) return;
  lv_layer_t *layer = lv_event_get_layer(e);
  lv_area_t a;
  lv_obj_get_coords(pad, &a);
  const int BW = fw::lay::lock_body_w, BH = fw::lay::lock_body_h, SW = fw::lay::lock_shackle_w, SH = fw::lay::lock_shackle_h;
  const int BAR = fw::lay::lock_bar, OV = fw::lay::lock_overlap, UP = fw::lay::lock_raise, top = SH - OV, sx = (BW - SW) / 2;
  const lv_color_t ink = lv_obj_get_style_bg_color(pad, LV_PART_MAIN), fill = lv_obj_get_style_border_color(pad, LV_PART_MAIN);
  auto rect = [&](int x, int y, int w, int h, int r, lv_color_t col, lv_opa_t opa) {
    if (w <= 0 || h <= 0 || opa == LV_OPA_TRANSP) return;
    lv_draw_rect_dsc_t d;
    lv_draw_rect_dsc_init(&d);
    d.bg_color = col, d.bg_opa = opa, d.radius = r;
    lv_area_t q = {a.x1 + x, a.y1 + y, a.x1 + x + w - 1, a.y1 + y + h - 1};
    lv_draw_rect(layer, &d, &q);
  };
  const int sy = UP - c->lift;  // the shackle's top
  lv_draw_arc_dsc_t ad;
  lv_draw_arc_dsc_init(&ad);
  ad.color = ink, ad.width = BAR, ad.start_angle = 180, ad.end_angle = 360, ad.rounded = 0;
  ad.center.x = a.x1 + sx + SW / 2, ad.center.y = a.y1 + sy + SW / 2, ad.radius = SW / 2;
  lv_draw_arc(layer, &ad);
  rect(sx, sy + SW / 2, BAR, SH - SW / 2, 0, ink, LV_OPA_COVER);
  rect(sx + SW - BAR, sy + SW / 2, BAR, SH - SW / 2, 0, ink, LV_OPA_COVER);
  rect(sx, SH - fw::sp::s2, BAR, UP - OV + 2 * fw::sp::s2, 0, ink, (lv_opa_t) (c->lift > 0 ? c->pin_opa : 0));  // the pin
  rect(0, UP + top, BW, BH, fw::rad::button, ink, LV_OPA_COVER);
  const int HO = fw::lay::lock_hole, gap = HO / 2 + fw::sp::s1, gh = gap + fw::lay::lock_slot_h, gy = UP + top + (BH - gh) / 2;
  rect((BW - HO) / 2, gy, HO, HO, LV_RADIUS_CIRCLE, fill, LV_OPA_COVER);
  rect((BW - fw::lay::lock_slot_w) / 2, gy + gap, fw::lay::lock_slot_w, fw::lay::lock_slot_h, LV_RADIUS_CIRCLE, fill, LV_OPA_COVER);
}
void padlock(LR *c) {
  const int R = fw::lay::lock_ring, BW = fw::lay::lock_body_w, BH = fw::lay::lock_body_h;
  const int SH = fw::lay::lock_shackle_h, OV = fw::lay::lock_overlap, UP = fw::lay::lock_raise;
  const int top = SH - OV, H = top + BH, E = fw::stroke::edge;  // children sit inside the ring's border
  c->pad = box(c->ring);  // holds the raised shackle too (UP over it), so nothing draws outside its box
  lv_obj_set_size(c->pad, BW, UP + H);
  lv_obj_set_pos(c->pad, (R - BW) / 2 - E, (R - H) / 2 - fw::sp::s2 - UP - E);  // the lab: sp::s2 high (margin -96 on 176)
  lv_obj_add_event_cb(c->pad, draw_pad, LV_EVENT_DRAW_MAIN, nullptr);
  fw::qa::free(c->pad, "the padlock: one drawn object at its own proportions (lab lock.css), the shackle rises inside it");
}
void act(lv_obj_t *root, LR *c, const char *cmd) {
  fw::emit(root, "lock", cmd);
  c->held = !strcmp(cmd, "lock") ? "locking" : "unlocking";
  c->held_until = lv_tick_get() + 5000;
  if (c->expire) lv_timer_delete(c->expire);
  c->expire = lv_timer_create([](lv_timer_t *tm) {  // no answer: back to the state prop
    auto *root = (lv_obj_t *) lv_timer_get_user_data(tm);
    LR *c = of(root);
    c->expire = nullptr;
    fw::Quiet q;
    paint(c, true);
  }, 5000 + 20, root);
  lv_timer_set_repeat_count(c->expire, 1);
  paint(c, true);
}
void hold_start(lv_obj_t *root, LR *c) {
  c->holding = true, c->done = false;
  spin(c, false);
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, c->arc);
  lv_anim_set_values(&a, 0, 1000);
  lv_anim_set_duration(&a, fw::mo::ms::hold_confirm);
  lv_anim_set_path_cb(&a, fw::mo::ease::linear);
  lv_anim_set_exec_cb(&a, [](void *o, int32_t v) { lv_arc_set_value((lv_obj_t *) o, v); });
  lv_anim_set_user_data(&a, root);
  lv_anim_set_completed_cb(&a, [](lv_anim_t *a) {
    auto *root = (lv_obj_t *) lv_anim_get_user_data(a);
    LR *c = of(root);
    c->holding = false, c->done = true;
    fw::sound(c->ring, "confirm");
    act(root, c, "unlock");
  });
  lv_anim_start(&a);
  fw::set_text(c->hint, fw::ty::label, hint_of(c));
}
void hold_end(LR *c) {
  c->holding = false;
  lv_anim_delete(c->arc, nullptr);
  const int32_t v = lv_arc_get_value(c->arc);
  if (v > 0) fw::animate(c->arc, fw::mo::release, {[](void *o, int32_t v) { lv_arc_set_value((lv_obj_t *) o, v); }, v, 0, fw::mo::VALUE});
  fw::set_text(c->hint, fw::ty::label, hint_of(c));
}
}  // namespace

FW_COMPONENT(lock_ring, "Lock ring", "components", R"({
  "about": "A door lock's padlock in its ring: the state at a glance and the control. Hold to unlock (the ring fills), tap to lock.",
  "frame": [320, 0],
  "props": [
    {"name": "state", "type": "select", "options": ["locked", "unlocked", "locking", "unlocking", "jammed", "open", "opening", "unavailable"], "default": "locked"}
  ],
  "variants": [
    {"label": "locked", "props": {}},
    {"label": "unlocked", "props": {"state": "unlocked"}},
    {"label": "jammed", "props": {"state": "jammed"}},
    {"label": "unlocking", "props": {"state": "unlocking"}},
    {"label": "offline", "props": {"state": "unavailable"}}
  ]
})") {
  const fw::Theme &t = *fw::T;
  const int R = fw::lay::lock_ring;
  auto *c = new LR;
  lv_obj_t *root = box(parent);
  lv_obj_set_user_data(root, c);
  lv_obj_add_event_cb(root, [](lv_event_t *e) {
    auto *c = (LR *) lv_event_get_user_data(e);
    if (c->expire) lv_timer_delete(c->expire);
    delete c;
  }, LV_EVENT_DELETE, c);
  lv_obj_set_size(root, R, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(root, fw::sp::s5, 0);  // the lab: 16 under the ring
  c->ring = box(root);
  lv_obj_add_flag(c->ring, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(c->ring, LV_OBJ_FLAG_PRESS_LOCK);  // slid off = press lost: the hold drains, a tap does nothing
  lv_obj_set_size(c->ring, R, R);
  lv_obj_set_style_radius(c->ring, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(c->ring, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(c->ring, fw::stroke::edge, 0);
  lv_obj_set_style_bg_color(c->ring, fw::st::pressed_tint(t), LV_STATE_PRESSED);  // states.json pressed tint
  lv_obj_set_style_transition(c->ring, press_tr(false), 0);
  lv_obj_set_style_transition(c->ring, press_tr(true), LV_STATE_PRESSED);
  fw::ArcLook look;
  look.start = 270, look.range = 360, look.ind_w = fw::stroke::indicator, look.ind_c = t.accent, look.steps = 1000;
  c->arc = fw::value_arc(c->ring, R, look);
  lv_obj_set_pos(c->arc, -fw::stroke::edge, -fw::stroke::edge);  // the ring's own box, over its border
  padlock(c);
  c->hint = fw::text(root, fw::ty::label, "", t.text_dim, R);
  lv_obj_set_size(c->hint, R, fw::style(fw::ty::label).box);
  lv_obj_set_style_text_align(c->hint, LV_TEXT_ALIGN_CENTER, 0);
  c->state = p["state"] | "locked";
  if (c->state.empty() || c->state == "unknown") c->state = "unavailable";
  const lv_event_cb_t cb = [](lv_event_t *e) {
    auto *root = (lv_obj_t *) lv_event_get_user_data(e);
    LR *c = of(root);
    const lv_event_code_t code = lv_event_get_code(e);
    const std::string s = shown(c);
    if (code == LV_EVENT_PRESSED) {
      if (s == "unavailable" || busy(s)) { fw::st::blocked(c->ring, s.c_str()); return; }
      if (s == "locked") hold_start(root, c);
    } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
      if (c->holding) { hold_end(c); return; }
      if (code == LV_EVENT_RELEASED && !c->done && s != "locked" && s != "unavailable" && !busy(s)) { fw::sound(c->ring, "tap"); act(root, c, "lock"); }
      c->done = false;
    }
  };
  for (lv_event_code_t ev : {LV_EVENT_PRESSED, LV_EVENT_RELEASED, LV_EVENT_PRESS_LOST})  // not ALL: no delete-time events
    lv_obj_add_event_cb(c->ring, cb, ev, root);
  paint(c, false);
}

FW_UPDATE(lock_ring) {
  LR *c = of(root);
  if (!c) return false;
  std::string s = p["state"] | "locked";
  if (s.empty() || s == "unknown") s = "unavailable";
  if (s != c->state && s == "jammed" && !fw::quiet()) fw::sound(root, "error");
  if (!c->held.empty() && s != c->state && !busy(s)) c->held.clear();  // the motor's answer arrived
  c->state = s;
  paint(c, true);
  return true;
}
