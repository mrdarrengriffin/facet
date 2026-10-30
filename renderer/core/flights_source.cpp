// The flights feed (CONTRACT.md C51): the reserved entity panel.flights (core/spec.h "local sources") in the HA store,
// the one shape the Flight radar binds to, whatever reports the aircraft. Same file on the panels and in WASM.
// The lead = the closest aircraft (stage 1: no selection yet); every line below is about it.
//   state      up | overhead (the lead 30 degrees up or more: look up) | emergency | quiet | loading | error
//   look       where to look: "high in the NW · 38°", "right overhead · 64°", "low in the SW · 4°"
//   look_tone  accent-text from 30 degrees up (not on an emergency), else text-dim
//   status     the status chip: "7700 emergency" (error), "military" / "helicopter" (info), "" = none; status_tone
//   route      "Catania → Leeds", else the callsign / registration / hex; who "Jet2.com · LS13DK · Boeing 737-800"
//   alt_label  altitude | climbing | descending (past ±300 ft/min); alt "32,000 ft"; speed "540 mph"; away "26 km SE"
//   planes     the sky radar's aircraft, a JSON list of [km, bearing, track, mark] (mark 1 = the lead, 2 = an emergency,
//              3 = both), the lead last so it paints on top
//   also       the next two closest as List row items: [{"over": "B738 · 35,000 ft · 33 km NE", "title": route,
//              "trailing": "value", "value": status} (trailing "none" without one)]; more "all aircraft · 6"; many "1" with two or more aircraft (shows the rows)
//   empty      "1" with no aircraft (the hero shows the quiet words): empty_title "Clear skies" / "Loading" /
//              "Feed offline", empty_body "Nothing flying within 50 km right now."
//   card_text  the Home card: the route / "Clear skies"; card_sub "32,000 ft · 26 km SE" ("look up" while high, the
//              squawk on an emergency); card_accent "true" while high
// Adapters (the feed's settings, the spec's "feeds"; core/ha.h feed_config): source
//   ha      a Home Assistant sensor in the panel overhead shape (setting sensor, such as sensor.panel_overhead:
//           attribute a = "J" + rows [hex, callsign, type, alt, vs, gs, track, km*10, brg, elev*10, squawk, em, kind,
//           mil, route "A → B", who, reg, model, operator]; error = the feed's error; radius km), a live subscription
//   sample  six made-up aircraft round Wisbech (one high overhead, an air ambulance, an RAF tanker), drifting along
//           their tracks (a quarter speed, a two-minute loop) every 2 s so the radar moves, always passing
// More adapters (adsb.fi / adsb.lol read by the panel itself = stage 3) = more rows here, no screen change.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <vector>
#include "fw.h"
#include "ha.h"
#include "units.h"

namespace fw::dev::flights_source {
namespace {
constexpr const char *E = "panel.flights";
constexpr double LOOK_UP = 30, EDGE_KM = 50;
std::string cfg_seen, ent_now;
bool sampling = false;

struct Flight {
  std::string hex, cs, type, model, who, from, to, reg, squawk, em, kind;
  double alt = NAN, vs = NAN, gs = NAN, track = 0, km = 0, brg = 0, elev = 0;
  bool mil = false;
};

const char *compass(double brg) {
  static const char *const P[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
  return P[(int) std::lround(std::fmod(std::fmod(brg, 360) + 360, 360) / 45) % 8];
}
std::string thousands(long n) {
  std::string s = std::to_string(std::labs(n));
  for (int i = (int) s.size() - 3; i > 0; i -= 3) s.insert(i, ",");
  return (n < 0 ? "-" : "") + s;
}
std::string alt_text(const Flight &f) { return std::isnan(f.alt) ? "--" : thousands(std::lround(f.alt / 25) * 25) + " ft"; }
std::string away(const Flight &f) { return units::distance(f.km, units::Distance::km) + " " + compass(f.brg); }
std::string route(const Flight &f) {
  if (!f.from.empty() && !f.to.empty()) return f.from + " \xE2\x86\x92 " + f.to;
  return !f.cs.empty() ? f.cs : !f.reg.empty() ? f.reg : f.hex;
}
std::string look(const Flight &f) {
  char b[48];
  const int e = (int) std::lround(f.elev);
  if (f.elev >= 60) snprintf(b, sizeof b, "right overhead \xC2\xB7 %d\xC2\xB0", e);
  else snprintf(b, sizeof b, "%s %s \xC2\xB7 %d\xC2\xB0", f.elev >= LOOK_UP ? "high in the" : f.elev >= 10 ? "in the" : "low in the", compass(f.brg), e);
  return b;
}
std::string em_text(const std::string &em) {
  if (em == "nordo") return "radio failure";
  if (em == "unlawful") return "hijack";
  if (em == "minfuel") return "low fuel";
  if (em == "lifeguard") return "medical";
  if (em == "downed") return "aircraft down";
  return "emergency";
}
// the status chip: emergency (error), military, helicopter (info), else none
std::string status(const Flight &f, const char **tone) {
  *tone = "info";
  if (!f.em.empty()) { *tone = "error"; return (f.squawk.empty() ? "" : f.squawk + " ") + em_text(f.em); }
  if (f.mil) return "military";
  if (f.kind == "heli") return "helicopter";
  *tone = "idle";
  return "";
}
std::string json_str(const std::string &s) {
  std::string o = "\"";
  for (char c : s) o += c == '"' || c == '\\' ? std::string("\\") + c : std::string(1, c);
  return o + "\"";
}

void put_empty(const char *title, const std::string &body, const char *state) {
  auto put = [](const char *a, const std::string &x) { ha::put(E, a, x); };
  put("empty", "1"), put("many", ""), put("empty_title", title), put("empty_body", body);
  put("look", ""), put("look_tone", "text-dim"), put("status", ""), put("status_tone", "idle");
  put("route", ""), put("who", ""), put("alt_label", "altitude"), put("alt", "--"), put("speed", "--"), put("away", "--");
  put("planes", "[]"), put("also", "[]"), put("more", "all aircraft");
  put("card_text", title), put("card_sub", body.empty() ? "" : "nothing within 50 km"), put("card_accent", "false");
  put("", state);
}
void put_data(std::vector<Flight> fl, const std::string &error) {
  if (fl.empty()) {
    if (!error.empty()) put_empty("Feed offline", error.substr(0, 80), "error");
    else put_empty("Clear skies", "Nothing flying within 50 km right now.", "quiet");
    return;
  }
  auto put = [](const char *a, const std::string &x) { ha::put(E, a, x); };
  std::stable_sort(fl.begin(), fl.end(), [](const Flight &a, const Flight &b) { return a.km < b.km; });
  const Flight &t = fl.front();
  const bool high = t.elev >= LOOK_UP && t.em.empty();
  const char *tone;
  const std::string st = status(t, &tone);
  put("empty", ""), put("empty_title", ""), put("empty_body", "");
  put("look", look(t)), put("look_tone", high ? "accent-text" : "text-dim");
  put("status", st), put("status_tone", tone);
  put("route", route(t));
  put("who", !t.who.empty() ? t.who : t.type);
  put("alt_label", std::isnan(t.vs) || std::fabs(t.vs) < 300 ? "altitude" : t.vs > 0 ? "climbing" : "descending");
  put("alt", alt_text(t)), put("speed", units::speed_kt(t.gs, units::Speed::mph)), put("away", away(t));
  std::string pl = "[";
  char b[96];
  for (size_t k = 1; k <= fl.size(); k++) {
    const Flight &f = fl[k % fl.size()];  // 1, 2, ..., n - 1, then 0 (the lead)
    snprintf(b, sizeof b, "%s[%.1f,%.0f,%.0f,%d]", pl.size() > 1 ? "," : "", f.km, f.brg, f.track, (&f == &t ? 1 : 0) | (f.em.empty() ? 0 : 2));
    pl += b;
  }
  put("planes", pl + "]");
  std::string al = "[";
  for (size_t i = 1; i < fl.size() && i < 3; i++) {
    const Flight &f = fl[i];
    const char *tn;
    std::string over = f.type;
    over += (over.empty() ? "" : " \xC2\xB7 ") + alt_text(f) + " \xC2\xB7 " + away(f);
    const std::string st = status(f, &tn);  // a status at the row's end (the trailing value), else nothing there
    al += (al.size() > 1 ? "," : "") + std::string("{\"over\":") + json_str(over) + ",\"title\":" + json_str(route(f)) +
          (st.empty() ? std::string(",\"trailing\":\"none\"") : ",\"trailing\":\"value\",\"value\":" + json_str(st)) + "}";
  }
  put("also", al + "]");
  snprintf(b, sizeof b, "all aircraft \xC2\xB7 %d", (int) fl.size());
  put("more", b), put("many", fl.size() >= 2 ? "1" : "");
  put("card_text", route(t));
  put("card_sub", !t.em.empty() ? t.squawk + " " + em_text(t.em) + " \xC2\xB7 " + units::distance(t.km, units::Distance::km)
                                : alt_text(t) + " \xC2\xB7 " + (high ? std::string("look up") : away(t)));
  put("card_accent", high ? "true" : "false");
  put("", !t.em.empty() ? "emergency" : high ? "overhead" : "up");
}

// ---- adapter: ha (a sensor in the panel overhead shape)
void rebuild() {
  if (ent_now.empty()) return;
  const std::string *st = ha::get(ent_now), *a = ha::get(ent_now, "a"), *er = ha::get(ent_now, "error");
  if (!st) { put_empty("Loading", "", "loading"); return; }
  std::vector<Flight> fl;
  ArduinoJson::JsonDocument j;
  if (a && a->size() > 1 && !deserializeJson(j, a->c_str() + ((*a)[0] == 'J' ? 1 : 0)))
    for (ArduinoJson::JsonArrayConst r : j.as<ArduinoJson::JsonArrayConst>()) {
      Flight f;
      auto s = [&](int i) { return std::string(r[i] | ""); };
      auto n = [&](int i) { return r[i].is<double>() ? r[i].as<double>() : NAN; };
      f.hex = s(0), f.cs = s(1), f.type = s(2), f.alt = n(3), f.vs = n(4), f.gs = n(5), f.track = r[6] | 0.0;
      f.km = (r[7] | 0.0) / 10, f.brg = r[8] | 0.0, f.elev = (r[9] | 0.0) / 10, f.squawk = s(10), f.em = s(11), f.kind = s(12);
      f.mil = (r[13] | 0) != 0;
      const std::string rt = s(14);
      const size_t arrow = rt.find(" \xE2\x86\x92 ");
      if (arrow != std::string::npos) f.from = rt.substr(0, arrow), f.to = rt.substr(arrow + 5);
      f.who = s(15), f.reg = s(16), f.model = s(17);
      if (f.km <= EDGE_KM) fl.push_back(f);
    }
  const bool bad = *st == "unavailable" || *st == "unknown";
  put_data(fl, bad ? std::string("feed unavailable") : er ? *er : std::string());
}
void entity_put(const std::string &e, const std::string &a, const std::string &) {
  if (!ent_now.empty() && e == ent_now && (a.empty() || a == "a" || a == "error")) rebuild();
}

// ---- adapter: sample (Wisbech's sky; they drift along their tracks, 20x, and loop)
struct Seed { const char *hex, *cs, *type, *who, *from, *to, *reg, *kind; double alt, vs, gs, track, km, brg; const char *squawk; bool mil; };
const Seed SKY[] = {
    {"4ca8d2", "EZY45UK", "A20N", "easyJet \xC2\xB7 U245UK \xC2\xB7 Airbus A320neo", "Amsterdam", "Edinburgh", "G-UZHA", "jet", 29000, 0, 460, 330, 6.5, 300, "2000", false},
    {"4ca1f3", "RYR6BA", "B38M", "Ryanair \xC2\xB7 FR6BA \xC2\xB7 Boeing 737 MAX 8", "Krak\xC3\xB3w", "Leeds", "EI-IHR", "jet", 32000, 0, 452, 302, 26, 118, "2000", false},
    {"406a3c", "HLE19", "EC35", "East Anglian Air Ambulance \xC2\xB7 Airbus H135", "", "", "G-EAAA", "heli", 1500, 0, 120, 210, 12, 200, "0020", false},
    {"40792b", "EXS13DK", "B738", "Jet2.com \xC2\xB7 LS13DK \xC2\xB7 Boeing 737-800", "Catania", "Leeds", "G-JZHF", "jet", 35000, 0, 470, 318, 33, 42, "2000", false},
    {"3c6589", "DLH3VK", "A20N", "Lufthansa \xC2\xB7 LH922 \xC2\xB7 Airbus A320neo", "Frankfurt", "London", "D-AINK", "jet", 24000, -1100, 440, 255, 38, 140, "2000", false},
    {"43c6f1", "TARTN24", "A332", "Royal Air Force \xC2\xB7 Airbus Voyager KC2", "", "", "ZZ336", "jet", 18000, 0, 330, 75, 41, 250, "2000", true},
};
void sample_now() {
  constexpr double PI = 3.14159265358979, LOOP_S = 120;  // two minutes of drift, then back to the start
  const double t = std::fmod((double) time(nullptr), LOOP_S) / 3600;  // hours flown in this loop
  std::vector<Flight> fl;
  for (const Seed &s : SKY) {
    Flight f;
    f.hex = s.hex, f.cs = s.cs, f.type = s.type, f.who = s.who, f.from = s.from, f.to = s.to, f.reg = s.reg, f.kind = s.kind;
    f.alt = s.alt, f.vs = s.vs, f.gs = s.gs, f.track = s.track, f.squawk = s.squawk, f.mil = s.mil;
    // home at 0, 0: move the seed position along the track at the ground speed (kt -> km), then back to km + bearing
    const double d = s.gs * 1.852 * t / 4;  // a quarter of the real speed (at most ~7 km a loop): the lead stays the lead
    double x = s.km * std::sin(s.brg * PI / 180) + d * std::sin(s.track * PI / 180);
    double y = s.km * std::cos(s.brg * PI / 180) + d * std::cos(s.track * PI / 180);
    f.km = std::hypot(x, y), f.brg = std::fmod(std::atan2(x, y) * 180 / PI + 360, 360);
    f.elev = std::atan2(f.alt * 0.3048 / 1000, std::max(f.km, 0.05)) * 180 / PI;
    if (f.km <= EDGE_KM) fl.push_back(f);
  }
  put_data(fl, "");
}
void fresh() {
  static lv_timer_t *fast = nullptr;
  const std::string &cfg = ha::feed_config(E);
  ArduinoJson::JsonDocument c;
  deserializeJson(c, cfg.empty() ? "{}" : cfg.c_str());
  const std::string src = c["source"] | "ha", ent = c["sensor"] | "";
  const bool changed = cfg != cfg_seen;
  cfg_seen = cfg;
  static const bool hooked = (ha::on_put(entity_put), true);
  (void) hooked;
  sampling = src != "ha";
  if (src == "ha" && ent.rfind("sensor.", 0) == 0) {
    if (changed || ent != ent_now) {
      ent_now = ent;
      for (const char *a : {"", "a", "error"}) ha::want(ent, a);
    }
    rebuild();
  } else if (src == "ha") { ent_now.clear(); put_empty("No sensor", "", "error"); }
  else {
    ent_now.clear();
    sample_now();
    if (!fast) fast = lv_timer_create([](lv_timer_t *) { if (sampling) sample_now(); }, 2000, nullptr);
  }
}
[[maybe_unused]] const bool registered = (ha::on_local(E, fresh), true);  // binds on panel.flights never reach a host
}  // namespace
}  // namespace fw::dev::flights_source
