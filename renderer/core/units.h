// Units (Foundations > Units; docs @facet/ui units.ts): Settings > Units > Distance (km | miles) and Speed (mph | km/h
// | knots), the distanceUnit / speedUnit settings (tokens/settings.json). Screens format every distance and speed
// through these, so one setting changes them all. Header-only (the firmware bundle picks up core/*.h by itself).
#pragma once
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

namespace fw::units {
enum class Distance { km, mi };
enum class Speed { mph, kmh, kn };
constexpr double KM_PER_MI = 1.609344;                                   // international mile
constexpr double PER_KT[3] = {1.15078, 1.852, 1.0};                      // mph, km/h, kn per knot
constexpr const char *SPEED_LABEL[3] = {"mph", "km/h", "kn"};
inline Distance distance_of(const char *s) { return s && !strcmp(s, "mi") ? Distance::mi : Distance::km; }
inline Speed speed_of(const char *s) { return s && !strcmp(s, "kmh") ? Speed::kmh : s && !strcmp(s, "kn") ? Speed::kn : Speed::mph; }
inline double from_km(double km, Distance u) { return u == Distance::mi ? km / KM_PER_MI : km; }
// "4.3 km", "2.7 mi": one decimal under 10 (docs distance())
inline std::string distance(double km, Distance u) {
  const double v = from_km(km, u);
  char b[24];
  if (v < 10) snprintf(b, sizeof b, "%.1f %s", v, u == Distance::mi ? "mi" : "km");
  else snprintf(b, sizeof b, "%ld %s", std::lround(v), u == Distance::mi ? "mi" : "km");
  return b;
}
// a speed in knots (what ADS-B reports) in the unit: "520 mph", "837 km/h", "452 kn"; NAN = the placeholder "--"
inline std::string speed_kt(double kt, Speed u) {
  if (std::isnan(kt)) return "--";
  char b[24];
  snprintf(b, sizeof b, "%ld %s", std::lround(kt * PER_KT[(int) u]), SPEED_LABEL[(int) u]);
  return b;
}
// a speed in km/h (wind, a storm's approach) in the unit
inline std::string speed_kmh(double kmh, Speed u) { return std::isnan(kmh) ? "--" : speed_kt(kmh / 1.852, u); }
}  // namespace fw::units
