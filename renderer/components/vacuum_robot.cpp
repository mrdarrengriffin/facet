// Vacuum robot: the robot vacuum seen from above in a ring (the panel lab's Vacuum Home card preview, previewVacuum3,
// apps/hub panel/vacuum/screen.ts + vacuum.css .p3-vp), on the contract (renderer/CONTRACT.md, "contract": 2). The
// Robot vacuum screen's Home card picture (C52): the card names the state in words beside it (never the ring alone).
//   box     w / h layout:vac-card (72 / 48: the card preview size), square
//   ring    full turn, outer edges on the box: track stroke:arc in line (part "arc"); from 12 o'clock clockwise the
//           value stroke:indicator with round caps (part "arc-fill"): the run's progress in accent-text while it runs,
//           else the battery in text-dim (the lab's steel is under 3:1 for a graphic); off = the track only
//   robot   the body ring (a sixth of the box in, stroke:edge, text-dim), the LiDAR turret (a disc, 2 / 9 of the box,
//           above the middle: accent-text while it runs, else text-dim), the bumper (a bar, 7 / 18 of the box wide,
//           stroke:edge tall, a sixth below the middle)
// Drawn through the Pen in one box (fx::strip), inks by token name: a theme switch repaints it. Static: no touch, no
// events, no sound. States: run | idle | off (prop run). n/a: pressed, on, disabled, loading, error (a picture: the
// card's words carry them). Update path: a new value sweeps the ring there (Motion value, a step of up to 2 % at once),
// a new run repaints in place (silent). Contrast:
// tokens/colour.json section "vacuum_robot".
#include <cmath>
#include <cstdlib>
#include "fw.h"
#include "fx.h"

namespace {
struct Bot { int value = 0, shown = 0, run = 1; };  // run: 0 off, 1 idle, 2 run; shown = the ring now (0-1000)
Bot *of(lv_obj_t *o) { return (Bot *) lv_obj_get_user_data(o); }
int S(const char *t) { return fx::size(t); }
int even(float v) { return (int) std::lround(v / 2) * 2; }

void draw(const fx::Pen &p) {
  const Bot *b = of(p.obj);
  if (!b) return;
  const int w = p.w, te = S("stroke:arc"), ti = S("stroke:indicator"), e = S("stroke:edge");
  p.arc(p.half - te / 2, 0.0f, 360.0f, "stroke:arc", "fg:line", false, "arc");
  if (b->run && b->shown > 0) p.arc(p.half - ti / 2, 270.0f, 270.0f + 0.36f * b->shown, "stroke:indicator", b->run == 2 ? "fg:accent-text" : "fg:text-dim", true, "arc-fill");
  const int in = even(w / 6.0f), body = w - 2 * in, lid = even(w * 2 / 9.0f), bump = even(w * 7 / 18.0f);
  p.rect(-body / 2, -body / 2, body, body, "fg:text-dim", body / 2, e);
  const char *mark = b->run == 2 ? "fg:accent-text" : "fg:text-dim";
  p.rect(-lid / 2, -even(w / 12.0f) - lid / 2, lid, lid, mark, lid / 2);
  p.rect(-bump / 2, even(w / 6.0f), bump, e, "fg:text-dim", e / 2);
}
void show(void *o, int32_t v) {  // the ring at v (0-1000): the box is card-sized, so it redraws whole
  auto *s = (lv_obj_t *) o;
  Bot *b = of(s);
  if (!b || b->shown == v) return;
  b->shown = v;
  lv_obj_invalidate(s);
}
void go(lv_obj_t *s, int32_t to) {  // a step of up to 2 % moves there at once, a bigger one sweeps (Motion value, silent)
  Bot *b = of(s);
  lv_anim_delete(s, (lv_anim_exec_xcb_t) show);
  if (std::abs(to - b->shown) <= 20) show(s, to);
  else fw::animate(s, fw::st::SWEEP, {show, b->shown, to, fw::mo::VALUE});
}
void take(lv_obj_t *s, fw::Props p) {
  Bot *b = of(s);
  const char *r = p["run"] | "idle";
  b->run = !strcmp(r, "off") ? 0 : !strcmp(r, "run") ? 2 : 1;
  double v = p["value"].is<const char *>() ? atof(p["value"].as<const char *>()) : (p["value"] | 0.0);
  if (!std::isfinite(v)) v = 0;
  b->value = (int) LV_CLAMP(0, std::lround(v), 100);
}
}  // namespace

FW_COMPONENT(vacuum_robot, "Vacuum robot", "components", R"({
  "about": "The robot vacuum seen from above inside a ring: the run's progress while it cleans, else its battery. The Robot vacuum screen's Home card picture.",
  "contract": 2,
  "parts": ["arc", "arc-fill"],
  "frame": [120, 120],
  "props": [
    {"name": "value", "about": "How full the ring is, from 0 to 100: the run's progress while it cleans, else the battery.", "type": "range", "min": 0, "max": 100, "step": 1, "default": 46},
    {"name": "run", "about": "run while it is out cleaning (the ring and the LiDAR in the accent), idle at rest (the battery in grey), off when it cannot be reached (the empty track).", "type": "select", "options": ["run", "idle", "off"], "default": "run"}
  ],
  "variants": [
    {"label": "cleaning · 46 %", "props": {}},
    {"label": "docked · battery 100 %", "props": {"run": "idle", "value": 100}},
    {"label": "low battery · 15 %", "props": {"run": "idle", "value": 15}},
    {"label": "offline", "props": {"run": "off", "value": 0}}
  ]
})") {
  auto *b = new Bot;
  lv_obj_t *s = fx::strip(parent, draw, "free");
  fx::name(s, "vacuum_robot");
  lv_obj_set_user_data(s, b);
  for (const char *t : {"w:vac-card", "h:vac-card"}) fx::use(s, t);
  take(s, p);
  b->shown = b->value * 10;
  lv_obj_add_event_cb(s, [](lv_event_t *e) { delete (Bot *) lv_event_get_user_data(e); }, LV_EVENT_DELETE, b);
}

FW_UPDATE(vacuum_robot) {
  Bot *b = of(root);
  if (!b) return false;
  const Bot was = *b;
  take(root, p);
  if (was.run != b->run) lv_obj_invalidate(root);
  if (was.value != b->value) go(root, b->value * 10);
  return true;
}
