// Lightning radar (docs: Components > Radar > Lightning radar, @facet/ui radar.ts lightningRadar(), CSS 40-radar.css):
// strikes by distance and bearing round home, coloured by age; the nearest one of the last 15 minutes gets a ring, a
// new one ripples. On the contract (renderer/CONTRACT.md, "contract": 2, 2026-10-29): drawn with the Pen only, every
// ink a colour token and every size a size token read at draw time, so a theme (colours, sizes, parts, snap) and the
// Layout lab restyle it with no code here. Data = core/lightning.h (fw::lx).
//   box     fills its parent's width up to max-w:radar (384 / 256), square (fx::drawn "fit")
//   rings   10 / 25 / 50 / 100 km at R x sqrt(km / 100) on the 4 px grid (docs: 64 / 96 / 136 / 192 at 384), stroke:hair
//           fg:line-hi (part "arc"); the 25 km ring = the alert ring, stroke:tick fg:warn (part "arc-fill"; docs accent-dim,
//           under 3:1 on the canvas)
//           Strikes sit piecewise-linearly between the drawn rings, so a dot always agrees with them (docs rPx)
//   ticks   every 30 degrees from the edge: minor layout:dial-tick fg:line-hi (part "tick"), N / E / S / W
//           layout:dial-tick-major fg:steel (part "tick-major"); stroke:tick
//   labels  each ring's distance just right of the south axis, space:2 above its ring (type:label fg:text-dim; the
//           outer one carries the unit), "N" under the north tick. unit km | mi (6 / 16 / 31 / 62 mi)
//   home    layout:radar-home: a fg:blue stroke:edge ring on fg:bg, with a layout:radar-home-dot fg:blue dot
//   strikes layout:radar-strike dots (part "strike": a theme's ink is NOT applied, age is the colour; its snap is):
//           < 1 min fg:on-accent inside a stroke:edge fg:accent edge, < 5 min fg:accent, < 15 min fg:lx-faded, older
//           fg:lx-old; newest on top
//   nearest layout:radar-near stroke:hair fg:accent-text ring round the nearest strike of the last 15 minutes
//   ripple  a strike under 90 s old that was not shown before: a stroke:edge fg:accent ring grows from the dot to
//           layout:radar-flash (Motion ripple, at most 6 at once; reduced motion: none). Silent.
// Data: prop strikes (the lightning feed's [[km, bearing, ms], ...], newest first) at the real time; empty = the docs
// demo storm, played at DEMO_SPEED when demo is on (meta "motion"), pinned by now when off.
// Update path: new strikes = one invalidate of the box (the rings are cheap; a radar updates at most once a minute
// live). Contrast: tokens/colour.json section "radar".
#include <cmath>
#include <set>
#include "fw.h"
#include "fx.h"
#include "lightning.h"

namespace {
constexpr int RING_KM[4] = {10, 25, 50, 100};
constexpr double DEMO_SPEED = 20;  // one Motion clock-tick (1 s) = 20 s of storm: a strike about every 2 s
struct Ripple { int x, y; int32_t v; };
struct LX {
  fw::lx::Data d;
  double now = 0;
  std::set<uint32_t> seen;
  std::vector<Ripple> ripples;
  fw::units::Distance unit = fw::units::Distance::km;
  lv_timer_t *demo = nullptr;
  bool quiet = false;
};
LX *of(lv_obj_t *o) { return (LX *) lv_obj_get_user_data(o); }

double now_of(fw::Props p) {
  const double fixed = p["now"] | 0.0;
  return fixed > 0 ? fixed : (double) time(nullptr) * 1000;
}
bool live(fw::Props p) { const char *s = p["strikes"] | ""; return *s == '['; }
void load(LX &x, fw::Props p) {
  if (!live(p)) { x.d = fw::lx::sample(x.now, x.quiet); return; }
  ArduinoJson::JsonDocument j;
  x.d = fw::lx::Data{};
  if (!deserializeJson(j, p["strikes"] | "[]"))
    for (ArduinoJson::JsonArrayConst a : j.as<ArduinoJson::JsonArrayConst>()) {
      const double t = a[2] | 0.0;
      x.d.strikes.push_back({(uint32_t) fw::lx::hash32((uint32_t) (t / 1000) ^ (uint32_t) ((a[1] | 0.0f) * 100)), a[0] | 0.0f, a[1] | 0.0f, t});
    }
  fw::lx::summarise(x.d, x.now);
}

// the geometry of a Pen of half-size R: ring radii on the 4 px grid, the outer one at the edge
int ring_px(int R, int i) { return i == 3 ? R : (int) std::lround(R * std::sqrt(RING_KM[i] / 100.0) / 4) * 4; }
float r_px(int R, float d) {  // px from the centre for d km: piecewise-linear between the drawn rings (docs rPx)
  float pd = 0, pr = 0;
  for (int i = 0; i < 4; i++) {
    if (d <= RING_KM[i]) return pr + (d - pd) / (RING_KM[i] - pd) * (ring_px(R, i) - pr);
    pd = (float) RING_KM[i];
    pr = (float) ring_px(R, i);
  }
  return (float) R;
}
// centre of a mark `size` px across, centre-relative; near the outer ring it is pulled in so the whole mark stays
// inside the major ticks (a space:1 gap), so a strike never sits on a compass mark
void pos(int R, float d, float az, int size, int *px, int *py) {
  const float rmax = (float) (R - fx::size("layout:dial-tick-major") - fx::size("space:1")) - size / 2.0f;
  const float r = std::min(r_px(R, d), rmax), a = az * 3.14159265f / 180;
  *px = (int) lroundf(r * sinf(a));
  *py = (int) lroundf(-r * cosf(a));
}
std::string ring_label(int i, fw::units::Distance u) {
  const long v = std::lround(fw::units::from_km(RING_KM[i], u));
  char b[16];
  if (i == 3) snprintf(b, sizeof b, "%ld %s", v, u == fw::units::Distance::mi ? "mi" : "km");
  else snprintf(b, sizeof b, "%ld", v);
  return b;
}
// a ring of diameter d centred on x, y (centre-relative), a `stroke` wide edge in `ink`
void ring_at(const fx::Pen &p, int x, int y, int d, const char *stroke, const char *ink) {
  p.rect(x - d / 2, y - d / 2, d, d, ink, d / 2, fx::size(stroke));
}

void draw(const fx::Pen &p) {
  const LX *x = of(p.obj);
  if (!x) return;
  const int R = p.half;
  for (int i = 0; i < 4; i++) {
    if (RING_KM[i] == 25) p.arc(ring_px(R, i) - fx::size("stroke:tick") / 2, 0.0f, 360.0f, "stroke:tick", "fg:warn", false, "arc-fill");
    else p.arc(ring_px(R, i) - fx::size("stroke:hair") / 2, 0.0f, 360.0f, "stroke:hair", "fg:line-hi", false, "arc");
  }
  for (int a = 0; a < 360; a += 30) {
    const bool major = a % 90 == 0;
    p.tick(270.0f + a, major ? "layout:dial-tick-major" : "layout:dial-tick", "stroke:tick", major ? "fg:steel" : "fg:line-hi", major ? "tick-major" : "tick");
  }
  const int bh = fx::type_box("type:label"), s2 = fx::size("space:2");
  for (int i = 0; i < 4; i++) {
    const std::string t = ring_label(i, x->unit);
    p.text(s2 + p.text_w(t.c_str(), "type:label") / 2, ring_px(R, i) - s2 - bh / 2, t.c_str(), "type:label", "fg:text-dim");
  }
  p.text(0, -R + fx::size("layout:dial-tick-major") + s2 + bh / 2, "N", "type:label", "fg:text-dim");
  // strikes, oldest first so the newest paint on top
  const int D = fx::size("layout:radar-strike");
  for (size_t i = x->d.strikes.size(); i-- > 0;) {
    const fw::lx::Strike &s = x->d.strikes[i];
    int sx, sy;
    pos(R, s.d, s.az, D, &sx, &sy);
    switch (fw::lx::age_class(s.t, x->now)) {
      case 0: p.dot(sx, sy, "layout:radar-strike", "fg:on-accent"); ring_at(p, sx, sy, D, "stroke:edge", "fg:accent"); break;
      case 1: p.dot(sx, sy, "layout:radar-strike", "fg:accent"); break;
      case 2: p.dot(sx, sy, "layout:radar-strike", "fg:lx-faded"); break;
      default: p.dot(sx, sy, "layout:radar-strike", "fg:lx-old"); break;
    }
  }
  if (x->d.has_nearest) {
    const int N = fx::size("layout:radar-near");
    int nx, ny;
    pos(R, x->d.nearest.d, x->d.nearest.az, N, &nx, &ny);
    ring_at(p, nx, ny, N, "stroke:hair", "fg:accent-text");
  }
  for (const Ripple &r : x->ripples) {  // grows from the dot to radar-flash
    const int d0 = D, d1 = fx::size("layout:radar-flash");
    ring_at(p, r.x, r.y, d0 + (d1 - d0) * r.v / 1000, "stroke:edge", "fg:accent");
  }
  const int H = fx::size("layout:radar-home");
  p.dot(0, 0, "layout:radar-home", "fg:bg");
  ring_at(p, 0, 0, H, "stroke:edge", "fg:blue");
  p.dot(0, 0, "layout:radar-home-dot", "fg:blue");
}

// the ripples' frame: one batch at a time, v 0..1000 (Motion ripple) shared, only their boxes redraw; the end drops them
void ripple_exec(void *vo, int32_t v) {
  auto *o = (lv_obj_t *) vo;
  LX *x = of(o);
  if (!x) return;
  const int d = fx::size("layout:radar-flash") + 2 * fx::size("stroke:edge");
  for (Ripple &r : x->ripples) { r.v = v; fx::redraw(o, r.x - d / 2, r.y - d / 2, d, d); }
}
void ripple(lv_obj_t *o, LX &x, const fw::lx::Strike &s) {
  int sx, sy;
  pos(fx::half(o), s.d, s.az, fx::size("layout:radar-strike"), &sx, &sy);
  x.ripples.push_back({sx, sy, 0});
}
void ripples_go(lv_obj_t *o) {
  fw::Anim a{ripple_exec, 0, 1000, fw::mo::OPA};
  a.done = [](lv_anim_t *an) {
    auto *o = (lv_obj_t *) an->var;
    if (LX *x = of(o)) { x->ripples.clear(); lv_obj_invalidate(o); }
  };
  fw::animate(o, fw::mo::ripple, a);
}

void show(lv_obj_t *o, LX &x, bool animate) {
  lv_obj_invalidate(o);
  int fresh = 0;
  const bool can = animate && !fw::mo::reduced(o) && x.ripples.empty();
  for (size_t i = x.d.strikes.size(); i-- > 0;) {
    const fw::lx::Strike &s = x.d.strikes[i];
    if (!x.seen.insert(s.id).second) continue;
    if (can && x.now - s.t < 90000 && fresh < 6) { ripple(o, x, s); fresh++; }
  }
  if (fresh) ripples_go(o);
  if (x.seen.size() > 4000) {
    x.seen.clear();
    for (const fw::lx::Strike &s : x.d.strikes) x.seen.insert(s.id);
  }
}
}  // namespace

FW_COMPONENT(lightning_radar, "Lightning radar", "components", R"({
  "about": "Strikes by distance (square-root scale) and bearing round home, coloured by age; the nearest one of the last 15 minutes gets a ring. New ones ripple.",
  "contract": 2,
  "parts": ["arc", "arc-fill", "tick", "tick-major"],
  "frame": [400, 400],
  "motion_off": "demo",
  "motion": "demo: strikes arrive (about every 2 s), age through their colours and ripple; the nearest ring moves",
  "props": [
    {"name": "strikes", "about": "The strikes to draw, newest first, as the lightning feed gives them: a list of [km, bearing in degrees, time in ms]. Empty shows the made-up storm.", "type": "text", "default": ""},
    {"name": "demo", "about": "For previews only: plays a made-up storm so strikes appear.", "type": "bool", "default": true},
    {"name": "quiet", "about": "For previews only: shows a quiet sky with no recent strikes.", "type": "bool", "default": false},
    {"name": "unit", "about": "Whether distances are shown in kilometres or miles.", "type": "select", "options": ["km", "mi"], "default": "km"}
  ],
  "variants": [
    {"label": "a storm passing", "props": {"demo": false, "now": 1792944000000}},
    {"label": "it has passed", "props": {"demo": false, "now": 1792946520000}},
    {"label": "miles", "props": {"demo": false, "now": 1792944000000, "unit": "mi"}},
    {"label": "quiet sky", "props": {"demo": false, "now": 1792944000000, "quiet": true}}
  ]
})") {
  auto *x = new LX;
  lv_obj_t *o = fx::drawn(parent, draw, "centre");
  fx::name(o, "lightning_radar");
  // its own square (a Split slot hugs its contents); max-h:radar lifts drawn()'s "fit" cap (a hugging parent's height, 0)
  for (const char *t : {"box", "centre", "w:radar", "h:radar", "max-h:radar"}) fx::use(o, t);
  lv_obj_set_user_data(o, x);
  lv_obj_add_event_cb(o, [](lv_event_t *e) {
    auto *x = (LX *) lv_event_get_user_data(e);
    if (x->demo) lv_timer_delete(x->demo);
    delete x;
  }, LV_EVENT_DELETE, x);
  lv_obj_remove_flag(o, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  x->unit = fw::units::distance_of(p["unit"] | "km");
  x->now = now_of(p);
  const bool demo = (p["demo"] | true) && !live(p);  // bound strikes: the real clock, no demo
  if (demo) x->now = std::floor(x->now / 3600000.0) * 3600000.0 + 0.3 * 3600000.0;
  x->quiet = p["quiet"] | false;
  load(*x, p);
  for (const fw::lx::Strike &s : x->d.strikes) x->seen.insert(s.id);  // opening: at rest (C34), no ripples
  if (demo) {
    x->demo = lv_timer_create([](lv_timer_t *tm) {
      auto *o = (lv_obj_t *) lv_timer_get_user_data(tm);
      LX *x = of(o);
      x->now += fw::mo::ms::clock_tick * DEMO_SPEED;
      x->d = fw::lx::sample(x->now, x->quiet);
      show(o, *x, true);
    }, fw::mo::ms::clock_tick, o);
  }
}

FW_UPDATE(lightning_radar) {
  LX *x = of(root);
  if (!x) return false;
  if (((p["demo"] | true) && !live(p)) != (x->demo != nullptr)) return false;  // demo on / off: rebuild
  x->unit = fw::units::distance_of(p["unit"] | "km");
  x->quiet = p["quiet"] | false;
  if (!x->demo) x->now = now_of(p);  // the demo keeps its own clock
  load(*x, p);
  show(root, *x, true);
  return true;
}
