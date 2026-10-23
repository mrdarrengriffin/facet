// Overhead view: the Overhead screen's body (the panel lab's Overhead screen, board facet-73; the factory specs
// renderer/specs/overhead.json + aircraft.json add the header): what is flying over the house.
//   now   the hero = the aircraft highest in the sky (what you see looking up; an emergency squawk always wins; a tapped
//         row picks another): a flex row of the words column (grows) and the sky radar (lay::sky_radar square).
//         Column: the look line (type label, caps: "high in the NW · 38°", "overhead to the NW · 64°"; fit8; accent-text when it is >= 30 degrees up =
//         look up, else text-dim) with a real Status chip (error, "7700 emergency") after it on an emergency squawk,
//         sp::s2, the route (type headline, text, one line: "Catania → Leeds", the callsign when no route), the who line
//         (body, text-dim: airline · flight · type), lay::gutter, then three real Stats in a row a gutter apart
//         (altitude + climbing / descending, speed, distance + compass point). Under it, lay::group down: a real Section
//         header "also up there", the two nearest others as real List rows (title = route, sub = type · altitude ·
//         distance; a tap = select it: the hero follows, event "select" <callsign>), and a real More row
//         "all aircraft · N" that opens aircraft (event "open", the spec's nav.open).
//   list  (prop view = list) Section header "closest first · within 50 km" + every aircraft as a List row, nearest
//         first (static: read-only).
//   radar the sky radar: rings at 5 / 15 / 30 / 50 km at R x sqrt(km / 50) on the grid (the Lightning radar's square-
//         root scale), stroke::hair line-hi; home = the blue lay::radar_home ring + dot; every aircraft a
//         lay::sky_plane dot at its distance and bearing (text-dim; the hero accent, an emergency error), placed by
//         value (qa::free).
//   quiet no aircraft: a real Empty state ("Clear skies", "Nothing flying within 50 km right now") beside the empty
//         radar.
// Feed: props sample (busy / overhead / emergency / quiet: the panel lab's sample sets, fixed). Live = the open ADS-B
// feeds (adsb.lol / adsb.fi, no key) fetched by the panel round its home location, or Home Assistant's Flightradar24
// integration (sources.json "overhead"); until that fetch exists on the device, the sample.
// Update path: refilled in place (the hero changes shape). Contrast: radar (rings, home), text, status_chip, stat,
// list_row, section_header, more_row, empty_state.
// audit deps: stat, list_row, section_header, more_row, status_chip, empty_state
#include <cmath>
#include "fw.h"
#include "links.h"

namespace {
struct Flight {
  const char *cs, *type, *airline, *flight, *from, *to;
  int alt, vs, gs;  // ft, ft/min, kt
  double km, brg;
  const char *squawk;
  double elev() const { return std::atan2(alt * 0.3048 / 1000.0, std::max(km, 0.05)) * 180 / M_PI; }
  bool emergency() const { return !strcmp(squawk, "7700") || !strcmp(squawk, "7600") || !strcmp(squawk, "7500"); }
};
// the panel lab's sample (apps/hub panel/overhead sampleOverhead): busy = all; overhead / emergency add one close by
const Flight BASE[] = {
    {"RYR6BA", "B38M", "Ryanair", "FR6BA", "Krak\xC3\xB3w", "Leeds", 32000, 0, 452, 26, 118, "2000"},
    {"EXS13DK", "B738", "Jet2.com", "LS13DK", "Catania", "Leeds", 35000, 0, 470, 33, 42, "2000"},
    {"DHK842", "B763", "DHL Air", "D0842", "Brussels", "Nottingham", 21000, -1400, 380, 22, 168, "2000"},
    {"TARTN24", "A332", "Royal Air Force", "", "", "", 18000, 0, 330, 41, 250, "2000"},
    {"GBNHP", "C172", "", "", "", "", 2300, 0, 95, 9.4, 205, "2000"},
    {"EXS81K", "B738", "Jet2.com", "", "", "", 11700, 1800, 300, 46, 320, "2000"},
    {"DLH3VK", "A20N", "Lufthansa", "LH922", "Frankfurt", "London", 24000, -1100, 440, 38, 140, "2000"},
};
const Flight CLOSE = {"EZY45UK", "A20N", "easyJet", "U245UK", "Amsterdam", "Edinburgh", 29000, 0, 460, 4.3, 300, "2000"};
const Flight SQUAWK = {"RYR27PM", "B738", "Ryanair", "FR27PM", "London", "Gda\xC5\x84sk", 9800, -2400, 310, 6.2, 175, "7700"};
constexpr double RANGE_KM = 50, LOOK_UP = 30;
constexpr double RING_KM[] = {5, 15, 30, 50};

std::vector<Flight> sample(const std::string &s) {
  std::vector<Flight> f;
  if (s == "quiet") return f;
  f.assign(std::begin(BASE), std::end(BASE));
  if (s == "overhead") f.insert(f.begin(), CLOSE);
  if (s == "emergency") f.insert(f.begin(), SQUAWK);
  if (s != "busy") f.erase(f.begin() + 4, f.begin() + 6);  // the lab sample: a quieter sky drops two
  std::stable_sort(f.begin(), f.end(), [](const Flight &a, const Flight &b) {
    if (a.emergency() != b.emergency()) return a.emergency();
    return a.elev() > b.elev();
  });
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
std::string km(const Flight &f) {
  char b[32];
  snprintf(b, sizeof b, f.km < 10 ? "%.1f km %s" : "%.0f km %s", f.km, compass(f.brg));
  return b;
}
std::string alt(const Flight &f) { return thousands(f.alt) + " ft"; }
std::string look(const Flight &f) {
  const double e = f.elev();
  char b[48];
  snprintf(b, sizeof b, "%s %s \xC2\xB7 %d\xC2\xB0", e >= 60 ? "overhead to the" : e >= LOOK_UP ? "high in the" : e < 10 ? "low in the" : "in the",
           compass(f.brg), (int) std::lround(e));
  return b;
}
std::string who(const Flight &f) {
  std::string s;
  for (const char *x : {f.airline, f.flight, f.type})
    if (*x) s += (s.empty() ? "" : " \xC2\xB7 ") + std::string(x);
  return s;
}

struct View { std::string sample, view, sel; lv_obj_t *root; };

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
lv_obj_t *ring(lv_obj_t *parent, int c, int r, int w, lv_color_t col) {
  lv_obj_t *o = box(parent);
  lv_obj_set_size(o, 2 * r, 2 * r);
  lv_obj_set_pos(o, c - r, c - r);
  lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_border_width(o, w, 0);
  lv_obj_set_style_border_color(o, col, 0);
  return o;
}
lv_obj_t *dot(lv_obj_t *parent, int x, int y, int d, lv_color_t col) {
  lv_obj_t *o = box(parent);
  lv_obj_set_size(o, d, d);
  lv_obj_set_pos(o, x - d / 2, y - d / 2);
  lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(o, col, 0);
  lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
  return o;
}
void radar(lv_obj_t *parent, const std::vector<Flight> &fl, const Flight *lead) {
  const fw::Theme &t = *fw::T;
  const int S = fw::lay::sky_radar, R = S / 2;
  lv_obj_t *r = box(parent);
  lv_obj_set_size(r, S, S);
  for (double k : RING_KM) ring(r, R, k == RANGE_KM ? R : (int) std::lround(R * std::sqrt(k / RANGE_KM) / 4) * 4, fw::stroke::hair, t.line_hi);
  lv_obj_t *home = ring(r, R, fw::lay::radar_home / 2, fw::stroke::edge, t.blue);
  lv_obj_set_style_bg_color(home, t.bg, 0);
  lv_obj_set_style_bg_opa(home, LV_OPA_COVER, 0);
  lv_obj_t *hd = dot(home, fw::lay::radar_home / 2 - fw::stroke::edge, fw::lay::radar_home / 2 - fw::stroke::edge, fw::lay::radar_home_dot, t.blue);
  lv_obj_center(hd);
  const int pd = fw::lay::sky_plane;
  for (int pass = 0; pass < 2; pass++)  // the others first, the lead on top
    for (const Flight &f : fl) {
      const bool is_lead = lead && !strcmp(f.cs, lead->cs);
      if (is_lead != (pass == 1)) continue;
      const double rr = std::min(1.0, std::sqrt(f.km / RANGE_KM)) * (R - pd / 2), a = f.brg * M_PI / 180;
      lv_obj_t *d = dot(r, R + (int) std::lround(rr * std::sin(a)), R - (int) std::lround(rr * std::cos(a)), pd,
                        f.emergency() ? t.error : is_lead ? t.accent : t.text_dim);
      fw::qa::free(d, "an aircraft: placed by its distance and bearing");
    }
}
lv_obj_t *section(lv_obj_t *parent, const std::string &title) {
  ArduinoJson::JsonDocument sp;
  sp["title"] = title, sp["badge"] = false, sp["right"] = "none";
  return build("section_header", parent, sp);
}
void row(lv_obj_t *parent, const Flight &f) {
  ArduinoJson::JsonDocument rp;
  rp["title"] = route(f), rp["icon"] = "none", rp["trailing"] = "none";
  rp["sub"] = std::string(f.type) + " \xC2\xB7 " + alt(f) + " \xC2\xB7 " + km(f);
  build("list_row", parent, rp);
}
lv_obj_t *col(lv_obj_t *parent) {
  lv_obj_t *c = box(parent);
  lv_obj_set_size(c, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
  return c;
}

void fill(View *v) {
  const fw::Theme &t = *fw::T;
  lv_obj_t *root = v->root;
  lv_obj_clean(root);
  const std::vector<Flight> fl = sample(v->sample);
  std::vector<const Flight *> near;
  for (const Flight &f : fl) near.push_back(&f);
  std::stable_sort(near.begin(), near.end(), [](const Flight *a, const Flight *b) { return a->km < b->km; });

  if (v->view == "list") {
    char b[64];
    snprintf(b, sizeof b, "closest first \xC2\xB7 within %d km", (int) RANGE_KM);
    section(root, b);
    lv_obj_t *rows = col(root);
    for (const Flight *f : near) row(rows, *f);
    return;
  }
  const Flight *lead = fl.empty() ? nullptr : &fl[0];
  for (const Flight &f : fl)
    if (v->sel == f.cs && !lead->emergency()) lead = &f;

  lv_obj_t *hero = box(root);
  lv_obj_set_size(hero, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(hero, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(hero, fw::lay::gutter, 0);
  lv_obj_t *words = col(hero);
  lv_obj_set_width(words, 0);
  lv_obj_set_flex_grow(words, 1);
  if (!lead) {
    ArduinoJson::JsonDocument ep;
    char b[64];
    snprintf(b, sizeof b, "Nothing flying within %d km right now", (int) RANGE_KM);
    ep["icon"] = "done", ep["title"] = "Clear skies", ep["body"] = b, ep["tone"] = "idle";
    if (lv_obj_t *e = build("empty_state", words, ep)) lv_obj_set_width(e, LV_PCT(100));
    radar(hero, fl, nullptr);
    return;
  }
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
  if (lead->emergency()) {
    ArduinoJson::JsonDocument cp;
    cp["tone"] = "error", cp["text"] = std::string(lead->squawk) + " emergency";
    build("status_chip", lk, cp);
  }
  lv_obj_update_layout(lk);  // the row = its tallest part, set (a content height counts the label's caps nudge)
  int lh = fw::style(fw::ty::label).box;
  for (uint32_t i = 0; i < lv_obj_get_child_count(lk); i++) lh = std::max(lh, (int) lv_obj_get_height(lv_obj_get_child(lk, i)));
  lv_obj_set_height(lk, lh);
  lv_obj_t *rt = fw::text(words, fw::ty::headline, route(*lead).c_str(), t.text);
  lv_obj_set_width(rt, LV_PCT(100));
  lv_label_set_long_mode(rt, LV_LABEL_LONG_DOT);
  lv_obj_set_style_margin_top(rt, fw::sp::s2, 0);
  lv_obj_t *wl = fw::text(words, fw::ty::body, who(*lead).c_str(), t.text_dim);
  lv_obj_set_width(wl, LV_PCT(100));
  lv_label_set_long_mode(wl, LV_LABEL_LONG_DOT);
  lv_obj_t *stats = box(words);
  lv_obj_set_size(stats, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(stats, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(stats, fw::lay::gutter, 0);
  lv_obj_set_style_margin_top(stats, fw::lay::gutter, 0);
  const std::string climb = std::abs(lead->vs) < 300 ? "altitude" : lead->vs > 0 ? "climbing" : "descending";
  char spd[24];
  snprintf(spd, sizeof spd, "%d kt", lead->gs);
  const std::pair<std::string, std::string> S3[] = {{climb, alt(*lead)}, {"speed", spd}, {"distance", km(*lead)}};
  for (const auto &s : S3) {
    ArduinoJson::JsonDocument sp;
    sp["label"] = s.first, sp["value"] = s.second, sp["tone"] = "dim", sp["fill"] = true;
    if (lv_obj_t *o = build("stat", stats, sp)) { lv_obj_set_width(o, 0); lv_obj_set_flex_grow(o, 1); }
  }
  radar(hero, fl, lead);

  if (fl.size() < 2) return;
  lv_obj_t *also = col(root);
  lv_obj_set_style_margin_top(also, fw::lay::group, 0);
  section(also, "also up there");
  int n = 0;
  for (const Flight *f : near) {
    if (f == lead || n == 2) continue;
    row(also, *f);
    n++;
  }
  lv_obj_t *m = fw::link_box(root, also, "aircraft");
  ArduinoJson::JsonDocument mp;
  mp["text"] = "all aircraft \xC2\xB7 " + std::to_string(fl.size());
  build("more_row", m, mp);
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
        lv_async_call([](void *u) { fill((View *) u); }, v);
        return;
      }
  }, v);
}
}  // namespace

FW_COMPONENT(overhead_view, "Overhead view", "components", R"({
  "about": "The Overhead screen's body: the aircraft highest in the sky with its route and a sky radar, the two nearest others, and the way to all of them.",
  "frame": [480, 0],
  "props": [
    {"name": "sample", "type": "select", "options": ["busy", "overhead", "emergency", "quiet"], "default": "busy"},
    {"name": "view", "type": "select", "options": ["now", "list"], "default": "now"}
  ],
  "variants": [
    {"label": "busy sky", "props": {}},
    {"label": "one right overhead", "props": {"sample": "overhead"}},
    {"label": "emergency squawk", "props": {"sample": "emergency"}},
    {"label": "clear skies", "props": {"sample": "quiet"}},
    {"label": "all aircraft", "props": {"view": "list"}}
  ]
})") {
  lv_obj_t *root = lv_obj_create(parent);
  lv_obj_remove_style_all(root);
  lv_obj_remove_flag(root, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_size(root, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
  auto *v = new View{p["sample"] | "busy", p["view"] | "now", "", root};
  lv_obj_set_user_data(root, v);
  lv_obj_add_event_cb(root, [](lv_event_t *e) {
    auto *v = (View *) lv_event_get_user_data(e);
    lv_async_call_cancel([](void *u) { fill((View *) u); }, v);
    delete v;
  }, LV_EVENT_DELETE, v);
  listen(v);
  fill(v);
}

FW_UPDATE(overhead_view) {
  auto *v = (View *) lv_obj_get_user_data(root);
  if (!v) return false;
  v->sample = p["sample"] | "busy", v->view = p["view"] | "now", v->sel.clear();
  fill(v);
  return true;
}
