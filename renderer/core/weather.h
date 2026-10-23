// Weather feed (docs @facet/icons weather.ts wxIcon() + WX_TEXT, @facet/ui types WeatherData): what the screens read
// from a Home Assistant weather.* entity. The state is the condition ("rainy", "partlycloudy", ...); attributes give the
// temperature etc. Screens bind them in specs ({"e": "weather.forecast_home"} = the condition, {"a": "temperature"})
// or a sample adapter fills the same fields. Header-only (the firmware bundle picks up core/*.h by itself).
#pragma once
#include <cmath>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>

namespace fw::wx {
// HA condition -> the Facet weather icon (fw::weather_icon names); day = the sun is up (sun.sun above_horizon),
// precip = the hour's precipitation in mm (light showers when under 0.3), NAN = unknown
inline const char *icon(const char *c, bool day = true, double precip = NAN) {
  if (!c || !*c) return "cloudy";
  if (!strcmp(c, "sunny")) return day ? "clear-day" : "clear-night";
  if (!strcmp(c, "clear-night")) return "clear-night";
  if (!strcmp(c, "partlycloudy")) return day ? "partly-day" : "partly-night";
  if (!strcmp(c, "cloudy")) return "cloudy";
  if (!strcmp(c, "fog")) return "fog";
  if (!strcmp(c, "rainy")) return !std::isnan(precip) && precip > 0 && precip < 0.3 ? (day ? "showers-day" : "showers-night") : "rain-light";
  if (!strcmp(c, "pouring")) return "rain-heavy";
  if (!strcmp(c, "snowy")) return "snow";
  if (!strcmp(c, "snowy-rainy")) return "sleet";
  if (!strcmp(c, "lightning")) return "thunder";
  if (!strcmp(c, "lightning-rainy")) return "thunder-rain";
  if (!strcmp(c, "hail")) return "hail";
  if (!strcmp(c, "windy") || !strcmp(c, "windy-variant")) return "wind";
  if (!strcmp(c, "exceptional")) return "exceptional";
  return "cloudy";
}
// HA condition -> words (docs WX_TEXT); unknown / unavailable = "" (the screen shows nothing rather than a guess)
inline const char *text(const char *c) {
  static const char *const T[][2] = {{"sunny", "Sunny"}, {"clear-night", "Clear"}, {"partlycloudy", "Partly cloudy"},
      {"cloudy", "Cloudy"}, {"fog", "Fog"}, {"rainy", "Rain"}, {"pouring", "Heavy rain"}, {"snowy", "Snow"},
      {"snowy-rainy", "Sleet"}, {"lightning", "Thunder"}, {"lightning-rainy", "Thunderstorms"}, {"hail", "Hail"},
      {"windy", "Windy"}, {"windy-variant", "Windy, cloudy"}, {"exceptional", "Exceptional"}};
  for (auto &r : T) if (c && !strcmp(c, r[0])) return r[1];
  return "";
}
// "13°" (whole degrees, docs deg()); NAN = ""
inline std::string deg(double t) {
  if (std::isnan(t)) return "";
  char b[16];
  snprintf(b, sizeof b, "%ld\xC2\xB0", std::lround(t));
  return b;
}
// the Clock's weather line: "rain · 13°" (lower-case words, docs weatherLine); either part may be missing
inline std::string line(const char *condition, double temp) {
  std::string w = text(condition);
  for (char &ch : w) ch = (char) tolower((unsigned char) ch);
  const std::string d = deg(temp);
  if (w.empty()) return d;
  return d.empty() ? w : w + " \xC2\xB7 " + d;
}
// ---- the feed shape the Weather screen reads (docs WeatherData) + the docs sample (demo-data weather(now)): HA gives
// the current state + attributes; hourly / daily come from weather.get_forecasts on a panel (a host job), the sample
// stands in until then
struct Hour { double t; const char *condition; double temp, precip; };
struct Day { double t; const char *condition; double hi, lo, precip; };
struct Data {
  const char *condition = "";
  double temp = NAN, feels = NAN;
  Hour hourly[12];
  Day daily[5];
  int n_hourly = 0, n_daily = 0;
  double rain_from = NAN;      // the first wet 15 minutes of the next hour (ms), NAN = dry
  double next_rain = NAN;      // the next wet hour after that (ms)
};
inline Data sample(double now) {
  Data d;
  const double HOUR = 3600000;
  for (int i = 0; i < 12; i++) {
    const double t = std::floor(now / HOUR) * HOUR + i * HOUR;
    const time_t tt = (time_t) (t / 1000);
    struct tm tm;
    localtime_r(&tt, &tm);
    const double temp = 11 + 5 * std::sin((tm.tm_hour - 9) / 24.0 * 2 * 3.14159265358979);
    const char *c = i >= 4 && i <= 6 ? "rainy" : i >= 3 && i <= 7 ? "cloudy" : "partlycloudy";
    d.hourly[i] = {t, c, std::round(temp), !strcmp(c, "rainy") ? 0.6 : 0};
  }
  d.n_hourly = 12;
  static const char *CONDS[5] = {"partlycloudy", "rainy", "cloudy", "sunny", "lightning-rainy"};
  const double HI[5] = {16, 13, 14, 18, 15}, LO[5] = {8, 9, 7, 9, 10}, PR[5] = {0, 4.2, 0.4, 0, 6.8};
  const time_t tn = (time_t) (now / 1000);
  struct tm tm;
  localtime_r(&tn, &tm);
  tm.tm_hour = 12, tm.tm_min = 0, tm.tm_sec = 0;
  const double noon = (double) mktime(&tm) * 1000;
  for (int i = 0; i < 5; i++) d.daily[i] = {noon + i * 24 * HOUR, CONDS[i], HI[i], LO[i], PR[i]};
  d.n_daily = 5;
  d.condition = d.hourly[0].condition;
  d.temp = d.hourly[0].temp;
  d.feels = d.temp - 2;
  d.next_rain = d.hourly[4].t;
  return d;
}
}  // namespace fw::wx
