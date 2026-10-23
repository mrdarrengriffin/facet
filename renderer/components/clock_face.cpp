// Clock faces: the Facet clock family (docs: Components > Clock > Clock faces, @facet/ui clocks.ts clockHero / clockRing /
// clockAnalogue / clockCompact): whole-screen faces built from the real Time and Date line components (fw::find) and the
// core tick ring (fw::tick_ring), so a face is the same parts as everywhere else.
//   hero      Time (hero) over the Date line and a seconds strip, a column centred on the screen, sp::s8 apart (docs
//             238 / 446 / 500 at 720): the strip = lay::clock_secbar x stroke::edge, line track, accent fill growing
//             over each minute
//   ring      60 dots (fw::tick_ring dots, stroke::arc, line-hi) on lay::clock_ring, 12 hour ticks inside on
//             lay::clock_ring_inner (lay::clock_tick text-mute, every 3rd lay::clock_tick_major text-dim), an accent dot
//             lay::clock_orbit_dot orbiting on the dots as the seconds; Time (mid) centred on the screen, the Date line
//             sp::s7 under it
//   analogue  60 ticks on lay::clock_dial (minor lay::dial_tick hair line-hi, every 5th lay::dial_tick_major text-dim),
//             numerals 12 3 6 9 (clock-numeral, text-dim) at lay::clock_numeral_r, the Date line lay::clock_date_y under
//             the centre, hands: hour lay::clock_hand_hour stroke::arc text, minute lay::clock_hand_min stroke::edge text
//             (both step to a new minute with Motion clock-hand), seconds lay::clock_hand_sec stroke::edge accent with
//             a lay::clock_hand_tail tail and a lay::clock_hand_tip dot, sweeping; centre cap lay::clock_cap, canvas
//             with a stroke::edge accent ring. Hands drawn in one draw event, each move repaints only its old + new box
//   compact   the compact Time alone (docs clockCompact: a header clock)
//   orbit     the Clock screen's own face (docs screens/clock.ts orbitFace), sized to the content box (a screen's
//             body under its header): a tick ring the content height (120 ticks, stroke::tick dots line-hi, every
//             10th lay::clock_orbit_tick steel), three dots orbiting inside it at different speeds (accent
//             lay::clock_orbit_dot one turn a minute with the seconds; blue and steel lay::clock_orbit_dot_sm,
//             lay::clock_orbit_step and 2x further in, Motion orbit-b reversed / orbit-c), and a column centred on the
//             ring: Time (mid), the Date line, the weather line (c-weather, text-dim: "rain \xB7 13\xB0", props
//             condition = an HA weather state, temp = its temperature, both bindable; empty = no line). Reduced
//             motion: the dots stay where they are (the seconds dot steps once a second).
//   seconds   prop (Darren 2026-10-22): the Time's seconds on hero and compact (ring and analogue show them their way)
//   context   (docs) not here: it needs the weather, calendar and timer feeds (a screen job, with the Clock screen)
// Time: prop "time" = a fixed "HH:MM[:SS]" (tests, the audit) that then runs on; empty = live (localtime). The seconds
// sweep one turn a minute (Motion token clock-sweep, linear) from the right second; reduced motion = they step once a
// second (nothing sweeps). Static to touch. Update path: font / format / date / time move in place (the children's own
// updates); another face refills. Contrast pairs: colour.json section "clock".
#include <ctime>
#include "fw.h"
#include "weather.h"

namespace {
enum Face { HERO, RING, ANALOGUE, COMPACT, ORBIT };
struct Clock {
  Face face = RING;
  long base_ms = 0;          // time of day at t0 (ms)
  uint32_t t0 = 0;
  int shown_min = -1;
  float hour_deg = 0, min_deg = 0, sec_deg = 0;  // shown angles (cumulative for hour / minute)
  float hour_from = 0, min_from = 0, hour_to = 0, min_to = 0;
  lv_obj_t *time = nullptr, *date = nullptr, *orbit = nullptr, *hands = nullptr, *bar = nullptr, *fillb = nullptr;
  lv_obj_t *wx = nullptr, *orb[3] = {};  // orbit face: the weather line, the three dots
  int orb_r[3] = {}, oc = 0;              // their radii, the ring centre (root px)
  lv_timer_t *timer = nullptr;
  std::string font, format, datestyle, fixed;
  bool seconds = false;
};
Clock *of(lv_obj_t *o) { return (Clock *) lv_obj_get_user_data(o); }
Face face_of(fw::Props p) {
  const char *f = p["face"] | "ring";
  return !strcmp(f, "hero") ? HERO : !strcmp(f, "analogue") ? ANALOGUE : !strcmp(f, "compact") ? COMPACT : !strcmp(f, "orbit") ? ORBIT : RING;
}
long now_ms(const Clock *d) { return d->base_ms + (long) lv_tick_elaps(d->t0); }
long day_ms(fw::Props p) {
  const char *t = p["time"] | "";
  if (*t) { int h = 10, m = 8, s = 30; sscanf(t, "%d:%d:%d", &h, &m, &s); return ((h * 60L + m) * 60 + s) * 1000; }
  const time_t now = time(nullptr);
  struct tm tm;
  localtime_r(&now, &tm);
  return ((tm.tm_hour * 60L + tm.tm_min) * 60 + tm.tm_sec) * 1000;
}

lv_obj_t *box(lv_obj_t *parent) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  return o;
}
lv_obj_t *child(lv_obj_t *parent, const char *id, ArduinoJson::JsonDocument &cp) {
  const fw::Component *c = fw::find(id);
  if (!c) return nullptr;
  const uint32_t n = lv_obj_get_child_count(parent);
  c->build(parent, cp.as<fw::Props>());
  return lv_obj_get_child_count(parent) > n ? lv_obj_get_child(parent, -1) : nullptr;
}
std::string hhmm(long ms) {
  char b[12];
  snprintf(b, sizeof b, "%02ld:%02ld:%02ld", (ms / 3600000) % 24, (ms / 60000) % 60, (ms / 1000) % 60);
  return b;
}
void time_props(const Clock *d, const char *size, ArduinoJson::JsonDocument &cp) {
  cp["size"] = size;
  cp["font"] = d->font;
  cp["format"] = d->format;
  cp["time"] = d->fixed.empty() ? "" : hhmm(now_ms(d));  // a fixed time runs on (the Time child shows it); live = its own
  // seconds on the faces that have no seconds of their own (hero has a strip, but digits were asked for; compact)
  cp["seconds"] = d->seconds && (d->face == HERO || d->face == COMPACT);
}
void date_props(const Clock *d, ArduinoJson::JsonDocument &cp, const char *date) {
  cp["style"] = d->datestyle;
  cp["date"] = date;
}

// ---- analogue hands (one draw event, each a line from the centre)
struct Hand { float deg, len, tail; int w; lv_color_t c; };
void hand_line(lv_layer_t *l, float cx, float cy, const Hand &h) {
  const fw::StrokePt a = fw::polar(cx, cy, -h.tail, h.deg), b = fw::polar(cx, cy, h.len, h.deg);
  lv_draw_line_dsc_t ld;
  lv_draw_line_dsc_init(&ld);
  ld.color = h.c;
  ld.width = h.w;
  ld.round_start = ld.round_end = 1;
  ld.p1.x = (int32_t) std::lround(a.x); ld.p1.y = (int32_t) std::lround(a.y);  // the pixel edge grid (atoms.cpp ring_draw)
  ld.p2.x = (int32_t) std::lround(b.x); ld.p2.y = (int32_t) std::lround(b.y);
  lv_draw_line(l, &ld);
}
void circle(lv_layer_t *l, float cx, float cy, int d, lv_color_t bg, int border, lv_color_t bc) {
  lv_draw_rect_dsc_t r;
  lv_draw_rect_dsc_init(&r);
  r.radius = LV_RADIUS_CIRCLE;
  r.bg_color = bg;
  r.border_width = border;
  r.border_color = bc;
  const int32_t x = (int32_t) std::lround(cx - d / 2.0f), y = (int32_t) std::lround(cy - d / 2.0f);
  lv_area_t a = {x, y, x + d - 1, y + d - 1};
  lv_draw_rect(l, &r, &a);
}
Hand hands_of(const Clock *d, int k) {  // 0 hour, 1 minute, 2 seconds
  const fw::Theme &t = *fw::T;
  if (k == 0) return {270 + d->hour_deg, (float) fw::lay::clock_hand_hour, 0, fw::stroke::arc, t.text};
  if (k == 1) return {270 + d->min_deg, (float) fw::lay::clock_hand_min, 0, fw::stroke::edge, t.text};
  return {270 + d->sec_deg, (float) fw::lay::clock_hand_sec, (float) fw::lay::clock_hand_tail, fw::stroke::edge, t.accent};
}
void hands_draw(lv_event_t *e) {
  auto *o = (lv_obj_t *) lv_event_get_current_target(e);
  const Clock *d = of((lv_obj_t *) lv_event_get_user_data(e));
  lv_layer_t *l = lv_event_get_layer(e);
  lv_area_t a;
  lv_obj_get_coords(o, &a);
  const float cx = (a.x1 + a.x2 + 1) / 2.0f, cy = (a.y1 + a.y2 + 1) / 2.0f;
  for (int k = 0; k < 3; k++) hand_line(l, cx, cy, hands_of(d, k));
  const Hand s = hands_of(d, 2);
  const fw::StrokePt tip = fw::polar(cx, cy, s.len, s.deg);
  circle(l, tip.x, tip.y, fw::lay::clock_hand_tip, fw::T->accent, 0, fw::T->accent);
  circle(l, cx, cy, fw::lay::clock_cap, fw::T->bg, fw::stroke::edge, fw::T->accent);
}
void hand_dirty(lv_obj_t *root, int k) {  // the box of hand k as it is now
  const Clock *d = of(root);
  if (!d->hands) return;
  lv_area_t a;
  lv_obj_get_coords(d->hands, &a);
  const float cx = (a.x1 + a.x2 + 1) / 2.0f, cy = (a.y1 + a.y2 + 1) / 2.0f;
  const Hand h = hands_of(d, k);
  const fw::StrokePt p = fw::polar(cx, cy, -h.tail, h.deg), q = fw::polar(cx, cy, h.len, h.deg);
  const int m = std::max(h.w, k == 2 ? fw::lay::clock_hand_tip : 0) / 2 + 2;
  lv_area_t r = {(int32_t) std::floor(std::min(p.x, q.x)) - m, (int32_t) std::floor(std::min(p.y, q.y)) - m,
                 (int32_t) std::ceil(std::max(p.x, q.x)) + m, (int32_t) std::ceil(std::max(p.y, q.y)) + m};
  lv_obj_invalidate_area(d->hands, &r);
  const int c = fw::lay::clock_cap / 2 + 2;  // the cap sits over every hand
  lv_area_t ca = {(int32_t) cx - c, (int32_t) cy - c, (int32_t) cx + c, (int32_t) cy + c};
  lv_obj_invalidate_area(d->hands, &ca);
}

// ---- orbit face: a dot k at an angle on its circle round the ring centre
void orbit_place(Clock *d, int k, float deg) {
  const int M = (int) lv_obj_get_style_width(d->orb[k], LV_PART_MAIN);
  const fw::StrokePt c = fw::polar((float) d->oc, (float) d->oc, (float) d->orb_r[k], deg);
  const int32_t x = (int32_t) std::lround(c.x - M / 2.0f), y = (int32_t) std::lround(c.y - M / 2.0f);
  if (x != lv_obj_get_x(d->orb[k]) || y != lv_obj_get_y(d->orb[k])) lv_obj_set_pos(d->orb[k], x, y);
}
void orbit_b(void *o, int32_t v) { auto *d = of((lv_obj_t *) o); orbit_place(d, 1, 270 - v / 100.0f); }  // reversed
void orbit_c(void *o, int32_t v) { auto *d = of((lv_obj_t *) o); orbit_place(d, 2, 270 + v / 100.0f); }
void orbit_spin(lv_obj_t *root, lv_anim_exec_xcb_t f, uint32_t ms) {
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, root);
  lv_anim_set_exec_cb(&a, f);
  lv_anim_set_values(&a, 0, 36000);
  lv_anim_set_duration(&a, ms);
  lv_anim_set_path_cb(&a, fw::mo::EASINGS[fw::mo::E_linear].path);
  lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
  lv_anim_start(&a);
}
std::string wx_line(fw::Props p) {
  const char *t = p["temp"] | "";
  return fw::wx::line(p["condition"] | "", *t ? strtod(t, nullptr) : NAN);
}

// ---- the seconds (orbit dot / seconds hand / strip): one angle from the time
void seconds(void *o, int32_t) {
  auto *root = (lv_obj_t *) o;
  auto *d = of(root);
  const long ms = now_ms(d);
  const bool step = fw::mo::reduced(root);
  const float deg = step ? (ms / 1000 % 60) * 6.0f : (ms % 60000) * 360.0f / 60000.0f;
  if (deg == d->sec_deg && d->shown_min >= 0) return;
  if (d->hands) hand_dirty(root, 2);
  d->sec_deg = deg;
  if (d->hands) hand_dirty(root, 2);
  if (d->orbit) {
    const int R = fw::lay::clock_ring / 2 - fw::stroke::arc / 2, M = fw::lay::clock_orbit_dot, C = fw::lay::screen_w / 2;
    const fw::StrokePt c = fw::polar((float) C, (float) C, (float) R, 270 + deg);
    const int32_t x = (int32_t) std::lround(c.x - M / 2.0f), y = (int32_t) std::lround(c.y - M / 2.0f);
    if (x != lv_obj_get_x(d->orbit) || y != lv_obj_get_y(d->orbit)) lv_obj_set_pos(d->orbit, x, y);
  }
  if (d->orb[0]) orbit_place(d, 0, 270 + deg);
  if (d->fillb) {
    const int w = (int) std::lround(fw::lay::clock_secbar * (ms % 60000) / 60000.0);
    if (w != lv_obj_get_width(d->fillb)) lv_obj_set_width(d->fillb, w);
  }
}
void hands_exec(void *o, int32_t k) {  // hour + minute stepping to a new minute (Motion clock-hand)
  auto *root = (lv_obj_t *) o;
  auto *d = of(root);
  hand_dirty(root, 0), hand_dirty(root, 1);
  d->hour_deg = d->hour_from + (d->hour_to - d->hour_from) * k / 1024.0f;
  d->min_deg = d->min_from + (d->min_to - d->min_from) * k / 1024.0f;
  hand_dirty(root, 0), hand_dirty(root, 1);
}
void minute(lv_obj_t *root, bool animate) {
  auto *d = of(root);
  const long ms = now_ms(d);
  const int mins = (int) (ms / 60000 % (24 * 60));
  if (mins == d->shown_min) return;
  const bool first = d->shown_min < 0;
  d->shown_min = mins;
  if (d->hands) {
    float mt = (mins % 60) * 6.0f + (ms % 60000) / 10000.0f, ht = (mins / 60 % 12) * 30.0f + (mins % 60) * 0.5f;
    while (mt < d->min_deg - 180) mt += 360;  // cumulative: 59 -> 0 keeps turning clockwise (docs)
    while (ht < d->hour_deg - 180) ht += 360;
    d->min_from = d->min_deg, d->hour_from = d->hour_deg, d->min_to = mt, d->hour_to = ht;
    if (first || !animate) hands_exec(root, 1024);
    else fw::animate(root, fw::mo::clock_hand, {hands_exec, 0, 1024, fw::mo::MOVE});
  }
  if (!first && d->time && !d->fixed.empty()) {  // a fixed time runs on: the Time child shows it
    ArduinoJson::JsonDocument cp;
    time_props(d, d->face == HERO ? "hero" : d->face == COMPACT ? "compact" : "mid", cp);
    fw::update_child(fw::find("time"), d->time, cp.as<fw::Props>());
  }
}
void start(lv_obj_t *root) {
  auto *d = of(root);
  lv_anim_delete(root, (lv_anim_exec_xcb_t) seconds);
  if (d->face == COMPACT) return;
  // the seconds: every frame while sweeping (a clock: one turn per minute, Motion token clock-sweep), from the time
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, root);
  lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t) seconds);
  lv_anim_set_values(&a, 0, 3600);
  lv_anim_set_duration(&a, fw::mo::ms::clock_sweep);
  lv_anim_set_path_cb(&a, fw::mo::EASINGS[fw::mo::E_linear].path);
  lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
  lv_anim_start(&a);
}

void fill(lv_obj_t *root, fw::Props p) {
  const fw::Theme &t = *fw::T;
  auto *d = of(root);
  d->face = face_of(p);
  d->font = p["font"] | "mono";
  d->format = p["format"] | "24h";
  d->datestyle = p["date"] | "short";
  d->fixed = p["time"] | "";
  d->seconds = p["seconds"] | false;
  d->base_ms = day_ms(p);
  d->t0 = lv_tick_get();
  d->shown_min = -1;
  d->time = d->date = d->orbit = d->hands = d->bar = d->fillb = d->wx = nullptr;
  d->orb[0] = d->orb[1] = d->orb[2] = nullptr;
  d->hour_deg = d->min_deg = d->sec_deg = 0;
  const int S = fw::lay::screen_w, C = S / 2;
  lv_obj_remove_style_all(root);
  lv_obj_remove_flag(root, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  if (d->face == ORBIT) lv_obj_set_size(root, fw::lay::content_w, fw::lay::content_h);  // a screen body
  else lv_obj_set_size(root, S, S);
  const char *date = p["day"] | "";
  ArduinoJson::JsonDocument tp, dp;
  date_props(d, dp, date);
  if (d->face == HERO) {
    lv_obj_t *col = box(root);
    lv_obj_set_size(col, S, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(col, fw::sp::s8, 0);
    time_props(d, "hero", tp);
    d->time = child(col, "time", tp);
    d->date = child(col, "date_line", dp);
    lv_obj_t *bar = d->bar = box(col);  // the seconds strip: track + fill
    lv_obj_set_size(bar, fw::lay::clock_secbar, fw::stroke::edge);
    lv_obj_set_style_bg_color(bar, t.line, 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    d->fillb = box(bar);
    lv_obj_set_size(d->fillb, 0, fw::stroke::edge);
    lv_obj_set_style_bg_color(d->fillb, t.accent, 0);
    lv_obj_set_style_bg_opa(d->fillb, LV_OPA_COVER, 0);
    fw::qa::free(d->fillb, "the seconds strip fills over each minute");
    lv_obj_center(col);
  } else if (d->face == RING) {
    fw::TickRing dots;  // 60 dots, a full turn
    dots.lo = 0, dots.hi = 60, dots.minor = 1, dots.major = 0, dots.start = 270, dots.range = 360;
    dots.dots = true, dots.width = fw::stroke::arc, dots.minor_c = dots.major_c = t.line_hi;
    lv_obj_center(fw::tick_ring(root, fw::lay::clock_ring, dots));
    fw::TickRing ticks;  // 12 hours, every 3rd a quarter
    ticks.lo = 0, ticks.hi = 12, ticks.minor = 1, ticks.major = 3, ticks.start = 270, ticks.range = 360;
    ticks.width = fw::stroke::tick, ticks.minor_len = fw::lay::clock_tick, ticks.major_len = fw::lay::clock_tick_major;
    ticks.minor_c = t.text_mute, ticks.major_c = t.text_dim;
    lv_obj_center(fw::tick_ring(root, fw::lay::clock_ring_inner, ticks));
    time_props(d, "mid", tp);
    d->time = child(root, "time", tp);
    lv_obj_align(d->time, LV_ALIGN_CENTER, 0, 0);  // the digits on the centre
    d->date = child(root, "date_line", dp);
    lv_obj_update_layout(root);  // the Time's size before the date is placed under it
    lv_obj_align_to(d->date, d->time, LV_ALIGN_OUT_BOTTOM_MID, 0, fw::sp::s7);
    lv_obj_t *dot = d->orbit = box(root);
    lv_obj_set_size(dot, fw::lay::clock_orbit_dot, fw::lay::clock_orbit_dot);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(dot, t.accent, 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    fw::qa::free(dot, "the seconds dot orbits by angle");
    fw::qa::circle(dot, root, C, C, fw::lay::clock_ring / 2 - fw::stroke::arc / 2);
  } else if (d->face == ANALOGUE) {
    fw::TickRing scale;  // 60 ticks, every 5th longer
    scale.lo = 0, scale.hi = 60, scale.minor = 1, scale.major = 5, scale.start = 270, scale.range = 360;
    scale.width = fw::stroke::hair, scale.minor_len = fw::lay::dial_tick, scale.major_len = fw::lay::dial_tick_major;
    scale.minor_c = t.line_hi, scale.major_c = t.text_dim;
    lv_obj_t *sc = fw::tick_ring(root, fw::lay::clock_dial, scale);
    lv_obj_center(sc);
    const bool mono = d->font != "inter";
    const fw::ty::Id ns = mono ? fw::ty::clock_numeral_mono : fw::ty::clock_numeral;
    const char *NUM[4] = {"12", "3", "6", "9"};
    for (int k = 0; k < 4; k++) {  // numerals centred on their point (cap height centred: the text-box rule)
      lv_obj_t *l = fw::text(root, ns, NUM[k], t.text_dim);
      lv_obj_set_height(l, fw::style(ns).box);
      lv_obj_update_layout(l);
      const fw::StrokePt c = fw::polar((float) C, (float) C, (float) fw::lay::clock_numeral_r, 270.0f + k * 90.0f);
      lv_obj_set_pos(l, (int32_t) std::lround(c.x - lv_obj_get_width(l) / 2.0f), (int32_t) std::lround(c.y - fw::style(ns).box / 2.0f));
    }
    d->date = child(root, "date_line", dp);
    lv_obj_align(d->date, LV_ALIGN_TOP_MID, 0, C + fw::lay::clock_date_y);
    d->hands = box(root);
    lv_obj_set_size(d->hands, S, S);
    lv_obj_add_event_cb(d->hands, hands_draw, LV_EVENT_DRAW_MAIN, root);
  } else if (d->face == ORBIT) {
    const int W = fw::lay::content_w, H = fw::lay::content_h, RG = std::min(W, H) / 8 * 8, R = RG / 2;
    fw::TickRing ring;  // docs tickRing 120 / every 10th: dots, majors short ticks
    ring.lo = 0, ring.hi = 120, ring.minor = 1, ring.major = 10, ring.start = 270, ring.range = 360;
    ring.width = fw::stroke::tick, ring.minor_len = fw::stroke::tick, ring.major_len = fw::lay::clock_orbit_tick;
    ring.minor_c = t.line_hi, ring.major_c = t.steel;
    lv_obj_t *rg = fw::tick_ring(root, RG, ring);
    lv_obj_set_pos(rg, (W - RG) / 2, (H - RG) / 2);
    d->oc = R;  // the dots live in a ring-sized box so their centre is the ring's
    lv_obj_t *ob = box(root);
    lv_obj_set_size(ob, RG, RG);
    lv_obj_set_pos(ob, (W - RG) / 2, (H - RG) / 2);
    const int sz[3] = {fw::lay::clock_orbit_dot, fw::lay::clock_orbit_dot_sm, fw::lay::clock_orbit_dot_sm};
    const lv_color_t ink[3] = {t.accent, t.blue, t.steel};
    for (int k = 0; k < 3; k++) {
      lv_obj_t *dot = d->orb[k] = box(ob);
      lv_obj_set_size(dot, sz[k], sz[k]);
      lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
      lv_obj_set_style_bg_color(dot, ink[k], 0);
      lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
      // the outer dot rides just inside the ticks (docs: centred on the ring's edge, half outside the box)
      d->orb_r[k] = R - fw::lay::clock_orbit_dot / 2 - fw::lay::clock_orbit_tick - k * fw::lay::clock_orbit_step;
      fw::qa::free(dot, "an orbit dot: placed by angle");
      fw::qa::circle(dot, ob, R, R, d->orb_r[k]);
      orbit_place(d, k, 270);
    }
    lv_obj_t *col = box(root);  // Time, Date line, weather, centred on the ring
    lv_obj_set_size(col, W, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(col, fw::sp::s5, 0);
    time_props(d, "mid", tp);
    d->time = child(col, "time", tp);
    d->date = child(col, "date_line", dp);
    const std::string w = wx_line(p);
    d->wx = fw::text(col, fw::ty::c_weather, w.c_str(), t.text_dim);
    lv_obj_set_style_opa(d->wx, w.empty() ? LV_OPA_TRANSP : LV_OPA_COVER, 0);  // always there: an update fades it
    lv_obj_center(col);
    if (!fw::mo::reduced(root)) {
      orbit_spin(root, (lv_anim_exec_xcb_t) orbit_b, fw::mo::ms::orbit_b);
      orbit_spin(root, (lv_anim_exec_xcb_t) orbit_c, fw::mo::ms::orbit_c);
    }
  } else {
    time_props(d, "compact", tp);
    d->time = child(root, "time", tp);
    lv_obj_center(d->time);
  }
  minute(root, false);
  d->sec_deg = -1;  // the first seconds() always places the dot / hand (at :00 it would match the 0 it started with)
  seconds(root, 0);
  start(root);
  if (!d->timer) d->timer = lv_timer_create([](lv_timer_t *tm) { minute((lv_obj_t *) lv_timer_get_user_data(tm), true); }, 1000, root);
  fw::shape(root, std::to_string(d->face));
}
}  // namespace

FW_COMPONENT(clock_face, "Clock faces", "components", R"({
  "about": "The faces of the clock: big digits, a dotted ring with an orbiting seconds dot, an analogue face with thin hands, the compact header time, or the Clock screen's orbit face (three orbiting dots, the time, date and weather).",
  "frame": [480, 480],
  "props": [
    {"name": "face", "type": "select", "options": ["hero", "ring", "analogue", "compact", "orbit"], "default": "ring"},
    {"name": "font", "type": "select", "options": ["mono", "inter"], "default": "mono"},
    {"name": "format", "type": "select", "options": ["24h", "12h"], "default": "24h"},
    {"name": "date", "type": "select", "options": ["short", "long"], "default": "short"},
    {"name": "seconds", "type": "bool", "default": false},
    {"name": "time", "type": "text", "default": ""},
    {"name": "day", "type": "text", "default": ""},
    {"name": "condition", "type": "text", "default": ""},
    {"name": "temp", "type": "text", "default": ""}
  ],
  "variants": [
    {"label": "hero · 14:52", "props": {"face": "hero", "time": "14:52:20", "day": "2026-10-19"}},
    {"label": "ring · 10:08", "props": {"face": "ring", "time": "10:08:30", "day": "2026-10-19"}},
    {"label": "analogue · 10:08", "props": {"face": "analogue", "time": "10:08:30", "day": "2026-10-19"}},
    {"label": "ring · inter · 12h · long date", "props": {"face": "ring", "font": "inter", "format": "12h", "date": "long", "time": "21:47:05", "day": "2026-10-19"}},
    {"label": "compact", "props": {"face": "compact", "time": "09:05:00"}},
    {"label": "hero · seconds", "props": {"face": "hero", "time": "14:52:20", "day": "2026-10-19", "seconds": true}},
    {"label": "compact · seconds", "props": {"face": "compact", "time": "09:05:00", "seconds": true}},
    {"label": "orbit · the Clock screen", "props": {"face": "orbit", "time": "10:08:30", "day": "2026-10-19", "condition": "rainy", "temp": "13.4"}},
    {"label": "orbit · no weather", "props": {"face": "orbit", "time": "21:47:05", "day": "2026-10-19"}}
  ]
})") {
  auto *d = new Clock;
  lv_obj_t *root = lv_obj_create(parent);
  lv_obj_set_user_data(root, d);
  lv_obj_add_event_cb(root, [](lv_event_t *e) {
    auto *d = (Clock *) lv_event_get_user_data(e);
    if (d->timer) lv_timer_delete(d->timer);
    delete d;
  }, LV_EVENT_DELETE, d);
  fill(root, p);
}

FW_UPDATE(clock_face) {
  auto *d = of(root);
  if (!d) return false;
  if (!fw::shape(root, std::to_string(face_of(p))) || (p["font"] | "mono") != d->font) {
    fw::refill(root);
    fill(root, p);
    return true;
  }
  d->format = p["format"] | "24h";
  d->datestyle = p["date"] | "short";
  d->seconds = p["seconds"] | false;
  const std::string fixed = p["time"] | "";
  if (fixed != d->fixed) {  // a new time: the hands step there, the seconds pick up from it
    d->fixed = fixed;
    d->base_ms = day_ms(p);
    d->t0 = lv_tick_get();
    d->shown_min = d->shown_min < 0 ? -1 : d->shown_min + 100000;  // force the next minute() to move
    minute(root, true);
  }
  if (d->time) {
    ArduinoJson::JsonDocument cp;
    time_props(d, d->face == HERO ? "hero" : d->face == COMPACT ? "compact" : "mid", cp);
    if (!fw::update_child(fw::find("time"), d->time, cp.as<fw::Props>())) { fw::refill(root); fill(root, p); return true; }
  }
  if (d->date) {
    ArduinoJson::JsonDocument cp;
    date_props(d, cp, p["day"] | "");
    if (!fw::update_child(fw::find("date_line"), d->date, cp.as<fw::Props>())) { fw::refill(root); fill(root, p); return true; }
    if (d->face == RING && d->time) lv_obj_align_to(d->date, d->time, LV_ALIGN_OUT_BOTTOM_MID, 0, fw::sp::s7);
  }
  if (d->wx) {  // the weather line: text in place, shown / hidden by opacity
    const std::string w = wx_line(p);
    if (!w.empty()) fw::set_text(d->wx, fw::ty::c_weather, w.c_str());
    lv_obj_set_style_opa(d->wx, w.empty() ? LV_OPA_TRANSP : LV_OPA_COVER, 0);
  }
  return true;
}
