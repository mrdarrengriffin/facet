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
//   arc     lay::dial_arc (432 / 288) centred, stroke atoms (fw::stroke_arc, lv_arc geometry): track stroke::edge (2)
//           line, square ends; indicator stroke::indicator (6 / 4) accent with round caps; an lv_arc on top draws only the
//           knob lay::dial_knob (28 / 20) accent with a stroke::knob ring in the canvas colour
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
// Opening = the docs "opening the screen sweeps up from zero": the arc and number sweep 0 -> value (silent).
// On / off (docs: brightness 0 means off): tap the button = off sweeps the arc to 0, on sweeps back to the value it had
// (or 100 if that was 0); emits "toggle" on / off + sound toggle-on / toggle-off, the sweep itself is silent. Dragging
// while off turns it on; releasing at 0 turns it off.
// Sound: fw::step_ticks for every step the shown value crosses on a drag or a ring tap (ends = landmarks).
// Events: "change" (detail = value) on release when it moved, "toggle" (on / off), "blocked" (disabled / unavailable).
// Disabled (USER_1, Button rule): scale numbers, majors, value, caption, unit and arc text-mute, knob text-dim, the
// button disabled; any press = "blocked" + error once, nothing moves, no opening sweep. Unavailable (Card rule, the
// entity is gone): disabled scale, no arc value or knob, the value slot says "Unavailable" (body, text-dim), no unit,
// no button.
// Redraw cost per drag frame: the indicator re-rasters and invalidates only the arc between the old and new angle
// (fw::stroke_set_arc; measured in Chromium 2026-10-22: avg 0.04 ms at 720, 0.02 ms at 480 per frame, max ~1 ms for a
// full 0 -> 100 redraw) + the knob; the value label hugs its text, so a new number invalidates just its own box; the
// tick ring is drawn in one draw event (no objects per tick: one cached stroke-atom mask per tick, ~0.5 KB each) and
// only repaints where those areas cross it.
// Contrast pairs: tokens/colour.json section "dial".
#include "fw.h"

namespace {
constexpr lv_state_t ST_OFF = fw::st::OFF;
constexpr int32_t FP = 256;
constexpr float START = 135.0f, RANGE = 270.0f;  // degrees, LVGL angles (0 = 3 o'clock, clockwise)

// ---- tick ring: a ring of ticks + optional numbers, drawn in one draw event. Shaped to move to the core as the
// shared step-mark helper (facet-129, parked) without changes: everything it draws comes in through this struct.
struct TickRing {
  double lo = 0, hi = 100;  // the value range the ring spans (lo at start, hi at start + range)
  double minor = 2, major = 10;  // tick intervals in value units: ticks on multiples of minor, majors on multiples of major
  float start = START, range = RANGE;
  int minor_len = 0, major_len = 0, width = 0;
  lv_color_t minor_c, major_c;
  bool numbers = true;      // a number inside every major tick
  int num_gap = 0, num_dec = 0;  // decimals of the numbers
  fw::ty::Id num_style = fw::ty::c_meta;
  lv_color_t num_c;
};
struct Tick { lv_draw_buf_t *mask; int x, y; bool major; };  // one tick's stroke-atom mask at (x, y) of the ring
struct Ring { TickRing spec; std::vector<Tick> ticks; };

// "nice" intervals for a range (Darren 2026-10-21, round 2): the label interval is the smallest 1 / 2 / 5 x 10^k that
// keeps the numbers to at most MAX_LABELS (the docs dial: 0-100 every 10 = 11) so they never crowd; minor ticks split
// it in 5 (a 2-interval in 4, so minors stay round: 2 -> 0.5). 0-100 gives the docs ring exactly (every 10, minor 2).
constexpr int MAX_LABELS = 11;
void nice_intervals(double span, double *major, double *minor) {
  const double raw = span / (MAX_LABELS - 1), e = std::pow(10.0, std::floor(std::log10(raw)));
  double m = 10 * e;
  for (double f : {1.0, 2.0, 5.0}) if (f * e >= raw - 1e-9) { m = f * e; break; }
  const double lead = m / std::pow(10.0, std::floor(std::log10(m) + 1e-9));
  *major = m;
  *minor = m / (std::lround(lead) == 2 ? 4 : 5);
}
int decimals_of(double v) {  // 0, 1 or 2 decimals to show v exactly (steps / intervals like 0.5, 0.25)
  for (int d = 0; d < 2; d++) if (std::fabs(v * std::pow(10.0, d) - std::lround(v * std::pow(10.0, d))) < 1e-6) return d;
  return 2;
}

// the value of tick k and whether it is a major one; its angle in radians
bool tick_at(const TickRing *r, long k, double *v, float *rad) {
  const int sub = (int) std::lround(r->major / r->minor);
  *v = k * r->minor;
  *rad = (r->start + (float) ((*v - r->lo) / (r->hi - r->lo)) * r->range) * 3.14159265f / 180.0f;
  return sub > 0 && ((k % sub) + sub) % sub == 0;
}
long tick_first(const TickRing *r) { return (long) std::ceil(r->lo / r->minor - 1e-6); }
long tick_last(const TickRing *r) { return (long) std::floor(r->hi / r->minor + 1e-6); }

void ring_draw(lv_event_t *e) {
  auto *o = (lv_obj_t *) lv_event_get_current_target(e);
  auto *rg = (const Ring *) lv_event_get_user_data(e);
  const TickRing *r = &rg->spec;
  lv_layer_t *layer = lv_event_get_layer(e);
  lv_area_t a;
  lv_obj_get_coords(o, &a);
  const float cx = (a.x1 + a.x2 + 1) / 2.0f, cy = (a.y1 + a.y2 + 1) / 2.0f, R = lv_area_get_width(&a) / 2.0f;
  for (const Tick &t : rg->ticks) fw::stroke_draw(layer, t.mask, a.x1 + t.x, a.y1 + t.y, t.major ? r->major_c : r->minor_c);
  if (!r->numbers) return;
  const lv_font_t *f = fw::font(r->num_style);
  const fw::ty::Style &st = fw::style(r->num_style);
  // visual centre of a digit below the line top (cap height centred, the text-box rule)
  lv_font_glyph_dsc_t g;
  float dig_mid = f->line_height / 2.0f;
  if (lv_font_get_glyph_dsc(f, &g, '0', 0)) dig_mid = (f->line_height - f->base_line) - (g.ofs_y + g.box_h / 2.0f);
  for (long k = tick_first(r); k <= tick_last(r); k++) {
    double v;
    float rad;
    if (!tick_at(r, k, &v, &rad)) continue;
    const float c = std::cos(rad), s = std::sin(rad);
    char b[12];
    snprintf(b, sizeof b, "%.*f", r->num_dec, std::fabs(v) < 1e-9 ? 0.0 : v);
    lv_point_t sz;
    lv_text_get_size(&sz, b, f, st.letter_space, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    const float nr = R - r->major_len - r->num_gap - st.box / 2.0f;  // the number's box sits num_gap inside the tick
    const int x = (int) std::lround(cx + c * nr - sz.x / 2.0f), y = (int) std::lround(cy + s * nr - dig_mid);
    lv_draw_label_dsc_t td;
    lv_draw_label_dsc_init(&td);
    td.font = f;
    td.color = r->num_c;
    td.letter_space = st.letter_space;
    td.text = b;
    td.text_local = 1;
    lv_area_t ta = {x, y, x + sz.x, y + f->line_height};
    lv_draw_label(layer, &td, &ta);
  }
}

lv_obj_t *tick_ring(lv_obj_t *parent, int size, const TickRing &spec) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_size(o, size, size);
  auto *rg = new Ring{spec, {}};
  // every tick = a stroke atom (radial, square ends like lv_draw_line), rasterised once into its own small mask
  const float c = size / 2.0f - 0.5f, R = size / 2.0f, m = spec.width / 2.0f + 1;  // centre in pixel-centre coords
  for (long k = tick_first(&spec); k <= tick_last(&spec); k++) {
    double v;
    float rad;
    const bool maj = tick_at(&spec, k, &v, &rad);
    const int len = maj ? spec.major_len : spec.minor_len;
    const float co = std::cos(rad), si = std::sin(rad);
    fw::StrokePt p[2] = {{c + co * R, c + si * R}, {c + co * (R - len), c + si * (R - len)}};
    const int x = (int) std::floor(std::min(p[0].x, p[1].x) - m), y = (int) std::floor(std::min(p[0].y, p[1].y) - m);
    const int w = (int) std::ceil(std::max(p[0].x, p[1].x) + m) - x + 1, h = (int) std::ceil(std::max(p[0].y, p[1].y) + m) - y + 1;
    for (auto &q : p) q.x -= x, q.y -= y;
    rg->ticks.push_back({fw::stroke_segments(w, h, p, 1, spec.width, fw::Cap::butt), x, y, maj});
  }
  lv_obj_add_event_cb(o, ring_draw, LV_EVENT_DRAW_MAIN, rg);
  lv_obj_add_event_cb(o, [](lv_event_t *e) {
    auto *rg = (Ring *) lv_event_get_user_data(e);
    for (Tick &t : rg->ticks) lv_draw_buf_destroy(t.mask);
    delete rg;
  }, LV_EVENT_DELETE, rg);
  return o;
}

// ---- the dial
struct Dial {
  double min = 0, max = 100, step = 1;
  int n = 100, dec = 0;    // value = min + idx x step (the last step = max), n steps; dec = decimals shown
  int idx = 0;             // the value (step index)
  int last_on = 100;       // value to come back to when switched on
  int press_idx = 0;
  bool on = true, quiet = false, grab = false, moved = false, active = false;
  int32_t shown = 0;       // drawn position, step index x FP
  int shown_idx = 0, shown_val = -1;  // shown_val = the step index on the label
  lv_point_t press_pt = {0, 0};
  lv_obj_t *arc = nullptr, *ind = nullptr, *val = nullptr, *btn_on = nullptr, *btn_off = nullptr;
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
  if (d->arc) lv_arc_set_value(d->arc, v);  // the knob
  if (d->ind) {  // the value arc: a stroke atom re-rasterised where it moved
    fw::stroke_set_arc(d->ind, START, START + RANGE * v / (float) (d->n * FP));
  }
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
    {"name": "state", "type": "select", "options": ["default", "disabled", "unavailable"], "default": "default"},
    {"name": "sweep", "type": "bool", "default": true}
  ],
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
  const fw::Theme &t = *fw::T;
  const char *state = p["state"] | "default";
  const bool unavail = !strcmp(state, "unavailable"), off = unavail || !strcmp(state, "disabled");
  auto *d = new Dial;
  // range (round 2): min / max / step like the Slider; a step that does not divide the range ends on max
  d->step = std::max(0.01, (double) (p["step"] | 1.0));
  d->min = p["min"] | 0.0;
  d->max = std::max(d->min + d->step, (double) (p["max"] | 100.0));
  d->n = std::max(1, (int) std::ceil((d->max - d->min) / d->step - 1e-6));
  d->dec = decimals_of(d->step);
  const double v0 = LV_CLAMP(d->min, (double) (p["value"] | 64.0), d->max);
  d->idx = (int) std::lround((v0 - d->min) / d->step);
  d->on = p["on"] | true;
  d->last_on = d->idx ? d->idx : d->n;

  const int S = fw::lay::dial_scale, A = fw::lay::dial_arc, C = S / 2;
  lv_obj_t *s = lv_obj_create(parent);
  lv_obj_remove_style_all(s);
  lv_obj_add_flag(s, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(s, (lv_obj_flag_t) (LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_SCROLL_CHAIN | LV_OBJ_FLAG_GESTURE_BUBBLE));
  lv_obj_set_size(s, S, S);
  lv_obj_set_user_data(s, d);

  TickRing tr;
  tr.lo = d->min;
  tr.hi = d->max;
  nice_intervals(d->max - d->min, &tr.major, &tr.minor);
  tr.num_dec = decimals_of(tr.major);
  tr.minor_len = fw::lay::dial_tick;
  tr.major_len = fw::lay::dial_tick_major;
  tr.width = fw::stroke::tick;
  tr.minor_c = t.line_hi;
  tr.major_c = off ? fw::st::disabled_ink(t) : t.blue;  // states.json disabled roles
  tr.num_c = off ? fw::st::disabled_ink(t) : t.text_dim;
  tr.num_gap = fw::sp::s3;
  tick_ring(s, S, tr);

  if (!unavail) {
    // track + value arc = stroke atoms (lv_arc geometry: drawn inward from the A x A box); the lv_arc on top only
    // draws the knob (its arcs transparent, widths kept so the knob sits on the indicator's middle line)
    const int ax = (S - A) / 2;
    fw::stroke_arc(s, ax, ax, A, START, START + RANGE, fw::stroke::edge, t.line, fw::Cap::butt);
    d->ind = fw::stroke_arc(s, ax, ax, A, START, START, fw::stroke::indicator,
                            off ? fw::st::disabled_ink(t) : fw::st::on_fill(t));
    lv_obj_t *a = d->arc = lv_arc_create(s);
    lv_obj_remove_flag(a, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
    lv_obj_set_size(a, A, A);
    lv_obj_center(a);
    lv_arc_set_rotation(a, 0);
    lv_arc_set_bg_angles(a, (lv_value_precise_t) START, (lv_value_precise_t) (START + RANGE - 360.0f));
    lv_arc_set_range(a, 0, d->n * FP);
    lv_arc_set_value(a, 0);
    lv_obj_set_style_arc_width(a, fw::stroke::edge, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(a, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_arc_width(a, fw::stroke::indicator, LV_PART_INDICATOR);
    lv_obj_set_style_arc_opa(a, LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(a, LV_OPA_COVER, LV_PART_KNOB);
    lv_obj_set_style_bg_color(a, off ? fw::st::disabled_knob(t) : t.accent, LV_PART_KNOB);
    lv_obj_set_style_radius(a, LV_RADIUS_CIRCLE, LV_PART_KNOB);
    lv_obj_set_style_border_width(a, fw::stroke::knob, LV_PART_KNOB);
    lv_obj_set_style_border_color(a, t.bg, LV_PART_KNOB);
    lv_obj_set_style_pad_all(a, (fw::lay::dial_knob - fw::stroke::indicator) / 2, LV_PART_KNOB);
  }

  // centre column: caption, value, unit, button; placed so the value's centre is the dial's centre
  const int cap_h = fw::style(fw::ty::label).box, val_h = fw::style(fw::ty::display).box;
  lv_obj_t *col = box(s);
  lv_obj_set_size(col, A, LV_SIZE_CONTENT);
  lv_obj_set_pos(col, (S - A) / 2, C - val_h / 2 - fw::sp::s7 - cap_h);
  lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(col, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_t *cap = fw::text(col, fw::ty::label, p["caption"] | "Brightness", off ? fw::st::disabled_ink(t) : t.text_dim);
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
    lv_obj_t *unit = fw::text(col, fw::ty::body, p["unit"] | "%", off ? fw::st::disabled_ink(t) : t.text_dim);
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

  // first draw: silent; the opening sweep rises from 0 (docs), unless disabled / sweep off / reduced motion
  const int32_t target = d->on ? d->idx * FP : 0;
  d->quiet = true;
  show(s, 0);
  if (off || !(p["sweep"] | true)) show(s, target), d->quiet = false;
  else sweep_quiet(s, 0, target);
  if (off) lv_obj_add_state(s, ST_OFF);

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
