// Slider: the Facet horizontal slider (docs: Components > Controls > Slider, @facet/ui slider(); CSS .f-slider), on
// the contract (renderer/CONTRACT.md, "contract": 2). A thin track with a value fill and a round knob centred on the
// value; drag anywhere on it. Drawn with the Pen (C24: the value is data, C12) in a strip (fx::strip).
//   size     its parent's width x touch:min (48 / 32 = docs slider-h) ("slider", core/fx_core.cpp); the track runs
//            half a knob in from each end (so the knob never draws outside the box): track = width - one knob
//   minimum  layout:card-min-w x touch:min. Steps: one
//   track    stroke space:1 (4 at both: the docs 4 px), round ends, ink line-hi; fill = on-fill from the start to
//            the value; knob layout:slider-knob (24 / 16 = touch min / 2), accent, its centre ON the value
// The docs define no pressed / dragging look (the knob follows the finger) and no value label or end icons.
//   disabled   (fw::st::OFF) track disabled-line, fill disabled-ink, knob disabled-knob; a press emits "blocked" and
//              plays error ONCE (on press, however long the drag), the value never moves
//   unavailable  the box, no track / knob, the word Unavailable (body, unavailable-note) where the track starts
// Input: press on the knob (within touch:min / 2 of its centre) = grab, it follows the finger; press elsewhere on the
// track = tap-to-jump: the value is set at once and the knob sweeps there with Motion "value" (reduced = instant);
// moving the finger during the sweep stops it and the knob follows the finger. Hit maths = the drawn geometry (C27).
// Sound: the core step-tick rule (fw::step_ticks) on every step the SHOWN value crosses; ends = slider-landmark.
// Events: "change" (detail = the new value) on release when it differs from the press; "blocked" when disabled.
// Live: fw::notify "input" (the shown value) on every step it crosses. Partial redraw (C26): a move repaints only the
// span between the old and new knob, a knob tall. Bleed: none. Contrast pairs: tokens/colour.json section "slider".
// A legacy caller that wants a set track length sizes the root itself (width = track + one knob).
#include "fw.h"
#include "fx.h"

namespace {
constexpr lv_state_t ST_OFF = fw::st::OFF;  // disabled (LVGL sends no events to LV_STATE_DISABLED; see Button)
constexpr int32_t FP = 256;                  // shown position: step index x 256 (sweeps move between steps)

struct Slider {
  double min = 0, max = 100, step = 1;
  bool unavail = false, quiet = false;  // quiet = an update's sweep: no step ticks
  lv_obj_t *word = nullptr;
  int n = 100;          // steps over the range (max - min) / step
  int idx = 0;          // the value, as a step index
  int press_idx = 0;    // value when the press started (change event on release)
  int32_t shown = 0;    // drawn position, step index x FP
  int shown_idx = 0;    // the step the shown position rounds to (ticks = changes of this)
  int32_t press_x = 0;
  bool grab = false, moved = false;
};
Slider *of(lv_obj_t *o) { return (Slider *) lv_obj_get_user_data(o); }

// the geometry, from the box's own content width and the tokens (the draw and the hit maths share it)
int knob() { return fx::size("layout:slider-knob"); }
int track(lv_obj_t *s) { return std::max(0, (int) lv_obj_get_content_width(s) - knob()); }
int knob_x(lv_obj_t *s, int32_t v) {  // centre-relative x of the knob at shown position v
  const Slider *d = of(s);
  const int w = track(s);
  return -w / 2 + (d->n ? (int) ((int64_t) w * v / ((int64_t) d->n * FP)) : 0);
}
void draw(const fx::Pen &p) {
  const Slider *d = of(p.obj);
  if (!d || d->unavail) return;
  const bool off = lv_obj_has_state(p.obj, ST_OFF);
  const int w = std::max(0, p.w - knob()), x0 = -w / 2, x = knob_x(p.obj, d->shown);
  p.line(x0, 0, x0 + w, 0, "space:1", off ? "fg:disabled-line" : "fg:line-hi", true, "track");
  if (x > x0) p.line(x0, 0, x, 0, "space:1", off ? "fg:disabled-ink" : "fg:on-fill", true, "track-fill");
  p.dot(x, 0, "layout:slider-knob", off ? "fg:disabled-knob" : "fg:accent", "knob");  // the theme parts (C46)
}

std::string fmt(const Slider *d, int i) {
  char b[24];
  snprintf(b, sizeof b, "%g", d->min + i * d->step);
  return b;
}
// draw position v (step index x FP): repaint the span the knob moved over; one tick per step the shown value crosses
void show(void *o, int32_t v) {
  auto *s = (lv_obj_t *) o;
  auto *d = of(s);
  v = LV_CLAMP(0, v, d->n * FP);
  const int a = knob_x(s, d->shown), b = knob_x(s, v), k = knob();
  d->shown = v;
  fx::redraw(s, std::min(a, b) - k, -k, std::abs(b - a) + 2 * k + 1, 2 * k + 1);
  const int i = (v + FP / 2) / FP;
  if (i != d->shown_idx && !lv_obj_has_state(s, ST_OFF)) {
    if (!d->quiet) fw::step_ticks(s, d->shown_idx, i, d->n);  // an update's sweep is silent
    fw::notify(s, "input", fmt(d, i).c_str());  // live value while it moves (a page following it); not an event
  }
  d->shown_idx = i;
}
int idx_at(lv_obj_t *s, int32_t x) {
  auto *d = of(s);
  lv_area_t a;
  lv_obj_get_content_coords(s, &a);
  const int w = track(s);
  const int32_t r = LV_CLAMP(0, x - a.x1 - knob() / 2, w);
  return w ? (int) std::lround((double) r * d->n / w) : 0;
}
void stop_sweep(lv_obj_t *s) { lv_anim_delete(s, (lv_anim_exec_xcb_t) show); }
void unquiet(lv_anim_t *a) { of((lv_obj_t *) lv_anim_get_user_data(a))->quiet = false; }

// structural props (a change = the contents again, fw::refill, snap); value + state move in place
std::string sig(fw::Props p) {
  char b[96];
  snprintf(b, sizeof b, "%g|%g|%g", (double) (p["min"] | 0.0), (double) (p["max"] | 100.0), (double) (p["step"] | 1.0));
  return b;
}
int idx_of(const Slider *d, fw::Props p) {
  const double v = LV_CLAMP(d->min, (double) (p["value"] | 40.0), d->max);
  return (int) std::lround((v - d->min) / d->step);
}
// the strip's contents: the unavailable word (hidden unless unavailable); the track, fill and knob are drawn
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
  fw::shape(s, sig(p));
  fx::use(s, "box");
  fx::use(s, "slider");
  lv_obj_add_flag(s, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(s, (lv_obj_flag_t) (LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_SCROLL_CHAIN | LV_OBJ_FLAG_GESTURE_BUBBLE | LV_OBJ_FLAG_EVENT_BUBBLE));
  lv_obj_t *u = d->word = fx::text(s, fw::st::UNAVAILABLE_TEXT, "type:body", "fg:unavailable-note");
  fx::use(u, "slider-word");
  lv_obj_set_flag(u, LV_OBJ_FLAG_HIDDEN, !d->unavail);
  d->shown_idx = d->idx;
  d->shown = d->idx * FP;
  if (off) lv_obj_add_state(s, ST_OFF);
  lv_obj_invalidate(s);  // the contents again (a new range): the whole strip once
}
}  // namespace

FW_COMPONENT(slider, "Slider", "components", R"({
  "about": "A horizontal control for picking a value in a range by dragging or tapping. It fills the width it is given.",
  "contract": 2,
  "parts": ["track", "track-fill", "knob"],
  "min": ["layout:card-min-w", "touch:min"],
  "frame": [280, 96],
  "props": [
    {"name": "value", "about": "The value the slider is set to.", "type": "range", "min": 0, "max": 100, "step": 1, "default": 40},
    {"name": "min", "about": "The lowest value, at the left end.", "type": "range", "min": 0, "max": 50, "step": 1, "default": 0},
    {"name": "max", "about": "The highest value, at the right end.", "type": "range", "min": 10, "max": 100, "step": 1, "default": 100},
    {"name": "step", "about": "How much the value changes from one position to the next.", "type": "range", "min": 1, "max": 25, "step": 1, "default": 1},
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
  lv_obj_t *s = fx::strip(parent, draw, "slider");
  fx::name(s, "slider");
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
      lv_obj_get_content_coords(s, &a);
      const int32_t kx = a.x1 + lv_area_get_width(&a) / 2 + knob_x(s, d->shown);  // the drawn knob's centre
      d->press_idx = d->idx;
      d->press_x = pt.x;
      d->moved = false;
      d->grab = std::abs(pt.x - kx) <= fx::size("touch:min") / 2;
      const int i = idx_at(s, pt.x);
      if (d->grab) { stop_sweep(s); d->idx = i; show(s, i * FP); return; }
      d->idx = i;  // tap-to-jump: the value is set now, the knob sweeps there
      fw::animate(s, fw::st::SWEEP, {show, d->shown, i * FP, fw::mo::VALUE});
    } else if (code == LV_EVENT_PRESSING) {
      if (!d->grab && !d->moved && std::abs(pt.x - d->press_x) < fx::size("space:2")) return;  // still a tap: let the sweep run
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
    if (unavail) fw::st::swap(nullptr, d->word); else lv_obj_add_flag(d->word, LV_OBJ_FLAG_HIDDEN);
    lv_obj_invalidate(root);  // the track and knob drawn or not: the whole strip once
  }
  if (off != lv_obj_has_state(root, ST_OFF)) { fw::st::set_off(root, off); lv_obj_invalidate(root); }  // the inks are read per state
  if (lv_obj_has_state(root, LV_STATE_PRESSED)) return true;  // the finger wins
  d->idx = idx_of(d, p);
  if (d->idx * FP != d->shown || lv_anim_get(root, (lv_anim_exec_xcb_t) show)) {
    d->quiet = true;
    fw::animate(root, fw::st::SWEEP, {show, d->shown, d->idx * FP, fw::mo::VALUE, 0, false, unquiet, root});
  }
  return true;
}
