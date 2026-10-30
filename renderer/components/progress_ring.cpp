// Progress ring: a read-only full-turn ring that empties as a value runs down (the Timer screen's ring, concept T1
// picked by Darren 2026-10-28; legacy components/timer_view.cpp lay::timer_ring), on the contract (renderer/CONTRACT.md,
// "contract": 2). Built only from token names and the Pen: a theme / accent switch repaints it with no code here.
//   box      fills its parent's width up to max-w:timer-ring (432 / 288) and is as tall as it is wide (fx::drawn "fit");
//            meta min layout:timer-ring both ways (below it the centre slot no longer fits inside the ring: the bounds
//            proof's to report, C10). Steps: one
//   ring     from 12 o'clock clockwise, outer edges together (as the legacy value arc): track stroke:arc in line (part
//            "arc"), the share left stroke:indicator in accent with round caps (part "arc-fill"; a theme's cap shape)
//   centre   a SLOT (meta "slots": centre, C8a): one component a spec puts there ("in": k, "slot": 0), a Hero block with
//            the label / time / end line; at most max-w:timer-ring-hole wide (the square inside the ring's inner edge),
//            centred both ways. The ring has no words of its own: done and unavailable words are the centre's.
//            Workbench: prop sample fills it with a made-up Hero block (the real one, fw::find, C8b)
// Value: value = the share left out of 1000, or with max > 0 value of max (a timer's seconds left of its length);
// clamped, a non-number = 0. States: default; done (value 0: the track only); unavailable (fw::st::OFF: the track only,
// the centre says why). n/a: pressed, on, disabled, loading (a read-out: no press, no events, no sound).
// Redraw (C26): a change invalidates only the arc span between the old and the new end, widened by the indicator's
// width (its round cap + the AA fringe; fx::redraw adds a snapped theme's cells): a 1 s countdown tick never repaints
// the box or the centre. Update path (C35): a step of up to 2 % (a countdown tick) moves there at once, a bigger one
// sweeps with Motion value; silent; the slot's contents stay (no refill). Contrast: tokens/colour.json "progress_ring".
#include "fw.h"
#include "fx.h"
#include "spec.h"
#include <cmath>
#include <cstdlib>

namespace {
constexpr int32_t N = 1000;
constexpr float START = 270.0f;  // 12 o'clock (Pen angles: 0 = 3 o'clock, clockwise)
struct Ring { int32_t shown = 0; bool off = false; lv_obj_t *slot = nullptr; };
Ring *of(lv_obj_t *o) { return (Ring *) lv_obj_get_user_data(o); }
float angle(int32_t v) { return START + 360.0f * v / N; }
double num(fw::Props p, const char *k, double d) {
  if (p[k].is<const char *>()) { const char *s = p[k].as<const char *>(); char *e = nullptr; const double v = strtod(s, &e); return e != s ? v : d; }
  return p[k] | d;
}
int32_t share_of(fw::Props p) {
  double v = num(p, "value", 0), mx = num(p, "max", 0);
  if (mx > 0) v = v / mx * N;
  if (!std::isfinite(v)) v = 0;
  return (int32_t) LV_CLAMP(0, std::lround(v), N);
}
bool off_of(fw::Props p) { return !strcmp(p["state"] | "default", "unavailable"); }

void draw(const fx::Pen &p) {
  const Ring *r = of(p.obj);
  if (!r) return;
  const int te = fx::size("stroke:arc"), ti = fx::size("stroke:indicator");
  p.arc(p.half - te / 2, 0.0f, 360.0f, "stroke:arc", "fg:line", false, "arc");
  if (r->off || r->shown <= 0) return;  // done / unavailable: the track only
  p.arc(p.half - ti / 2, START, angle(r->shown), "stroke:indicator", "fg:accent-text", true, "arc-fill");  // accent-text: 3:1 on the light canvas too
}
void show(void *o, int32_t v) {
  auto *s = (lv_obj_t *) o;
  Ring *r = of(s);
  v = LV_CLAMP(0, v, N);
  if (v == r->shown) return;
  const int ti = fx::size("stroke:indicator");
  fx::redraw_arc(s, fx::half(s) - ti / 2, angle(r->shown), angle(v), ti);  // the span + the cap's reach
  r->shown = v;
}
void go(lv_obj_t *s, int32_t to) {
  Ring *r = of(s);
  lv_anim_delete(s, (lv_anim_exec_xcb_t) show);
  if (std::abs(to - r->shown) <= N / 50) show(s, to);  // a tick: there at once
  else fw::animate(s, fw::st::SWEEP, {show, r->shown, to, fw::mo::VALUE});
}

// the workbench sample: a made-up Hero block in the slot (the real one, C8b)
void sample(lv_obj_t *slot, const char *kind) {
  lv_obj_clean(slot);
  if (!strcmp(kind, "none")) return;
  const fw::Component *c = fw::find("hero_block");
  if (!c) return;
  const bool done = !strcmp(kind, "done"), off = !strcmp(kind, "unavailable");
  ArduinoJson::JsonDocument d;
  d["n"] = done ? "0:00" : "4:32";
  d["label"] = "pasta";
  d["layout"] = "stack";  // as on the Timer page: label over the time over the line
  d["line"] = done ? "done" : "ends 14:52";
  d["accent"] = done;
  if (off) d["state"] = "unavailable";
  c->build(slot, d.as<fw::Props>());
}
constexpr lv_state_t ST_SM = LV_STATE_USER_3;  // size compact: the smaller ring (a state, so an update switches it in place)
void style(lv_obj_t *s) {
  for (const char *t : {"box", "centre", "fit", "max-w:timer-ring"}) fx::use(s, t);
  fx::use(s, "max-w:timer-ring-sm", ST_SM);
}
bool compact(fw::Props p) { return !strcmp(p["size"] | "normal", "compact"); }
}  // namespace

FW_COMPONENT(progress_ring, "Progress ring", "components", R"({
  "about": "A ring that empties as something runs down, such as a timer, with one component in its centre for the words.",
  "contract": 2,
  "parts": ["arc", "arc-fill"],
  "min": ["layout:timer-ring", "layout:timer-ring"],
  "slots": [
    {"name": "centre", "count": 1, "about": "One component in the middle of the ring, usually a Hero block with the name, the time left and when it ends (in + slot 0).", "accepts": ["hero_block", "label", "stat", "title", "big_number"], "max": 1}
  ],
  "frame": [448, 448],
  "props": [
    {"name": "value", "about": "How much is left: out of 1000, or out of max when max is set (such as seconds left of the timer's length).", "type": "range", "min": 0, "max": 1000, "step": 1, "default": 0},
    {"name": "max", "about": "What value counts up to, such as the timer's length in seconds. 0 means value is out of 1000.", "type": "range", "min": 0, "max": 3600, "step": 1, "default": 0},
    {"name": "size", "about": "normal is the full ring; compact is smaller, to leave room under it (the Timer with several timers).", "type": "select", "options": ["normal", "compact"], "default": "normal"},
    {"name": "state", "debug": true, "about": "default shows how much is left; unavailable shows the empty track when the value cannot be read.", "type": "select", "options": ["default", "unavailable"], "default": "default"},
    {"name": "sample", "about": "For previews only: a made-up Hero block in the centre (running, done or unavailable), or none for the empty slot.", "type": "select", "options": ["none", "running", "done", "unavailable"], "default": "none"}
  ],
  "variants": [
    {"label": "running · 4:32 of 10 min", "props": {"value": 272, "max": 600, "sample": "running"}},
    {"label": "nearly done", "props": {"value": 40, "sample": "running"}},
    {"label": "full", "props": {"value": 1000, "sample": "running"}},
    {"label": "compact", "props": {"value": 272, "max": 600, "sample": "running", "size": "compact"}},
    {"label": "done", "props": {"value": 0, "sample": "done"}},
    {"label": "unavailable", "props": {"state": "unavailable", "sample": "unavailable"}},
    {"label": "empty slot", "props": {"value": 600}}
  ]
})") {
  auto *r = new Ring;
  lv_obj_t *s = fx::drawn(parent, draw, "centre");
  fx::name(s, "progress_ring");
  lv_obj_set_user_data(s, r);
  lv_obj_add_event_cb(s, [](lv_event_t *e) { delete (Ring *) lv_event_get_user_data(e); }, LV_EVENT_DELETE, r);
  lv_obj_remove_flag(s, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  style(s);
  r->slot = fx::box(s, "centre");  // the centre slot (meta "slots": centre)
  fx::use(r->slot, "max-w:timer-ring-hole");
  fx::use(r->slot, "max-w:timer-ring-sm-hole", ST_SM);
  if (compact(p)) fx::state(s, ST_SM, true);
  fx::slot(r->slot, "centre");
  lv_obj_add_flag(r->slot, fw::spec::SLOT);
  fw::shape(s, p["sample"] | "none");
  sample(r->slot, p["sample"] | "none");
  r->off = off_of(p);
  if (r->off) fx::state(s, fw::st::OFF, true);
  r->shown = r->off ? 0 : share_of(p);  // opening: at rest (C34), no sweep
}

// Update path: the value moves in place (a tick at once, more sweeps: Motion value), unavailable in / out repaints the
// ring; the slot keeps its contents (a spec's node is its own update's). A new sample builds the sample again. Silent.
FW_UPDATE(progress_ring) {
  Ring *r = of(root);
  if (!r) return false;
  if (!fw::shape(root, p["sample"] | "none")) sample(r->slot, p["sample"] | "none");
  if (compact(p) != lv_obj_has_state(root, ST_SM)) { fx::state(root, ST_SM, compact(p)); lv_obj_invalidate(root); }  // a new size: restyled in place
  const bool off = off_of(p);
  if (off != r->off) {
    lv_anim_delete(root, (lv_anim_exec_xcb_t) show);
    r->off = off;
    fx::state(root, fw::st::OFF, off);
    r->shown = off ? 0 : share_of(p);
    lv_obj_invalidate(root);  // a state change, not a moving part
    return true;
  }
  if (!off) go(root, share_of(p));
  return true;
}
