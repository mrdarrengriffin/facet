// The weather feed (CONTRACT.md C51): the reserved entity panel.weather (core/spec.h "local sources") in the HA store,
// the one shape the Weather screen, its next 5 days and its Home card bind to, whatever gives the forecast. Built on
// the Feed base (core/feed.h: rounds every minute while read, settings, state, fields + placeholders, the sample in
// sample()). Same file on the panels and in WASM. Words and units are Facet's (core/weather.h: the condition's words, whole degrees).
//   state      ok | loading (no weather yet) | error (no weather entity picked) | missing (Home Assistant has no such
//              entity) | unavailable (it has no state now); not ok = feed::words (Loading / Offline / Not found /
//              Unavailable) as the condition and the card, every other field its placeholder (FIELDS)
//   icon       the weather icon now ("partly-day", "rain-light", "clear-night": by day or night)
//   temp       "13°" ("--°" while loading / on error); condition "Partly cloudy" ("Loading" / "Offline")
//   line       "Feels 11° · high 16°, low 8°" (the parts it knows)
//   rain       "Rain from 14:00" / "Dry for the next hour · rain at 16:00" / "Dry for the next hour"; rain_tone
//              info (rain within the hour) | idle (a Status line's dot)
//   brief      a short line for under a time: "rain · 13°" (the condition in lower case + the temperature,
//              core/weather.h line); "" while loading / on error (no line); missing / unavailable: the words (the
//              entity says why it shows no weather). No factory screen binds it now (the Clock shows no weather).
//   card       the Home card's line: "rain in 20 min" / "raining now" within the hour, else the condition in lower
//              case ("partly cloudy"), "loading" / "offline"
//   hours      the next eight hours, a JSON list of Weather icon columns: [{"label": "now" | "14", "icon": "...",
//              "value": "13°", "accent": "on" | "off"}]; has_hours "1" when known, else "" (the screen hides the strip)
//   days_empty what the 5 days list says with no days: "No forecast yet", or missing / unavailable's words
//   days       the next five days, a JSON list of Weather icon rows: [{"label": "Today" | "Saturday", "icon": "...",
//              "line": "Rain · 4.2 mm", "low": "8°", "value": "16°", "divider": "off" for the first, else "on"}]
// Adapters (the feed's settings, the spec's "feeds"; core/ha.h feed_config): source
//   ha      a Home Assistant weather entity (setting weather, such as weather.forecast_home): the condition, temperature
//           and apparent_temperature are a live subscription like any bind; the next hours and days come from
//           weather.get_forecasts (core/ha.h ask: hourly, then daily) on build, every 30 minutes and on a new hour.
//           Without action responses (or a failed ask) the screen shows the weather now only, never an error.
//           HA forecasts come by the hour, so "rain from" is the first wet hour (the docs used 15-minute steps).
//   open_meteo  the public Open-Meteo forecast API (no account or key; CC BY 4.0, free for non-commercial use, fewer
//           than 10,000 calls a day), read by the panel itself (core/net.h) every 15 minutes while the feed is shown
//           (after a failure 1, 2, 4, 8 minutes, then every 15): GET https://api.open-meteo.com/v1/forecast for home
//           (2 decimals, about 1 km) with the weather now, the next 13 hours and 6 days (~2 KB). WMO weather codes
//           become the Home Assistant conditions the rest of this file speaks (wmo()). A failed read keeps the last
//           forecast; with none yet the feed is error (Offline). No home = error.
//   sample  the docs forecast (core/weather.h sample: today, real hours), for previews and tests
//   auto    (the default) ha when Home Assistant has the weather entity (or, failing it, any weather entity: the first
//           in its registry), else open_meteo; a picked source always wins
// Settings: source, weather (the ha adapter's entity), lat, lon (home; "" = Home Assistant's zone.home, the default).
// Day or night (the icon): the sun at home (lat / lon, else zone.home, else the docs sample place), core/sun.h.
// More adapters (Met Office's own API ...) = more rows here, no screen change.
#include <cstdio>
#include <ctime>
#include "device.h"
#include "feed.h"
#include "fw.h"
#include "ha.h"
#include "net.h"
#include "sun.h"
#include "weather.h"

namespace fw::dev::weather_source {
namespace {
constexpr double HOUR = 3600000;
std::string ent_now;
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

// the feed's fields: required = what every screen on it needs; the rest are optional, with what they show when a
// source cannot fill them (C51: the component hides an empty one)
std::vector<feed::Field> FIELDS = {{"icon", true, "cloudy"}, {"temp", true, "--\xC2\xB0"}, {"condition", true, ""},
                                   {"card", true, ""}, {"brief", false, ""}, {"line", false, ""}, {"rain", false, ""},
                                   {"rain_tone", false, "idle"}, {"hours", false, "[]"}, {"has_hours", false, ""},
                                   {"days", false, "[]"}, {"days_empty", false, "No forecast yet"}};
std::function<void(const char *, const std::string &)> put;  // the feed's put (Weather below)
void put_state(feed::State s);

// the feed from one Data (the sample's, or what the ha adapter gathered)
void put_data(const wx::Data &d, double now) {
  put_state(feed::OK);
  put("icon", wx::icon(d.condition, is_day(now)));
  put("temp", std::isnan(d.temp) ? "--\xC2\xB0" : wx::deg(d.temp));
  put("condition", words(d.condition));
  put("brief", wx::line(d.condition, d.temp));
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
void put_empty(feed::State st) {  // not ok: the words only (no icon, rain line or hours: the docs); the rest shows its
  put_state(st);                 // placeholders. The entity's two states say so on every screen on the feed
  put("condition", feed::words(st));
  const bool gone = st == feed::MISSING || st == feed::UNAVAILABLE;  // the entity's two words, as written everywhere
  put("card", gone ? feed::words(st) : lower(feed::words(st)));
  if (gone) put("brief", feed::words(st)), put("days_empty", feed::words(st));
}

// the feed from the weather now + the forecasts an adapter gathered (hours_got, days_got)
void assemble(const char *condition, double temp, double feels) {
  const double now = now_ms();
  wx::Data d;
  d.condition = condition;
  d.temp = temp;
  d.feels = feels;
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

// ---- ha adapter
void rebuild() {
  if (ent_now.empty()) return;
  const std::string *state = ha::get(ent_now), *t = ha::get(ent_now, "temperature"), *f = ha::get(ent_now, "apparent_temperature");
  if (const feed::State st = feed::of(state); st != feed::OK) { put_empty(st); return; }  // loading, missing, unavailable
  assemble(keep(state->c_str()), num(t), num(f));  // Met.no has no apparent_temperature: no "Feels" then
}

// ---- open_meteo adapter
// a WMO weather code (Open-Meteo's weather_code) -> the Home Assistant condition; day = false turns clear into clear-night
const char *wmo(int c, bool day = true) {
  if (c <= 1) return day ? "sunny" : "clear-night";
  if (c == 2) return "partlycloudy";
  if (c == 3) return "cloudy";
  if (c == 45 || c == 48) return "fog";
  if (c == 56 || c == 57 || c == 66 || c == 67) return "snowy-rainy";  // freezing drizzle / rain
  if (c == 65 || c == 82) return "pouring";
  if ((c >= 51 && c <= 63) || c == 80 || c == 81) return "rainy";
  if ((c >= 71 && c <= 77) || c == 85 || c == 86) return "snowy";
  if (c == 95) return "lightning-rainy";
  if (c == 96 || c == 99) return "hail";
  return "";
}
struct Now { std::string cond; double temp = NAN, feels = NAN; };
// Open-Meteo's reply (timeformat=unixtime): current {weather_code, is_day, temperature_2m, apparent_temperature},
// hourly / daily {time [...], ...}; false = not that shape
bool read_open_meteo(const std::string &body, Now &n) {
  ArduinoJson::JsonDocument d;
  if (deserializeJson(d, body)) return false;
  ArduinoJson::JsonObjectConst c = d["current"], h = d["hourly"], y = d["daily"];
  if (c.isNull() || !c["weather_code"].is<int>()) return false;
  n.cond = wmo(c["weather_code"] | 3, (c["is_day"] | 1) != 0);
  n.temp = c["temperature_2m"] | NAN, n.feels = c["apparent_temperature"] | NAN;
  hours_got.clear(), days_got.clear();
  ArduinoJson::JsonArrayConst ht = h["time"], hc = h["weather_code"], htp = h["temperature_2m"], hp = h["precipitation"];
  for (size_t i = 0; i < ht.size() && hours_got.size() < 24; i++)
    hours_got.push_back({(ht[i] | 0.0) * 1000, keep(wmo(hc[i] | 3)), htp[i] | NAN, hp[i] | 0.0});
  ArduinoJson::JsonArrayConst dt = y["time"], dc = y["weather_code"], dhi = y["temperature_2m_max"], dlo = y["temperature_2m_min"], dp = y["precipitation_sum"];
  // a day's time is its local midnight: noon stands for the day (local dates, as the ha adapter's)
  for (size_t i = 0; i < dt.size() && days_got.size() < 8; i++)
    days_got.push_back({((dt[i] | 0.0) + 12 * 3600) * 1000, keep(wmo(dc[i] | 3)), dhi[i] | NAN, dlo[i] | NAN, dp[i] | 0.0});
  return true;
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
// no forecast yet (the first ask failed or went unanswered: a boot before Home Assistant was linked): ask again a
// minute on, not at the next hour
constexpr uint32_t RETRY_MS = 45000;  // under the feed's 60 s round: its next round asks again
void answered_h(void *, bool ok, const std::string &json);
void answered_d(void *, bool ok, const std::string &json);
void fetch(const std::string &ent) {
  const bool lost = lv_tick_elaps(fetched_at) > RETRY_MS;  // an ask unanswered this long (sent before the host's link was up)
  fetched_hour = std::floor(now_ms() / HOUR), fetched_at = lv_tick_get();
  for (int daily = 0; daily < 2; daily++) {
    bool &w = daily ? waiting_d : waiting_h;
    if (w && !lost) continue;
    ArduinoJson::JsonDocument q;
    q["entity_id"] = ent, q["type"] = daily ? "daily" : "hourly";
    std::string qs;
    serializeJson(q, qs);
    w = true;  // before the ask: a host may answer at once
    if (*ha::ask("weather.get_forecasts", qs.c_str(), daily ? answered_d : answered_h, nullptr)) w = false;
  }
}
class Weather : public feed::Feed {
 public:
  Weather() : Feed("panel.weather", 60000, "auto", FIELDS) {}
  using Feed::put;
  using Feed::state;

 protected:
  void sample() override {
    ent_now.clear();
    reads({});
    lat = 52.2, lon = -1.2;  // the docs place
    const double now = now_ms();
    put_data(wx::sample(now), now);
  }
  void fill() override {
    std::string src = source(), ent = cfg()["weather"] | "";
    if (src == "auto") {  // the default: Home Assistant's forecast when it has one, else Open-Meteo
      if (ent.rfind("weather.", 0) == 0) reads(with_home({{ent, ""}, {ent, "temperature"}, {ent, "apparent_temperature"}}));
      const std::string *st = ent.rfind("weather.", 0) == 0 ? ha::get(ent) : nullptr;
      const feed::State s = st ? feed::of(st) : feed::MISSING;
      if (s == feed::OK || s == feed::UNAVAILABLE) src = "ha";
      else {
        dev::ha_link::reg_fetch("weather");  // another weather entity in Home Assistant (its registry, once read)
        const auto *r = dev::ha_link::reg("weather");
        if (r && !r->empty()) src = "ha", ent = r->front().id;
        else if (s == feed::LOADING && ha::S().connected && !ha::settled()) {  // Home Assistant has not answered yet
          ent_now.clear();
          put_empty(feed::LOADING);
          return;
        } else src = "open_meteo";
      }
    }
    if (src == "open_meteo") { open_meteo(source() == "auto" ? ent : ""); return; }
    om_have = false, om_reading = false, om_at = 0, om_fails = 0, om_key.clear();
    place();
    if (ent.rfind("weather.", 0) != 0) { ent_now.clear(); reads({}); put_empty(feed::ERROR); return; }  // no weather entity picked
    if (changed() || ent != ent_now) hours_got.clear(), days_got.clear(), fetched_hour = -1, ent_now = ent;
    if (back()) waiting_h = waiting_d = false;  // asks sent before Home Assistant dropped never come back
    reads(with_home({{ent, ""}, {ent, "temperature"}, {ent, "apparent_temperature"}}));  // the weather now, live
    rebuild();
    if (changed() || back() || std::floor(now_ms() / HOUR) != fetched_hour || lv_tick_elaps(fetched_at) > 30 * 60000u ||
        ((hours_got.empty() || days_got.empty()) && lv_tick_elaps(fetched_at) > RETRY_MS)) fetch(ent);
  }

 private:
  bool om_have = false, om_reading = false;
  uint32_t om_at = 0;  // the last read's start (lv ticks; 0 = none yet)
  int om_fails = 0;    // reads failed in a row (the backoff)
  Now om_now;
  std::string om_key;  // the place the forecast is for
  bool own() const { return *(cfg()["lat"] | "") && *(cfg()["lon"] | ""); }
  std::vector<std::pair<std::string, std::string>> with_home(std::vector<std::pair<std::string, std::string>> rd) const {
    if (!own()) rd.push_back({"zone.home", "latitude"}), rd.push_back({"zone.home", "longitude"});
    return rd;
  }
  // home: the settings' place, else Home Assistant's (zone.home), else none (false; the sun then uses the docs place)
  bool place() {
    double la = NAN, lo = NAN;
    if (own()) la = strtod(cfg()["lat"] | "", nullptr), lo = strtod(cfg()["lon"] | "", nullptr);
    else if (const std::string *a = ha::get("zone.home", "latitude"), *b = ha::get("zone.home", "longitude"); a && b && !a->empty() && !b->empty())
      la = strtod(a->c_str(), nullptr), lo = strtod(b->c_str(), nullptr);
    const bool home = std::isfinite(la) && std::isfinite(lo) && std::fabs(la) <= 90 && std::fabs(lo) <= 180;
    lat = home ? la : 52.2, lon = home ? lo : -1.2;
    return home;
  }
  void open_meteo(const std::string &watch) {  // watch: the entity the automatic default goes back to when it comes
    ent_now.clear();
    reads(with_home(watch.rfind("weather.", 0) == 0
                        ? std::vector<std::pair<std::string, std::string>>{{watch, ""}, {watch, "temperature"}, {watch, "apparent_temperature"}}
                        : std::vector<std::pair<std::string, std::string>>{}));
    if (!place()) { put_empty(feed::ERROR); return; }  // nowhere to forecast for: not set up
    char url[400];
    snprintf(url, sizeof url,
             "https://api.open-meteo.com/v1/forecast?latitude=%.2f&longitude=%.2f"
             "&current=temperature_2m,apparent_temperature,weather_code,is_day"
             "&hourly=temperature_2m,precipitation,weather_code&forecast_hours=13"
             "&daily=weather_code,temperature_2m_max,temperature_2m_min,precipitation_sum&forecast_days=6"
             "&timezone=auto&timeformat=unixtime",
             lat, lon);
    if (changed() || om_key != url) om_have = false, om_at = 0, om_fails = 0, om_key = url, hours_got.clear(), days_got.clear();
    // every 15 minutes while shown; after a failure 1, 2, 4, 8 minutes, then every 15 until a read works
    const uint32_t every = om_fails ? std::min<uint32_t>(60000u << std::min(om_fails - 1, 4), 15 * 60000u) : 15 * 60000u;
    if (!om_reading && (!om_at || lv_tick_elaps(om_at) >= every - 500)) {
      om_reading = true;
      om_at = lv_tick_get() ? lv_tick_get() : 1;
      net::get(url, [this](bool ok, const std::string &body, const std::string &) {
        om_reading = false;
        Now n;
        if (ok && read_open_meteo(body, n)) om_now = n, om_have = true, om_fails = 0;
        else om_fails++;
        run();
      });
    }
    if (!om_have) { put_empty(om_fails ? feed::ERROR : feed::LOADING); return; }
    assemble(keep(om_now.cond.c_str()), om_now.temp, om_now.feels);
  }
};
Weather F;
void put_state(feed::State s) { F.state(s); }
[[maybe_unused]] const bool wired = (put = [](const char *a, const std::string &x) { F.put(a, x); }, true);
void answered_h(void *, bool ok, const std::string &json) { waiting_h = false; if (ok) forecasts(json, false); F.run(); }
void answered_d(void *, bool ok, const std::string &json) { waiting_d = false; if (ok) forecasts(json, true); F.run(); }
}  // namespace
}  // namespace fw::dev::weather_source
