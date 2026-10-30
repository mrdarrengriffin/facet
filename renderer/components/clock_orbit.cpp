// Clock orbit: the Clock screen's picture (docs screens/clock.ts orbitFace; the orbit face that was part of the legacy
// Clock faces, components/clock_face.cpp), on the contract (renderer/CONTRACT.md, "contract": 2). Built only from token
// names and the Pen: a theme / accent switch repaints it with no code here.
//   box      fills its parent's width and is as tall as it is wide, never taller than its parent (fx::drawn "fit"): on
//            the Clock screen a square the height of the body. Steps: one
//   ring     120 ticks round the edge from 12 o'clock (docs tickRing 120, range 357 = every 3 degrees): minor
//            stroke:tick long (a dot) in line-hi (part "tick"), every 10th layout:clock-orbit-tick long in steel (part
//            "tick-major": a theme's mark shape)
//   dots     three dots orbiting inside the ticks (docs orbit-a / b / c), each a layout:clock-orbit-step further in:
//            accent layout:clock-orbit-dot one turn a minute with the seconds (from the real time: at :15 it is at 3
//            o'clock), blue layout:clock-orbit-dot-sm reversed one turn per Motion orbit-b, steel the same size one turn
//            per orbit-c. The dots' circles stay on the dot-size grid of the legacy face (r = half - orbit-dot / 2 -
//            orbit-tick - k x orbit-step)
//   centre   a SLOT (meta "slots": centre, C8a): one component a spec puts there ("in": k, "slot": 0), the Time with the
//            date and the weather line. The picture has no words of its own. Workbench: prop sample fills it with the
//            real Time (fw::find, C8b)
// Time: prop "time" = a fixed "HH:MM:SS" (previews, tests) that runs on from there; empty = live (localtime).
// Motion: Motion orbit-a / b / c, linear, forever (prop animated false = still where they start); reduced motion = the blue and steel dots stay at 12 o'clock and the
// accent dot steps once a second. States: default only (a picture: no press, no events, no sound; n/a pressed, on,
// disabled, loading, unavailable). Redraw (C26): a moving dot invalidates only its old and new box (fx::redraw adds a
// snapped theme's cells), never the ring or the slot. Update path (C35): a new fixed time moves the accent dot there;
// a new sample builds the sample again; the slot's contents stay. Contrast: tokens/colour.json section "clock".
#include <cmath>
#include <ctime>
#include "fw.h"
#include "fx.h"
#include "spec.h"

namespace {
struct Orbit {
  long base_ms = 0;   // ms of the day at t0 (the seconds dot)
  uint32_t t0 = 0;    // lv_tick at build (the inner dots start at 12 o'clock)
  bool reduced = false, still = false;
  int x[3] = {}, y[3] = {}; bool placed = false;
  lv_obj_t *slot = nullptr;
  lv_timer_t *timer = nullptr;
  std::string fixed;
};
Orbit *of(lv_obj_t *o) { return (Orbit *) lv_obj_get_user_data(o); }
const char *const SIZE[3] = {"layout:clock-orbit-dot", "layout:clock-orbit-dot-sm", "layout:clock-orbit-dot-sm"};
const char *const INK[3] = {"fg:accent", "fg:blue", "fg:steel"};

long day_ms(const std::string &fixed) {
  int h = 0, m = 0, s = 0;
  if (!fixed.empty() && sscanf(fixed.c_str(), "%d:%d:%d", &h, &m, &s) >= 2) return ((h * 60L + m) * 60 + s) * 1000;
  const time_t t = time(nullptr);
  struct tm tm {};
  localtime_r(&t, &tm);
  return ((tm.tm_hour * 60L + tm.tm_min) * 60 + tm.tm_sec) * 1000;
}
int radius(int half, int k) {
  return half - fx::size("layout:clock-orbit-dot") / 2 - fx::size("layout:clock-orbit-tick") - k * fx::size("layout:clock-orbit-step");
}
// where dot k is now (centre-relative), from the time since build
void where(const Orbit *o, int half, int k, int *x, int *y) {
  const long el = o->still ? 0 : (long) lv_tick_elaps(o->t0);
  float deg = 270.0f;
  if (k == 0) {
    const long ms = (o->base_ms + el) % 60000;
    deg += o->reduced ? (ms / 1000) * 6.0f : ms * 360.0f / 60000.0f;
  } else if (!o->reduced) {
    const long T = k == 1 ? fw::mo::ms::orbit_b : fw::mo::ms::orbit_c;
    deg += (k == 1 ? -1.0f : 1.0f) * (el % T) * 360.0f / T;  // blue reversed
  }
  fx::at(deg, radius(half, k), x, y);
}

void draw(const fx::Pen &p) {
  Orbit *o = of(p.obj);
  if (!o) return;
  for (int i = 0; i < 120; i++) {
    const bool major = i % 10 == 0;
    p.tick(270.0f + i * 3.0f, major ? "layout:clock-orbit-tick" : "stroke:tick", "stroke:tick", major ? "fg:steel" : "fg:line-hi", major ? "tick-major" : "tick");
  }
  if (!o->placed)  // the first draw places them; after that only step() moves them (a redraw of anything else
    for (int k = 0; k < 3; k++) where(o, p.half, k, &o->x[k], &o->y[k]);  // in the box never moves a dot unseen)
  o->placed = true;
  for (int k = 2; k >= 0; k--) p.dot(o->x[k], o->y[k], SIZE[k], INK[k]);  // the seconds dot on top
}
// a frame: each dot that moved repaints its old and new box only
void step(lv_obj_t *s) {
  Orbit *o = of(s);
  if (!o || !o->placed) return;
  const int half = fx::half(s);
  for (int k = 0; k < 3; k++) {
    int x, y;
    where(o, half, k, &x, &y);
    if (x == o->x[k] && y == o->y[k]) continue;
    const int d = fx::size(SIZE[k]) + 2;  // + the AA fringe
    fx::redraw(s, o->x[k] - d / 2, o->y[k] - d / 2, d, d);
    fx::redraw(s, x - d / 2, y - d / 2, d, d);
    o->x[k] = x, o->y[k] = y;
  }
}

// the workbench sample: the real Time in the slot (C8b), as on the Clock screen
void sample(lv_obj_t *slot, const char *kind, const std::string &fixed) {
  lv_obj_clean(slot);
  if (strcmp(kind, "time")) return;
  const fw::Component *c = fw::find("time");
  if (!c) return;
  ArduinoJson::JsonDocument d;
  d["size"] = "mid";
  d["font"] = "inter";  // the docs orbit face: the display style (sans)
  d["date"] = "short";
  d["time"] = fixed;
  d["day"] = fixed.empty() ? "" : "2026-10-19";
  d["line"] = "rain \xC2\xB7 13\xC2\xB0";
  c->build(slot, d.as<fw::Props>());
}
}  // namespace

FW_COMPONENT(clock_orbit, "Clock orbit", "components", R"({
  "about": "The Clock screen's picture: a ring of ticks with three dots orbiting inside it at different speeds, the outer one with the seconds, and one component in its centre for the time.",
  "contract": 2,
  "parts": ["tick", "tick-major"],
  "min": ["layout:timer-ring", "layout:timer-ring"],
  "slots": [
    {"name": "centre", "count": 1, "about": "One component in the middle of the ring, usually the Time with the date and the weather line (in + slot 0).", "accepts": ["time"], "max": 1}
  ],
  "frame": [560, 560],
  "props": [
    {"name": "time", "about": "For previews only: a fixed time, like 10:08:30, that the seconds dot starts from. Leave empty for the real time.", "type": "text", "default": ""},
    {"name": "animated", "about": "Moves the dots. Off holds them still where they start: a still picture.", "type": "bool", "default": true},
    {"name": "sample", "about": "For previews only: the real Time with the date and a weather line in the centre, or none for the empty slot.", "type": "select", "options": ["none", "time"], "default": "none"}
  ],
  "variants": [
    {"label": "the Clock screen · 10:08:30", "props": {"time": "10:08:30", "sample": "time"}},
    {"label": "live", "props": {"sample": "time"}},
    {"label": "empty slot", "props": {"time": "21:47:05"}}
  ]
})") {
  auto *o = new Orbit;
  lv_obj_t *s = fx::drawn(parent, draw, "centre");
  fx::name(s, "clock_orbit");
  lv_obj_set_user_data(s, o);
  lv_obj_add_event_cb(s, [](lv_event_t *e) {
    auto *o = (Orbit *) lv_event_get_user_data(e);
    if (o->timer) lv_timer_delete(o->timer);
    delete o;
  }, LV_EVENT_DELETE, o);
  lv_obj_remove_flag(s, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  for (const char *t : {"box", "centre", "fill-w"}) fx::use(s, t);
  o->slot = fx::box(s, "centre");  // the centre slot (meta "slots": centre)
  fx::slot(o->slot, "centre");
  lv_obj_add_flag(o->slot, fw::spec::SLOT);
  o->fixed = p["time"] | "";
  o->base_ms = day_ms(o->fixed);
  o->t0 = lv_tick_get();
  o->reduced = fw::mo::reduced(s);
  o->still = !(p["animated"] | true);
  fw::shape(s, p["sample"] | "none");
  sample(o->slot, p["sample"] | "none", o->fixed);
  // frames: often enough for a smooth orbit while moving (a dot moves a px every ~30 ms), once a second when reduced
  if (!o->still) o->timer = lv_timer_create([](lv_timer_t *t) { step((lv_obj_t *) lv_timer_get_user_data(t)); }, o->reduced ? 1000 : 33, s);
}

// Update path: a new fixed time restarts the seconds dot there (it moves on the next frame); a new sample builds the
// sample again; the slot keeps a spec's node (its own update's). Silent.
FW_UPDATE(clock_orbit) {
  Orbit *o = of(root);
  if (!o) return false;
  const std::string fixed = p["time"] | "";
  if (fixed != o->fixed) {
    o->fixed = fixed;
    o->base_ms = (day_ms(fixed) - (long) lv_tick_elaps(o->t0) % 86400000 + 86400000) % 86400000;
  }
  if (!fw::shape(root, p["sample"] | "none")) sample(o->slot, p["sample"] | "none", o->fixed);
  return true;
}
