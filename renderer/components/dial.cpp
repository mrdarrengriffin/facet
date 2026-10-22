// Round 2 (Darren 2026-10-21): the range comes from props like the Slider: min, max, step (0.5 steps fine: the value
// shows the step's decimals) and the unit string; the scale numbers pick a 1 / 2 / 5 x 10^k interval with at most 11
// numbers (0-100 -> every 10, minor 2 = the docs ring; 16-30 -> every 2, minor 0.5), ticks on multiples of the minor
// interval. Value, sweep, step ticks, landmarks (min / max) and the tick pitch (step / n) all follow the range.
// Letting go at 0 switches off only when 0 is a real value of the range (brightness), not at a thermostat's minimum.
// Dial: the Facet light dial (docs: Screens > Light, Components > Controls > Dial, @facet/ui dial(); CSS .f-dial). A
// square tick scale 0-100 over 270 degrees (open at the bottom), a value arc inside it with a round knob, and on the
// centre the caption, the value, the unit and an ON / OFF button. Drag the ring to set the value; the centre stays
// free for the button.
//   scale   lay::dial_scale (560 / 376), 51 ticks every 2 % from 135 deg, drawn inward from the edge: minor
//           lay::dial_tick (12 / 8) line-hi, every 5th = major lay::dial_tick_major (20 / 12) blue with its number
//           (0, 10 ... 100; type c-meta, text-dim: text-mute is disabled-only) sp::s3 inside it; ticks stroke::tick
//   arc     lay::dial_arc (432 / 288) centred: lv_arc (already anti-aliased, no buffer; Darren 2026-10-22: no stroke
//           atom), track stroke::edge (2) line, indicator stroke::indicator (6 / 4) accent with round caps, knob lay::dial_knob (28 / 20) accent with a stroke::knob ring in the canvas colour
//   centre  a flex column: caption (type label, caps, text-dim), sp::s7, value (type display, text, the dial's
//           centre = the value's centre), sp::s2, unit (type body, text-dim), sp::s9, the button (lay::dial_toggle_w
//           x touch::button_h: the Button component, outline "ON" / secondary "OFF", docs)
// 480 sizes = docs x 2/3 onto the grid (the docs have no 1x dial): scale 376 so its centre (188) is on the grid.
// Value model = Slider (slider.cpp): the value is a step index (step 1 = 100 steps, or 2 / 5 / 10 / 20 / 25 %), the
// drawn position moves in 1/256 steps so sweeps glide; press on the knob (within touch::min / 2) = grab, press elsewhere
// on the ring = the value is set at once and the arc sweeps there (Motion "value", 400 ms ease-out = docs dial-sweep;
// reduced motion = instant); moving during the sweep makes it follow the finger. Presses inside dial_arc / 2 -
// touch::target from the centre are ignored (docs: the centre stays free, 150 of 216). In the bottom gap the value holds
// the nearer end; a drag never jumps across the gap (100 -> 0), it stays at the end it reached.
// Opening = the docs "opening the screen sweeps up from zero": every build sweeps the arc and number 0 -> value
// (silent), always (no prop). Not on / off, drag or ring taps. Workbench only: props key "opening": false = a re-build
// that is not an opening (playground control changes, demo resets), so the dial appears at its value; meta "opening"
// = the workbench's "Replay opening" button.
// On / off (docs: brightness 0 means off): tap the button = off sweeps the arc to 0, on sweeps back to the value it had
// (or 100 if that was 0); emits "toggle" on / off + sound toggle-on / toggle-off, the sweep itself is silent. Dragging
// while off turns it on; releasing at 0 turns it off.
// Sound: fw::step_ticks for every step the shown value crosses on a drag or a ring tap (ends = landmarks).
// Events: "change" (detail = value) on release when it moved, "toggle" (on / off), "blocked" (disabled / unavailable).
// Disabled (USER_1, Button rule): scale numbers, majors, value, caption, unit and arc text-mute, knob text-dim, the
// button disabled; any press = "blocked" + error once, nothing moves, no opening sweep. Unavailable (Card rule, the
// entity is gone): disabled scale, no arc value or knob, the value slot says "Unavailable" (body, text-dim), no unit,
// no button.
// Redraw cost per drag frame: lv_arc invalidates only the arc between the old and new angle + the knob; the value label
// hugs its text, so a new number invalidates just its own box; the tick ring is drawn in one draw event (no objects per
// tick) and only repaints where those areas cross it.
// Contrast pairs: tokens/colour.json section "dial".
#include "fw.h"

namespace {
constexpr lv_state_t ST_OFF = fw::st::OFF;
constexpr int32_t FP = 256;
constexpr float START = 135.0f, RANGE = 270.0f;  // degrees, LVGL angles (0 = 3 o'clock, clockwise)

// tick ring, scale intervals, value arc: core "Dials and rings" (atoms.h; moved there from here, facet-129)
using fw::TickRing;

// ---- the dial
struct Dial {
  double min = 0, max = 100, step = 1;
  int n = 100, dec = 0;    // value = min + idx x step (the last step = max), n steps; dec = decimals shown
  int idx = 0;             // the value (step index)
  int last_on = 100;       // value to come back to when switched on
  int press_idx = 0;
  bool on = true, quiet = false, grab = false, moved = false, active = false;
  int state = 0;           // 0 default, 1 disabled, 2 unavailable (a change = rebuild)
  int32_t shown = 0;       // drawn position, step index x FP
  int shown_idx = 0, shown_val = -1;  // shown_val = the step index on the label
  lv_point_t press_pt = {0, 0};
  lv_obj_t *arc = nullptr, *val = nullptr, *btn_on = nullptr, *btn_off = nullptr, *cap = nullptr, *unit = nullptr;
};

Dial *of(lv_obj_t *o) { return (Dial *) lv_obj_get_user_data(o); }
double value_of(const Dial *d, int i) { return i >= d->n ? d->max : d->min + i * d->step; }
std::string fmt(const Dial *d, double v) {
  char b[16];
  snprintf(b, sizeof b, "%.*f", d->dec, std::fabs(v) < 1e-9 ? 0.0 : v);
  return b;
}

void show(void *o, int32_t v) {
  auto *s = (lv_obj_t *) o;
  auto *d = of(s);
  v = LV_CLAMP(0, v, d->n * FP);
  d->shown = v;
  if (d->arc) lv_arc_set_value(d->arc, v);
  const int i = (v + FP / 2) / FP;
  if (i != d->shown_val && d->val) {  // only a new number touches the label (its own box)
    lv_label_set_text(d->val, fmt(d, value_of(d, i)).c_str());
    d->shown_val = i;
  }
  if (i != d->shown_idx && !d->quiet && !lv_obj_has_state(s, ST_OFF)) fw::step_ticks(s, d->shown_idx, i, d->n);
  d->shown_idx = i;
}

void stop_sweep(lv_obj_t *s) { lv_anim_delete(s, (lv_anim_exec_xcb_t) show); }
void unquiet(lv_anim_t *a) { of((lv_obj_t *) lv_anim_get_user_data(a))->quiet = false; }
// a silent sweep (opening, on / off): the arc and number glide, no step ticks
void sweep_quiet(lv_obj_t *s, int32_t from, int32_t to) {
  of(s)->quiet = true;
  fw::animate(s, fw::st::SWEEP, {show, from, to, fw::mo::VALUE, 0, false, unquiet, s});
}

void set_on(lv_obj_t *s, bool on) {
  auto *d = of(s);
  d->on = on;
  if (d->btn_on) lv_obj_set_flag(d->btn_on, LV_OBJ_FLAG_HIDDEN, !on);
  if (d->btn_off) lv_obj_set_flag(d->btn_off, LV_OBJ_FLAG_HIDDEN, on);
}

// press point -> step index; in the bottom gap: the nearer end, or while dragging the end the value is at
int idx_at(lv_obj_t *s, lv_point_t pt, bool dragging, float *dist) {
  auto *d = of(s);
  lv_area_t a;
  lv_obj_get_coords(s, &a);
  const float dx = pt.x - (a.x1 + a.x2 + 1) / 2.0f, dy = pt.y - (a.y1 + a.y2 + 1) / 2.0f;
  if (dist) *dist = std::sqrt(dx * dx + dy * dy);
  float rel = std::fmod(std::atan2(dy, dx) * 180.0f / 3.14159265f - START + 720.0f, 360.0f);
  int i;
  if (rel > RANGE) i = dragging ? (d->idx > d->n / 2 ? d->n : 0) : (rel > RANGE + (360.0f - RANGE) / 2 ? 0 : d->n);
  else i = (int) std::lround(rel / RANGE * d->n);
  if (dragging && std::abs(i - d->idx) > d->n / 2) i = d->idx > d->n / 2 ? d->n : 0;  // never across the gap
  return i;
}

// range (round 2): min / max / step like the Slider; a step that does not divide the range ends on max
void range_of(fw::Props p, Dial *d) {
  d->step = std::max(0.01, (double) (p["step"] | 1.0));
  d->min = p["min"] | 0.0;
  d->max = std::max(d->min + d->step, (double) (p["max"] | 100.0));
  d->n = std::max(1, (int) std::ceil((d->max - d->min) / d->step - 1e-6));
  d->dec = fw::decimals_of(d->step);
}
int idx_of(const Dial *d, fw::Props p) {
  const double v = LV_CLAMP(d->min, (double) (p["value"] | 64.0), d->max);
  return (int) std::lround((v - d->min) / d->step);
}
void retext(lv_obj_t *l, fw::ty::Id s, const char *txt) {  // only a changed text touches the label
  if (l && fw::cased(s, txt) != lv_label_get_text(l)) fw::set_text(l, s, txt);
}

lv_obj_t *box(lv_obj_t *parent) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  return o;
}

lv_obj_t *button(lv_obj_t *parent, bool on, bool disabled) {
  const fw::Component *bc = fw::find("button");
  if (!bc) return nullptr;
  ArduinoJson::JsonDocument bp;
  bp["kind"] = on ? "outline" : "secondary";
  bp["label"] = on ? "ON" : "OFF";
  bp["width"] = (int) std::lround(fw::lay::dial_toggle_w / fw::D);  // Button takes whole dp (a float reads as 0 = hug)
  bp["state"] = disabled ? "disabled" : "default";
  bc->build(parent, bp.as<fw::Props>());
  lv_obj_t *b = lv_obj_get_child(parent, -1);
  lv_obj_set_width(b, fw::lay::dial_toggle_w);  // exactly the token, filling the hit box
  lv_obj_remove_flag(b, LV_OBJ_FLAG_CLICKABLE);  // the hit box owns the tap (toggle sounds) and trickles the press look
  lv_obj_add_flag(b, LV_OBJ_FLAG_IGNORE_LAYOUT);
  lv_obj_set_pos(b, 0, 0);
  return b;
}
}  // namespace

namespace {
void fill(lv_obj_t *s, fw::Props p, bool opening) {
  const fw::Theme &t = *fw::T;
  const char *state = p["state"] | "default";
  const bool unavail = !strcmp(state, "unavailable"), off = unavail || !strcmp(state, "disabled");
  auto *d = of(s);
  *d = Dial{};
  range_of(p, d);
  d->state = unavail ? 2 : off ? 1 : 0;
  d->idx = idx_of(d, p);
  d->on = p["on"] | true;
  d->last_on = d->idx ? d->idx : d->n;

  const int S = fw::lay::dial_scale, A = fw::lay::dial_arc, C = S / 2;
  lv_obj_remove_style_all(s);
  lv_obj_add_flag(s, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(s, (lv_obj_flag_t) (LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_SCROLL_CHAIN | LV_OBJ_FLAG_GESTURE_BUBBLE));
  lv_obj_set_size(s, S, S);

  TickRing tr;
  tr.lo = d->min;
  tr.hi = d->max;
  fw::nice_intervals(d->max - d->min, &tr.major, &tr.minor);
  tr.numbers = true;
  tr.num_dec = fw::decimals_of(tr.major);
  tr.minor_len = fw::lay::dial_tick;
  tr.major_len = fw::lay::dial_tick_major;
  tr.width = fw::stroke::tick;
  tr.minor_c = t.line_hi;
  tr.major_c = off ? fw::st::disabled_ink(t) : t.blue;  // states.json disabled roles
  tr.num_c = off ? fw::st::disabled_ink(t) : t.text_dim;
  tr.num_gap = fw::sp::s3;
  fw::tick_ring(s, S, tr);

  if (!unavail) {
    fw::ArcLook k;
    k.start = START;
    k.range = RANGE;
    k.steps = d->n * FP;
    k.track_w = fw::stroke::edge;
    k.track_c = t.line;
    k.ind_w = fw::stroke::indicator;
    k.ind_c = off ? fw::st::disabled_ink(t) : fw::st::on_fill(t);
    k.knob = fw::lay::dial_knob;
    k.knob_c = off ? fw::st::disabled_knob(t) : t.accent;
    k.knob_ring = fw::stroke::knob;
    k.knob_ring_c = t.bg;
    lv_obj_t *a = d->arc = fw::value_arc(s, A, k);
    lv_obj_center(a);
  }

  // centre column: caption, value, unit, button; placed so the value's centre is the dial's centre
  const int cap_h = fw::style(fw::ty::label).box, val_h = fw::style(fw::ty::display).box;
  lv_obj_t *col = box(s);
  lv_obj_set_size(col, A, LV_SIZE_CONTENT);
  lv_obj_set_pos(col, (S - A) / 2, C - val_h / 2 - fw::sp::s7 - cap_h);
  lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(col, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_t *cap = d->cap = fw::text(col, fw::ty::label, p["caption"] | "Brightness", off ? fw::st::disabled_ink(t) : t.text_dim);
  lv_obj_set_height(cap, cap_h);  // the type box (labels default to WRAP, where fw::text leaves the height to content)
  lv_obj_set_style_margin_bottom(cap, fw::sp::s7, 0);
  if (unavail) {
    lv_obj_t *slot = box(col);  // the value slot, holding "Unavailable" on its centre
    lv_obj_set_size(slot, A, val_h);
    lv_obj_center(fw::text(slot, fw::ty::body, fw::st::UNAVAILABLE_TEXT, fw::st::unavailable_note(t)));
  } else {
    d->val = fw::text(col, fw::ty::display, "0", off ? fw::st::disabled_ink(t) : t.text);
    lv_obj_set_height(d->val, val_h);
    lv_obj_set_style_margin_bottom(d->val, fw::sp::s2, 0);
    lv_obj_t *unit = d->unit = fw::text(col, fw::ty::body, p["unit"] | "%", off ? fw::st::disabled_ink(t) : t.text_dim);
    lv_obj_set_height(unit, fw::style(fw::ty::body).box);
    lv_obj_set_style_margin_bottom(unit, fw::sp::s9, 0);
    lv_obj_t *hit = box(col);  // the button's hit box: owns the tap, trickles PRESSED into the button
    lv_obj_add_flag(hit, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_STATE_TRICKLE));
    lv_obj_set_size(hit, fw::lay::dial_toggle_w, fw::touch::button_h);
    d->btn_on = button(hit, true, off);
    d->btn_off = button(hit, false, off);
    lv_obj_add_event_cb(hit, [](lv_event_t *e) {
      auto *s = (lv_obj_t *) lv_event_get_user_data(e);
      auto *d = of(s);
      const lv_event_code_t code = lv_event_get_code(e);
      if (lv_obj_has_state(s, ST_OFF)) {
        if (code == LV_EVENT_PRESSED) fw::st::blocked(s, d->on ? "on" : "off");
        return;
      }
      if (code != LV_EVENT_CLICKED) return;
      stop_sweep(s);
      if (d->on) { if (d->idx) d->last_on = d->idx; }
      else d->idx = d->last_on ? d->last_on : d->n;
      set_on(s, !d->on);
      fw::emit(s, fw::st::SWITCH_EVENT, d->on ? "on" : "off");
      fw::sound(s, d->on ? fw::st::ON_SOUND : fw::st::OFF_SOUND);
      sweep_quiet(s, d->shown, d->on ? d->idx * FP : 0);
    }, LV_EVENT_ALL, s);
  }
  set_on(s, d->on);

  // first draw: silent; the opening sweep rises from 0 (docs), unless disabled / a workbench re-build / reduced motion
  const int32_t target = d->on ? d->idx * FP : 0;
  d->quiet = true;
  show(s, 0);
  if (off || !opening) show(s, target), d->quiet = false;
  else sweep_quiet(s, 0, target);
  if (off) lv_obj_add_state(s, ST_OFF);


}
}  // namespace

FW_COMPONENT(dial, "Dial", "components", R"({
  "about": "A large circular control for a value such as brightness or temperature, with an on/off button in the centre.",
  "frame": [384, 384],
  "props": [
    {"name": "value", "type": "range", "min": -20, "max": 200, "step": 0.5, "default": 64},
    {"name": "min", "type": "range", "min": -20, "max": 100, "step": 1, "default": 0},
    {"name": "max", "type": "range", "min": 1, "max": 200, "step": 1, "default": 100},
    {"name": "on", "type": "bool", "default": true},
    {"name": "caption", "type": "text", "default": "Brightness"},
    {"name": "unit", "type": "text", "default": "%"},
    {"name": "step", "type": "range", "min": 0.5, "max": 25, "step": 0.5, "default": 1},
    {"name": "state", "type": "select", "options": ["default", "disabled", "unavailable"], "default": "default"}
  ],
  "opening": "Opening the screen sweeps the arc and number up from 0 (Motion value, 400 ms).",
  "variants": [
    {"label": "value · 40", "props": {"value": 40}},
    {"label": "value · 100", "props": {"value": 100}},
    {"label": "value · 0", "props": {"value": 0}},
    {"label": "off", "props": {"value": 64, "on": false}},
    {"label": "step 10 · 40", "props": {"value": 40, "step": 10}},
    {"label": "16-30 °C · step 0.5", "props": {"caption": "Target", "unit": "°C", "min": 16, "max": 30, "step": 0.5, "value": 21.5}},
    {"label": "disabled · 64", "props": {"state": "disabled"}},
    {"label": "unavailable", "props": {"state": "unavailable"}}
  ]
})") {
  auto *d = new Dial;
  lv_obj_t *s = lv_obj_create(parent);
  lv_obj_set_user_data(s, d);
  fill(s, p, p["opening"] | true);

  lv_obj_add_event_cb(s, [](lv_event_t *e) { delete (Dial *) lv_event_get_user_data(e); }, LV_EVENT_DELETE, d);
  lv_obj_add_event_cb(s, [](lv_event_t *e) {
    auto *s = (lv_obj_t *) lv_event_get_current_target(e);
    auto *d = of(s);
    const lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_PRESSED && code != LV_EVENT_PRESSING && code != LV_EVENT_RELEASED && code != LV_EVENT_PRESS_LOST) return;
    lv_point_t pt = {0, 0};
    if (lv_indev_t *in = lv_indev_active()) lv_indev_get_point(in, &pt);
    if (lv_obj_has_state(s, ST_OFF)) {
      if (code == LV_EVENT_PRESSED) fw::st::blocked(s, d->arc ? fmt(d, value_of(d, d->idx)).c_str() : "unavailable");
      return;
    }
    if (code == LV_EVENT_PRESSED) {
      float dist = 0;
      const int i = idx_at(s, pt, false, &dist);
      d->active = dist >= fw::lay::dial_arc / 2 - fw::touch::target && dist <= fw::lay::dial_scale / 2;
      if (!d->active) return;  // the centre stays free (and the corners outside the scale)
      stop_sweep(s);
      d->quiet = false;
      d->press_idx = d->idx;
      d->press_pt = pt;
      d->moved = false;
      // knob centre: on the indicator's middle line at the shown angle
      lv_area_t a;
      lv_obj_get_coords(s, &a);
      const float ang = (START + RANGE * d->shown / (float) (d->n * FP)) * 3.14159265f / 180.0f;
      const float kr = fw::lay::dial_arc / 2.0f - fw::stroke::indicator / 2.0f;
      const float kx = (a.x1 + a.x2 + 1) / 2.0f + std::cos(ang) * kr, ky = (a.y1 + a.y2 + 1) / 2.0f + std::sin(ang) * kr;
      d->grab = std::hypot(pt.x - kx, pt.y - ky) <= fw::touch::min / 2;
      if (!d->on) { set_on(s, true); fw::emit(s, "toggle", "on"); }  // setting a value switches it on
      d->idx = i;
      if (d->grab) show(s, i * FP);
      else fw::animate(s, fw::st::SWEEP, {show, d->shown, i * FP, fw::mo::VALUE});
    } else if (!d->active) {
      return;
    } else if (code == LV_EVENT_PRESSING) {
      if (!d->grab && !d->moved && std::hypot(pt.x - d->press_pt.x, pt.y - d->press_pt.y) < fw::sp::s2) return;  // still a tap
      d->moved = true;
      if (!d->grab) { stop_sweep(s); d->grab = true; }
      d->idx = idx_at(s, pt, true, nullptr);
      show(s, d->idx * FP);
    } else {  // released / lost
      d->active = false;
      if (d->idx != d->press_idx) fw::emit(s, "change", fmt(d, value_of(d, d->idx)).c_str());
      if (d->idx == 0 && d->on && std::fabs(d->min) < 1e-9) { set_on(s, false); fw::emit(s, "toggle", "off"); }  // a value of 0 = off (brightness)
      else if (d->idx) d->last_on = d->idx;
    }
  }, LV_EVENT_ALL, nullptr);
}

// Update path (README "Update path"): value -> the arc and number sweep there (Motion value, up or down), on / off ->
// the on / off sweep, caption / unit -> new text; all silent (no step ticks, no toggle / change events or sounds).
// A different state (default / disabled / unavailable) or range (min / max / step) = the dial's contents again in the
// same root (fw::refill, snap at the value, no opening sweep); a state change fades the new contents in
// (fw::st::fade_in_children). While a finger holds the dial the user wins: value and on wait for the next update (the
// release reports the user's value).
FW_UPDATE(dial) {
  auto *d = of(root);
  if (!d) return false;
  const char *state = p["state"] | "default";
  const int st = !strcmp(state, "unavailable") ? 2 : !strcmp(state, "disabled") ? 1 : 0;
  Dial r;
  range_of(p, &r);
  if (st != d->state || r.n != d->n || std::fabs(r.min - d->min) > 1e-9 || std::fabs(r.max - d->max) > 1e-9 ||
      std::fabs(r.step - d->step) > 1e-9) {
    const bool state_changed = st != d->state;
    fw::refill(root);
    fill(root, p, false);
    if (state_changed) fw::st::fade_in_children(root);
    return true;
  }
  retext(d->cap, fw::ty::label, p["caption"] | "Brightness");
  retext(d->unit, fw::ty::body, p["unit"] | "%");
  if (d->active || !d->arc) return true;
  const int idx = idx_of(d, p);
  const bool on = p["on"] | true;
  d->idx = idx;
  if (idx) d->last_on = idx;
  if (on != d->on) set_on(root, on);
  const int32_t target = on ? idx * FP : 0;
  if (target != d->shown || lv_anim_get(root, (lv_anim_exec_xcb_t) show)) sweep_quiet(root, d->shown, target);
  return true;
}
