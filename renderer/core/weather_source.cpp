// The weather feed (CONTRACT.md C51): the reserved entity panel.weather (core/spec.h "local sources") in the HA store,
// the one shape the Weather screen, its next 5 days and its Home card bind to, whatever gives the forecast. Same file
// on the panels and in WASM. Words and units are Facet's (core/weather.h: the condition's words, whole degrees).
//   state      ok | loading (no weather yet) | error (the source is unavailable)
//   icon       the weather icon now ("partly-day", "rain-light", "clear-night": by day or night)
//   temp       "13°" ("--°" while loading / on error); condition "Partly cloudy" ("Loading" / "Offline")
//   line       "Feels 11° · high 16°, low 8°" (the parts it knows)
//   rain       "Rain from 14:00" / "Dry for the next hour · rain at 16:00" / "Dry for the next hour"; rain_tone
//              info (rain within the hour) | idle (a Status line's dot)
//   card       the Home card's line: "rain in 20 min" / "raining now" within the hour, else the condition in lower
//              case ("partly cloudy"), "loading" / "offline"
//   hours      the next eight hours, a JSON list of Weather icon columns: [{"label": "now" | "14", "icon": "...",
//              "value": "13°", "accent": "on" | "off"}]; has_hours "1" when known, else "" (the screen hides the strip)
//   days       the next five days, a JSON list of Weather icon rows: [{"label": "Today" | "Saturday", "icon": "...",
//              "line": "Rain · 4.2 mm", "low": "8°", "value": "16°", "divider": "off" for the first, else "on"}]
// Adapters (the feed's settings, the spec's "feeds"; core/ha.h feed_config): source
//   ha      a Home Assistant weather entity (setting weather, such as weather.forecast_home): the condition, temperature
//           and apparent_temperature are a live subscription like any bind; the next hours and days come from
//           weather.get_forecasts (core/ha.h ask: hourly, then daily) on build, every 30 minutes and on a new hour.
//           Without action responses (or a failed ask) the screen shows the weather now only, never an error.
//           HA forecasts come by the hour, so "rain from" is the first wet hour (the docs used 15-minute steps).
//   sample  the docs forecast (core/weather.h sample: today, real hours), for previews and tests
// Day or night (the icon): the sun at lat / lon (feed settings, default the docs sample place), core/sun.h.
// More adapters (Open-Meteo called by the panel itself, Met Office ...) = more rows here, no screen change.
#include <cstdio>
#include <ctime>
#include "fw.h"
#include "ha.h"
#include "sun.h"
#include "weather.h"

namespace fw::dev::weather_source {
namespace {
constexpr const char *E = "panel.weather";
constexpr double HOUR = 3600000;
lv_timer_t *tick = nullptr;
std::string cfg_seen, ent_now;
double lat = 52.2, lon = -1.2;
// the forecasts an ask brought (ha adapter): ms times
std::vector<wx::Hour> hours_got;
std::vector<wx::Day> days_got;
std::vector<std::string> conds;  // the condition texts the Hour / Day entries point at (kept alive here)
bool waiting_h = false, waiting_d = false;
double fetched_hour = -1;
uint32_t fetched_at = 0;

double now_ms() { return (double) time(nullptr) * 1000; }
std::string hm(double ms) {
  const time_t t = (time_t) (ms / 1000);
  struct tm tm;
  localtime_r(&t, &tm);
  char b[8];
  snprintf(b, sizeof b, "%02d:%02d", tm.tm_hour, tm.tm_min);
  return b;
}
bool is_day(double t) {
  const sun::Times s = sun::times(t, lat, lon);
  return !std::isnan(s.rise) && t >= s.rise && t < s.set;
}
long days_from_civil(int y, int m, int d) {  // days since 1970-01-01 (H. Hinnant)
  y -= m <= 2;
  const long era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = (unsigned) (y - era * 400), doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  return era * 146097 + (long) (yoe * 365 + yoe / 4 - yoe / 100 + doy) - 719468;
}
// an ISO time ("2026-10-29T14:00:00+00:00", "...Z") -> ms; NAN = not one
double iso_ms(const char *s) {
  int y, mo, d, h = 0, mi = 0, se = 0, n = 0;
  if (!s || sscanf(s, "%d-%d-%dT%d:%d:%d%n", &y, &mo, &d, &h, &mi, &se, &n) < 5) return NAN;
  double t = ((double) days_from_civil(y, mo, d) * 86400 + h * 3600 + mi * 60 + se) * 1000;
  const char *z = strpbrk(s + 10, "+-Z");
  if (z && *z != 'Z') {
    int oh = 0, om = 0;
    if (sscanf(z + 1, "%d:%d", &oh, &om) >= 1) t -= (*z == '-' ? -1 : 1) * (oh * 3600 + om * 60) * 1000.0;
  }
  return t;
}
const char *keep(const char *c) {  // a condition text that outlives the reply's JSON
  for (const std::string &x : conds) if (x == c) return x.c_str();
  conds.reserve(32);
  if (conds.size() >= 32) return "";
  conds.push_back(c);
  return conds.back().c_str();
}
double num(const std::string *s) {  // a number attribute; missing, empty, "None" or "unknown" = NAN
  if (!s || s->empty()) return NAN;
  char *end = nullptr;
  const double v = strtod(s->c_str(), &end);
  return end == s->c_str() ? NAN : v;
}
std::string words(const char *c) { return *wx::text(c) ? wx::text(c) : c; }
std::string lower(std::string s) {
  for (char &ch : s) ch = (char) tolower((unsigned char) ch);
  return s;
}

// the feed from one Data (the sample's, or what the ha adapter gathered)
void put_data(const wx::Data &d, double now) {
  auto put = [](const char *a, const std::string &x) { ha::put(E, a, x); };
  put("", "ok");
  put("icon", wx::icon(d.condition, is_day(now)));
  put("temp", std::isnan(d.temp) ? "--\xC2\xB0" : wx::deg(d.temp));
  put("condition", words(d.condition));
  std::string line;
  if (!std::isnan(d.feels)) line = "Feels " + wx::deg(d.feels);
  if (d.n_daily > 0) line += (line.empty() ? "high " : " \xC2\xB7 high ") + wx::deg(d.daily[0].hi) + ", low " + wx::deg(d.daily[0].lo);
  put("line", line);
  char b[96];
  if (!std::isnan(d.rain_from)) snprintf(b, sizeof b, "Rain from %s", hm(d.rain_from).c_str());
  else if (!std::isnan(d.next_rain)) snprintf(b, sizeof b, "Dry for the next hour \xC2\xB7 rain at %s", hm(d.next_rain).c_str());
  else snprintf(b, sizeof b, "Dry for the next hour");
  put("rain", d.n_hourly ? b : "");
  put("rain_tone", std::isnan(d.rain_from) ? "idle" : "info");
  if (!std::isnan(d.rain_from)) {
    const long m = std::lround((d.rain_from - now) / 60000);
    snprintf(b, sizeof b, m <= 0 ? "raining now" : "rain in %ld min", m);
    put("card", b);
  } else put("card", lower(words(d.condition)));
  ArduinoJson::JsonDocument hs;
  for (int i = 0; i < 8 && i < d.n_hourly; i++) {
    const wx::Hour &h = d.hourly[i];
    ArduinoJson::JsonObject o = hs.add<ArduinoJson::JsonObject>();
    o["label"] = i == 0 ? "now" : hm(h.t).substr(0, 2);
    o["icon"] = wx::icon(h.condition, is_day(h.t), h.precip);
    o["value"] = wx::deg(h.temp);
    o["accent"] = i == 0 ? "on" : "off";
  }
  std::string js;
  if (hs.isNull()) js = "[]"; else serializeJson(hs, js);
  put("hours", js);
  put("has_hours", d.n_hourly ? "1" : "");
  static const char *WD[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
  ArduinoJson::JsonDocument ds;
  for (int i = 0; i < 5 && i < d.n_daily; i++) {
    const wx::Day &x = d.daily[i];
    const time_t t = (time_t) (x.t / 1000);
    struct tm tm;
    localtime_r(&t, &tm);
    ArduinoJson::JsonObject o = ds.add<ArduinoJson::JsonObject>();
    o["label"] = i == 0 ? "Today" : WD[tm.tm_wday];
    o["icon"] = wx::icon(x.condition, true, x.precip);
    if (x.precip > 0) snprintf(b, sizeof b, "%s \xC2\xB7 %.1f mm", words(x.condition).c_str(), x.precip);
    else snprintf(b, sizeof b, "%s", words(x.condition).c_str());
    o["line"] = b;
    o["low"] = wx::deg(x.lo);
    o["value"] = wx::deg(x.hi);
    o["divider"] = i ? "on" : "off";
  }
  js.clear();
  if (ds.isNull()) js = "[]"; else serializeJson(ds, js);
  put("days", js);
}
void put_empty(const char *state) {  // loading / error: the words only (no icon, rain line or hours: the docs)
  const bool err = !strcmp(state, "error");
  ha::put(E, "", state);
  ha::put(E, "icon", "cloudy");
  ha::put(E, "temp", "--\xC2\xB0");
  ha::put(E, "condition", err ? "Offline" : "Loading");
  ha::put(E, "line", "");
  ha::put(E, "rain", "");
  ha::put(E, "rain_tone", "idle");
  ha::put(E, "card", err ? "offline" : "loading");
  ha::put(E, "hours", "[]");
  ha::put(E, "has_hours", "");
  ha::put(E, "days", "[]");
}

// ---- ha adapter
void rebuild() {
  if (ent_now.empty()) return;
  const std::string *state = ha::get(ent_now), *t = ha::get(ent_now, "temperature"), *f = ha::get(ent_now, "apparent_temperature");
  if (!state || state->empty()) { put_empty("loading"); return; }
  if (*state == "unavailable" || *state == "unknown") { put_empty("error"); return; }
  const double now = now_ms();
  wx::Data d;
  d.condition = keep(state->c_str());
  d.temp = num(t);
  d.feels = num(f);  // weather.forecast_home (Met.no) has no apparent_temperature: no "Feels" then
  for (const wx::Hour &h : hours_got) {  // from the hour holding now
    if (h.t + HOUR <= now) continue;
    if (d.n_hourly >= 12) break;
    d.hourly[d.n_hourly++] = h;
  }
  for (int i = 0; i < d.n_hourly; i++) {  // rain within the hour, else the next wet hour
    const wx::Hour &h = d.hourly[i];
    if (!(h.precip >= 0.1)) continue;
    if (h.t <= now + HOUR) d.rain_from = std::max(h.t, now);
    else if (std::isnan(d.next_rain)) d.next_rain = h.t;
    break;
  }
  const time_t tn = (time_t) (now / 1000);
  struct tm a;
  localtime_r(&tn, &a);
  for (const wx::Day &x : days_got) {  // from today (local dates)
    const time_t tx = (time_t) (x.t / 1000);
    struct tm b;
    localtime_r(&tx, &b);
    if (b.tm_year < a.tm_year || (b.tm_year == a.tm_year && b.tm_yday < a.tm_yday)) continue;
    if (d.n_daily >= 5) break;
    d.daily[d.n_daily++] = x;
  }
  put_data(d, now);
}
void forecasts(const std::string &json, bool daily) {
  ArduinoJson::JsonDocument d;
  if (deserializeJson(d, json)) return;
  ArduinoJson::JsonArrayConst list;
  std::vector<ArduinoJson::JsonVariantConst> todo{d.as<ArduinoJson::JsonVariantConst>()};
  for (size_t k = 0; k < todo.size() && k < 32 && list.isNull(); k++) {  // finds the "forecast" list anywhere in the reply
    for (ArduinoJson::JsonPairConst kv : todo[k].as<ArduinoJson::JsonObjectConst>()) {
      if (!strcmp(kv.key().c_str(), "forecast") && kv.value().is<ArduinoJson::JsonArrayConst>()) { list = kv.value(); break; }
      if (kv.value().is<ArduinoJson::JsonObjectConst>()) todo.push_back(kv.value());
    }
  }
  if (list.isNull()) return;
  if (daily) days_got.clear(); else hours_got.clear();
  for (ArduinoJson::JsonVariantConst e : list) {
    const double t = iso_ms(e["datetime"] | "");
    if (std::isnan(t)) continue;
    const char *c = keep(e["condition"] | "");
    const double temp = e["temperature"] | NAN, pr = e["precipitation"] | 0.0;
    if (daily) { if (days_got.size() < 8) days_got.push_back({t, c, temp, e["templow"] | NAN, pr}); }
    else if (hours_got.size() < 24) hours_got.push_back({t, c, temp, pr});
  }
}
void answered_h(void *, bool ok, const std::string &json) { waiting_h = false; if (ok) forecasts(json, false); rebuild(); }
void answered_d(void *, bool ok, const std::string &json) { waiting_d = false; if (ok) forecasts(json, true); rebuild(); }
void fetch(const std::string &ent) {
  fetched_hour = std::floor(now_ms() / HOUR), fetched_at = lv_tick_get();
  for (int daily = 0; daily < 2; daily++) {
    bool &w = daily ? waiting_d : waiting_h;
    if (w) continue;
    ArduinoJson::JsonDocument q;
    q["entity_id"] = ent, q["type"] = daily ? "daily" : "hourly";
    std::string qs;
    serializeJson(q, qs);
    w = true;  // before the ask: a host may answer at once
    if (*ha::ask("weather.get_forecasts", qs.c_str(), daily ? answered_d : answered_h, nullptr)) w = false;
  }
}
void entity_put(const std::string &e, const std::string &a, const std::string &) {
  if (!ent_now.empty() && e == ent_now && (a.empty() || a == "temperature" || a == "apparent_temperature")) rebuild();
}

void fresh() {
  if (!tick) tick = lv_timer_create([](lv_timer_t *) { fresh(); }, 60000, nullptr);
  const std::string &cfg = ha::feed_config(E);
  ArduinoJson::JsonDocument c;
  deserializeJson(c, cfg.empty() ? "{}" : cfg.c_str());
  const std::string src = c["source"] | "ha", ent = c["weather"] | "";
  const char *la = c["lat"] | "", *lo = c["lon"] | "";
  lat = *la ? strtod(la, nullptr) : 52.2, lon = *lo ? strtod(lo, nullptr) : -1.2;
  const bool changed = cfg != cfg_seen;
  cfg_seen = cfg;
  static const bool hooked = (ha::on_put(entity_put), true);
  (void) hooked;
  if (src == "ha" && ent.rfind("weather.", 0) == 0) {
    if (changed || ent != ent_now) {
      hours_got.clear(), days_got.clear(), fetched_hour = -1, ent_now = ent;
      for (const char *a : {"", "temperature", "apparent_temperature"}) ha::want(ent, a);  // the weather now, live
    }
    rebuild();
    if (changed || std::floor(now_ms() / HOUR) != fetched_hour || lv_tick_elaps(fetched_at) > 30 * 60000u) fetch(ent);
  } else if (src == "ha") { ent_now.clear(); put_empty("error"); }  // no weather entity picked
  else { ent_now.clear(); const double now = now_ms(); put_data(wx::sample(now), now); }
}
[[maybe_unused]] const bool registered = (ha::on_local(E, fresh), true);  // binds on panel.weather never reach a host
}  // namespace
}  // namespace fw::dev::weather_source
