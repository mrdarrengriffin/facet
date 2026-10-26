// Lightning radar (docs: Components > Radar > Lightning radar, @facet/ui radar.ts lightningRadar(), CSS 40-radar.css):
// strikes by distance and bearing round home, coloured by age; the nearest one of the last 15 minutes gets a ring, a
// new one flashes. Data = core/lightning.h (fw::lx; the workbench feeds the docs demo storm).
//   box     size x size (prop, dp: 256 = lay::radar 384 / 256), on the canvas
//   rings   10 / 25 / 50 / 100 km at R x sqrt(km / 100) on the grid (docs square-root scale: 64 / 96 / 136 / 192 at
//           384), 1 px line-hi; the 25 km ring = the alert ring, stroke::tick accent-dim. Strikes sit piecewise-linearly
//           between the drawn rings, so a dot always agrees with them (docs rPx).
//   ticks   every 30 degrees, the core tick ring (fw::tick_ring, the Dial's lengths): minor line-hi, N / E / S / W steel
//   labels  each ring's distance just right of the south axis, sp::s2 above its ring (type label, text-dim; the outer
//           one carries the unit), "N" under the north tick. unit = Settings > Units > Distance (fw::units): miles
//           relabels the same rings (6 / 16 / 31 / 62 mi)
//   home    lay::radar_home blue ring (stroke::edge) on the canvas with a lay::radar_home_dot blue dot
//   strikes lay::radar_strike dots, all drawn in one draw event (no object each): < 1 min on-accent with an accent edge (an accent ring),
//           < 5 min accent, < 15 min accent at radar.faded, older steel at radar.old; newest on top
//   nearest lay::radar_near accent-text ring round the nearest strike of the last 15 minutes
// Demo (prop demo, default on: the workbench page): the docs storm plays at DEMO_SPEED, so strikes arrive, age, flash
// and the nearest ring moves (meta "motion": tools/visual.mjs checks it shows). Variants pin a time (demo off).
// Update path: new strikes in place (one invalidate of the strike layer, the nearest ring moves); a strike under 90 s
// old that was not shown before ripples (docs: a stroke::edge accent ring grows from the dot to lay::radar_flash, its stroke with it, and fades:
// Motion ripple 600 ms ease-out; at most 6 at once; reduced motion: none, the newest dot's look says it). Silent. Static otherwise.
// Contrast: tokens/colour.json section "radar".
#include <set>
#include "fw.h"
#include "lightning.h"

namespace {
constexpr int RING_KM[4] = {10, 25, 50, 100};
struct LX {
  int S = 0, R = 0;
  int ring_px[4] = {};
  fw::lx::Data d;
  double now = 0;
  std::set<uint32_t> seen;
  lv_obj_t *dots = nullptr, *near = nullptr, *fx = nullptr;
  lv_obj_t *labels[4] = {};
  fw::units::Distance unit = fw::units::Distance::km;
  lv_timer_t *demo = nullptr;  // the workbench demo: storm time runs at DEMO_SPEED
  bool quiet = false;
};
// the demo plays the storm 20x faster than life: one Motion clock-tick (1 s) = 20 s of storm, so a strike arrives about
// every 2 s and ages through its colours in 3 / 15 / 45 s (a real feed ages them in 1 / 5 / 15 minutes)
constexpr double DEMO_SPEED = 20;
LX *of(lv_obj_t *o) { return (LX *) lv_obj_get_user_data(o); }

double now_of(fw::Props p) {
  const double fixed = p["now"] | 0.0;
  return fixed > 0 ? fixed : (double) time(nullptr) * 1000;
}
// px from the centre for d km: piecewise-linear between the drawn rings (docs rPx)
float r_px(const LX &x, float d) {
  float pd = 0, pr = 0;
  for (int i = 0; i < 4; i++) {
    if (d <= RING_KM[i]) return pr + (d - pd) / (RING_KM[i] - pd) * (x.ring_px[i] - pr);
    pd = (float) RING_KM[i];
    pr = (float) x.ring_px[i];
  }
  return (float) x.R;
}
// centre of a mark `size` px across: near the outer ring it is pulled in so the whole mark stays inside the tick ring
// (a sp::s1 gap inside the major ticks' length), so a strike never sits on a compass mark
void pos(const LX &x, float d, float az, float *px, float *py, int size = fw::lay::radar_strike) {
  const float rmax = (float) (x.R - fw::lay::dial_tick_major - fw::sp::s1) - size / 2.0f;
  const float r = std::min(r_px(x, d), rmax), a = az * 3.14159265f / 180;
  *px = x.R + r * sinf(a);
  *py = x.R - r * cosf(a);
}
lv_obj_t *box(lv_obj_t *parent) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  return o;
}
lv_obj_t *ring(lv_obj_t *parent, int cx, int cy, int r, int w, lv_color_t c) {
  lv_obj_t *o = box(parent);
  lv_obj_set_size(o, 2 * r, 2 * r);
  lv_obj_set_pos(o, cx - r, cy - r);
  lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_border_width(o, w, 0);
  lv_obj_set_style_border_color(o, c, 0);
  return o;
}

void draw_dots(lv_event_t *e) {
  auto *o = (lv_obj_t *) lv_event_get_current_target(e);
  const LX &x = *(const LX *) lv_event_get_user_data(e);
  const fw::Theme &t = *fw::T;
  lv_layer_t *layer = lv_event_get_layer(e);
  lv_area_t a;
  lv_obj_get_coords(o, &a);
  const int D = fw::lay::radar_strike;
  for (size_t i = x.d.strikes.size(); i-- > 0;) {  // oldest first, so the newest paint on top
    const fw::lx::Strike &s = x.d.strikes[i];
    float px, py;
    pos(x, s.d, s.az, &px, &py);
    lv_area_t r;
    r.x1 = a.x1 + (int) lroundf(px - D / 2.0f);
    r.y1 = a.y1 + (int) lroundf(py - D / 2.0f);
    r.x2 = r.x1 + D - 1;
    r.y2 = r.y1 + D - 1;
    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    dsc.radius = LV_RADIUS_CIRCLE;
    switch (fw::lx::age_class(s.t, x.now)) {
      case 0:
        dsc.bg_color = t.on_accent;
        dsc.border_color = t.accent;
        dsc.border_width = fw::stroke::edge;
        break;
      case 1: dsc.bg_color = t.accent; break;
      case 2: dsc.bg_color = t.accent; dsc.bg_opa = (lv_opa_t) lroundf(255 * fw::tokens::comp::radar_faded); break;
      default: dsc.bg_color = t.steel; dsc.bg_opa = (lv_opa_t) lroundf(255 * fw::tokens::comp::radar_old); break;
    }
    lv_draw_rect(layer, &dsc, &r);
  }
}

std::string ring_label(int i, fw::units::Distance u) {
  const long v = std::lround(fw::units::from_km(RING_KM[i], u));
  char b[16];
  if (i == 3) snprintf(b, sizeof b, "%ld %s", v, u == fw::units::Distance::mi ? "mi" : "km");
  else snprintf(b, sizeof b, "%ld", v);
  return b;
}

// one ripple (docs radar.ts flash: an 8 px ring with a 2 px accent stroke scaled 1 -> 6 while its opacity goes 1 -> 0,
// 600 ms ease-out = Motion ripple): v 0..1000 grows the ring from the dot to lay::radar_flash, its stroke with it, and
// fades it. A small object (radar_flash square at most), so only that box redraws.
void flash_exec(void *o, int32_t v) {
  auto *f = (lv_obj_t *) o;
  const int cx = (int) (intptr_t) lv_obj_get_user_data(f) >> 16, cy = (int) (intptr_t) lv_obj_get_user_data(f) & 0xffff;
  const int d0 = fw::lay::radar_strike, d1 = fw::lay::radar_flash;
  const int d = d0 + (d1 - d0) * v / 1000;
  lv_obj_set_size(f, d, d);
  lv_obj_set_pos(f, cx - d / 2, cy - d / 2);
  lv_obj_set_style_border_width(f, std::max(1, fw::stroke::edge * d / d0), 0);
  lv_obj_set_style_opa(f, (lv_opa_t) (255 - 255 * v / 1000), 0);
}
void flash(LX &x, const fw::lx::Strike &s) {
  float px, py;
  pos(x, s.d, s.az, &px, &py);
  const int cx = (int) lroundf(px), cy = (int) lroundf(py);
  lv_obj_t *f = ring(x.fx, cx, cy, fw::lay::radar_strike / 2, fw::stroke::edge, fw::T->accent);
  lv_obj_set_user_data(f, (void *) (intptr_t) ((cx << 16) | (cy & 0xffff)));
  fw::qa::free(f, "a strike's flash: placed by its distance and bearing");
  fw::Anim a{flash_exec, 0, 1000, fw::mo::OPA};
  a.done = [](lv_anim_t *an) { lv_obj_delete_async((lv_obj_t *) an->var); };
  fw::animate(f, fw::mo::ripple, a);
}

void show(LX &x, bool animate) {
  if (x.d.has_nearest) {
    float px, py;
    pos(x, x.d.nearest.d, x.d.nearest.az, &px, &py, fw::lay::radar_near);
    const int N = fw::lay::radar_near;
    lv_obj_set_pos(x.near, (int) lroundf(px) - N / 2, (int) lroundf(py) - N / 2);
    lv_obj_remove_flag(x.near, LV_OBJ_FLAG_HIDDEN);
  } else lv_obj_add_flag(x.near, LV_OBJ_FLAG_HIDDEN);
  lv_obj_invalidate(x.dots);
  int fresh = 0;
  const bool can_flash = animate && !fw::mo::reduced(x.dots);
  for (size_t i = x.d.strikes.size(); i-- > 0;) {
    const fw::lx::Strike &s = x.d.strikes[i];
    if (!x.seen.insert(s.id).second) continue;
    if (can_flash && x.now - s.t < 90000 && fresh < 6) { flash(x, s); fresh++; }
  }
  if (x.seen.size() > 4000) {
    x.seen.clear();
    for (const fw::lx::Strike &s : x.d.strikes) x.seen.insert(s.id);
  }
}
}  // namespace

FW_COMPONENT(lightning_radar, "Lightning radar", "components", R"({
  "about": "Strikes by distance (square-root scale) and bearing round home, coloured by age; the nearest one of the last 15 minutes gets a ring. New ones flash.",
  "frame": [336, 336],
  "motion_off": "demo",
  "motion": "demo: strikes arrive (about every 2 s), age through their colours and flash; the nearest ring moves",
  "props": [
    {"name": "demo", "about": "For previews only: plays a made-up storm so strikes appear.", "type": "bool", "default": true},
    {"name": "quiet", "about": "For previews only: shows a quiet sky with no recent strikes.", "type": "bool", "default": false},
    {"name": "size", "about": "How wide the radar is across, in pixels.", "type": "range", "min": 192, "max": 320, "step": 32, "default": 256},
    {"name": "unit", "about": "Whether distances are shown in kilometres or miles.", "type": "select", "options": ["km", "mi"], "default": "km"}
  ],
  "variants": [
    {"label": "a storm passing", "props": {"demo": false, "now": 1792944000000}},
    {"label": "it has passed", "props": {"demo": false, "now": 1792946520000}},
    {"label": "miles", "props": {"demo": false, "now": 1792944000000, "unit": "mi"}},
    {"label": "quiet sky", "props": {"demo": false, "now": 1792944000000, "quiet": true}},
    {"label": "small", "props": {"demo": false, "now": 1792944000000, "size": 192}}
  ]
})") {
  const fw::Theme &t = *fw::T;
  auto *x = new LX;
  x->S = fw::lay::radar * (int) (p["size"] | 256) / 256 / 4 * 4;
  x->R = x->S / 2;
  for (int i = 0; i < 4; i++) x->ring_px[i] = i == 3 ? x->R : (int) std::lround(x->R * std::sqrt(RING_KM[i] / 100.0) / 4) * 4;
  x->unit = fw::units::distance_of(p["unit"] | "km");
  x->now = now_of(p);
  // the demo starts with the storm over the area (the cell crosses once an hour; at 20x it passes in about 3 minutes)
  if (p["demo"] | true) x->now = std::floor(x->now / 3600000.0) * 3600000.0 + 0.3 * 3600000.0;
  x->quiet = p["quiet"] | false;
  x->d = fw::lx::sample(x->now, x->quiet);
  lv_obj_t *root = box(parent);
  lv_obj_set_size(root, x->S, x->S);
  lv_obj_set_user_data(root, x);
  lv_obj_add_event_cb(root, [](lv_event_t *e) {
    auto *x = (LX *) lv_event_get_user_data(e);
    if (x->demo) lv_timer_delete(x->demo);
    delete x;
  }, LV_EVENT_DELETE, x);
  const int R = x->R;
  for (int i = 0; i < 4; i++) {
    const bool alert = RING_KM[i] == 25;
    ring(root, R, R, x->ring_px[i], alert ? fw::stroke::tick : fw::stroke::hair, alert ? t.accent_dim : t.line_hi);
  }
  fw::TickRing tr;
  tr.lo = 0;
  tr.hi = 12;
  tr.minor = 1;
  tr.major = 3;
  tr.start = 270.0f;
  tr.range = 360.0f;
  tr.minor_len = fw::lay::dial_tick;
  tr.major_len = fw::lay::dial_tick_major;
  tr.width = fw::stroke::tick;
  tr.minor_c = t.line_hi;
  tr.major_c = t.steel;
  tr.lit_c = t.accent;
  tr.num_c = t.text_dim;
  lv_obj_t *ticks = fw::tick_ring(root, x->S, tr);
  lv_obj_set_pos(ticks, 0, 0);
  const int box_h = fw::style(fw::ty::label).box;
  for (int i = 0; i < 4; i++) {
    x->labels[i] = fw::text(root, fw::ty::label, ring_label(i, x->unit).c_str(), t.text_dim);
    lv_obj_set_pos(x->labels[i], R + fw::sp::s2, R + x->ring_px[i] - fw::sp::s2 - box_h);
  }
  lv_obj_t *n = fw::text(root, fw::ty::label, "N", t.text_dim);
  lv_obj_set_width(n, 2 * fw::sp::s7);  // a centred box, so "N" sits on the north axis
  lv_obj_set_style_text_align(n, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_pos(n, R - fw::sp::s7, fw::lay::dial_tick_major + fw::sp::s2);
  x->dots = box(root);
  lv_obj_set_size(x->dots, x->S, x->S);
  lv_obj_add_event_cb(x->dots, draw_dots, LV_EVENT_DRAW_MAIN, x);
  const int N = fw::lay::radar_near;
  x->near = ring(root, 0, 0, N / 2, fw::stroke::hair, t.accent_text);
  fw::qa::free(x->near, "the nearest strike's ring: placed by its distance and bearing");
  x->fx = box(root);
  lv_obj_set_size(x->fx, x->S, x->S);
  fw::qa::clips(x->fx, "a ripple from a strike near the edge stops at the radar's square");
  const int H = fw::lay::radar_home, h0 = fw::lay::radar_home_dot;
  lv_obj_t *home = ring(root, R, R, H / 2, fw::stroke::edge, t.blue);
  lv_obj_set_style_bg_color(home, t.bg, 0);
  lv_obj_set_style_bg_opa(home, LV_OPA_COVER, 0);
  lv_obj_t *dot = box(home);
  lv_obj_set_size(dot, h0, h0);
  lv_obj_center(dot);
  lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(dot, t.blue, 0);
  lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
  show(*x, false);
  if (p["demo"] | true) {  // strikes arrive, age, flash and fade while the page is open
    x->demo = lv_timer_create([](lv_timer_t *tm) {
      auto *x = (LX *) lv_timer_get_user_data(tm);
      x->now += fw::mo::ms::clock_tick * DEMO_SPEED;
      x->d = fw::lx::sample(x->now, x->quiet);
      show(*x, true);
    }, fw::mo::ms::clock_tick, x);
  }
}

FW_UPDATE(lightning_radar) {
  LX *x = of(root);
  if (!x) return false;
  const int S = fw::lay::radar * (int) (p["size"] | 256) / 256 / 4 * 4;
  if (S != x->S || (bool) (p["demo"] | true) != (x->demo != nullptr)) return false;  // new rings / demo on-off: rebuild
  const fw::units::Distance u = fw::units::distance_of(p["unit"] | "km");
  if (u != x->unit) {
    x->unit = u;
    for (int i = 0; i < 4; i++) fw::set_text(x->labels[i], fw::ty::label, ring_label(i, u).c_str());
  }
  x->quiet = p["quiet"] | false;
  if (!x->demo) x->now = now_of(p);  // the demo keeps its own clock
  x->d = fw::lx::sample(x->now, x->quiet);
  show(*x, true);
  return true;
}
