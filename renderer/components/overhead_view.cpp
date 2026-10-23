// Overhead view: the Overhead screen's body (the panel lab's Overhead screen apps/hub panel/overhead/screen.ts = Facet
// main packages/ui screens/overhead.ts, board facet-73; the factory specs renderer/specs/overhead.json + aircraft.json
// add the header): what is flying over the house. Selection (Darren 2026-10-20): the accent marks the SELECTED
// aircraft (hero + radar); nothing picked = the closest.
//   header  on a spec screen (fw::spec::building_header()) a real Segmented "Now, Radar" goes in the screen's own
//           Header row before the chrome (docs header({right: [seg]})); a change swaps the view (silent refill).
//   now     the hero = the selected aircraft, else the closest: a flex row of the words column (grows) and the sky
//           radar (lay::sky_radar square, top-aligned). Column, fixed boxes so the stats never move (720: 24 + 16 + 96
//           + 16 + 32 + 16 + 64 = the lab's 264): the look line (type label, caps: "low in the SW · 4°", "high in the
//           NW · 38°", "right overhead · 64°"; fit8; text-dim, accent-text from 30 degrees up = look up) with a real
//           Status chip after it (error "7700 emergency", info "military"), sp::s4, the route (type headline, text, two
//           lines' box, wraps then ellipsis: "Catania → Leeds", the callsign when no route), sp::s4, the who line (body,
//           text-dim: airline · flight · model), sp::s4, three real Stats a gutter apart (altitude / climbing /
//           descending, speed in the speed unit, distance + compass point). Under it, lay::group down: a real Section
//           header "also up there", the two nearest others as real List rows (over = type · altitude · distance in
//           caps, title = the route; a tap = select it: the hero follows, event "select" <callsign>), and a real More
//           row "all aircraft · N" that opens aircraft (event "open", the spec's nav.open).
//   radar   (the Now | Radar tab, or prop view = radar) the sky radar at the content square (576 / 388): the same
//           drawing at sky_plane_lg, ring distances and the callsigns of the five closest + the selected one.
//   list    (prop view = list: the aircraft spec) Section header "closest first · within 50 km" + every aircraft as a
//           List row, nearest first (static: read-only).
//   quiet   no aircraft: a real Empty state ("Clear skies", "Nothing flying within 50 km right now") beside the empty
//           radar.
// The sky radar (one draw event, so a refill is one dirty area): rings on a square-root scale (km 5 / 15 / 30 / 50,
// mi 3 / 10 / 20 / 30 = Units) out to R - plane / 2, inner rings stroke::hair line, the outer one stroke::edge line-hi;
// N / E / S / W (type label, text-dim, a bg box that cuts the outer ring); home = a text dot (radar_home_dot); every
// aircraft = a plane seen from above (the lab's 24 grid glyph: fuselage line + wing and tail triangles) turned to its
// track, text-dim; the selected one accent with a stroke::edge ring sp::s2 out, drawn last; an emergency error.
// Feed: props sample (busy / overhead / emergency / quiet: the panel lab's sample sets, fixed), unit (km | mi) and
// speed (mph | kmh | kn) = Settings > Units. Live = the open ADS-B feeds (adsb.lol / adsb.fi, no key) fetched by the
// panel round its home location, or Home Assistant's Flightradar24 integration (sources.json "overhead"); until that
// fetch exists on the device, the sample.
// Update path: refilled in place (the hero changes shape). Contrast: overhead, status_chip, stat, list_row,
// section_header, more_row, empty_state, segmented.
// audit deps: stat, list_row, section_header, more_row, status_chip, empty_state, segmented
#include <cmath>
#include "fw.h"
#include "links.h"
#include "spec.h"
#include "units.h"

namespace {
struct Flight {
  const char *cs, *reg, *type, *model, *airline, *op, *flight, *from, *to;
  int alt, vs, gs, track;  // ft, ft/min, kt, degrees
  double km, brg;
  const char *squawk;
  bool military;
  double elev() const { return std::atan2(alt * 0.3048 / 1000.0, std::max(km, 0.05)) * 180 / M_PI; }
  bool emergency() const { return !strcmp(squawk, "7700") || !strcmp(squawk, "7600") || !strcmp(squawk, "7500"); }
};
// the panel lab's sample (apps/hub panel/overhead sampleOverhead): busy = all; overhead / emergency add one close by
const Flight BASE[] = {
    {"RYR6BA", "EI-IHR", "B38M", "Boeing 737 MAX 8", "Ryanair", "", "FR6BA", "Krak\xC3\xB3w", "Leeds", 32000, 0, 452, 302, 26, 118, "2000", false},
    {"EXS13DK", "G-JZHF", "B738", "Boeing 737-800", "Jet2.com", "", "LS13DK", "Catania", "Leeds", 35000, 0, 470, 318, 33, 42, "2000", false},
    {"DHK842", "", "B763", "Boeing 767-300", "DHL Air", "", "D0842", "Brussels", "Nottingham", 21000, -1400, 380, 290, 22, 168, "2000", false},
    {"TARTN24", "ZZ336", "A332", "Airbus Voyager KC2", "", "Royal Air Force", "", "", "", 18000, 0, 330, 75, 41, 250, "2000", true},
    {"GBNHP", "G-BNHP", "C172", "Cessna 172", "", "", "", "", "", 2300, 0, 95, 20, 9.4, 205, "2000", false},
    {"EXS81K", "", "B738", "Boeing 737-800", "Jet2.com", "", "", "", "", 11700, 1800, 300, 140, 46, 320, "2000", false},
    {"DLH3VK", "", "A20N", "Airbus A320neo", "Lufthansa", "", "LH922", "Frankfurt", "London", 24000, -1100, 440, 255, 38, 140, "2000", false},
};
const Flight CLOSE = {"EZY45UK", "G-UZHA", "A20N", "Airbus A320neo", "easyJet", "", "U245UK", "Amsterdam", "Edinburgh", 29000, 0, 460, 330, 4.3, 300, "2000", false};
const Flight SQUAWK = {"RYR27PM", "", "B738", "Boeing 737-800", "Ryanair", "", "FR27PM", "London", "Gda\xC5\x84sk", 9800, -2400, 310, 200, 6.2, 175, "7700", false};
constexpr double LOOK_UP = 30, KM_PER_MI = 1.609344;
struct Range { double edge; double rings[4]; };  // in the distance unit
const Range RANGE_KM = {50, {5, 15, 30, 50}}, RANGE_MI = {30, {3, 10, 20, 30}};

std::vector<Flight> sample(const std::string &s) {
  std::vector<Flight> f;
  if (s == "quiet") return f;
  f.assign(std::begin(BASE), std::end(BASE));
  if (s == "overhead") f.insert(f.begin(), CLOSE);
  if (s == "emergency") f.insert(f.begin(), SQUAWK);
  if (s != "busy") f.erase(f.begin() + 4, f.begin() + 6);  // the lab sample: a quieter sky drops two
  return f;
}
const char *compass(double brg) {
  static const char *const P[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
  return P[(int) std::lround(std::fmod(brg + 360, 360) / 45) % 8];
}
std::string thousands(int n) {
  std::string s = std::to_string(std::abs(n));
  for (int i = (int) s.size() - 3; i > 0; i -= 3) s.insert(i, ",");
  return (n < 0 ? "-" : "") + s;
}
std::string route(const Flight &f) { return *f.from && *f.to ? std::string(f.from) + " \xE2\x86\x92 " + f.to : f.cs; }
std::string away(const Flight &f, fw::units::Distance u) { return fw::units::distance(f.km, u) + " " + compass(f.brg); }
std::string alt(const Flight &f) { return thousands((int) std::lround(f.alt / 25.0) * 25) + " ft"; }
std::string look(const Flight &f) {
  const double e = f.elev();
  char b[48];
  if (e >= 60) snprintf(b, sizeof b, "right overhead \xC2\xB7 %d\xC2\xB0", (int) std::lround(e));
  else snprintf(b, sizeof b, "%s %s \xC2\xB7 %d\xC2\xB0", e >= LOOK_UP ? "high in the" : e >= 10 ? "in the" : "low in the",
                compass(f.brg), (int) std::lround(e));
  return b;
}
// "Jet2.com · LS13DK · Boeing 737-800": what is known (the lab's whoText)
std::string who(const Flight &f) {
  const bool rt = *f.from && *f.to;
  const char *parts[] = {*f.airline ? f.airline : (rt ? "" : f.op), *f.flight ? f.flight : (rt ? f.cs : ""), *f.model ? f.model : f.type};
  std::string s;
  for (const char *x : parts)
    if (*x) s += (s.empty() ? "" : " \xC2\xB7 ") + std::string(x);
  return s;
}

struct View {
  std::string sample, view, sel;
  fw::units::Distance du = fw::units::Distance::km;
  fw::units::Speed su = fw::units::Speed::mph;
  lv_obj_t *root = nullptr, *seg = nullptr;
};

lv_obj_t *box(lv_obj_t *parent) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  return o;
}
lv_obj_t *build(const char *id, lv_obj_t *parent, ArduinoJson::JsonDocument &d) {
  const uint32_t n = lv_obj_get_child_count(parent);
  if (const fw::Component *c = fw::find(id)) c->build(parent, d.as<fw::Props>());
  return lv_obj_get_child_count(parent) > n ? lv_obj_get_child(parent, -1) : nullptr;
}

// ---------------------------------------------------------------- the sky radar
struct Mark { float x, y, track; lv_color_t col; bool hero; };
struct Sky {
  int S = 0, glyph = 0;
  std::vector<std::pair<float, bool>> rings;  // radius px, outer
  std::vector<Mark> marks;                    // the selected one last (on top)
};
int32_t pv(float v) { return (int32_t) std::lround(v); }
void tri(lv_layer_t *l, const float (*p)[2], lv_color_t col) {
  lv_draw_triangle_dsc_t td;
  lv_draw_triangle_dsc_init(&td);
  td.color = col;
  td.opa = LV_OPA_COVER;
  for (int i = 0; i < 3; i++) td.p[i] = {pv(p[i][0]), pv(p[i][1])};
  lv_draw_triangle(l, &td);
}
void arc(lv_layer_t *l, float cx, float cy, float r, int w, lv_color_t col) {
  lv_draw_arc_dsc_t ad;
  lv_draw_arc_dsc_init(&ad);
  ad.color = col;
  ad.center = {pv(cx), pv(cy)};
  ad.radius = (uint16_t) std::max(1L, std::lround(r));
  ad.width = w <= 0 ? ad.radius : std::min<int32_t>(w, ad.radius);
  ad.start_angle = 0;
  ad.end_angle = 360;
  lv_draw_arc(l, &ad);
}
// the lab's plane (24 grid, nose up) at (cx, cy), size px, turned clockwise by track: fuselage + wings + tail
void plane(lv_layer_t *l, float cx, float cy, float size, float track, lv_color_t col) {
  const float k = size / 24.f, a = track * (float) M_PI / 180, c = std::cos(a), s = std::sin(a);
  auto P = [&](float x, float y, float *o) {
    x = (x - 12) * k, y = (y - 12) * k;
    o[0] = cx + x * c - y * s, o[1] = cy + x * s + y * c;
  };
  float n[2], t[2];
  P(12, 3.2f, n), P(12, 19.6f, t);
  lv_draw_line_dsc_t ld;
  lv_draw_line_dsc_init(&ld);
  ld.color = col;
  ld.width = std::max(2, (int) std::lround(2.6f * k));
  ld.round_start = ld.round_end = true;
  ld.p1 = {pv(n[0]), pv(n[1])}, ld.p2 = {pv(t[0]), pv(t[1])};
  lv_draw_line(l, &ld);
  const float W[2][3][2] = {{{12, 8.6f}, {21, 14.8f}, {12, 13.4f}}, {{12, 16.4f}, {15.6f, 20.6f}, {12, 19.8f}}};
  for (const auto &w : W)
    for (int side = 0; side < 2; side++) {
      float p[3][2];
      for (int i = 0; i < 3; i++) P(side ? 24 - w[i][0] : w[i][0], w[i][1], p[i]);
      tri(l, p, col);
    }
}
void sky_draw(lv_event_t *e) {
  auto *k = (Sky *) lv_event_get_user_data(e);
  auto *o = (lv_obj_t *) lv_event_get_current_target(e);
  lv_layer_t *l = lv_event_get_layer(e);
  lv_area_t a;
  lv_obj_get_coords(o, &a);
  const fw::Theme &t = *fw::T;
  const float cx = a.x1 + k->S / 2.f, cy = a.y1 + k->S / 2.f;
  for (const auto &r : k->rings) {
    const int w = r.second ? fw::stroke::edge : fw::stroke::hair;
    arc(l, cx, cy, r.first + w / 2.f, w, r.second ? t.line_hi : t.line);  // centred on the radius like an SVG stroke
  }
  arc(l, cx, cy, fw::lay::radar_home_dot / 2.f, 0, t.text);
  for (const Mark &m : k->marks) {
    plane(l, a.x1 + m.x, a.y1 + m.y, (float) k->glyph, m.track, m.col);
    if (m.hero) arc(l, a.x1 + m.x, a.y1 + m.y, k->glyph / 2.f + fw::sp::s2, fw::stroke::edge, m.col);
  }
}
// the radar: S square, planes glyph px; labels = callsigns on the n closest + the selected one, and ring distances
lv_obj_t *radar(lv_obj_t *parent, const View *v, const std::vector<Flight> &fl, const Flight *lead, int S, int glyph, int labels) {
  const fw::Theme &t = *fw::T;
  const Range &rg = v->du == fw::units::Distance::mi ? RANGE_MI : RANGE_KM;
  const float R = S / 2.f, rmax = R - glyph / 2.f;
  auto rpx = [&](double unit_v) { return rmax * (float) std::sqrt(std::min(unit_v, rg.edge) / rg.edge); };
  lv_obj_t *r = box(parent);
  lv_obj_set_size(r, S, S);
  auto *k = new Sky;
  k->S = S, k->glyph = glyph;
  for (double ring : rg.rings) k->rings.push_back({std::round(rpx(ring)), ring == rg.edge});
  std::vector<const Flight *> near;
  for (const Flight &f : fl) near.push_back(&f);
  std::stable_sort(near.begin(), near.end(), [](const Flight *a, const Flight *b) { return a->km < b->km; });
  const fw::ty::Id LT = fw::ty::label;
  const int B = fw::style(LT).box;
  for (int pass = 0; pass < 2; pass++)
    for (size_t i = 0; i < near.size(); i++) {
      const Flight &f = *near[i];
      const bool hero = &f == lead;
      if (hero != (pass == 1)) continue;
      // the selected one's ring stays inside the square
      const float rr = std::min(rpx(fw::units::from_km(f.km, v->du)), hero ? rmax - fw::sp::s2 : rmax), an = (float) (f.brg * M_PI / 180);
      const float x = R + rr * std::sin(an), y = R - rr * std::cos(an);
      k->marks.push_back({x, y, (float) f.track, f.emergency() ? t.error : hero ? t.accent : t.text_dim, hero});
      if (labels && ((int) i < labels || hero)) {
        lv_obj_t *cs = fw::text(r, LT, f.cs, hero ? t.text : t.text_dim);
        lv_obj_update_layout(cs);
        const int w = lv_obj_get_width(cs), gap = glyph / 2 + fw::sp::s2 + (hero ? fw::sp::s2 + fw::stroke::edge : 0);  // clear of the ring
        const int lx = x + gap + w <= S ? (int) std::lround(x) + gap : (int) std::lround(x) - gap - w;
        lv_obj_set_pos(cs, lx, (int) std::lround(y) - B / 2);
        fw::qa::free(cs, "a callsign: beside its aircraft, placed by distance and bearing");
      }
    }
  lv_obj_add_event_cb(r, sky_draw, LV_EVENT_DRAW_MAIN, k);
  lv_obj_add_event_cb(r, [](lv_event_t *e) { delete (Sky *) lv_event_get_user_data(e); }, LV_EVENT_DELETE, k);
  // N / E / S / W on the outer ring (a bg box cuts the ring under the letter)
  const char *const NESW[] = {"N", "E", "S", "W"};
  const int ctr = (int) std::lround(R - B / 2.f), pos[4][2] = {{ctr, 0}, {S - B, ctr}, {ctr, S - B}, {0, ctr}};
  for (int i = 0; i < 4; i++) {
    lv_obj_t *c = fw::text(r, LT, NESW[i], t.text_dim);
    lv_obj_set_size(c, B, B);
    lv_obj_set_style_text_align(c, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_bg_color(c, t.bg, 0);
    lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
    lv_obj_set_pos(c, pos[i][0], pos[i][1]);
    fw::qa::free(c, "a compass point: centred on the radar's edge");
  }
  if (labels)  // ring distances, just inside each ring below home
    for (double ring : rg.rings) {
      char b[16];
      snprintf(b, sizeof b, ring == rg.edge ? "%g %s" : "%g", ring, v->du == fw::units::Distance::mi ? "mi" : "km");
      lv_obj_t *d = fw::text(r, LT, b, t.text_dim);
      lv_obj_set_pos(d, (int) std::lround(R) + fw::sp::s2, (int) std::lround(R + rpx(ring)) - B - fw::sp::s2);
      fw::qa::free(d, "a ring distance: placed by the ring's radius");
    }
  return r;
}

lv_obj_t *section(lv_obj_t *parent, const std::string &title) {
  ArduinoJson::JsonDocument sp;
  sp["title"] = title, sp["badge"] = false, sp["right"] = "none";
  return build("section_header", parent, sp);
}
void row(lv_obj_t *parent, const Flight &f, const View *v) {
  ArduinoJson::JsonDocument rp;
  rp["title"] = route(f), rp["icon"] = "none", rp["trailing"] = "none";
  rp["over"] = std::string(f.type) + " \xC2\xB7 " + alt(f) + " \xC2\xB7 " + away(f, v->du);
  build("list_row", parent, rp);
}
lv_obj_t *col(lv_obj_t *parent) {
  lv_obj_t *c = box(parent);
  lv_obj_set_size(c, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
  return c;
}
int grid_down(int v) { return v / fw::sp::GRID * fw::sp::GRID; }

void fill(View *v) {
  const fw::Theme &t = *fw::T;
  lv_obj_t *root = v->root;
  lv_obj_clean(root);
  const std::vector<Flight> fl = sample(v->sample);
  std::vector<const Flight *> near;
  for (const Flight &f : fl) near.push_back(&f);
  std::stable_sort(near.begin(), near.end(), [](const Flight *a, const Flight *b) { return a->km < b->km; });
  const Range &rg = v->du == fw::units::Distance::mi ? RANGE_MI : RANGE_KM;
  char within[32];
  snprintf(within, sizeof within, "%g %s", rg.edge, v->du == fw::units::Distance::mi ? "mi" : "km");

  if (v->view == "list") {
    section(root, std::string("closest first \xC2\xB7 within ") + within);
    lv_obj_t *rows = col(root);
    for (const Flight *f : near) row(rows, *f, v);
    return;
  }
  // the selected aircraft, else the closest
  const Flight *lead = near.empty() ? nullptr : near[0];
  for (const Flight *f : near)
    if (v->sel == f->cs) lead = f;

  if (v->view == "radar") {
    const int S = grid_down(std::min(fw::lay::content_w, fw::lay::screen_h - fw::lay::top - fw::lay::inset));
    lv_obj_t *c = box(root);
    lv_obj_set_size(c, LV_PCT(100), S);
    lv_obj_t *r = radar(c, v, fl, lead, S, fw::lay::sky_plane_lg, 5);
    lv_obj_set_pos(r, grid_down((fw::lay::content_w - S) / 2), 0);
    return;
  }

  lv_obj_t *hero = box(root);
  lv_obj_set_size(hero, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(hero, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(hero, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_set_style_pad_column(hero, fw::lay::gutter, 0);
  lv_obj_t *words = col(hero);
  lv_obj_set_width(words, 0);
  lv_obj_set_flex_grow(words, 1);
  if (!lead) {
    ArduinoJson::JsonDocument ep;
    const std::string body = std::string("Nothing flying within ") + within + " right now";
    ep["icon"] = "done", ep["title"] = "Clear skies", ep["body"] = body, ep["tone"] = "idle";
    if (lv_obj_t *e = build("empty_state", words, ep)) lv_obj_set_width(e, LV_PCT(100));
    radar(hero, v, fl, nullptr, fw::lay::sky_radar, fw::lay::sky_plane, 0);
    return;
  }
  // the look line + status
  lv_obj_t *lk = box(words);
  lv_obj_set_size(lk, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(lk, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(lk, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(lk, fw::lay::gutter, 0);
  std::string lt = look(*lead);
  for (char &c : lt) c = (char) toupper((unsigned char) c);
  lv_obj_t *ll = fw::text(lk, fw::ty::label, lt.c_str(), lead->elev() >= LOOK_UP && !lead->emergency() ? t.accent_text : t.text_dim);
  lv_obj_update_layout(ll);
  const int g = fw::sp::GRID, lw = lv_obj_get_width(ll);
  lv_obj_set_width(ll, (lw + g - 1) / g * g);  // fit8: the chip after it starts on the grid
  lv_obj_set_style_max_width(ll, LV_PCT(100), 0);
  lv_label_set_long_mode(ll, LV_LABEL_LONG_DOT);
  lv_obj_set_height(ll, fw::style(fw::ty::label).box);  // the whole type box (the row stays on the grid)
  const char *chip = lead->emergency() ? "emergency" : lead->military ? "military" : nullptr;
  if (chip) {
    ArduinoJson::JsonDocument cp;
    cp["tone"] = lead->emergency() ? "error" : "info";
    cp["text"] = lead->emergency() ? std::string(lead->squawk) + " emergency" : std::string(chip);
    build("status_chip", lk, cp);
  }
  lv_obj_update_layout(lk);  // the row = its tallest part, set (a content height counts the label's caps nudge)
  int lh = fw::style(fw::ty::label).box;
  for (uint32_t i = 0; i < lv_obj_get_child_count(lk); i++) lh = std::max(lh, (int) lv_obj_get_height(lv_obj_get_child(lk, i)));
  lv_obj_set_height(lk, lh);
  // the route: two lines' box, wraps then ellipsis (the lab's line-clamp 2), so the parts under it never move
  lv_obj_t *rt = fw::text(words, fw::ty::headline, route(*lead).c_str(), t.text);
  lv_obj_set_width(rt, LV_PCT(100));
  lv_label_set_long_mode(rt, LV_LABEL_LONG_DOT);
  lv_obj_set_height(rt, 2 * fw::style(fw::ty::headline).box);
  lv_obj_set_style_margin_top(rt, fw::sp::s4, 0);
  lv_obj_t *wl = fw::text(words, fw::ty::body, who(*lead).c_str(), t.text_dim);
  lv_obj_set_width(wl, LV_PCT(100));
  lv_label_set_long_mode(wl, LV_LABEL_LONG_DOT);
  lv_obj_set_height(wl, fw::style(fw::ty::body).box);
  lv_obj_set_style_margin_top(wl, fw::sp::s4, 0);
  lv_obj_t *stats = box(words);
  lv_obj_set_size(stats, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(stats, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(stats, fw::lay::gutter, 0);
  lv_obj_set_style_margin_top(stats, fw::sp::s4, 0);
  const std::string climb = std::abs(lead->vs) < 300 ? "altitude" : lead->vs > 0 ? "climbing" : "descending";
  const std::pair<std::string, std::string> S3[] = {
      {climb, alt(*lead)}, {"speed", fw::units::speed_kt(lead->gs, v->su)}, {"distance", away(*lead, v->du)}};
  for (const auto &s : S3) {
    ArduinoJson::JsonDocument sp;
    sp["label"] = s.first, sp["value"] = s.second, sp["tone"] = "dim";
    if (lv_obj_t *o = build("stat", stats, sp)) { lv_obj_set_width(o, 0); lv_obj_set_flex_grow(o, 1); }
  }
  radar(hero, v, fl, lead, fw::lay::sky_radar, fw::lay::sky_plane, 0);

  if (fl.size() < 2) return;
  lv_obj_t *also = col(root);
  lv_obj_set_style_margin_top(also, fw::lay::group, 0);
  section(also, "also up there");
  int n = 0;
  for (const Flight *f : near) {
    if (f == lead || n == 2) continue;
    row(also, *f, v);
    n++;
  }
  lv_obj_t *m = fw::link_box(root, also, "aircraft");
  ArduinoJson::JsonDocument mp;
  mp["text"] = "all aircraft \xC2\xB7 " + std::to_string(fl.size());
  build("more_row", m, mp);
}
void fill_cb(void *u) { fill((View *) u); }
void refill_later(View *v) {  // once the tap is done; one pending refill at a time
  lv_async_call_cancel(fill_cb, v);
  lv_async_call(fill_cb, v);
}
void listen(View *v) {
  fw::listen(v->root, [](lv_obj_t *from, const char *name, const char *detail, void *user) {
    auto *v = (View *) user;
    if (strcmp(name, "tap") || v->view == "list") return void(fw::notify(lv_obj_get_parent(v->root), name, detail));
    // a List row tap (detail = its title = the route): select that aircraft once the tap is done
    for (const Flight &f : sample(v->sample))
      if (route(f) == detail) {
        v->sel = f.cs;
        fw::emit(v->root, "select", f.cs);
        refill_later(v);
        return;
      }
  }, v);
}
// Now | Radar in the spec screen's Header row (not on the list view, not when built alone)
void header_tabs(View *v) {
  lv_obj_t *hdr = fw::spec::building_header();
  if (!hdr || v->view == "list") return;
  ArduinoJson::JsonDocument sp;
  sp["options"] = "Now, Radar";
  sp["selected"] = v->view == "radar" ? "radar" : "now";
  v->seg = build("segmented", hdr, sp);
  if (!v->seg) return;
  fw::spec::header_place(v->seg);
  fw::listen(v->seg, [](lv_obj_t *, const char *name, const char *detail, void *user) {
    auto *v = (View *) user;
    if (strcmp(name, "change") || v->view == detail) return;
    v->view = detail;
    refill_later(v);
  }, v);
}
void take(View *v, fw::Props p) {
  v->sample = p["sample"] | "busy", v->view = p["view"] | "now";
  v->du = fw::units::distance_of(p["unit"] | "km");
  v->su = fw::units::speed_of(p["speed"] | "mph");
}
}  // namespace

FW_COMPONENT(overhead_view, "Overhead view", "components", R"({
  "about": "The Overhead screen's body: the selected aircraft (else the closest) with its route and a sky radar, the two nearest others, the big radar, and the way to all of them.",
  "frame": [480, 0],
  "props": [
    {"name": "sample", "type": "select", "options": ["busy", "overhead", "emergency", "quiet"], "default": "busy"},
    {"name": "view", "type": "select", "options": ["now", "radar", "list"], "default": "now"},
    {"name": "unit", "type": "select", "options": ["km", "mi"], "default": "km"},
    {"name": "speed", "type": "select", "options": ["mph", "kmh", "kn"], "default": "mph"}
  ],
  "variants": [
    {"label": "busy sky", "props": {}},
    {"label": "one right overhead", "props": {"sample": "overhead"}},
    {"label": "emergency squawk", "props": {"sample": "emergency"}},
    {"label": "clear skies", "props": {"sample": "quiet"}},
    {"label": "radar", "props": {"view": "radar"}},
    {"label": "all aircraft", "props": {"view": "list"}},
    {"label": "miles", "props": {"unit": "mi"}}
  ]
})") {
  lv_obj_t *root = lv_obj_create(parent);
  lv_obj_remove_style_all(root);
  lv_obj_remove_flag(root, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_size(root, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
  auto *v = new View;
  v->root = root;
  take(v, p);
  lv_obj_set_user_data(root, v);
  lv_obj_add_event_cb(root, [](lv_event_t *e) {
    auto *v = (View *) lv_event_get_user_data(e);
    lv_async_call_cancel(fill_cb, v);
    delete v;
  }, LV_EVENT_DELETE, v);
  listen(v);
  header_tabs(v);
  fill(v);
}

FW_UPDATE(overhead_view) {
  auto *v = (View *) lv_obj_get_user_data(root);
  if (!v) return false;
  take(v, p);
  v->sel.clear();
  if (v->seg) {  // the tabs follow (the segmented's own update path)
    ArduinoJson::JsonDocument sp;
    sp["options"] = "Now, Radar";
    sp["selected"] = v->view == "radar" ? "radar" : "now";
    fw::update_child(fw::find("segmented"), v->seg, sp.as<fw::Props>());
  }
  fill(v);
  return true;
}
