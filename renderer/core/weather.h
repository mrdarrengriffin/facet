// Weather feed (docs @facet/icons weather.ts wxIcon() + WX_TEXT, @facet/ui types WeatherData): what the screens read
// from a Home Assistant weather.* entity. The state is the condition ("rainy", "partlycloudy", ...); attributes give the
// temperature etc. Screens bind them in specs ({"e": "weather.forecast_home"} = the condition, {"a": "temperature"})
// or a sample adapter fills the same fields. Header-only (the firmware bundle picks up core/*.h by itself).
#pragma once
#include <cmath>
#include <cctype>
#include <cstdio>
#include <cstring>
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
}  // namespace fw::wx
