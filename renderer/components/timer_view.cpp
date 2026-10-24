// Timer view: the Timer screen's body (docs screens/timer.ts timerScreen; the factory spec renderer/specs/timer.json
// adds the header): the time left inside a ring that empties as it runs, the timer's name above it, and under the ring
// two buttons (+1 min, Cancel); with no timer running, one-tap presets (3 / 5 / 10 / 20 min, 5 the primary).
//   box      the content box (lay::content_w x content_h); a column centred in it: the ring, sp::s9, the button row
//   ring     a full-turn value arc (fw::value_arc) lay::timer_ring, track stroke::arc line, indicator stroke::indicator
//            accent, 1000 steps; inside it, centred, sp::s3 apart: the label (type label, text-mute; "done" in
//            accent-text when it ran out), the time left (display, text: "4:32"; an hour or more in hero, so "1:02:05" fits inside the ring), the sub (body, text-dim:
//            "of 10 min · ends 14:52", "pick a length")
//   buttons  real Buttons (fw::find): presets secondary + the 5 min primary; running: "+1 min" secondary, "Cancel"
//            outline; lay::gutter apart
// Feed (docs Timer {label, endsAt, total}): props state idle | running | done, label, total (s), left (s) = a timer
// the host reports (HA: a timer.* entity's remaining / duration, or the panel's own). The view then runs it down
// itself, once a second (the ring steps with it). Taps work on their own too (the panel's local timer, like the docs
// actions): a preset starts one, "+1 min" adds a minute, Cancel stops it; each emits "timer" (start <s> | extend 60 |
// cancel) for a host to mirror. Running out plays the timer-done sound and shows "done" until the next one.
// Update path: state / left / total / label move in place (the ring steps to the new share, silently); the button
// rows swap with Motion fade (both always built). Contrast: tokens/colour.json section "timer" (+ button).
#include <ctime>
// audit deps: button
#include "fw.h"
#include "device.h"

namespace {
constexpr int PRESETS[4] = {3, 5, 10, 20};
struct TV {
  lv_obj_t *ring = nullptr, *lbl = nullptr, *left = nullptr, *sub = nullptr, *run_row = nullptr, *pre_row = nullptr;
  lv_timer_t *tick = nullptr;
  std::string label;
  int total = 0;          // s
  uint32_t ends = 0;      // lv_tick of the end (running)
  int state = 0;          // 0 idle, 1 running, 2 done
  int shown = -1;         // seconds left shown
  fw::ty::Id left_style = fw::ty::display;
};
TV *of(lv_obj_t *o) { return (TV *) lv_obj_get_user_data(o); }
lv_obj_t *box(lv_obj_t *parent) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  return o;
}
lv_obj_t *button(lv_obj_t *parent, const char *kind, const char *label) {
  ArduinoJson::JsonDocument d;
  d["kind"] = kind;
  d["label"] = label;
  const uint32_t n = lv_obj_get_child_count(parent);
  if (const fw::Component *c = fw::find("button")) c->build(parent, d.as<fw::Props>());
  return lv_obj_get_child_count(parent) > n ? lv_obj_get_child(parent, -1) : nullptr;
}
// "4:32", "1:02:05" (docs countdown: rounded up, so it shows 0:00 only when it has run out)
std::string countdown(int s) {
  s = std::max(0, s);
  char b[16];
  if (s >= 3600) snprintf(b, sizeof b, "%d:%02d:%02d", s / 3600, s / 60 % 60, s % 60);
  else snprintf(b, sizeof b, "%d:%02d", s / 60, s % 60);
  return b;
}
int left_s(const TV *t) {
  if (t->state != 1) return 0;
  const int32_t ms = (int32_t) (t->ends - lv_tick_get());
  return ms <= 0 ? 0 : (ms + 999) / 1000;
}
void opa_exec(void *o, int32_t v) { lv_obj_set_style_opa((lv_obj_t *) o, (lv_opa_t) v, 0); }
// the running buttons or the presets: both built in one place, one shown (and touchable); a change fades them like a
// tap would (Motion fade), the first build is at rest
void rows(TV *t, bool animate = true) {
  const bool on = t->state == 1;
  lv_obj_t *in = on ? t->run_row : t->pre_row, *out = on ? t->pre_row : t->run_row;
  lv_obj_add_flag(out, LV_OBJ_FLAG_HIDDEN);  // out of the way of the finger at once
  const bool was_hidden = lv_obj_has_flag(in, LV_OBJ_FLAG_HIDDEN);
  lv_obj_remove_flag(in, LV_OBJ_FLAG_HIDDEN);
  if (animate && was_hidden) fw::animate(in, fw::mo::fade, {opa_exec, 0, 255, fw::mo::OPA});
  else lv_obj_set_style_opa(in, LV_OPA_COVER, 0);
}
void render(lv_obj_t *root, bool force = false) {
  TV *t = of(root);
  const fw::Theme &th = *fw::T;
  int s = left_s(t);
  if (t->state == 1 && s == 0) {  // ran out
    t->state = 2;
    fw::sound(root, "timer-done");
    fw::emit(root, "timer", "done");
    rows(t);
    force = true;
  }
  if (!force && s == t->shown) return;
  t->shown = s;
  if (t->state == 1) {
    fw::set_text(t->lbl, fw::ty::label, t->label.c_str());
    lv_obj_set_style_text_color(t->lbl, th.text_mute, 0);
    const fw::ty::Id ls = s >= 3600 ? fw::ty::hero : fw::ty::display;  // h:mm:ss fits the ring a size down
    if (ls != t->left_style) { fw::text_style(t->left, ls); t->left_style = ls; }
    fw::set_text(t->left, ls, countdown(s).c_str());
    const time_t end = time(nullptr) + s;
    struct tm tm;
    localtime_r(&end, &tm);
    char b[64];
    snprintf(b, sizeof b, "of %d min \xC2\xB7 ends %02d:%02d", (t->total + 30) / 60, tm.tm_hour, tm.tm_min);
    fw::set_text(t->sub, fw::ty::body, b);
    lv_arc_set_value(t->ring, t->total > 0 ? (int32_t) std::lround(1000.0 * s / t->total) : 0);
  } else {
    fw::set_text(t->lbl, fw::ty::label, t->state == 2 ? "done" : "timer");
    lv_obj_set_style_text_color(t->lbl, t->state == 2 ? th.accent_text : th.text_mute, 0);
    if (t->left_style != fw::ty::display) { fw::text_style(t->left, fw::ty::display); t->left_style = fw::ty::display; }
    fw::set_text(t->left, fw::ty::display, "0:00");
    fw::set_text(t->sub, fw::ty::body, t->state == 2 ? t->label.c_str() : "pick a length");
    lv_arc_set_value(t->ring, 0);
  }
}
void start(lv_obj_t *root, int secs, const char *label) {
  TV *t = of(root);
  t->state = 1;
  t->total = secs;
  t->label = label;
  t->ends = lv_tick_get() + (uint32_t) secs * 1000;
  rows(t);
  render(root, true);
}
void fill(lv_obj_t *root, fw::Props p) {
  const fw::Theme &th = *fw::T;
  TV *t = of(root);
  lv_obj_remove_style_all(root);
  lv_obj_remove_flag(root, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_size(root, fw::lay::content_w, fw::lay::content_h);
  lv_obj_t *col = box(root);
  lv_obj_set_size(col, fw::lay::content_w, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(col, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(col, fw::sp::s9, 0);
  const int R = fw::lay::timer_ring;
  lv_obj_t *rbox = box(col);
  lv_obj_set_size(rbox, R, R);
  fw::ArcLook l;
  l.start = 270.0f;
  l.range = 360.0f;
  l.track_w = fw::stroke::arc;
  l.track_c = th.line;
  l.ind_w = fw::stroke::indicator;
  l.ind_c = th.accent;
  l.steps = 1000;
  t->ring = fw::value_arc(rbox, R, l);
  lv_obj_center(t->ring);
  lv_obj_t *mid = box(rbox);
  lv_obj_set_size(mid, R, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(mid, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(mid, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(mid, fw::sp::s3, 0);
  t->lbl = fw::text(mid, fw::ty::label, "timer", th.text_mute);
  t->left = fw::text(mid, fw::ty::display, "0:00", th.text);
  t->sub = fw::text(mid, fw::ty::body, "pick a length", th.text_dim);
  lv_obj_center(mid);
  lv_obj_t *rows_box = box(col);  // both rows in one place: one shown
  lv_obj_set_size(rows_box, fw::lay::content_w, LV_SIZE_CONTENT);
  for (int r = 0; r < 2; r++) {
    lv_obj_t *row = box(rows_box);
    lv_obj_set_size(row, fw::lay::content_w, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, fw::lay::gutter, 0);
    (r ? t->pre_row : t->run_row) = row;
  }
  if (lv_obj_t *b = button(t->run_row, "secondary", "+1 min"))
    lv_obj_add_event_cb(b, [](lv_event_t *e) {
      auto *root = (lv_obj_t *) lv_event_get_user_data(e);
      TV *t = of(root);
      if (t->state != 1) return;
      t->ends += 60000;
      t->total += 60;
      fw::emit(root, "timer", "extend 60");
      render(root, true);
    }, LV_EVENT_CLICKED, root);
  if (lv_obj_t *b = button(t->run_row, "outline", "Cancel"))
    lv_obj_add_event_cb(b, [](lv_event_t *e) {
      auto *root = (lv_obj_t *) lv_event_get_user_data(e);
      TV *t = of(root);
      t->state = 0;
      fw::emit(root, "timer", "cancel");
      rows(t);
      render(root, true);
    }, LV_EVENT_CLICKED, root);
  for (int m : PRESETS) {
    char lab[16];
    snprintf(lab, sizeof lab, "%d min", m);
    if (lv_obj_t *b = button(t->pre_row, m == PRESETS[1] ? "primary" : "secondary", lab)) {
      lv_obj_set_user_data(b, (void *) (intptr_t) m);  // a Button keeps no user data of its own
      lv_obj_add_event_cb(b, [](lv_event_t *e) {
        auto *root = (lv_obj_t *) lv_event_get_user_data(e);
        const int m = (int) (intptr_t) lv_obj_get_user_data((lv_obj_t *) lv_event_get_current_target(e));
        char lab[24], det[24];
        snprintf(lab, sizeof lab, "%d min timer", m);
        snprintf(det, sizeof det, "start %d", m * 60);
        fw::emit(root, "timer", det);
        start(root, m * 60, lab);
      }, LV_EVENT_CLICKED, root);
    }
  }
  lv_obj_update_layout(rows_box);
  lv_obj_set_height(rows_box, std::max(lv_obj_get_height(t->run_row), lv_obj_get_height(t->pre_row)));
  lv_obj_center(col);
}
// the props' timer -> the view's (state, label, total, end)
void take(lv_obj_t *root, fw::Props p, bool animate) {
  TV *t = of(root);
  const char *st = p["state"] | "idle";
  t->label = p["label"] | "Pasta";
  t->total = std::max(1, (int) (p["total"] | 600));
  t->state = !strcmp(st, "running") ? 1 : !strcmp(st, "done") ? 2 : 0;
  if (t->state == 1) t->ends = lv_tick_get() + (uint32_t) std::max(1, (int) (p["left"] | 272)) * 1000;
  rows(t, animate);
  render(root, true);
}
}  // namespace

FW_COMPONENT(timer_view, "Timer view", "components", R"({
  "about": "The Timer screen's body: the time left inside a ring that empties as it runs, the timer's name above it, +1 min and Cancel under it; with nothing running, one-tap presets.",
  "frame": [480, 400],
  "props": [
    {"name": "state", "type": "select", "options": ["idle", "running", "done"], "default": "running"},
    {"name": "label", "type": "text", "default": "Pasta"},
    {"name": "total", "type": "range", "min": 60, "max": 7200, "step": 60, "default": 600},
    {"name": "left", "type": "range", "min": 1, "max": 7200, "step": 1, "default": 272}
  ],
  "variants": [
    {"label": "running · 4:32 of 10 min", "props": {}},
    {"label": "nothing running · presets", "props": {"state": "idle"}},
    {"label": "done", "props": {"state": "done"}},
    {"label": "over an hour", "props": {"total": 5400, "left": 3725, "label": "Bread"}}
  ]
})") {
  auto *t = new TV;
  lv_obj_t *root = lv_obj_create(parent);
  lv_obj_set_user_data(root, t);
  // the panel's hardware key (core/device.h, Contextual): a timer that ran out ("done") is dismissed (back to idle)
  fw::dev::claim_button(root, fw::dev::BUTTON_ALERT, [](void *u, const char *press) {
    auto *root = (lv_obj_t *) u;
    TV *t = of(root);
    if (!t || t->state != 2 || strcmp(press, "single")) return false;
    t->state = 0;
    fw::emit(root, "timer", "dismiss");
    rows(t);
    render(root, true);
    return true;
  }, root);
  lv_obj_add_event_cb(root, [](lv_event_t *e) {
    auto *t = (TV *) lv_event_get_user_data(e);
    if (t->tick) lv_timer_delete(t->tick);
    delete t;
  }, LV_EVENT_DELETE, t);
  fill(root, p);
  take(root, p, false);
  t->tick = lv_timer_create([](lv_timer_t *tm) { render((lv_obj_t *) lv_timer_get_user_data(tm)); }, fw::mo::ms::clock_tick, root);
}

FW_UPDATE(timer_view) {
  if (!of(root)) return false;
  take(root, p, true);
  return true;
}
