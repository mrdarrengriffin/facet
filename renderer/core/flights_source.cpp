// The flights feed (CONTRACT.md C51, C51a): the reserved entity panel.flights (core/spec.h "local sources"), the one
// shape the Overhead screen and its Home card bind to, whatever reports the aircraft. On the Feed base (core/feed.h).
// Same file on the panels and in WASM.
// The model: every adapter turns its source into Flights (one aircraft: hex, callsign, type, registration, operator,
// route, altitude ft, vertical speed ft/min, ground speed kt, track, latitude / longitude or distance + bearing from
// home, squawk, emergency, kind, military); the rest of this file (filters, the lead, words, units, trails) is shared.
// The lead = the selected aircraft (flights.select) while it is in the circle, else the closest; the hero, the card and
// the detail view are about it. Distances and speeds follow Settings > Units (distanceUnit, speedUnit).
// Fields:
//   unit, range  the radar's rings: km | mi and the circle's edge in that unit (10 | 25 | 50 | 100: the range setting)
//   look       where to look: "high in the NW · 38°", "right overhead · 64°", "low in the SW · 4°"; look_tone
//              accent-text from 30 degrees up (not on an emergency), else text-dim
//   status     the status after the look line: "7700 emergency" (error), "military" / "helicopter" (info), "" = none;
//              status_tone
//   route      "Catania → Leeds", else the callsign / registration / hex; who "Jet2.com · LS13DK · Boeing 737-800"
//   alt_label  altitude | climbing | descending (past ±300 ft/min); alt "32,000 ft"; speed "540 mph"; away "26 km SE"
//   planes     the Sky radar's aircraft: [[km, bearing, track, mark, callsign, hex, [trail km, bearing, ...]], ...]
//              (mark bits: 1 = the lead, 2 = an emergency, 4 = a helicopter, 8 = a military jet: the shape, #160; the
//              callsign on the 5 closest + the lead), the lead last
//   also       the next two closest as List row items; all = every aircraft, closest first; more "all aircraft · 6";
//              many "1" with two or more; list_head "closest first · within 50 km"
//   empty      "1" with no aircraft: empty_title / empty_body: the quiet words ("Clear skies", "Nothing flying within
//              50 km right now.") on an ok feed, the feed's words (Loading, Offline, Not found, Unavailable) otherwise;
//              stale "1" = the last read failed but the aircraft before it still show (the header's "offline · last known")
//   title_now / title_radar / title_list / title_detail  the header per view (the spec binds title_$view)
//   f_aircraft f_reg f_operator f_squawk f_squawk_tone f_heading f_vs f_src: the detail view's facts about the lead
//   found      Settings > Screens > Overhead, the From row: "Centred on Home · FlightRadar24 Current in area" (where the
//              radar is centred, from Home Assistant's home, and the sensor it found)
//   card_text, card_sub, card_accent  the Home card: the route / "Clear skies" / the feed's word; "32,000 ft · 26 km SE"
//              ("look up" while high, the squawk on an emergency); "true" while high
// Settings (the spec's "feeds"; core/ha.h feed_config): source, sensor ("" = found by itself: sources.json "finds",
// the integration's own sensor in HA's registry), lat, lon (home; "" = Home Assistant's zone.home, the default), range (10 | 25 | 50 | 100 in the distance unit), min_alt (feet: lower aircraft are left out; aircraft on
// the ground always are).
// Adapters (sources/sources.json "overhead", in order):
//   adsb_lol  the public adsb.lol API (open ADS-B data, ODbL, no key), read by the panel itself every 10 s while the
//             feed is shown (after a failure 20, 40, 80, 160 s, then every 5 min until one works): GET
//             https://api.adsb.lol/v2/point/<lat>/<lon>/<radius nm> (only the circle round home; ~520 B an aircraft);
//             readsb's JSON ("ac": hex, flight, r, t, alt_baro, gs, track, baro_rate, squawk, emergency, category,
//             dbFlags, lat, lon, dst, dir)
//   fr24      Home Assistant's FlightRadar24 integration (HACS): its "Current in area" sensor's attribute flights (a
//             list of {id, callsign, flight_number, aircraft_code, aircraft_model, aircraft_registration, airline,
//             airport_origin_city, airport_destination_city, latitude, longitude, altitude, ground_speed,
//             vertical_speed, heading, squawk, on_ground, distance}); Home Assistant may send it as Python text
//             ('...', None, True): read either way
//   adsb_ha   an ADS-B receiver (dump1090, readsb, tar1090) read by Home Assistant: a REST sensor on its
//             data/aircraft.json with json_attributes: aircraft (readsb's JSON, as adsb_lol)
//   sample    six made-up aircraft drifting along their tracks every 2 s (previews and tests)
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <map>
#include <vector>
#include "device.h"
#include "feed.h"
#include "fw.h"
#include "ha.h"
#include "net.h"
#include "sources.h"
#include "units.h"

namespace fw::dev::flights_source {
namespace {
constexpr double LOOK_UP = 30, PI = 3.14159265358979, KM_PER_NM = 1.852, EARTH_KM = 6371.0;
constexpr uint32_t POLL_MS = 10000;  // the public API: one read per 10 s while shown
constexpr int TRAIL = 6;             // trail points kept per aircraft
constexpr size_t MAX_PLANES = 40;    // the radar draws the closest 40 (+ the lead): the frame time stays in budget

struct Flight {
  std::string hex, cs, type, model, op, from, to, reg, squawk, em, kind;
  double alt = NAN, vs = NAN, gs = NAN, track = 0, km = NAN, brg = NAN, lat = NAN, lon = NAN, elev = 0;
  bool mil = false, ground = false;
};
std::vector<Flight> last;              // what the feed shows now, closest first (a selection re-puts it)
std::map<std::string, std::vector<std::pair<double, double>>> trails;  // hex -> [km, bearing] oldest first
std::string sel, src_name;

units::Distance du() { return units::distance_of(dev::setting("distanceUnit")); }
units::Speed su() { return units::speed_of(dev::setting("speedUnit")); }
const char *unit_word() { return du() == units::Distance::mi ? "mi" : "km"; }

const char *compass(double brg) {
  static const char *const P[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
  return P[(int) std::lround(std::fmod(std::fmod(brg, 360) + 360, 360) / 45) % 8];
}
std::string thousands(long n) {
  std::string s = std::to_string(std::labs(n));
  for (int i = (int) s.size() - 3; i > 0; i -= 3) s.insert(i, ",");
  return (n < 0 ? "-" : "") + s;
}
std::string trim(std::string s) {
  while (!s.empty() && isspace((unsigned char) s.back())) s.pop_back();
  size_t i = 0;
  while (i < s.size() && isspace((unsigned char) s[i])) i++;
  return s.substr(i);
}
std::string upper(std::string s) { for (char &c : s) c = (char) toupper((unsigned char) c); return s; }
std::string alt_text(const Flight &f) { return std::isnan(f.alt) ? "--" : thousands(std::lround(f.alt / 25) * 25) + " ft"; }
std::string away(const Flight &f) { return units::distance(f.km, du()) + " " + compass(f.brg); }
std::string name(const Flight &f) { return !f.cs.empty() ? f.cs : !f.reg.empty() ? f.reg : upper(f.hex); }
std::string route(const Flight &f) { return !f.from.empty() && !f.to.empty() ? f.from + " \xE2\x86\x92 " + f.to : name(f); }
std::string who(const Flight &f) {  // "Jet2.com · LS13DK · Boeing 737-800": what is known
  std::string s;
  for (const std::string &x : {f.op, !f.from.empty() && !f.to.empty() ? f.cs : std::string(), !f.model.empty() ? f.model : f.type})
    if (!x.empty()) s += (s.empty() ? "" : " \xC2\xB7 ") + x;
  return s;
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
  for (char c : s) {
    if (c == '"' || c == '\\') o += '\\', o += c;
    else if ((unsigned char) c < 0x20) o += ' ';
    else o += c;
  }
  return o + "\"";
}
std::string row(const Flight &f) {  // a List row item: "B738 · 32,000 ft · 26 km SE" over the route, a status at its end
  const char *tn;
  std::string over = f.type;
  over += (over.empty() ? "" : " \xC2\xB7 ") + alt_text(f) + " \xC2\xB7 " + away(f);
  const std::string st = status(f, &tn);
  return std::string("{\"over\":") + json_str(over) + ",\"title\":" + json_str(route(f)) +
         (st.empty() ? std::string(",\"trailing\":\"none\"") : ",\"trailing\":\"value\",\"value\":" + json_str(st)) + "}";
}
const Flight *lead_of(const std::vector<Flight> &fl) {
  for (const Flight &f : fl)
    if (!sel.empty() && f.hex == sel) return &f;
  return fl.empty() ? nullptr : &fl.front();
}
// distance (km) and bearing (degrees) from home to a point (haversine; the circle is small, so the error is nothing)
void from_home(double hlat, double hlon, Flight &f) {
  const double r = PI / 180, p1 = hlat * r, p2 = f.lat * r, dp = (f.lat - hlat) * r, dl = (f.lon - hlon) * r;
  const double a = std::sin(dp / 2) * std::sin(dp / 2) + std::cos(p1) * std::cos(p2) * std::sin(dl / 2) * std::sin(dl / 2);
  f.km = 2 * EARTH_KM * std::atan2(std::sqrt(a), std::sqrt(1 - a));
  f.brg = std::fmod(std::atan2(std::sin(dl) * std::cos(p2), std::cos(p1) * std::sin(p2) - std::sin(p1) * std::cos(p2) * std::cos(dl)) / r + 360, 360);
}

// ---------------------------------------------------------------- readers: a source's JSON -> Flights
// Home Assistant sends a list attribute as Python text ([{'a': None, 'b': True}]): made JSON here (strings in either
// quote kept whole, None / True / False -> null / true / false)
std::string py_to_json(const std::string &s) {
  std::string o;
  o.reserve(s.size() + 16);
  for (size_t i = 0; i < s.size(); i++) {
    const char c = s[i];
    if (c == '\'' || c == '"') {  // a string: re-quote it with "
      o += '"';
      for (i++; i < s.size() && s[i] != c; i++) {
        if (s[i] == '\\' && i + 1 < s.size()) {
          if (s[i + 1] == '\'') { o += '\''; i++; continue; }
          o += s[i], o += s[++i];
          continue;
        }
        if (s[i] == '"') o += "\\\"";
        else o += s[i];
      }
      o += '"';
      continue;
    }
    if (!s.compare(i, 4, "None")) { o += "null", i += 3; continue; }
    if (!s.compare(i, 4, "True")) { o += "true", i += 3; continue; }
    if (!s.compare(i, 5, "False")) { o += "false", i += 4; continue; }
    o += c;
  }
  return o;
}
// the shape the radar draws (#160), from the source's data: heli = ADS-B emitter category A7 (rotorcraft) or a helicopter
// type; fighter = the source's military flag with category A6 (high performance) or a fighter / trainer jet type; else
// jet (the airliner). ICAO type designators
bool in(const char *const *list, const std::string &t) { for (; *list; list++) if (t == *list) return true; return false; }
bool heli_type(const std::string &t) {
  static const char *const H[] = {"A109", "A119", "A129", "A139", "A149", "A169", "A189", "AS32", "AS3B", "AS50", "AS55", "AS65", "B06", "B06T",
                                  "B212", "B222", "B230", "B407", "B412", "B427", "B429", "B430", "B47G", "B505", "BK17", "EC20", "EC25", "EC30",
                                  "EC35", "EC45", "EC55", "EC75", "EH10", "EXPL", "G2CA", "H160", "H269", "H47", "H500", "H53", "H53S", "H60",
                                  "H64", "H72", "KMAX", "LYNX", "MI8", "MI17", "MI24", "NH90", "PUMA", "R22", "R44", "R66", "S61", "S76", "S92",
                                  "TIGR", "UH1", "UH1Y", "WILD", nullptr};
  return in(H, t);
}
bool fighter_type(const std::string &t) {
  static const char *const F[] = {"A10", "AJET", "AV8B", "EUFI", "F4", "F5", "F14", "F15", "F16", "F18", "F18S", "F22", "F35", "GRIF", "HAWK", "JS39",
                                  "L39", "L159", "M339", "M346", "MG29", "MIR2", "RFAL", "SU27", "SU30", "SU35", "T38", "T45", "T50", "TOR", nullptr};
  return in(F, t);
}
std::string kind_of(const std::string &category, const std::string &type, bool mil) {
  if (category == "A7" || heli_type(type)) return "heli";
  if (mil && (category == "A6" || fighter_type(type))) return "fighter";
  return "jet";
}
double num(ArduinoJson::JsonVariantConst v) {
  if (v.is<double>()) { const double d = v.as<double>(); return std::isfinite(d) ? d : NAN; }
  if (v.is<const char *>()) { char *e; const char *s = v.as<const char *>(); const double d = strtod(s, &e); return e != s && std::isfinite(d) ? d : NAN; }
  return NAN;
}
std::string str(ArduinoJson::JsonVariantConst v) { return v.is<const char *>() ? trim(v.as<const char *>()) : std::string(); }

// ---- the route lookup (#157): a shared step after every adapter. A flight with a callsign and no from / to (ADS-B data
// carries none: adsb.lol, a receiver through Home Assistant) is looked up once by callsign in the public adsbdb.com
// route database (no key: GET https://api.adsbdb.com/v0/callsign/<callsign>, ~1 KB: the origin and destination airports'
// city, IATA code and position, the airline). adsb.lol's own route lookup (routeset) answers nothing at the moment.
// The answer is kept per callsign while the flight is seen (and 15 minutes after), at most ROUTES_MAX; at most one
// lookup at a time, ROUTE_GAP_MS apart (the lead first, then the closest); an unknown callsign or a failed read is
// not asked again for ROUTE_RETRY_MS. A route is used only when it is plausible: the aircraft within 150 km (or a
// fifth of the route's length) of the great circle between the two airports, between them along it (100 km past either
// end), so a callsign reused for another route today shows the callsign, never a wrong from / to. A source that gives
// its own route (FlightRadar24) keeps it.
constexpr uint32_t ROUTE_GAP_MS = 3000, ROUTE_RETRY_MS = 10 * 60000, ROUTE_KEEP_MS = 15 * 60000;
constexpr size_t ROUTES_MAX = 64;
struct Route {
  int st = 0;  // 0 asking, 1 known, 2 unknown / failed (asked again after ROUTE_RETRY_MS)
  std::string from, to, from_iata, to_iata, op;
  double olat = NAN, olon = NAN, dlat = NAN, dlon = NAN;
  uint32_t seen = 0, at = 0;
};
// the great-circle distances (km) of a point from the route's line (cross-track) and along it from the origin
void track_km(double lat1, double lon1, double lat2, double lon2, double lat, double lon, double *xt, double *at, double *len) {
  const double r = PI / 180;
  auto dist = [&](double a1, double o1, double a2, double o2) {
    const double dp = (a2 - a1) * r, dl = (o2 - o1) * r;
    const double h = std::sin(dp / 2) * std::sin(dp / 2) + std::cos(a1 * r) * std::cos(a2 * r) * std::sin(dl / 2) * std::sin(dl / 2);
    return 2 * std::atan2(std::sqrt(h), std::sqrt(1 - h));  // radians
  };
  auto brg = [&](double a1, double o1, double a2, double o2) {
    const double dl = (o2 - o1) * r;
    return std::atan2(std::sin(dl) * std::cos(a2 * r), std::cos(a1 * r) * std::sin(a2 * r) - std::sin(a1 * r) * std::cos(a2 * r) * std::cos(dl));
  };
  const double d13 = dist(lat1, lon1, lat, lon), t13 = brg(lat1, lon1, lat, lon), t12 = brg(lat1, lon1, lat2, lon2);
  const double x = std::asin(std::sin(d13) * std::sin(t13 - t12));
  const double a = std::acos(std::max(-1.0, std::min(1.0, std::cos(d13) / std::max(1e-12, std::cos(x)))));
  *xt = std::fabs(x) * EARTH_KM;
  *at = (std::cos(t13 - t12) < 0 ? -a : a) * EARTH_KM;
  *len = dist(lat1, lon1, lat2, lon2) * EARTH_KM;
}
bool plausible(const Route &r, const Flight &f) {
  if (std::isnan(f.lat) || std::isnan(f.lon) || std::isnan(r.olat) || std::isnan(r.dlat)) return true;  // nothing to check against
  double xt, at, len;
  track_km(r.olat, r.olon, r.dlat, r.dlon, f.lat, f.lon, &xt, &at, &len);
  return xt <= std::max(150.0, len / 5) && at >= -100 && at <= len + 100;
}
// a callsign worth a lookup: an airline's ICAO code + a flight number (3 letters, then a digit); a registration or a
// made-up one (GBNHP, @@@@) is never in a route database
bool airline_cs(const std::string &cs) {
  if (cs.size() < 4 || cs.size() > 8) return false;
  for (int i = 0; i < 3; i++) if (!isalpha((unsigned char) cs[i])) return false;
  if (!isdigit((unsigned char) cs[3])) return false;
  for (char c : cs) if (!isalnum((unsigned char) c)) return false;
  return true;
}
// adsbdb's answer -> r (false = no route: "unknown callsign", or not readable)
bool read_route(const std::string &body, Route &r) {
  ArduinoJson::JsonDocument filter;
  for (const char *e : {"origin", "destination"})
    for (const char *k : {"municipality", "iata_code", "icao_code", "latitude", "longitude"}) filter["response"]["flightroute"][e][k] = true;
  filter["response"]["flightroute"]["airline"]["name"] = true;
  ArduinoJson::JsonDocument d;
  if (deserializeJson(d, body, ArduinoJson::DeserializationOption::Filter(filter))) return false;
  ArduinoJson::JsonObjectConst fr = d["response"]["flightroute"];
  if (fr.isNull()) return false;
  ArduinoJson::JsonObjectConst o = fr["origin"], e = fr["destination"];
  auto place = [](ArduinoJson::JsonObjectConst a) {  // the city, else the airport's code
    std::string c = str(a["municipality"]);
    return !c.empty() ? c : !str(a["iata_code"]).empty() ? str(a["iata_code"]) : str(a["icao_code"]);
  };
  r.from = place(o), r.to = place(e), r.from_iata = str(o["iata_code"]), r.to_iata = str(e["iata_code"]), r.op = str(fr["airline"]["name"]);
  r.olat = num(o["latitude"]), r.olon = num(o["longitude"]), r.dlat = num(e["latitude"]), r.dlon = num(e["longitude"]);
  return !r.from.empty() && !r.to.empty() && r.from != r.to;
}

// readsb / dump1090 / tar1090 / adsb.lol: {"ac": [...]} or {"aircraft": [...]} or the list itself
bool read_readsb(const std::string &body, double hlat, double hlon, std::vector<Flight> &out) {
  ArduinoJson::JsonDocument filter;  // only the keys used (a list, or an object holding one: two filter shapes)
  const size_t first = body.find_first_not_of(" \t\r\n");
  const bool list_top = first != std::string::npos && body[first] == '[';
  for (const char *k : {"hex", "flight", "r", "t", "desc", "ownOp", "alt_baro", "gs", "track", "baro_rate", "geom_rate", "squawk", "emergency",
                        "category", "dbFlags", "lat", "lon", "dst", "dir"}) {
    if (list_top) filter[0][k] = true;
    else filter["ac"][0][k] = true, filter["aircraft"][0][k] = true;
  }
  ArduinoJson::JsonDocument d;
  if (deserializeJson(d, body, ArduinoJson::DeserializationOption::Filter(filter))) return false;
  ArduinoJson::JsonArrayConst list = d.is<ArduinoJson::JsonArrayConst>() ? d.as<ArduinoJson::JsonArrayConst>()
                                     : d["ac"].is<ArduinoJson::JsonArrayConst>() ? d["ac"].as<ArduinoJson::JsonArrayConst>()
                                                                                 : d["aircraft"].as<ArduinoJson::JsonArrayConst>();
  if (list.isNull()) return false;
  for (ArduinoJson::JsonObjectConst a : list) {
    Flight f;
    f.hex = str(a["hex"]), f.cs = str(a["flight"]), f.reg = str(a["r"]), f.type = str(a["t"]), f.model = str(a["desc"]), f.op = str(a["ownOp"]);
    if (f.hex.empty()) continue;
    if (a["alt_baro"].is<const char *>() && !strcmp(a["alt_baro"].as<const char *>(), "ground")) f.ground = true;
    else f.alt = num(a["alt_baro"]);
    f.gs = num(a["gs"]), f.track = a["track"].is<double>() ? a["track"].as<double>() : 0;
    f.vs = num(a["baro_rate"]);
    if (std::isnan(f.vs)) f.vs = num(a["geom_rate"]);
    f.squawk = str(a["squawk"]);
    const std::string em = str(a["emergency"]);
    f.em = em == "none" ? "" : em;
    f.mil = ((int) (a["dbFlags"] | 0) & 1) != 0;
    f.kind = kind_of(str(a["category"]), f.type, f.mil);
    f.lat = num(a["lat"]), f.lon = num(a["lon"]);
    if (!std::isnan(f.lat) && !std::isnan(f.lon) && std::isfinite(hlat)) from_home(hlat, hlon, f);
    else if (!std::isnan(num(a["dst"])) && !std::isnan(num(a["dir"]))) f.km = num(a["dst"]) * KM_PER_NM, f.brg = num(a["dir"]);
    if (std::isnan(f.km)) continue;  // no position: not on the radar
    out.push_back(f);
  }
  return true;
}
// Home Assistant's FlightRadar24 integration: its sensor's attribute flights
bool read_fr24(const std::string &raw, double hlat, double hlon, std::vector<Flight> &out) {
  ArduinoJson::JsonDocument filter;
  for (const char *k : {"id", "aircraft_icao_24bit", "callsign", "flight_number", "aircraft_code", "aircraft_model", "aircraft_registration", "airline",
                        "airline_short", "airport_origin_city", "airport_destination_city", "latitude", "longitude", "altitude", "ground_speed",
                        "vertical_speed", "heading", "squawk", "on_ground", "distance"})
    filter[0][k] = true;
  ArduinoJson::JsonDocument d;
  const bool py = raw.find('\'') != std::string::npos || raw.find("None") != std::string::npos;
  if (deserializeJson(d, py ? py_to_json(raw) : raw, ArduinoJson::DeserializationOption::Filter(filter))) return false;
  if (!d.is<ArduinoJson::JsonArrayConst>()) return false;
  for (ArduinoJson::JsonObjectConst a : d.as<ArduinoJson::JsonArrayConst>()) {
    Flight f;
    f.hex = str(a["aircraft_icao_24bit"]);
    if (f.hex.empty()) f.hex = str(a["id"]);
    f.cs = str(a["callsign"]), f.type = str(a["aircraft_code"]), f.model = str(a["aircraft_model"]), f.reg = str(a["aircraft_registration"]);
    f.op = str(a["airline_short"]);
    if (f.op.empty()) f.op = str(a["airline"]);
    f.from = str(a["airport_origin_city"]), f.to = str(a["airport_destination_city"]);
    if (f.hex.empty()) continue;
    f.alt = num(a["altitude"]), f.gs = num(a["ground_speed"]), f.vs = num(a["vertical_speed"]);
    f.track = a["heading"].is<double>() ? a["heading"].as<double>() : 0;
    f.squawk = str(a["squawk"]);
    if (f.squawk == "7700" || f.squawk == "7600" || f.squawk == "7500") f.em = f.squawk == "7600" ? "nordo" : f.squawk == "7500" ? "unlawful" : "general";
    f.ground = (int) num(a["on_ground"]) == 1;
    f.kind = kind_of("", f.type, false);  // FlightRadar24 gives no category or military flag: the type alone
    f.lat = num(a["latitude"]), f.lon = num(a["longitude"]);
    if (!std::isnan(f.lat) && !std::isnan(f.lon) && std::isfinite(hlat)) from_home(hlat, hlon, f);
    if (std::isnan(f.km)) continue;
    out.push_back(f);
  }
  return true;
}

// ---------------------------------------------------------------- the feed
std::vector<feed::Field> flight_fields() {
  std::vector<feed::Field> v;
  for (const char *n : {"unit", "range", "planes", "empty", "card_text", "title_now", "title_radar", "title_list"}) v.push_back({n, true, ""});
  for (const char *n : {"look", "status", "route", "who", "also", "all", "many", "list_head", "empty_title", "empty_body", "stale", "title_detail",
                        "f_src", "card_sub", "more", "found"})
    v.push_back({n, false, ""});
  for (const char *n : {"alt", "speed", "away", "f_aircraft", "f_reg", "f_operator", "f_squawk", "f_heading", "f_vs"}) v.push_back({n, false, "--"});
  v.push_back({"look_tone", false, "text-dim"}), v.push_back({"status_tone", false, "idle"}), v.push_back({"alt_label", false, "altitude"});
  v.push_back({"f_squawk_tone", false, "dim"}), v.push_back({"card_accent", false, "false"});
  return v;
}

class Flights : public feed::Feed {
 public:
  Flights() : Feed("panel.flights", POLL_MS, "adsb_lol", flight_fields()) {}
  void again() { put_all(last, stale_); }  // a selection or a units change: the same aircraft again
  bool sampling() const { return sampling_; }

 protected:
  void fill() override {
    sampling_ = false;
    const std::string src = source();
    range_ = clamp_range(cfg()["range"] | "50");
    min_alt_ = strtod(cfg()["min_alt"] | "0", nullptr);
    if (!std::isfinite(min_alt_)) min_alt_ = 0;
    const std::string la = cfg()["lat"] | "", lo = cfg()["lon"] | "";
    const bool own = !la.empty() && !lo.empty();
    std::vector<std::pair<std::string, std::string>> rd;
    if (!own) rd.push_back({"zone.home", "latitude"}), rd.push_back({"zone.home", "longitude"});
    const std::string ent = sensor_of(src, cfg()["sensor"] | "");
    if (src == "fr24" || src == "adsb_ha") rd.push_back({ent, ""}), rd.push_back({ent, src == "fr24" ? "flights" : "aircraft"}), rd.push_back({ent, "friendly_name"});
    if (src == "adsb_ha") rd.push_back({ent, "ac"});
    reads(rd);
    if (changed()) last.clear(), trails.clear(), sel.clear(), fetched_ = 0, err_.clear(), fails_ = 0, have_ = false;
    // home: the settings' place, else Home Assistant's
    hlat_ = NAN, hlon_ = NAN;
    if (own) hlat_ = strtod(la.c_str(), nullptr), hlon_ = strtod(lo.c_str(), nullptr);
    else if (const std::string *a = ha::get("zone.home", "latitude"), *b = ha::get("zone.home", "longitude"); a && b && !a->empty() && !b->empty())
      hlat_ = strtod(a->c_str(), nullptr), hlon_ = strtod(b->c_str(), nullptr);
    const bool home = std::isfinite(hlat_) && std::isfinite(hlon_) && std::fabs(hlat_) <= 90 && std::fabs(hlon_) <= 180;
    {  // Settings > Screens > Overhead, the From row: where the radar is centred and what it reads
      std::string f = !home ? std::string("No home location in Home Assistant") : own ? std::string("Centred on the place set here") : std::string("Centred on Home");
      if ((src == "fr24" || src == "adsb_ha") && !ent.empty()) {
        const std::string *fn = ha::get(ent, "friendly_name");
        f += " \xC2\xB7 " + (fn && !fn->empty() ? *fn : ent);
      }
      put("found", f);
    }
    if (src == "fr24" || src == "adsb_ha") {
      src_name = src == "fr24" ? "FlightRadar24" : "ADS-B receiver";
      const std::string *st = ha::get(ent);
      const feed::State s = ent.rfind("sensor.", 0) ? feed::MISSING : feed::of(st);
      if (s != feed::OK) { put_word(s); return; }
      if (!home) { put_word(feed::ERROR); return; }  // nowhere to centre the radar: not set up
      const std::string *a = ha::get(ent, src == "fr24" ? "flights" : "aircraft");
      if (src == "adsb_ha" && (!a || a->empty())) a = ha::get(ent, "ac");
      std::vector<Flight> fl;
      if (!a || a->empty() || (src == "fr24" ? !read_fr24(*a, hlat_, hlon_, fl) : !read_readsb(*a, hlat_, hlon_, fl))) {
        if (!a) { put_word(feed::LOADING); return; }
        fl.clear();  // an empty or unreadable list: nothing flying as far as it knows
      }
      take(fl, false);
      return;
    }
    // adsb_lol: the panel reads the public API itself
    src_name = "adsb.lol";
    if (!home) { put_word(feed::ERROR); return; }
    const uint32_t now = lv_tick_get();
    // every 10 s while shown; after a failure 20, 40, 80, 160 s, then every 5 min until a read works
    const uint32_t every = std::min<uint32_t>(POLL_MS << std::min(fails_, 5), 300000);
    if (!reading_ && (!fetched_ || lv_tick_elaps(fetched_) >= every - 500)) {
      char url[128];
      const double km = units::from_km(1, du()) ? range_ / units::from_km(1, du()) : range_;  // the range in km
      snprintf(url, sizeof url, "https://api.adsb.lol/v2/point/%.3f/%.3f/%d", hlat_, hlon_, (int) std::min(250.0, std::ceil(km / KM_PER_NM)));
      reading_ = true;
      fetched_ = now ? now : 1;
      fw::net::get(url, [this](bool ok, const std::string &body, const std::string &why) {
        reading_ = false;
        std::vector<Flight> fl;
        if (ok && read_readsb(body, hlat_, hlon_, fl)) got_ = std::move(fl), err_.clear(), have_ = true, fails_ = 0;
        else err_ = why.empty() ? std::string("unreadable answer") : why, fails_++;
        run();
      });
    }
    if (!have_) { put_word(err_.empty() ? feed::LOADING : feed::ERROR); return; }
    take(got_, !err_.empty());
  }
  void sample() override {
    sampling_ = true;
    range_ = clamp_range(cfg()["range"] | "50");
    min_alt_ = strtod(cfg()["min_alt"] | "0", nullptr);
    if (!std::isfinite(min_alt_)) min_alt_ = 0;
    src_name = "sample";
    if (!sample_tick_) sample_tick_ = lv_timer_create([](lv_timer_t *t) {
      auto *f = (Flights *) lv_timer_get_user_data(t);
      if (f->sampling_ && ha::watched("panel.flights")) f->run();
    }, 2000, this);
    take(sky(), false);
  }

 private:
  bool sampling_ = false, reading_ = false, have_ = false, stale_ = false;
  int fails_ = 0;  // reads failed in a row (the backoff)
  double range_ = 50, min_alt_ = 0, hlat_ = NAN, hlon_ = NAN;
  uint32_t fetched_ = 0;
  std::string err_;
  std::map<std::string, Route> routes_;  // callsign -> its route (#157)
  std::string asking_;
  uint32_t asked_at_ = 0, lookups_ = 0;
  std::vector<Flight> got_;
  lv_timer_t *sample_tick_ = nullptr, *route_tick_ = nullptr;

  // the sensor a Home Assistant source reads: the one set (the Source sheet, when HA has more than one), else the one
  // found (sources.json "finds": the integration's own sensor in HA's registry, else its usual id); again when the
  // registry changes
  std::string found_src_, found_ent_;
  uint32_t found_rev_ = 0;
  std::string sensor_of(const std::string &src, const std::string &set) {
    if (!set.empty() || (src != "fr24" && src != "adsb_ha")) return set;
    if (src == found_src_ && dev::ha_link::rev() == found_rev_ && !found_ent_.empty()) return found_ent_;
    ArduinoJson::JsonDocument d;
    found_src_ = src, found_rev_ = dev::ha_link::rev(), found_ent_.clear();
    if (fw::src::load(d))
      for (ArduinoJson::JsonVariantConst x : fw::src::of(d, "overhead"))
        if (src == (x["adapter"] | "")) found_ent_ = fw::src::found(x["finds"]);
    return found_ent_;
  }
  static double clamp_range(const char *s) {
    const double r = strtod(s, nullptr);
    for (double x : {10.0, 25.0, 50.0, 100.0}) if (r <= x) return x;
    return 100;
  }
  double edge_km() const { return du() == units::Distance::mi ? range_ * units::KM_PER_MI : range_; }
  std::string edge_text() const { char b[24]; snprintf(b, sizeof b, "%g %s", range_, unit_word()); return b; }
  // the route lookup's step (see "the route lookup" above): fill what is known, ask for the next one missing
  void routes(std::vector<Flight> &fl) {
    const uint32_t now = lv_tick_get() | 1;
    std::vector<std::string> want;
    for (int pass = 0; pass < 2; pass++)  // the lead (selected) first, then the closest
      for (Flight &f : fl) {
        if ((pass == 0) != (!sel.empty() && f.hex == sel)) continue;
        if ((!f.from.empty() && !f.to.empty()) || !airline_cs(f.cs)) continue;
        auto it = routes_.find(f.cs);
        if (it == routes_.end()) { want.push_back(f.cs); continue; }
        Route &r = it->second;
        r.seen = now;
        if (r.st == 2 && lv_tick_diff(now, r.at) >= ROUTE_RETRY_MS && f.cs != asking_) { want.push_back(f.cs); continue; }
        if (r.st != 1 || !plausible(r, f)) continue;
        f.from = r.from, f.to = r.to;
        if (f.op.empty()) f.op = r.op;
      }
    for (auto it = routes_.begin(); it != routes_.end();)  // gone for 15 minutes: forgotten
      if (it->first != asking_ && lv_tick_diff(now, it->second.seen) >= ROUTE_KEEP_MS) it = routes_.erase(it); else ++it;
    if (want.empty() || !asking_.empty() || (asked_at_ && lv_tick_diff(now, asked_at_) < ROUTE_GAP_MS)) {
      if (!want.empty() && asking_.empty() && !route_tick_) {  // asked too recently: once the gap is over
        route_tick_ = lv_timer_create([](lv_timer_t *t) { auto *f = (Flights *) lv_timer_get_user_data(t); f->route_tick_ = nullptr; if (ha::watched("panel.flights")) f->run(); }, ROUTE_GAP_MS, this);
        lv_timer_set_repeat_count(route_tick_, 1);
      }
      return;
    }
    while (routes_.size() >= ROUTES_MAX) {  // full: the longest unseen goes
      auto old = routes_.begin();
      for (auto it = routes_.begin(); it != routes_.end(); ++it) if (lv_tick_diff(now, it->second.seen) > lv_tick_diff(now, old->second.seen)) old = it;
      routes_.erase(old);
    }
    const std::string cs = want.front();
    Route &r = routes_[cs];
    r.st = 0, r.seen = now, r.at = now;
    asking_ = cs, asked_at_ = now, lookups_++;
    fw::net::get("https://api.adsbdb.com/v0/callsign/" + cs, [this, cs](bool ok, const std::string &body, const std::string &) {
      if (asking_ == cs) asking_.clear();
      auto it = routes_.find(cs);
      if (it == routes_.end()) return;
      Route got;
      got.seen = it->second.seen;
      got.at = lv_tick_get() | 1;
      got.st = ok && read_route(body, got) ? 1 : 2;
      it->second = got;
      if (ha::watched("panel.flights")) run();
    }, 8 * 1024);
  }
  // the filters and the trails, then every field
  void take(std::vector<Flight> fl, bool stale) {
    const double edge = edge_km();
    fl.erase(std::remove_if(fl.begin(), fl.end(), [&](const Flight &f) {
      return f.ground || !(f.km <= edge) || (min_alt_ > 0 && !(f.alt >= min_alt_));
    }), fl.end());
    for (Flight &f : fl) f.elev = std::atan2((std::isnan(f.alt) ? 0 : f.alt) * 0.3048 / 1000, std::max(f.km, 0.05)) * 180 / PI;
    std::stable_sort(fl.begin(), fl.end(), [](const Flight &a, const Flight &b) { return a.km < b.km; });
    std::map<std::string, std::vector<std::pair<double, double>>> next;
    for (const Flight &f : fl) {
      auto &t = next[f.hex];
      if (auto it = trails.find(f.hex); it != trails.end()) t = it->second;
      if (t.empty() || std::hypot(t.back().first - f.km, (t.back().second - f.brg) / 57.3 * f.km) > 0.2) t.push_back({f.km, f.brg});
      if ((int) t.size() > TRAIL) t.erase(t.begin());
    }
    trails.swap(next);
    if (!sampling_) routes(fl);
    last = fl;
    put_all(last, stale);
  }
  void put_common() {
    char b[16];
    put("unit", unit_word());
    snprintf(b, sizeof b, "%g", range_), put("range", b);
    put("title_now", "Overhead"), put("title_radar", "Overhead"), put("title_list", "All aircraft");
    put("list_head", "closest first \xC2\xB7 within " + edge_text());
  }
  // a feed state that is not ok: its word everywhere the aircraft would be
  void put_word(feed::State s) {
    state(s);
    put_common();
    put("planes", "[]"), put("empty", "1"), put("empty_title", feed::words(s)), put("card_text", feed::words(s));
    put("stale", "");
  }
  void put_all(const std::vector<Flight> &fl, bool stale) {
    stale_ = stale;
    state(feed::OK);
    put_common();
    if (fl.empty()) {
      put("planes", "[]"), put("empty", "1"), put("empty_title", "Clear skies");
      put("empty_body", "Nothing flying within " + edge_text() + " right now."), put("stale", stale ? "1" : "");
      put("card_text", "Clear skies"), put("card_sub", "nothing within " + edge_text());
      return;
    }
    const Flight &t = *lead_of(fl);
    const bool high = t.elev >= LOOK_UP && t.em.empty();
    const char *tone;
    const std::string st = status(t, &tone);
    put("empty", ""), put("stale", stale ? "1" : "");
    put("look", look(t)), put("look_tone", high ? "accent-text" : "text-dim");
    put("status", st), put("status_tone", tone);
    put("route", route(t)), put("who", who(t));
    put("alt_label", std::isnan(t.vs) || std::fabs(t.vs) < 300 ? "altitude" : t.vs > 0 ? "climbing" : "descending");
    put("alt", alt_text(t)), put("speed", units::speed_kt(t.gs, su())), put("away", away(t));
    std::string pl = "[";
    char b[64];
    auto plane = [&](const Flight &f, size_t i) {
      const bool lead = &f == &t;
      snprintf(b, sizeof b, "%s[%.1f,%.0f,%.0f,%d,", pl.size() > 1 ? "," : "", f.km, f.brg, f.track, (lead ? 1 : 0) | (f.em.empty() ? 0 : 2) | (f.kind == "heli" ? 4 : f.kind == "fighter" ? 8 : 0));
      pl += b + json_str(i < 5 || lead ? name(f) : "") + "," + json_str(f.hex) + ",[";
      const auto it = trails.find(f.hex);
      if (it != trails.end())
        for (size_t k = 0; k + 1 < it->second.size(); k++) {  // the points before now
          snprintf(b, sizeof b, "%s%.1f,%.0f", k ? "," : "", it->second[k].first, it->second[k].second);
          pl += b;
        }
      pl += "]]";
    };
    for (size_t i = 0; i < fl.size() && i < MAX_PLANES; i++)  // the closest MAX_PLANES on the radar (the list has all)
      if (&fl[i] != &t) plane(fl[i], i);
    plane(t, 0);
    put("planes", pl + "]");
    std::string al = "[", all = "[";
    int n = 0;
    for (const Flight &f : fl) {
      all += (all.size() > 1 ? "," : "") + row(f);
      if (&f != &t && n < 2) al += (n++ ? "," : "") + row(f);
    }
    put("also", al + "]"), put("all", all + "]");
    snprintf(b, sizeof b, "all aircraft \xC2\xB7 %d", (int) fl.size());
    put("more", b), put("many", fl.size() >= 2 ? "1" : "");
    auto or_dash = [](const std::string &s) { return s.empty() ? std::string("--") : s; };
    put("title_detail", name(t));
    put("f_aircraft", or_dash(!t.model.empty() ? t.model : t.type)), put("f_reg", or_dash(t.reg)), put("f_operator", or_dash(t.op));
    put("f_squawk", or_dash(t.squawk)), put("f_squawk_tone", t.em.empty() ? "dim" : "warn");
    snprintf(b, sizeof b, "%ld\xC2\xB0 %s", std::lround(t.track), compass(t.track)), put("f_heading", b);
    if (std::isnan(t.vs)) put("f_vs", "--");
    else if (std::fabs(t.vs) < 300) put("f_vs", "level");
    else put("f_vs", (t.vs > 0 ? "+" : "\xE2\x88\x92") + thousands(std::lround(std::fabs(t.vs) / 100) * 100) + " ft/min");
    put("f_src", upper(t.hex) + " \xC2\xB7 data " + src_name);
    put("card_text", route(t));
    put("card_sub", !t.em.empty() ? status(t, &tone) : alt_text(t) + " \xC2\xB7 " + (high ? std::string("look up") : away(t)));
    put("card_accent", high ? "true" : "false");
  }

  // the sample sky: six made-up aircraft drifting along their tracks (a quarter speed, a two-minute loop)
  struct Seed { const char *hex, *cs, *type, *model, *op, *from, *to, *reg, *kind; double alt, vs, gs, track, km, brg; const char *squawk; bool mil; };
  static std::vector<Flight> sky() {
    static const Seed SKY[] = {
        {"4ca8d2", "EZY45UK", "A20N", "Airbus A320neo", "easyJet", "Amsterdam", "Edinburgh", "G-UZHA", "jet", 29000, 0, 460, 330, 6.5, 300, "2000", false},
        {"4ca1f3", "RYR6BA", "B38M", "Boeing 737 MAX 8", "Ryanair", "Krak\xC3\xB3w", "Leeds", "EI-IHR", "jet", 32000, 0, 452, 302, 26, 118, "2000", false},
        {"406a3c", "HLE19", "EC35", "Airbus H135", "Air ambulance", "", "", "G-HEMS", "heli", 1500, 0, 120, 210, 12, 200, "0020", false},
        {"40792b", "EXS13DK", "B738", "Boeing 737-800", "Jet2.com", "Catania", "Leeds", "G-JZHF", "jet", 35000, 0, 470, 318, 33, 42, "2000", false},
        {"3c6589", "DLH3VK", "A20N", "Airbus A320neo", "Lufthansa", "Frankfurt", "London", "D-AINK", "jet", 24000, -1100, 440, 255, 38, 140, "2000", false},
        {"43c6f1", "TARTN24", "A332", "Airbus Voyager KC2", "Royal Air Force", "", "", "ZZ336", "jet", 18000, 0, 330, 75, 41, 250, "2000", true},
    };
    const double t = std::fmod((double) time(nullptr), 120.0) / 3600;  // hours flown in this loop
    std::vector<Flight> fl;
    for (const Seed &s : SKY) {
      Flight f;
      f.hex = s.hex, f.cs = s.cs, f.type = s.type, f.model = s.model, f.op = s.op, f.from = s.from, f.to = s.to, f.reg = s.reg, f.kind = s.kind;
      f.alt = s.alt, f.vs = s.vs, f.gs = s.gs, f.track = s.track, f.squawk = s.squawk, f.mil = s.mil;
      const double d = s.gs * 1.852 * t / 4;
      const double x = s.km * std::sin(s.brg * PI / 180) + d * std::sin(s.track * PI / 180);
      const double y = s.km * std::cos(s.brg * PI / 180) + d * std::cos(s.track * PI / 180);
      f.km = std::hypot(x, y), f.brg = std::fmod(std::atan2(x, y) * 180 / PI + 360, 360);
      fl.push_back(f);
    }
    return fl;
  }
};
Flights F;
void units_changed(const char *id, const char *, dev::From) {
  if (!strcmp(id, "distanceUnit") || !strcmp(id, "speedUnit")) F.run();
}
[[maybe_unused]] const bool observed = (dev::observe(units_changed), true);
}  // namespace

// flights.select (core/spec.h): an aircraft by hex, callsign or route (a radar plane, a row; "" = the hero = the lead).
// The lead again = true (the screen opens its detail view); another one becomes the lead (the feed is put again).
bool select(const std::string &d) {
  const Flight *lead = lead_of(last);
  if (d.empty()) return lead != nullptr;
  const Flight *f = nullptr;
  for (const Flight &x : last)
    if (x.hex == d || x.cs == d || name(x) == d) { f = &x; break; }
  if (!f)
    for (const Flight &x : last)
      if (route(x) == d) { f = &x; break; }
  if (!f) return false;
  if (lead && f->hex == lead->hex) return true;
  sel = f->hex;
  F.again();
  return false;
}
}  // namespace fw::dev::flights_source
