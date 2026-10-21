// Slider: the Facet horizontal slider (docs: Components > Controls > Slider, @facet/ui slider(); CSS .f-slider). A
// thin track with a value fill and a round knob centred on the value; drag anywhere on it.
//   720: 48 tall (touch-min = docs slider-h), track 4 (round ends), knob 24 (= docs slider-knob)
//   480: 32 tall (touch-min = docs 1x slider-h), track 4 (docs CSS 4 px literal; x 2/3 would be 2.67), knob 16 (= docs
//        1x slider-knob). Knob = height / 2 at both densities. Width = prop in dp (snapped to the 4 px grid by px()).
//   track line-hi, fill blue (from the track start to the value), knob accent; the knob centre sits ON the value, so at
//   min / max it overhangs the track end by half a knob (docs: margin-left -knob/2). The box is the track + half a knob
//   on each side (track inset knob / 2 = 12 / 8), so the knob never draws outside it: width prop = the track length.
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
// Unavailable (states.json): the same box, track / fill / knob hidden, "Unavailable" (body, text-dim) where the track
// starts; finger down = blocked + error.
// Contrast pairs: tokens/colour.json section "slider".
#include "fw.h"

namespace {
constexpr lv_state_t ST_OFF = fw::st::OFF;  // disabled (LVGL sends no events to LV_STATE_DISABLED; see Button)
constexpr int32_t FP = 256;                       // shown position: step index x 256 (sweeps move between steps)

struct Slider {
  double min = 0, step = 1;
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
  if (i != d->shown_idx && !lv_obj_has_state(s, ST_OFF)) fw::step_ticks(s, d->shown_idx, i, d->n);
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

std::string fmt(const Slider *d, int i) {
  char b[24];
  snprintf(b, sizeof b, "%g", d->min + i * d->step);
  return b;
}
}  // namespace

FW_COMPONENT(slider, "Slider", "components", R"({
  "about": "A horizontal control for picking a value in a range by dragging or tapping.",
  "frame": [320, 96],
  "props": [
    {"name": "value", "type": "range", "min": 0, "max": 100, "step": 1, "default": 40},
    {"name": "min", "type": "range", "min": 0, "max": 50, "step": 1, "default": 0},
    {"name": "max", "type": "range", "min": 10, "max": 100, "step": 1, "default": 100},
    {"name": "step", "type": "range", "min": 1, "max": 25, "step": 1, "default": 1},
    {"name": "width", "type": "range", "min": 96, "max": 288, "step": 8, "default": 256},
    {"name": "state", "type": "select", "options": ["default", "disabled", "unavailable"], "default": "default"}
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
  const bool disabled = !strcmp(p["state"] | "default", "disabled");
  auto *d = new Slider;
  d->min = p["min"] | 0.0;
  const double max = std::max(d->min, (double) (p["max"] | 100.0));
  d->step = std::max(1e-6, (double) (p["step"] | 1.0));
  d->n = std::max(1, (int) std::lround((max - d->min) / d->step));
  const double v = LV_CLAMP(d->min, (double) (p["value"] | 40.0), max);
  d->idx = (int) std::lround((v - d->min) / d->step);

  const int h = fw::touch::min;   // 48 / 32 = docs slider-h
  const int tr = fw::sp::s1;      // 4 / 4: the docs track (4 px at both)
  d->knob = h / 2;                // 24 / 16 = docs slider-knob
  d->w = fw::px((float) (p["width"] | 256));
  if (!strcmp(p["state"] | "default", "unavailable")) {  // states.json unavailable: the word in place of the track
    delete d;  // same box (rows do not jump), track / fill / knob hidden, the word where the track starts
    lv_obj_t *u = lv_obj_create(parent);
    lv_obj_remove_style_all(u);
    lv_obj_add_flag(u, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(u, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(u, fw::px((float) (p["width"] | 256)) + h / 2, h);
    lv_obj_set_style_pad_left(u, h / 4, 0);  // = the track start (half a knob)
    lv_obj_t *l = fw::text(u, fw::ty::body, fw::st::UNAVAILABLE_TEXT, fw::st::unavailable_note(*fw::T));
    lv_obj_remove_flag(l, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(l, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_add_state(u, ST_OFF);
    fw::st::on_tap(u, [](lv_event_t *e) {
      if (fw::st::tap_now(e)) fw::st::blocked((lv_obj_t *) lv_event_get_current_target(e), "unavailable");
    }, nullptr);
    return;
  }

  lv_obj_t *s = lv_obj_create(parent);
  lv_obj_remove_style_all(s);
  lv_obj_add_flag(s, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(s, (lv_obj_flag_t) (LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_SCROLL_CHAIN | LV_OBJ_FLAG_GESTURE_BUBBLE));
  lv_obj_set_size(s, d->w + d->knob, h);  // track + half a knob each side: the knob stays inside at min / max
  lv_obj_set_user_data(s, d);

  lv_obj_t *t = lv_obj_create(s);  // track
  lv_obj_remove_style_all(t);
  lv_obj_remove_flag(t, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_size(t, d->w, tr);
  lv_obj_set_pos(t, d->knob / 2, (h - tr) / 2);
  lv_obj_set_style_radius(t, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(t, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(t, disabled ? fw::st::disabled_line(*fw::T) : fw::T->line_hi, 0);
  lv_obj_set_style_clip_corner(t, true, 0);  // the fill keeps the track's round start (docs: overflow hidden)

  d->fill = lv_obj_create(t);
  lv_obj_remove_style_all(d->fill);
  lv_obj_remove_flag(d->fill, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_size(d->fill, 0, tr);
  lv_obj_set_style_bg_opa(d->fill, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(d->fill, disabled ? fw::st::disabled_ink(*fw::T) : fw::T->blue, 0);

  d->kn = lv_obj_create(s);
  lv_obj_remove_style_all(d->kn);
  lv_obj_remove_flag(d->kn, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_size(d->kn, d->knob, d->knob);
  lv_obj_set_y(d->kn, (h - d->knob) / 2);
  lv_obj_set_style_radius(d->kn, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(d->kn, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(d->kn, disabled ? fw::st::disabled_knob(*fw::T) : fw::T->accent, 0);

  d->shown_idx = d->idx;  // the first draw makes no sound
  show(s, d->idx * FP);
  if (disabled) lv_obj_add_state(s, ST_OFF);

  lv_obj_add_event_cb(s, [](lv_event_t *e) { delete (Slider *) lv_event_get_user_data(e); }, LV_EVENT_DELETE, d);
  lv_obj_add_event_cb(s, [](lv_event_t *e) {
    auto *s = (lv_obj_t *) lv_event_get_current_target(e);
    auto *d = of(s);
    const lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_PRESSED && code != LV_EVENT_PRESSING && code != LV_EVENT_RELEASED && code != LV_EVENT_PRESS_LOST) return;
    lv_point_t pt = {0, 0};
    if (lv_indev_t *in = lv_indev_active()) lv_indev_get_point(in, &pt);
    if (lv_obj_has_state(s, ST_OFF)) {
      if (code == LV_EVENT_PRESSED) fw::st::blocked(s, fmt(d, d->idx).c_str());
      return;
    }
    if (code == LV_EVENT_PRESSED) {
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
