// Thermostat dial: the Facet thermostat ring (docs: Screens > Thermostat, @facet/ui controls.ts thermostatDial(), CSS
// .f-thermo). A square scale with a tick a degree over 270 degrees (open at the bottom), an arc track inside it, the
// set point as the knob and the measured temperature as a long tick in the text colour. While it heats, the arc from
// the measured temperature up to the set point is the accent; cooling: blue, from the set point up to the measured
// one. Drag the ring to move the set point in steps; the centre stays free for the screen's text and buttons.
// Built only from the core "Dials and rings" atoms (atoms.h): fw::tick_ring (the scale), fw::value_arc (track + span)
// and fw::ring_frac (where the finger is), the same ones the Dial uses.
//   scale   lay::thermo_scale (480 / 320); a tick on every whole degree in [min, max], drawn inward from the edge:
//           minor lay::dial_tick (12 / 8) line-hi, multiples of 5 = major lay::dial_tick_major (20 / 12) blue,
//           stroke::tick (docs 10 / 18: the Dial's tick lengths, so both dials share the marks), no numbers (docs)
//   arc     lay::thermo_arc (416 / 280) centred: track stroke::edge line, the span stroke::indicator (6 / 4) round caps
//           on the same lv_arc's indicator (lv_arc_set_angles: the span runs between two values, not from the start)
//   knob    lay::dial_knob (28 / 20) accent, stroke::knob ring in the canvas colour, on the indicator's middle line
//   marker  the measured temperature: lay::thermo_now (32 / 20) inward from the edge, stroke::arc (4 / 2) text, round
//           caps (docs 30 x 4), drawn in a draw event; a move invalidates only the old and new marker boxes
// Value model = the Dial's: the set point is a step index (min + i x step, the last step = max), drawn in 1/256 steps
// so sweeps glide; press on the knob (within touch::min / 2) = grab, press elsewhere on the ring = the set point jumps
// there and the knob sweeps (Motion value; docs jumps: the Dial's tap sweep, so Facet's dials feel the same), moving
// makes it follow the finger; a drag never jumps across the gap. Presses inside thermo_arc / 2 - touch::target of the
// centre are ignored (docs: r < 150 stays free). Opening = the knob sweeps up from min (docs set(..., fromMin), silent;
// props "opening": false = a re-build that is not an opening).
// action: heating (accent span current -> set point when the set point is above), cooling (blue span set point ->
// current when below), idle (no span), off (no set point: no knob, no span, the ring takes no touch; docs target null).
// Sound: fw::step_ticks for every step the shown set point crosses on a drag or ring tap (ends = landmarks).
// Events: "change" (detail = the set point) on release when it moved, "blocked" (disabled / unavailable); live:
// fw::notify "input" (the set point) on every step while dragging (listeners only, the Dial's rule).
// Disabled (st::OFF): majors, span and marker text-mute, knob text-dim; any press = "blocked" + error once, no opening
// sweep. Unavailable: disabled scale, no knob, span or marker, "Unavailable" (body, text-dim) on the centre.
// Update path: set point -> quiet sweep; current -> the marker moves (Motion value, quiet); action -> the span (snap);
// range / state -> the contents again in the same root (state changes fade in). While a finger holds the ring the user
// wins: the set point waits for the next update. Contrast pairs: tokens/colour.json section "thermostat_dial".
#include "fw.h"

namespace {
constexpr lv_state_t ST_OFF = fw::st::OFF;
constexpr int32_t FP = 256;
constexpr float START = 135.0f, RANGE = 270.0f;
// a number prop that may come as a string (a playground select: "0.5")
double num(fw::Props p, const char *k, double def) {
  if (p[k].is<const char *>()) return atof(p[k].as<const char *>());
  return p[k] | def;
}

struct Thermo {
  double min = 5, max = 30, step = 0.5, current = 19.4;
  int n = 50, dec = 1, idx = 0, press_idx = 0, live_idx = -1;
  int action = 0;  // 0 heating, 1 cooling, 2 idle, 3 off
  int state = 0;   // 0 default, 1 disabled, 2 unavailable
  bool quiet = false, grab = false, moved = false, active = false;
  int32_t shown = 0;  // drawn set point, step index x FP
  int shown_idx = 0;
  double shown_cur = 19.4;  // drawn measured temperature
  lv_point_t press_pt = {0, 0};
  lv_obj_t *arc = nullptr, *knob = nullptr, *marker = nullptr;
};
Thermo *of(lv_obj_t *o) { return (Thermo *) lv_obj_get_user_data(o); }
double value_of(const Thermo *d, int i) { return i >= d->n ? d->max : d->min + i * d->step; }
std::string fmt(const Thermo *d, double v) {
  char b[16];
  snprintf(b, sizeof b, "%.*f", d->dec, std::fabs(v) < 1e-9 ? 0.0 : v);
  return b;
}
float frac_of(const Thermo *d, double v) { return (float) LV_CLAMP(0.0, (v - d->min) / (d->max - d->min), 1.0); }
float deg_of(const Thermo *d, double v) { return fw::ring_deg(START, RANGE, frac_of(d, v)); }
double shown_value(const Thermo *d) { return std::min(d->max, d->min + d->shown / (double) FP * d->step); }

int action_of(fw::Props p) {
  const char *a = p["action"] | "heating";
  return !strcmp(a, "cooling") ? 1 : !strcmp(a, "idle") ? 2 : !strcmp(a, "off") ? 3 : 0;
}
void range_of(fw::Props p, Thermo *d) {
  d->step = std::max(0.1, num(p, "step", 0.5));
  d->min = p["min"] | 5.0;
  d->max = std::max(d->min + d->step, (double) (p["max"] | 30.0));
  d->n = std::max(1, (int) std::ceil((d->max - d->min) / d->step - 1e-6));
  d->dec = fw::decimals_of(d->step);
}
int idx_of(const Thermo *d, fw::Props p) {
  const double v = LV_CLAMP(d->min, (double) (p["target"] | 21.0), d->max);
  return (int) std::lround((v - d->min) / d->step);
}

}  // namespace

namespace {
// the measured-temperature marker: one line in a draw event on a scale-sized box; moves invalidate its two boxes only
struct StrokeLine { float x1, y1, x2, y2; };
StrokeLine marker_at(lv_obj_t *m, const Thermo *d, double v) {
  lv_area_t a;
  lv_obj_get_coords(m, &a);
  const float cx = (a.x1 + a.x2 + 1) / 2.0f, cy = (a.y1 + a.y2 + 1) / 2.0f, R = lv_area_get_width(&a) / 2.0f;
  const fw::StrokePt p1 = fw::polar(cx, cy, R - fw::stroke::arc / 2.0f, deg_of(d, v));  // the round cap ends on the edge
  const fw::StrokePt p2 = fw::polar(cx, cy, R - fw::lay::thermo_now, deg_of(d, v));
  return {p1.x, p1.y, p2.x, p2.y};
}
void marker_dirty(lv_obj_t *m, const Thermo *d, double v) {
  const StrokeLine l = marker_at(m, d, v);
  const int w = fw::stroke::arc + 2;
  lv_area_t a = {(int32_t) std::floor(std::min(l.x1, l.x2)) - w, (int32_t) std::floor(std::min(l.y1, l.y2)) - w,
                 (int32_t) std::ceil(std::max(l.x1, l.x2)) + w, (int32_t) std::ceil(std::max(l.y1, l.y2)) + w};
  lv_obj_invalidate_area(m, &a);
}
void marker_draw(lv_event_t *e) {
  auto *m = (lv_obj_t *) lv_event_get_current_target(e);
  auto *s = (lv_obj_t *) lv_event_get_user_data(e);
  const Thermo *d = of(s);
  const StrokeLine l = marker_at(m, d, d->shown_cur);
  lv_draw_line_dsc_t ld;
  lv_draw_line_dsc_init(&ld);
  ld.color = d->state ? fw::st::disabled_ink(*fw::T) : fw::T->text;
  ld.width = fw::stroke::arc;
  ld.round_start = ld.round_end = 1;
  auto pix = [](float v) { return (int32_t) std::lround(v); };  // the pixel edge grid (atoms.cpp ring_draw)
  ld.p1.x = pix(l.x1); ld.p1.y = pix(l.y1);
  ld.p2.x = pix(l.x2); ld.p2.y = pix(l.y2);
  lv_draw_line(lv_event_get_layer(e), &ld);
}

// the span between the measured temperature and the shown set point (heating up / cooling down), else hidden
void span(lv_obj_t *s) {
  auto *d = of(s);
  if (!d->arc) return;
  const double t = shown_value(d), c = d->shown_cur;
  const bool heat = d->action == 0 && t > c, cool = d->action == 1 && t < c;
  // styles only when they change: a style set invalidates the whole arc (35 % of the screen per sweep frame)
  const lv_opa_t opa = heat || cool ? LV_OPA_COVER : LV_OPA_TRANSP;
  if (lv_obj_get_style_arc_opa(d->arc, LV_PART_INDICATOR) != opa) lv_obj_set_style_arc_opa(d->arc, opa, LV_PART_INDICATOR);
  if (!heat && !cool) return;
  const fw::Theme &tk = *fw::T;
  const lv_color_t col = d->state ? fw::st::disabled_ink(tk) : heat ? fw::st::on_fill(tk) : tk.blue;
  if (!lv_color_eq(lv_obj_get_style_arc_color(d->arc, LV_PART_INDICATOR), col)) lv_obj_set_style_arc_color(d->arc, col, LV_PART_INDICATOR);
  const float a0 = deg_of(d, heat ? c : t), a1 = deg_of(d, heat ? t : c);
  lv_arc_set_angles(d->arc, (lv_value_precise_t) std::fmod(a0, 360.0f), (lv_value_precise_t) std::fmod(a1, 360.0f));
}
void place_knob(lv_obj_t *s) {
  auto *d = of(s);
  if (!d->knob) return;
  const int S = fw::lay::thermo_scale, K = fw::lay::dial_knob;
  const float R = fw::lay::thermo_arc / 2.0f - fw::stroke::indicator / 2.0f;
  const fw::StrokePt c = fw::polar(S / 2.0f, S / 2.0f, R, deg_of(d, shown_value(d)));
  lv_obj_set_pos(d->knob, (int32_t) std::lround(c.x - K / 2.0f), (int32_t) std::lround(c.y - K / 2.0f));
}

void show(void *o, int32_t v) {
  auto *s = (lv_obj_t *) o;
  auto *d = of(s);
  v = LV_CLAMP(0, v, d->n * FP);
  d->shown = v;
  place_knob(s);
  span(s);
  const int i = (v + FP / 2) / FP;
  if (i != d->shown_idx && !d->quiet && !lv_obj_has_state(s, ST_OFF)) fw::step_ticks(s, d->shown_idx, i, d->n);
  d->shown_idx = i;
}
void show_cur(void *o, int32_t v) {  // v = the measured temperature x 100
  auto *s = (lv_obj_t *) o;
  auto *d = of(s);
  if (!d->marker) { d->shown_cur = v / 100.0; return; }
  marker_dirty(d->marker, d, d->shown_cur);
  d->shown_cur = v / 100.0;
  marker_dirty(d->marker, d, d->shown_cur);
  span(s);
}
void stop_sweep(lv_obj_t *s) { lv_anim_delete(s, (lv_anim_exec_xcb_t) show); }
void unquiet(lv_anim_t *a) { of((lv_obj_t *) lv_anim_get_user_data(a))->quiet = false; }
void sweep_quiet(lv_obj_t *s, int32_t from, int32_t to) {
  of(s)->quiet = true;
  fw::animate(s, fw::st::SWEEP, {show, from, to, fw::mo::VALUE, 0, false, unquiet, s});
}

lv_obj_t *box(lv_obj_t *parent) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  return o;
}

void fill(lv_obj_t *s, fw::Props p, bool opening) {
  const fw::Theme &t = *fw::T;
  const char *state = p["state"] | "default";
  const bool unavail = !strcmp(state, "unavailable"), off = unavail || !strcmp(state, "disabled");
  auto *d = of(s);
  *d = Thermo{};
  range_of(p, d);
  d->state = unavail ? 2 : off ? 1 : 0;
  d->idx = idx_of(d, p);
  d->action = action_of(p);
  d->current = d->shown_cur = LV_CLAMP(d->min, (double) (p["current"] | 19.4), d->max);

  const int S = fw::lay::thermo_scale, A = fw::lay::thermo_arc;
  lv_obj_remove_style_all(s);
  lv_obj_add_flag(s, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(s, (lv_obj_flag_t) (LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_SCROLL_CHAIN | LV_OBJ_FLAG_GESTURE_BUBBLE));
  lv_obj_set_size(s, S, S);

  fw::TickRing tr;  // a tick a degree, majors every 5 (docs)
  tr.lo = d->min;
  tr.hi = d->max;
  tr.minor = 1;
  tr.major = 5;
  tr.start = START;
  tr.range = RANGE;
  tr.minor_len = fw::lay::dial_tick;
  tr.major_len = fw::lay::dial_tick_major;
  tr.width = fw::stroke::tick;
  tr.minor_c = t.line_hi;
  tr.major_c = off ? fw::st::disabled_ink(t) : t.blue;
  fw::tick_ring(s, S, tr);

  fw::ArcLook k;
  k.start = START;
  k.range = RANGE;
  k.track_w = fw::stroke::edge;
  k.track_c = t.line;
  k.ind_w = fw::stroke::indicator;
  k.ind_c = fw::st::on_fill(t);
  lv_obj_t *a = fw::value_arc(s, A, k);
  lv_obj_center(a);
  if (unavail) {
    lv_obj_set_style_arc_opa(a, LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_obj_t *slot = box(s);  // the centre: "Unavailable"
    lv_obj_set_size(slot, A, fw::style(fw::ty::body).box);
    lv_obj_center(slot);
    lv_obj_center(fw::text(slot, fw::ty::body, fw::st::UNAVAILABLE_TEXT, fw::st::unavailable_note(t)));
  } else {
    d->arc = a;
    lv_obj_t *m = d->marker = box(s);  // measured temperature
    lv_obj_set_size(m, S, S);
    lv_obj_add_event_cb(m, marker_draw, LV_EVENT_DRAW_MAIN, s);
    if (d->action != 3) {
      lv_obj_t *kn = d->knob = box(s);
      lv_obj_add_flag(kn, LV_OBJ_FLAG_IGNORE_LAYOUT);
      lv_obj_set_size(kn, fw::lay::dial_knob, fw::lay::dial_knob);
      lv_obj_set_style_radius(kn, LV_RADIUS_CIRCLE, 0);
      lv_obj_set_style_bg_opa(kn, LV_OPA_COVER, 0);
      lv_obj_set_style_bg_color(kn, off ? fw::st::disabled_knob(t) : t.accent, 0);
      lv_obj_set_style_border_width(kn, fw::stroke::knob, 0);
      lv_obj_set_style_border_color(kn, t.bg, 0);
      fw::qa::free(kn, "the set-point knob sits on the arc at its value's angle");
    }
  }
  // first draw silent; the opening sweep rises from min (docs), unless disabled / a re-build / off
  const int32_t target = d->idx * FP;
  d->quiet = true;
  show(s, 0);
  if (off || !opening || d->action == 3) show(s, target), d->quiet = false;
  else sweep_quiet(s, 0, target);
  if (off) lv_obj_add_state(s, ST_OFF);
}
}  // namespace

FW_COMPONENT(thermostat_dial, "Thermostat dial", "components", R"({
  "about": "The thermostat ring: a tick a degree, the set point as the knob, the measured temperature as a long tick, and the heating or cooling span between them. Drag the ring to set it.",
  "frame": [336, 336],
  "props": [
    {"name": "target", "about": "The temperature it is set to, shown as the knob on the ring.", "type": "range", "min": 5, "max": 30, "step": 0.5, "default": 21},
    {"name": "current", "about": "The temperature measured in the room, shown as a long tick.", "type": "range", "min": 5, "max": 30, "step": 0.1, "default": 19.4},
    {"name": "action", "about": "What it is doing now: heating or cooling colours the span between the two, idle and off leave it plain.", "type": "select", "options": ["heating", "cooling", "idle", "off"], "default": "heating"},
    {"name": "min", "about": "The lowest temperature on the ring.", "type": "range", "min": 0, "max": 20, "step": 1, "default": 5},
    {"name": "max", "about": "The highest temperature on the ring.", "type": "range", "min": 20, "max": 40, "step": 1, "default": 30},
    {"name": "step", "about": "How much one step changes the set temperature, in degrees.", "type": "select", "options": ["0.5", "1", "0.1"], "default": "0.5"},
    {"name": "state", "about": "default can be turned; disabled cannot be changed; unavailable shows the thermostat cannot be reached.", "type": "select", "options": ["default", "disabled", "unavailable"], "default": "default"}
  ],
  "opening": "Opening the screen sweeps the set point up from the minimum (Motion value, 400 ms).",
  "variants": [
    {"label": "heating · 21 / 19.4", "props": {}},
    {"label": "cooling · 19 / 23", "props": {"action": "cooling", "target": 19, "current": 23}},
    {"label": "idle · at 21", "props": {"action": "idle", "current": 21}},
    {"label": "off", "props": {"action": "off"}},
    {"label": "disabled", "props": {"state": "disabled"}},
    {"label": "unavailable", "props": {"state": "unavailable"}}
  ]
})") {
  auto *d = new Thermo;
  lv_obj_t *s = lv_obj_create(parent);
  lv_obj_set_user_data(s, d);
  fill(s, p, p["opening"] | true);
  lv_obj_add_event_cb(s, [](lv_event_t *e) { delete (Thermo *) lv_event_get_user_data(e); }, LV_EVENT_DELETE, d);
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
    if (!d->knob) return;  // off: nothing to set (docs)
    if (code == LV_EVENT_PRESSED) {
      float dist = 0;
      const float f = fw::ring_frac(s, pt, START, RANGE, &dist);
      d->active = dist >= fw::lay::thermo_arc / 2 - fw::touch::target && dist <= fw::lay::thermo_scale / 2;
      if (!d->active) return;  // the centre stays free
      stop_sweep(s);
      d->quiet = false;
      d->press_idx = d->idx;
      d->live_idx = d->idx;
      d->press_pt = pt;
      d->moved = false;
      lv_area_t ka;
      lv_obj_get_coords(d->knob, &ka);
      d->grab = std::hypot(pt.x - (ka.x1 + ka.x2 + 1) / 2.0f, pt.y - (ka.y1 + ka.y2 + 1) / 2.0f) <= fw::touch::min / 2;
      if (!d->grab) {  // a ring tap: the set point jumps there, the knob sweeps
        d->idx = (int) std::lround(f * (d->max - d->min) / d->step);
        d->idx = LV_CLAMP(0, d->idx, d->n);
        fw::animate(s, fw::st::SWEEP, {show, d->shown, d->idx * FP, fw::mo::VALUE});
      }
    } else if (!d->active) {
      return;
    } else if (code == LV_EVENT_PRESSING) {
      if (!d->grab && !d->moved && std::hypot(pt.x - d->press_pt.x, pt.y - d->press_pt.y) < fw::sp::s2) return;  // still a tap
      d->moved = true;
      if (!d->grab) { stop_sweep(s); d->grab = true; }
      const float f = fw::ring_frac(s, pt, START, RANGE, nullptr, frac_of(d, value_of(d, d->idx)));
      d->idx = LV_CLAMP(0, (int) std::lround(f * (d->max - d->min) / d->step), d->n);
      show(s, d->idx * FP);
      if (d->idx != d->live_idx) {  // live set point while dragging (the Dial's rule: listeners only, not an event)
        d->live_idx = d->idx;
        fw::notify(s, "input", fmt(d, value_of(d, d->idx)).c_str());
      }
    } else {  // released / lost
      d->active = false;
      if (d->idx != d->press_idx) fw::emit(s, "change", fmt(d, value_of(d, d->idx)).c_str());
    }
  }, LV_EVENT_ALL, nullptr);
}

FW_UPDATE(thermostat_dial) {
  auto *d = of(root);
  if (!d) return false;
  const char *state = p["state"] | "default";
  const int st = !strcmp(state, "unavailable") ? 2 : !strcmp(state, "disabled") ? 1 : 0;
  Thermo r;
  range_of(p, &r);
  const int act = action_of(p);
  if (st != d->state || r.n != d->n || std::fabs(r.min - d->min) > 1e-9 || std::fabs(r.max - d->max) > 1e-9 ||
      std::fabs(r.step - d->step) > 1e-9 || (act == 3) != (d->action == 3)) {
    const bool state_changed = st != d->state;
    fw::refill(root);
    fill(root, p, false);
    if (state_changed) fw::st::fade_in_children(root);
    return true;
  }
  if (!d->arc) return true;
  if (act != d->action) fw::st::restyle(root, [&] { d->action = act; span(root); });  // the span fades (like a tap)
  d->action = act;
  const double cur = LV_CLAMP(d->min, (double) (p["current"] | 19.4), d->max);
  if (std::fabs(cur - d->current) > 1e-9) {
    d->current = cur;
    fw::animate(root, fw::st::SWEEP, {show_cur, (int32_t) std::lround(d->shown_cur * 100), (int32_t) std::lround(cur * 100), fw::mo::VALUE});
  }
  span(root);
  if (d->active) return true;  // the user holds the ring: the set point waits
  const int idx = idx_of(d, p);
  d->idx = idx;
  if (idx * FP != d->shown || lv_anim_get(root, (lv_anim_exec_xcb_t) show)) sweep_quiet(root, d->shown, idx * FP);
  return true;
}
