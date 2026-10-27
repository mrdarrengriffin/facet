// Dial: the Facet light dial (docs: Screens > Light, Components > Controls > Dial, @facet/ui dial(); approved round 2,
// Darren 2026-10-21). CONTRACT v2 (renderer/core/fx.h, 2026-10-26 release slice 1): built only from token names and the
// Pen; it fills its parent's width up to max-w:dial-scale and is as tall as it is wide; every length comes from the box
// (Pen::half) and size tokens, every ink is a shared colour style read at draw time (a theme / accent switch repaints
// it with no code here).
//   scale   a tick every minor interval over 270 degrees from 135 (open at the bottom), drawn inward from the edge:
//           minor layout:dial-tick line-hi, major layout:dial-tick-major blue with its number (type c-meta, text-dim)
//           space 3 inside it; the range picks 1 / 2 / 5 x 10^k majors, at most 11 numbers (fw::nice_intervals)
//   arc     outer edge (dial-scale - dial-arc) / 2 inside the scale: track stroke:edge line, value stroke:indicator
//           on-fill with round caps, knob layout:dial-knob accent ringed stroke:knob in the canvas colour (bg)
//   centre  wing + value + wing (derived dial-wing-up / -down: the value's middle is the dial's middle): caption
//           (type label, text-dim) / value (display, text) / unit (body, text-dim) and the ON / OFF button
//           (w:dial-toggle-w x h:button-h, radius button: outline ON = accent edge + accent-text label, secondary OFF =
//           surface + line edge; pressed + disabled = the state roles)
// Value model (as before): a step index of min / max / step; drawn position in 1/256 steps; press on the knob = grab,
// elsewhere on the ring = jump with a sweep (Motion value); the centre (inside the arc by more than touch:target) is
// free, so the ring and the button never share a point (the Fresh -/+ bug: tests/functional/light_screen.test.mjs).
// A drag never jumps across the bottom gap. Opening sweeps 0 -> value silently (props "opening": false = no sweep).
// On / off: the button; a drag while off turns it on; releasing at 0 (min 0) turns it off.
// dimmable false (an on/off light, a switch): the ring shows full when on, empty when off, no knob, presses on it do
// nothing; the value slot says On / Off; only the button acts.
// States: disabled (USER_1) = everything in the disabled roles, any press = "blocked" + error; unavailable = disabled
// scale, no arc or knob, "Unavailable" in the value slot, no button; missing = the same with "Not found" (no such entity).
// Redraw (the partial redraw rule): a move invalidates only the arc span between the old and the new angle plus the
// knob's reach (fx::redraw_arc); the Pen skips every primitive outside that area, so the scale is not drawn again.
// Events: "input" (notify, live while dragging), "change" on release, "toggle" on / off, "blocked".
// Contrast pairs: tokens/colour.json section "dial".
#include "fw.h"
#include "fx.h"

namespace {
constexpr int32_t FP = 256;
constexpr float START = 135.0f, RANGE = 270.0f;

struct Dial {
  double min = 0, max = 100, step = 1, major = 10, minor = 2;
  int n = 100, dec = 0, num_dec = 0;
  int idx = 0, live_idx = -1, last_on = 100, press_idx = 0;
  bool on = true, quiet = false, grab = false, moved = false, active = false, dimmable = true;
  int state = 0;           // 0 default, 1 disabled, 2 unavailable, 3 missing (a change = new contents)
  int32_t shown = 0;
  int shown_idx = 0, shown_val = -1;
  lv_point_t press_pt = {0, 0};
  lv_obj_t *val = nullptr, *btn = nullptr, *btn_txt = nullptr, *cap = nullptr, *unit = nullptr;
};

Dial *of(lv_obj_t *o) { return (Dial *) lv_obj_get_user_data(o); }
double value_of(const Dial *d, int i) { return i >= d->n ? d->max : d->min + i * d->step; }
std::string fmt(const Dial *d, double v) {
  char b[16];
  snprintf(b, sizeof b, "%.*f", d->dec, std::fabs(v) < 1e-9 ? 0.0 : v);
  return b;
}
bool off_look(const Dial *d) { return d->state != 0; }
// the geometry, all from the box and tokens: R = the scale's edge, ra = the arc's outer edge
int R_of(int half) { return half + 1; }
int arc_out(int half) { return R_of(half) - (fx::size("layout:dial-scale") - fx::size("layout:dial-arc")) / 2; }
int knob_r(int half) { return arc_out(half) - fx::size("stroke:indicator") / 2; }
float angle(const Dial *d, int32_t shown) { return START + RANGE * shown / (float) (d->n * FP); }

void draw(const fx::Pen &p) {
  const Dial *d = of(p.obj);
  if (!d) return;
  const bool dim = off_look(d);
  // the scale
  const int sub = (int) std::lround(d->major / d->minor);
  const long k0 = (long) std::ceil(d->min / d->minor - 1e-6), k1 = (long) std::floor(d->max / d->minor + 1e-6);
  const int nr = R_of(p.half) - fx::size("layout:dial-tick-major") - fx::size("space:3") - fx::type_box("type:c-meta") / 2;
  for (long k = k0; k <= k1; k++) {
    const double v = k * d->minor;
    const bool maj = sub > 0 && ((k % sub) + sub) % sub == 0;
    const float deg = START + (float) ((v - d->min) / (d->max - d->min)) * RANGE;
    p.tick(deg, maj ? "layout:dial-tick-major" : "layout:dial-tick", "stroke:tick", maj ? (dim ? "fg:disabled-ink" : "fg:blue") : "fg:line-hi", maj ? "tick-major" : "tick");
    if (!maj) continue;
    char b[16];
    snprintf(b, sizeof b, "%.*f", d->num_dec, std::fabs(v) < 1e-9 ? 0.0 : v);
    int x, y;
    fx::at(deg, nr, &x, &y);
    p.text(x, y, b, "type:c-meta", dim ? "fg:disabled-ink" : "fg:text-dim");
  }
  if (d->state >= 2) return;  // unavailable / missing: the scale only
  const int ra = arc_out(p.half), te = fx::size("stroke:edge"), ti = fx::size("stroke:indicator");
  p.arc(ra - te / 2, START, START + RANGE, "stroke:edge", "fg:line", false, "arc");
  const float a1 = angle(d, d->shown);
  p.arc(ra - ti / 2, START, a1, "stroke:indicator", dim ? "fg:disabled-ink" : "fg:on-fill", true, "arc-fill");
  if (!d->dimmable) return;  // an on / off light: no handle
  int x, y;
  fx::at(a1, knob_r(p.half), &x, &y);
  p.knob(x, y, "layout:dial-knob", dim ? "fg:disabled-knob" : "fg:accent", "fg:bg", "stroke:knob");
}

void show(void *o, int32_t v) {
  auto *s = (lv_obj_t *) o;
  auto *d = of(s);
  v = LV_CLAMP(0, v, d->n * FP);
  if (v != d->shown) {  // only the span between the old and new end + the knob's reach repaints
    const int h = fx::half(s);
    fx::redraw_arc(s, knob_r(h), angle(d, d->shown), angle(d, v), fx::size("layout:dial-knob") / 2 + 2);  // + AA fringe
  }
  d->shown = v;
  const int i = (v + FP / 2) / FP;
  if (d->dimmable && i != d->shown_val && d->val) {
    fx::set_text(d->val, fmt(d, value_of(d, i)).c_str(), "type:display");
    d->shown_val = i;
  }
  if (i != d->shown_idx && !d->quiet && !lv_obj_has_state(s, fw::st::OFF)) fw::step_ticks(s, d->shown_idx, i, d->n);
  d->shown_idx = i;
}
void stop_sweep(lv_obj_t *s) { lv_anim_delete(s, (lv_anim_exec_xcb_t) show); }
void unquiet(lv_anim_t *a) { of((lv_obj_t *) lv_anim_get_user_data(a))->quiet = false; }
void sweep_quiet(lv_obj_t *s, int32_t from, int32_t to) {
  of(s)->quiet = true;
  fw::animate(s, fw::st::SWEEP, {show, from, to, fw::mo::VALUE, 0, false, unquiet, s});
}
int32_t target_of(const Dial *d) { return d->on ? (d->dimmable ? d->idx * FP : d->n * FP) : 0; }

void set_on(lv_obj_t *s, bool on) {
  auto *d = of(s);
  d->on = on;
  if (d->btn) {
    if (on) lv_obj_add_state(d->btn, LV_STATE_CHECKED); else lv_obj_remove_state(d->btn, LV_STATE_CHECKED);
    fx::set_text(d->btn_txt, on ? "ON" : "OFF", "type:c-label");
  }
  if (!d->dimmable && d->val) fx::set_text(d->val, on ? "On" : "Off", "type:display");
}

// press point -> step index; in the bottom gap: the nearer end, or while dragging the end the value is at
int idx_at(lv_obj_t *s, bool dragging, int *dist, int *half) {
  auto *d = of(s);
  float deg;
  fx::pointer(s, &deg, dist, half);
  const float rel = std::fmod(deg - START + 720.0f, 360.0f);
  int i;
  if (rel > RANGE) i = dragging ? (d->idx > d->n / 2 ? d->n : 0) : (rel > RANGE + (360.0f - RANGE) / 2 ? 0 : d->n);
  else i = (int) std::lround(rel / RANGE * d->n);
  if (dragging && std::abs(i - d->idx) > d->n / 2) i = d->idx > d->n / 2 ? d->n : 0;  // never across the gap
  return i;
}

void range_of(fw::Props p, Dial *d) {
  d->step = std::max(0.01, (double) (p["step"] | 1.0));
  d->min = p["min"] | 0.0;
  d->max = std::max(d->min + d->step, (double) (p["max"] | 100.0));
  d->n = std::max(1, (int) std::ceil((d->max - d->min) / d->step - 1e-6));
  d->dec = fw::decimals_of(d->step);
  fw::nice_intervals(d->max - d->min, &d->major, &d->minor);
  d->num_dec = fw::decimals_of(d->major);
}
int idx_of(const Dial *d, fw::Props p) {
  double v = p["value"] | 64.0;
  if (!std::isfinite(v)) v = d->min;
  v = LV_CLAMP(d->min, v, d->max);
  return (int) std::lround((v - d->min) / d->step);
}
int state_of(fw::Props p) {
  const char *st = p["state"] | "default";
  return !strcmp(st, "missing") ? 3 : !strcmp(st, "unavailable") ? 2 : !strcmp(st, "disabled") ? 1 : 0;
}

void tap_button(lv_event_t *e) {
  auto *s = (lv_obj_t *) lv_event_get_user_data(e);
  auto *d = of(s);
  const lv_event_code_t code = lv_event_get_code(e);
  if (lv_obj_has_state(s, fw::st::OFF)) {
    if (code == LV_EVENT_PRESSED) fw::st::blocked(s, d->on ? "on" : "off");
    return;
  }
  if (code != LV_EVENT_CLICKED) return;
  stop_sweep(s);
  if (d->on) { if (d->idx) d->last_on = d->idx; }
  else if (d->dimmable) d->idx = d->last_on ? d->last_on : d->n;
  set_on(s, !d->on);
  fw::emit(s, fw::st::SWITCH_EVENT, d->on ? "on" : "off");
  fw::sound(s, d->on ? fw::st::ON_SOUND : fw::st::OFF_SOUND);
  sweep_quiet(s, d->shown, target_of(d));
}

// the ON / OFF button: one box whose looks follow its states (CHECKED = on = outline; PRESSED; USER_1 = disabled)
lv_obj_t *button(lv_obj_t *parent, lv_obj_t *s) {
  auto *d = of(s);
  lv_obj_t *b = fx::box(parent, "centre");
  for (const char *t : {"w:dial-toggle-w", "h:button-h", "radius:button", "bg:surface", "border:line"}) fx::use(b, t);
  fx::use(b, "bg:pressed-surface", LV_STATE_PRESSED);
  fx::use(b, "border:pressed-line", LV_STATE_PRESSED);
  fx::use(b, "bg-none", LV_STATE_CHECKED);
  fx::use(b, "border:accent", LV_STATE_CHECKED);
  fx::use(b, "bg:pressed-outline", LV_STATE_CHECKED | LV_STATE_PRESSED);
  fx::use(b, "border:accent", LV_STATE_CHECKED | LV_STATE_PRESSED);
  fx::use(b, "border:disabled-accent", LV_STATE_CHECKED | fw::st::OFF);
  fx::use(b, "part:control");  // the theme's control (C46), last
  lv_obj_add_flag(b, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_STATE_TRICKLE));
  lv_obj_remove_flag(b, LV_OBJ_FLAG_EVENT_BUBBLE);  // the ring never sees the button's press
  d->btn_txt = fx::text(b, "ON", "type:c-label", "fg:text");
  fx::use(d->btn_txt, "fg:accent-text", LV_STATE_CHECKED);
  fx::use(d->btn_txt, "fg:pressed-ink", LV_STATE_CHECKED | LV_STATE_PRESSED);
  fx::use(d->btn_txt, "fg:disabled-ink", fw::st::OFF);
  lv_obj_add_event_cb(b, tap_button, LV_EVENT_ALL, s);
  return b;
}

void fill(lv_obj_t *s, fw::Props p, bool opening) {
  auto *d = of(s);
  *d = Dial{};
  range_of(p, d);
  d->state = state_of(p);
  d->dimmable = p["dimmable"] | true;
  d->idx = idx_of(d, p);
  d->on = p["on"] | true;
  d->last_on = d->idx ? d->idx : d->n;
  for (const char *t : {"box", "centre", "fit", "max-w:dial-scale"}) fx::use(s, t);
  const bool dim = d->state != 0;

  lv_obj_t *up = fx::box(s, "free"), *down = fx::box(s, "free");
  fx::use(up, "dial-wing-up");
  d->cap = fx::text(up, d->dimmable ? (p["caption"] | "Brightness") : "Power", "type:label", dim ? "fg:disabled-ink" : "fg:text-dim");
  if (d->state >= 2) {
    d->val = nullptr;
    lv_obj_t *slot = fx::box(s, "free");  // the value's slot, the word on its middle (as the approved dial)
    fx::use(slot, "dial-slot");
    fx::text(slot, d->state == 3 ? "Not found" : fw::st::UNAVAILABLE_TEXT, "type:body", "fg:unavailable-note");
    lv_obj_move_to_index(down, -1);
    fx::use(down, "dial-wing-down");
  } else {
    d->val = fx::text(s, d->dimmable ? "0" : "Off", "type:display", dim ? "fg:disabled-ink" : "fg:text");
    lv_obj_move_to_index(down, -1);
    fx::use(down, "dial-wing-down");
    if (d->dimmable) d->unit = fx::text(down, p["unit"] | "%", "type:body", dim ? "fg:disabled-ink" : "fg:text-dim");
    d->btn = button(down, s);
  }
  set_on(s, d->on);
  if (dim) fx::state(s, fw::st::OFF, true);

  const int32_t target = target_of(d);
  d->quiet = true;
  d->shown = 0;
  show(s, 0);
  if (dim || !opening) show(s, target), d->quiet = false;
  else sweep_quiet(s, 0, target);
  lv_obj_invalidate(s);
}

void ring(lv_event_t *e) {
  auto *s = (lv_obj_t *) lv_event_get_current_target(e);
  auto *d = of(s);
  const lv_event_code_t code = lv_event_get_code(e);
  if (code != LV_EVENT_PRESSED && code != LV_EVENT_PRESSING && code != LV_EVENT_RELEASED && code != LV_EVENT_PRESS_LOST) return;
  if (lv_obj_has_state(s, fw::st::OFF)) {
    if (code == LV_EVENT_PRESSED) fw::st::blocked(s, d->state >= 2 ? "unavailable" : fmt(d, value_of(d, d->idx)).c_str());
    return;
  }
  if (!d->dimmable) return;  // an on / off light: only the button acts
  lv_point_t pt = {0, 0};
  if (lv_indev_t *in = lv_indev_active()) lv_indev_get_point(in, &pt);
  if (code == LV_EVENT_PRESSED) {
    int dist, half;
    const int i = idx_at(s, false, &dist, &half);
    d->active = dist >= arc_out(half) - fx::size("touch:target") && dist <= R_of(half);
    if (!d->active) return;  // the centre stays free (and the corners outside the scale)
    stop_sweep(s);
    d->quiet = false;
    d->press_idx = d->idx;
    d->press_pt = pt;
    d->moved = false;
    float deg;
    int pd, ph;
    fx::pointer(s, &deg, &pd, &ph);
    int kx, ky, px, py;
    fx::at(angle(d, d->shown), knob_r(half), &kx, &ky);
    fx::at(deg, pd, &px, &py);
    d->grab = std::hypot(px - kx, py - ky) <= fx::size("touch:min") / 2;
    if (!d->on) { set_on(s, true); fw::emit(s, "toggle", "on"); }  // setting a value switches it on
    d->idx = i;
    if (d->grab) show(s, i * FP);
    else fw::animate(s, fw::st::SWEEP, {show, d->shown, i * FP, fw::mo::VALUE});
  } else if (!d->active) {
    return;
  } else if (code == LV_EVENT_PRESSING) {
    if (!d->grab && !d->moved && std::hypot(pt.x - d->press_pt.x, pt.y - d->press_pt.y) < fx::size("space:2")) return;  // still a tap
    d->moved = true;
    if (!d->grab) { stop_sweep(s); d->grab = true; }
    int dist, half;
    d->idx = idx_at(s, true, &dist, &half);
    show(s, d->idx * FP);
    if (d->idx != d->live_idx) {
      d->live_idx = d->idx;
      fw::notify(s, "input", fmt(d, value_of(d, d->idx)).c_str());
    }
  } else {
    d->active = false;
    if (d->idx != d->press_idx) fw::emit(s, "change", fmt(d, value_of(d, d->idx)).c_str());
    if (d->idx == 0 && d->on && std::fabs(d->min) < 1e-9) { set_on(s, false); fw::emit(s, "toggle", "off"); }
    else if (d->idx) d->last_on = d->idx;
  }
}
}  // namespace

FW_COMPONENT(dial, "Dial", "components", R"({
  "about": "A large circular control for a value such as brightness or temperature, with an on/off button in the centre.",
  "contract": 2,
  "parts": ["control", "knob", "tick", "tick-major", "arc", "arc-fill"],
  "frame": [384, 384],
  "props": [
    {"name": "value", "about": "The value the dial is set to, between its lowest and highest.", "type": "range", "min": -20, "max": 200, "step": 0.5, "default": 64},
    {"name": "min", "about": "The lowest value the dial goes down to.", "type": "range", "min": -20, "max": 100, "step": 1, "default": 0},
    {"name": "max", "about": "The highest value the dial goes up to.", "type": "range", "min": 1, "max": 200, "step": 1, "default": 100},
    {"name": "on", "about": "Whether the thing it controls is on. Off dims the dial; the button in the middle switches it.", "type": "bool", "default": true},
    {"name": "caption", "about": "The word under the value that says what it is, such as Brightness.", "type": "text", "default": "Brightness"},
    {"name": "unit", "about": "The unit shown after the value, such as % or °.", "type": "text", "default": "%"},
    {"name": "step", "about": "How much one step of the dial changes the value.", "type": "range", "min": 0.5, "max": 25, "step": 0.5, "default": 1},
    {"name": "dimmable", "about": "Whether the value can be set. Off = an on/off light or a switch: the ring shows on or off, only the button in the middle works.", "type": "bool", "default": true},
    {"name": "state", "about": "default can be turned; disabled cannot be changed; unavailable shows the device cannot be reached; missing shows there is no such device.", "type": "select", "options": ["default", "disabled", "unavailable", "missing"], "default": "default"}
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
    {"label": "unavailable", "props": {"state": "unavailable"}},
    {"label": "on/off only · on", "props": {"dimmable": false}},
    {"label": "on/off only · off", "props": {"dimmable": false, "on": false}},
    {"label": "missing", "props": {"state": "missing"}}
  ]
})") {
  auto *d = new Dial;
  lv_obj_t *s = fx::drawn(parent, draw, "centre");
  fx::name(s, "Dial");
  lv_obj_set_user_data(s, d);
  lv_obj_add_flag(s, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(s, (lv_obj_flag_t) (LV_OBJ_FLAG_SCROLL_CHAIN | LV_OBJ_FLAG_GESTURE_BUBBLE));
  lv_obj_remove_flag(s, LV_OBJ_FLAG_EVENT_BUBBLE);
  fill(s, p, p["opening"] | true);
  lv_obj_add_event_cb(s, [](lv_event_t *e) { delete (Dial *) lv_event_get_user_data(e); }, LV_EVENT_DELETE, d);
  lv_obj_add_event_cb(s, ring, LV_EVENT_ALL, nullptr);
}

// Update path: value -> sweep there, on / off -> the on / off sweep, caption / unit -> new text; all silent. A different
// state, range or dimmable = new contents in the same root (fw::refill, snap at the value); a state change fades in.
// While a finger holds the dial the user wins (the release reports the user's value).
FW_UPDATE(dial) {
  auto *d = of(root);
  if (!d) return false;
  Dial r;
  range_of(p, &r);
  const int st = state_of(p);
  const bool dimmable = p["dimmable"] | true;
  if (st != d->state || dimmable != d->dimmable || r.n != d->n || std::fabs(r.min - d->min) > 1e-9 ||
      std::fabs(r.max - d->max) > 1e-9 || std::fabs(r.step - d->step) > 1e-9) {
    const bool state_changed = st != d->state;
    fw::refill(root);
    fill(root, p, false);
    if (state_changed) fw::st::fade_in_children(root);
    return true;
  }
  if (d->cap && d->dimmable) fx::set_text(d->cap, p["caption"] | "Brightness", "type:label");
  if (d->unit) fx::set_text(d->unit, p["unit"] | "%", "type:body");
  if (d->active || d->state >= 2) return true;
  const int idx = idx_of(d, p);
  const bool on = p["on"] | true;
  d->idx = idx;
  if (idx) d->last_on = idx;
  if (on != d->on) set_on(root, on);
  const int32_t target = target_of(d);
  if (target != d->shown || lv_anim_get(root, (lv_anim_exec_xcb_t) show)) sweep_quiet(root, d->shown, target);
  return true;
}
