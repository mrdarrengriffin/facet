// Slider: the Facet horizontal slider (docs: Components > Controls > Slider, @facet/ui slider(); CSS .f-slider). A
// thin track with a value fill and a round knob centred on the value; drag anywhere on it.
//   720: 48 tall (touch-min = docs slider-h), track 4 (round ends), knob 24 (= docs slider-knob)
//   480: 32 tall (touch-min = docs 1x slider-h), track 4 (docs CSS 4 px literal; x 2/3 would be 2.67), knob 16 (= docs
//        1x slider-knob). Knob = height / 2 at both densities. Width = prop in dp (snapped to the 4 px grid by px()).
//   track line-hi, fill st::on_fill (accent, the lit-tile colour; from the track start to the value), knob accent; the knob centre sits ON the value, so at
//   min / max it overhangs the track end by half a knob (docs: margin-left -knob/2). The box is the track + half a knob
//   on each side (track inset knob / 2 = 12 / 8), so the knob never draws outside it: width prop = the track length
//   (0 = the parent's content width less one knob, on the grid).
// The docs define no pressed / dragging look (the knob follows the finger) and no value label or end icons (a labelled
// row = List row / screen part, later). Disabled is not in the docs' slider; it follows Button / Toggle:
//   disabled   track line, fill text-mute, knob text-dim; a press emits "blocked" and plays error ONCE (on press,
//              however long the drag), the value never moves.
// Input: press on the knob (within touch-min / 2 of its centre) = grab, it follows the finger; press elsewhere on the
// track = tap-to-jump: the value is set at once (docs) and the knob sweeps there with the Motion pattern "value" (400 ms
// ease-out, reduced motion = instant); moving the finger during the sweep stops it and the knob follows the finger.
// Sound (Darren 2026-10-21): the core step-tick rule (fw::step_ticks): every step the SHOWN value crosses (drag, sweep or
// jump) counts; a slider-tick plays for it unless the last tick was under Motion "tick-min" (26 ms) ago, so a slow,
// dimmer-like drag ticks on every single step and a fast one thins out (never more ticks than steps); the ends (0 and
// max) play the stronger slider-landmark and are never dropped.
// Events: "change" (detail = the new value) on release, when the value differs from the press; "blocked" when disabled.
// Live: fw::notify "input" (detail = the shown value) on every step the shown value crosses (drag or sweep), for a page
// listening with fw::listen (Icons page phase); not an event (nothing in the event log).
// Unavailable (states.json): the same box, track / fill / knob hidden, "Unavailable" (body, text-dim) where the track
// starts; finger down = blocked + error.
// Contrast pairs: tokens/colour.json section "slider".
#include "fw.h"

namespace {
constexpr lv_state_t ST_OFF = fw::st::OFF;  // disabled (LVGL sends no events to LV_STATE_DISABLED; see Button)
constexpr int32_t FP = 256;                       // shown position: step index x 256 (sweeps move between steps)

struct Slider {
  double min = 0, max = 100, step = 1;
  bool unavail = false, quiet = false;  // quiet = an update's sweep: no step ticks
  lv_obj_t *track = nullptr, *word = nullptr;
  int n = 100;          // steps over the range (max - min) / step
  int idx = 0;          // the value, as a step index
  int press_idx = 0;    // value when the press started (change event on release)
  int32_t shown = 0;    // drawn position, step index x FP
  int shown_idx = 0;    // the step the shown position rounds to (ticks = changes of this)
  int w = 0, knob = 0;
  int32_t press_x = 0;
  bool grab = false, moved = false;
  lv_obj_t *fill = nullptr, *kn = nullptr;
};

Slider *of(lv_obj_t *o) { return (Slider *) lv_obj_get_user_data(o); }

std::string fmt(const Slider *d, int i);
// draw position v (step index x FP); one tick per step the rounded shown value crosses
void show(void *o, int32_t v) {
  auto *s = (lv_obj_t *) o;
  auto *d = of(s);
  v = LV_CLAMP(0, v, d->n * FP);
  const int x = d->n ? (int) ((int64_t) d->w * v / ((int64_t) d->n * FP)) : 0;
  lv_obj_set_width(d->fill, x);
  lv_obj_set_x(d->kn, x);  // box x of the knob = track x - knob / 2 + knob / 2
  d->shown = v;
  const int i = (v + FP / 2) / FP;
  if (i != d->shown_idx && !d->quiet && !lv_obj_has_state(s, ST_OFF)) {
    fw::step_ticks(s, d->shown_idx, i, d->n);
    fw::notify(s, "input", fmt(d, i).c_str());  // live value while it moves (a page following the drag); not an event
  }
  d->shown_idx = i;
}

int idx_at(lv_obj_t *s, int32_t x) {
  auto *d = of(s);
  lv_area_t a;
  lv_obj_get_coords(s, &a);
  const int32_t r = LV_CLAMP(0, x - a.x1 - d->knob / 2, d->w);
  return d->w ? (int) std::lround((double) r * d->n / d->w) : 0;
}

void stop_sweep(lv_obj_t *s) { lv_anim_delete(s, (lv_anim_exec_xcb_t) show); }
void unquiet(lv_anim_t *a) { of((lv_obj_t *) lv_anim_get_user_data(a))->quiet = false; }

std::string fmt(const Slider *d, int i) {
  char b[24];
  snprintf(b, sizeof b, "%g", d->min + i * d->step);
  return b;
}
}  // namespace

namespace {
// structural props (a change = the contents again, fw::refill, snap); value + state move in place
std::string sig(fw::Props p) {
  char b[96];
  snprintf(b, sizeof b, "%g|%g|%g|%g", (double) (p["min"] | 0.0), (double) (p["max"] | 100.0), (double) (p["step"] | 1.0),
           (double) (p["width"] | 256));
  return b;
}
int idx_of(const Slider *d, fw::Props p) {
  const double v = LV_CLAMP(d->min, (double) (p["value"] | 40.0), d->max);
  return (int) std::lround((v - d->min) / d->step);
}
// the slider in `s` (user data d, made once): track + fill + knob, and the unavailable word in the same box (states.json
// unavailable: the word in place of the track, rows do not jump), one of the two hidden. Disabled = st::OFF state
// styles (fw::st::set_off fades between them), unavailable = OFF + the word.
void fill(lv_obj_t *s, fw::Props p) {
  auto *d = of(s);
  const char *state = p["state"] | "default";
  d->unavail = !strcmp(state, "unavailable");
  const bool off = d->unavail || !strcmp(state, "disabled");
  d->min = p["min"] | 0.0;
  d->max = std::max(d->min, (double) (p["max"] | 100.0));
  d->step = std::max(1e-6, (double) (p["step"] | 1.0));
  d->n = std::max(1, (int) std::lround((d->max - d->min) / d->step));
  d->idx = idx_of(d, p);

  const int h = fw::touch::min;   // 48 / 32 = docs slider-h
  const int tr = fw::sp::s1;      // 4 / 4: the docs track (4 px at both)
  d->knob = h / 2;                // 24 / 16 = docs slider-knob
  const int wdp = p["width"] | 256;  // 0 = its parent's content width (the track = that less one knob)
  if (wdp > 0) d->w = fw::px((float) wdp);
  else { lv_obj_update_layout(lv_obj_get_parent(s)); d->w = std::max(0, (int) lv_obj_get_content_width(lv_obj_get_parent(s)) - d->knob) / fw::sp::GRID * fw::sp::GRID; }
  fw::shape(s, sig(p));

  lv_obj_remove_style_all(s);
  lv_obj_add_flag(s, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(s, (lv_obj_flag_t) (LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_SCROLL_CHAIN | LV_OBJ_FLAG_GESTURE_BUBBLE));
  lv_obj_set_size(s, d->w + d->knob, h);  // track + half a knob each side: the knob stays inside at min / max

  lv_obj_t *t = d->track = lv_obj_create(s);  // track
  lv_obj_remove_style_all(t);
  lv_obj_remove_flag(t, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_size(t, d->w, tr);
  lv_obj_set_pos(t, d->knob / 2, (h - tr) / 2);
  lv_obj_set_style_radius(t, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(t, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(t, fw::T->line_hi, 0);
  lv_obj_set_style_bg_color(t, fw::st::disabled_line(*fw::T), ST_OFF);
  lv_obj_set_style_clip_corner(t, true, 0);  // the fill keeps the track's round start (docs: overflow hidden)

  d->fill = lv_obj_create(t);
  fw::qa::free(d->fill, "the fill ends at the value");
  lv_obj_remove_style_all(d->fill);
  lv_obj_remove_flag(d->fill, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_size(d->fill, 0, tr);
  lv_obj_set_style_bg_opa(d->fill, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(d->fill, fw::st::on_fill(*fw::T), 0);
  lv_obj_set_style_bg_color(d->fill, fw::st::disabled_ink(*fw::T), ST_OFF);

  d->kn = lv_obj_create(s);
  fw::qa::free(d->kn, "the knob sits on the value");
  lv_obj_remove_style_all(d->kn);
  lv_obj_remove_flag(d->kn, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_size(d->kn, d->knob, d->knob);
  lv_obj_set_y(d->kn, (h - d->knob) / 2);
  lv_obj_set_style_radius(d->kn, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(d->kn, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(d->kn, fw::T->accent, 0);
  lv_obj_set_style_bg_color(d->kn, fw::st::disabled_knob(*fw::T), ST_OFF);

  // the word where the track starts (half a knob in)
  lv_obj_t *u = d->word = fw::text(s, fw::ty::body, fw::st::UNAVAILABLE_TEXT, fw::st::unavailable_note(*fw::T));
  lv_obj_remove_flag(u, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_align(u, LV_ALIGN_LEFT_MID, h / 4, 0);
  lv_obj_set_flag(u, LV_OBJ_FLAG_HIDDEN, !d->unavail);
  lv_obj_set_flag(t, LV_OBJ_FLAG_HIDDEN, d->unavail);
  lv_obj_set_flag(d->kn, LV_OBJ_FLAG_HIDDEN, d->unavail);

  d->quiet = true;  // the first draw makes no sound
  d->shown_idx = d->idx;
  show(s, d->idx * FP);
  d->quiet = false;
  fw::st::set_off(s, off, false);
}
}  // namespace

FW_COMPONENT(slider, "Slider", "components", R"({
  "about": "A horizontal control for picking a value in a range by dragging or tapping.",
  "frame": [320, 96],
  "props": [
    {"name": "value", "about": "The value the slider is set to.", "type": "range", "min": 0, "max": 100, "step": 1, "default": 40},
    {"name": "min", "about": "The lowest value, at the left end.", "type": "range", "min": 0, "max": 50, "step": 1, "default": 0},
    {"name": "max", "about": "The highest value, at the right end.", "type": "range", "min": 10, "max": 100, "step": 1, "default": 100},
    {"name": "step", "about": "How much the value changes from one position to the next.", "type": "range", "min": 1, "max": 25, "step": 1, "default": 1},
    {"name": "width", "about": "How long the slider is, in pixels; 0 fills the space it is in.", "type": "range", "min": 0, "max": 288, "step": 8, "default": 256},
    {"name": "state", "about": "default can be dragged; disabled cannot be changed; unavailable shows the device cannot be reached.", "type": "select", "options": ["default", "disabled", "unavailable"], "default": "default"}
  ],
  "variants": [
    {"label": "min · 0", "props": {"value": 0}},
    {"label": "value · 40", "props": {}},
    {"label": "max · 100", "props": {"value": 100}},
    {"label": "step 10 · 40", "props": {"step": 10}},
    {"label": "disabled · 40", "props": {"state": "disabled"}},
    {"label": "disabled · 0", "props": {"value": 0, "state": "disabled"}},
    {"label": "unavailable", "props": {"state": "unavailable"}}
  ]
})") {
  auto *d = new Slider;
  lv_obj_t *s = lv_obj_create(parent);
  lv_obj_set_user_data(s, d);
  fill(s, p);

  lv_obj_add_event_cb(s, [](lv_event_t *e) { delete (Slider *) lv_event_get_user_data(e); }, LV_EVENT_DELETE, d);
  lv_obj_add_event_cb(s, [](lv_event_t *e) {
    auto *s = (lv_obj_t *) lv_event_get_current_target(e);
    auto *d = of(s);
    const lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_PRESSED && code != LV_EVENT_PRESSING && code != LV_EVENT_RELEASED && code != LV_EVENT_PRESS_LOST) return;
    lv_point_t pt = {0, 0};
    if (lv_indev_t *in = lv_indev_active()) lv_indev_get_point(in, &pt);
    if (lv_obj_has_state(s, ST_OFF)) {
      if (code == LV_EVENT_PRESSED) fw::st::blocked(s, d->unavail ? "unavailable" : fmt(d, d->idx).c_str());
      return;
    }
    if (code == LV_EVENT_PRESSED) {
      d->quiet = false;
      lv_area_t a;
      lv_obj_get_coords(s, &a);
      const int32_t kx = a.x1 + d->knob / 2 + (int32_t) ((int64_t) d->w * d->shown / ((int64_t) d->n * FP));
      d->press_idx = d->idx;
      d->press_x = pt.x;
      d->moved = false;
      d->grab = std::abs(pt.x - kx) <= fw::touch::min / 2;
      const int i = idx_at(s, pt.x);
      if (d->grab) { stop_sweep(s); d->idx = i; show(s, i * FP); return; }
      d->idx = i;  // tap-to-jump: the value is set now, the knob sweeps there
      fw::animate(s, fw::st::SWEEP, {show, d->shown, i * FP, fw::mo::VALUE});
    } else if (code == LV_EVENT_PRESSING) {
      if (!d->grab && !d->moved && std::abs(pt.x - d->press_x) < fw::sp::s2) return;  // still a tap: let the sweep run
      d->moved = true;
      const int i = idx_at(s, pt.x);
      if (!d->grab) { stop_sweep(s); d->grab = true; }
      d->idx = i;
      show(s, i * FP);
    } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
      if (d->idx != d->press_idx) fw::emit(s, "change", fmt(d, d->idx).c_str());
    }
  }, LV_EVENT_ALL, nullptr);
}

// Update path: value -> the knob and fill sweep there (Motion value, silent: no step ticks), default <-> disabled <->
// unavailable -> fw::st::set_off fades the colours and fw::st::swap trades the track for the word (Motion fade); a new
// range (min / max / step) or width = the contents again (fw::refill, snap). While a finger holds it the user wins.
FW_UPDATE(slider) {
  auto *d = of(root);
  if (!d) return false;
  if (!fw::shape(root, sig(p))) {
    stop_sweep(root);
    fw::refill(root);
    fill(root, p);
    return true;
  }
  const char *state = p["state"] | "default";
  const bool unavail = !strcmp(state, "unavailable"), off = unavail || !strcmp(state, "disabled");
  if (unavail != d->unavail) {
    d->unavail = unavail;
    if (unavail) { fw::st::swap(d->track, d->word); lv_obj_add_flag(d->kn, LV_OBJ_FLAG_HIDDEN); }
    else { fw::st::swap(d->word, d->track); fw::st::swap(nullptr, d->kn); }
  }
  if (off != lv_obj_has_state(root, ST_OFF)) fw::st::set_off(root, off);
  if (lv_obj_has_state(root, LV_STATE_PRESSED)) return true;  // the finger wins
  d->idx = idx_of(d, p);
  if (d->idx * FP != d->shown || lv_anim_get(root, (lv_anim_exec_xcb_t) show)) {
    d->quiet = true;
    fw::animate(root, fw::st::SWEEP, {show, d->shown, d->idx * FP, fw::mo::VALUE, 0, false, unquiet, root});
  }
  return true;
}
