// Timer chip: one of several timers as a small chip (the Timer screen, concept T1 "Several" picked by Darren
// 2026-10-28: the soonest timer is in the Progress ring, the others are chips above the buttons; tap one to bring it
// into the ring; the one that is done goes accent). On the contract (renderer/CONTRACT.md, "contract": 2). Not a Button
// kind: its own component (a mini ring, a name and a time are not a Button's label + icon).
//   box      hugs its contents (a chip is as wide as its words, as the Status chip), at least touch:min tall (h:min, so
//            the hit area is >= touch:min both ways: C32), the timer-chip style: space 3 in from the outer edge (inside the hair edge), space 3 apart; radius:chip; face surface, hair edge line
//            (border:line); the theme's control part (C46 part:control, last), so a theme gives it its control look
//   ring     a mini progress ring (the Pen, w/h:timer-chip-ring 24 / 16): track stroke:edge in line-hi, the share left
//            stroke:chip-arc in accent-text (3:1 on the pressed surface too) from 12 o'clock clockwise, round caps (parts "arc" / "arc-fill")
//   done     instead of the ring a filled accent disc (timer-chip-ring, radius round, bg:accent-text: 3:1 on accent-dim) with the
//            icons foundation check (icon size, surface); the face accent-dim and the edge accent
//   name     type c-body, text (the T1 chip: the timer's name as written)
//   left     type c-meta (mono), text-dim: "4:32", "1:02:05"; done = DONE in text (accent-text drops under 4.5:1 on accent-dim)
// States: running, idle (the ring full or as left, no countdown of its own: the host ticks it), done; active (the one in
// the big ring: LV_STATE_CHECKED = the on edge, border:on-line); pressed (surface-press + pressed-line, Motion press /
// release); unavailable (fw::st::OFF: disabled inks, no press look, finger down = blocked + error). n/a: loading.
// Taps (fw::st::on_tap, C37): running / idle = event "select" (detail = name) + sound tap; done = event "stop" (detail
// = name) + sound toggle-off (stopping it).
// Redraw (C26): a new left moves only the mini ring's arc span (fx::redraw_arc + the cap) and the time text
// (fx::set_text); the chip box is not repainted. Update path (C35): name / left / total in place, silent; a new state,
// active or unavailable = the contents again in the same root, faded (fw::st::refill_fade). Contrast: tokens/colour.json
// section "timer_chip".
#include "fw.h"
#include "fx.h"
#include <cmath>
#include <cstdlib>
#include <string>

namespace {
constexpr int32_t N = 1000;
constexpr float START = 270.0f;
enum State { RUNNING, IDLE, DONE };
struct Chip {
  int state = RUNNING;
  bool active = false, off = false;
  int32_t shown = 0;
  std::string name, key;
  const char *type = "type:c-meta";  // the time's type: c-body when it is the only text  // key = what a tap sends (the timer's id in a spec; default the name)
  lv_obj_t *ring = nullptr, *left = nullptr, *nm = nullptr;
};
void chip_free(lv_event_t *e) { delete (Chip *) lv_event_get_user_data(e); }
// a bool prop that may come as text from a spec ("$item.active" = "1" / "")
bool on(ArduinoJson::JsonVariantConst v) { return v.is<bool>() ? v.as<bool>() : v.is<const char *>() ? (!strcmp(v | "", "1") || !strcmp(v | "", "true")) : (v | 0) != 0; }
Chip *of(lv_obj_t *o) {  // rides on the root's DELETE event (refill keeps event callbacks)
  for (uint32_t i = 0, n = lv_obj_get_event_count(o); i < n; i++) {
    lv_event_dsc_t *d = lv_obj_get_event_dsc(o, i);
    if (lv_event_dsc_get_cb(d) == chip_free) return (Chip *) lv_event_dsc_get_user_data(d);
  }
  return nullptr;
}
int state_of(fw::Props p) {
  const char *s = p["state"] | "running";
  return !strcmp(s, "done") ? DONE : !strcmp(s, "idle") ? IDLE : RUNNING;
}
double num(fw::Props p, const char *k, double d) {
  if (p[k].is<const char *>()) { const char *s = p[k].as<const char *>(); char *e = nullptr; const double v = strtod(s, &e); return e != s ? v : d; }
  return p[k] | d;
}
int left_of(fw::Props p) { const double v = num(p, "left", 272); return std::isfinite(v) ? std::max(0, (int) std::ceil(v)) : 0; }
int32_t share_of(fw::Props p) {
  const double t = num(p, "total", 600);
  if (!(t > 0) || !std::isfinite(t)) return N;
  return (int32_t) LV_CLAMP(0, std::lround(left_of(p) / t * N), N);
}
std::string countdown(int s) {
  char b[16];
  if (s >= 3600) snprintf(b, sizeof b, "%d:%02d:%02d", s / 3600, s / 60 % 60, s % 60);
  else snprintf(b, sizeof b, "%d:%02d", s / 60, s % 60);
  return b;
}
float angle(int32_t v) { return START + 360.0f * v / N; }

void draw(const fx::Pen &p) {
  lv_obj_t *root = lv_obj_get_parent(p.obj);
  const Chip *c = root ? of(root) : nullptr;
  if (!c) return;
  const int te = fx::size("stroke:edge"), ta = fx::size("stroke:chip-arc");
  p.arc(p.half - te / 2, 0.0f, 360.0f, "stroke:edge", c->off ? "fg:disabled-ink" : "fg:line-hi", false, "arc");
  if (c->shown <= 0) return;
  p.arc(p.half - ta / 2, START, angle(c->shown), "stroke:chip-arc", c->off ? "fg:disabled-ink" : "fg:accent-text", true, "arc-fill");
}
void disc(const fx::Pen &p) {
  lv_obj_t *root = lv_obj_get_parent(p.obj);
  const Chip *c = root ? of(root) : nullptr;
  p.dot(0, 0, "layout:timer-chip-ring", c && c->off ? "fg:disabled-ink" : "fg:accent-text");
}
void show(lv_obj_t *root, int32_t v) {
  Chip *c = of(root);
  if (!c || v == c->shown) return;
  if (c->ring) {
    const int ta = fx::size("stroke:chip-arc");
    fx::redraw_arc(c->ring, fx::half(c->ring) - ta / 2, angle(c->shown), angle(v), ta);  // the span + the cap's reach
  }
  c->shown = v;
}

void tap(lv_event_t *e) {
  if (!fw::st::tap_now(e)) return;
  auto *o = (lv_obj_t *) lv_event_get_current_target(e);
  Chip *c = of(o);
  if (!c) return;
  if (lv_obj_has_state(o, fw::st::OFF)) { fw::st::blocked(o, c->name.c_str()); return; }
  if (c->state == DONE) { fw::emit(o, "stop", c->key.c_str()); fw::sound(o, fw::st::OFF_SOUND); return; }
  fw::emit(o, "select", c->key.c_str());
  fw::sound(o, fw::st::TAP_SOUND);
}

void fill(lv_obj_t *b, fw::Props p) {
  Chip *c = of(b);
  c->state = state_of(p);
  c->active = on(p["active"]);
  c->off = p["unavailable"] | false;
  c->name = p["name"] | "";
  c->key = p["key"] | ""; if (c->key.empty()) c->key = c->name;
  const bool done = c->state == DONE;
  constexpr lv_state_t P = LV_STATE_PRESSED, C = LV_STATE_CHECKED, O = fw::st::OFF;
  for (const char *t : {"timer-chip", "radius:chip", "fade-release"}) fx::use(b, t);
  fx::use(b, done ? "bg:accent-dim" : "bg:surface");
  fx::use(b, done ? "border:accent" : "border:line");
  fx::use(b, "border:accent", C);  // active (the one in the ring): the accent edge
  fx::use(b, "fade-press", P);
  fx::use(b, "bg:pressed-surface", P);
  fx::use(b, "border:pressed-line", P);
  fx::use(b, "bg:pressed-surface", C | P);
  fx::use(b, "border:pressed-line", C | P);
  fx::use(b, "bg:surface", O);
  fx::use(b, "border:line", O);
  fx::use(b, "bg:surface", O | P);
  fx::use(b, "border:line", O | P);
  lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(b, LV_OBJ_FLAG_SCROLLABLE);
  if (done) {
    c->ring = nullptr;
    lv_obj_t *d = fx::drawn(b, disc, "centre");  // the disc (the Pen) with the check on it (icon size = timer-chip-ring)
    for (const char *t : {"w:timer-chip-ring", "h:timer-chip-ring", "max-h:timer-chip-ring"}) fx::use(d, t);
    fx::icon_use(fx::icon(d, "check", "icon", "fg:surface"), "ink:bg", O);
    c->shown = 0;
  } else {
    c->ring = fx::drawn(b, draw, "free");
    for (const char *t : {"w:timer-chip-ring", "h:timer-chip-ring", "max-h:timer-chip-ring"}) fx::use(c->ring, t);
    c->shown = share_of(p);
  }
  // no name (the Timer bar, Darren 2026-10-28): the countdown is the chip's main text
  const bool named = !c->name.empty();
  c->nm = named ? fx::text(b, c->name.c_str(), "type:c-body", "fg:text") : nullptr;
  if (c->nm) { fx::use(c->nm, "fg:disabled-ink", O); fx::fit_grid(c->nm); }
  c->type = named ? "type:c-meta" : "type:c-body";
  c->left = fx::text(b, done ? "DONE" : countdown(left_of(p)).c_str(), c->type, done || !named ? "fg:text" : "fg:text-dim");
  fx::use(c->left, "fg:disabled-ink", O);
  fx::fit_grid(c->left);
  fx::use(b, "part:control");  // the theme's control (C46), last
  if (c->active) lv_obj_add_state(b, C);
  if (c->off) fx::state(b, O, true);
}
std::string sig(fw::Props p) { return std::to_string(state_of(p)) + (on(p["active"]) ? "a" : "") + (*(p["name"] | "") ? "n" : "") + ((p["unavailable"] | false) ? "u" : ""); }
}  // namespace

FW_COMPONENT(timer_chip, "Timer chip", "components", R"({
  "about": "One of several running timers as a small chip: a mini ring, its name and the time left; tap it to show that timer, or to stop one that is done.",
  "contract": 2,
  "parts": ["control", "arc", "arc-fill"],
  "min": ["touch:min", "touch:min"],
  "frame": [320, 64],
  "icons": ["check@icon"],
  "props": [
    {"name": "name", "about": "The timer's name, such as Eggs. Empty leaves the name out and makes the countdown the main text (the Timer bar).", "type": "text", "default": "Eggs"},
    {"name": "key", "about": "What a tap sends to say which timer it is, such as its id. Empty sends the name.", "type": "text", "default": ""},
    {"name": "left", "about": "Seconds left on the timer.", "type": "range", "min": 0, "max": 3600, "step": 1, "default": 430},
    {"name": "total", "about": "The timer's whole length in seconds; the mini ring shows left out of it.", "type": "range", "min": 1, "max": 3600, "step": 1, "default": 480},
    {"name": "state", "about": "running counts down; idle is set but not running; done has run out and shows DONE in the accent (a tap stops it).", "type": "select", "options": ["running", "idle", "done"], "default": "running"},
    {"name": "active", "about": "Marks the chip whose timer is shown in the big ring.", "type": "bool", "default": false},
    {"name": "unavailable", "about": "Shows the chip greyed out when the timer cannot be read; a tap does nothing but say so.", "type": "bool", "default": false}
  ],
  "variants": [
    {"label": "running · Eggs 7:10", "props": {}},
    {"label": "running · Pasta 4:32", "props": {"name": "Pasta", "left": 272, "total": 600}},
    {"label": "active", "props": {"name": "Pasta", "left": 272, "total": 600, "active": true}},
    {"label": "done · Oven", "props": {"name": "Oven", "left": 0, "total": 1500, "state": "done"}},
    {"label": "idle", "props": {"name": "Tea", "left": 240, "total": 240, "state": "idle"}},
    {"label": "an hour or more", "props": {"name": "Roast", "left": 3725, "total": 5400}},
    {"label": "unavailable", "props": {"name": "Eggs", "unavailable": true}}
  ]
})") {
  lv_obj_t *b = lv_obj_create(parent);
  lv_obj_remove_style_all(b);
  fx::name(b, "timer_chip");
  lv_obj_add_event_cb(b, chip_free, LV_EVENT_DELETE, new Chip);
  fw::shape(b, sig(p));
  fill(b, p);
  fw::st::on_tap(b, tap, nullptr);
}

// Update path: the same state / active / unavailable = name, time and ring in place (the ring's arc span and the time
// text only), silent; another = the contents again in the same root, faded (shared styles).
FW_UPDATE(timer_chip) {
  Chip *c = of(root);
  if (!c) return false;
  if (!fw::shape(root, sig(p))) {
    fw::st::refill_fade(root, [&] { c->ring = c->left = c->nm = nullptr; fw::refill(root); fill(root, p); }, true, true);
    return true;
  }
  const char *nm = p["name"] | "Eggs";  // in place: no style is added again (that repaints the whole chip)
  if (c->nm && c->name != nm) { c->name = nm; fx::set_text(c->nm, nm, "type:c-body"); }
  c->key = p["key"] | ""; if (c->key.empty()) c->key = c->name;
  if (c->state != DONE) {
    if (c->left) fx::set_text(c->left, countdown(left_of(p)).c_str(), c->type);
    show(root, share_of(p));
  }
  return true;
}
