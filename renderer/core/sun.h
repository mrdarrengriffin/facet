// The sun, worked out locally (docs @facet/ui sun.ts, no API): today's sunrise and sunset, first and last light (civil
// twilight, -6 deg), the day length and its change since yesterday, tomorrow's sunrise; the next rise / set from now and
// how far through the day it is. The sunrise equation (NOAA, simplified) with -0.833 deg for refraction and the sun's
// radius lands within about a minute. Times are ms since the epoch; NAN = the sun doesn't cross (polar day / night).
// A panel can take HA sun.sun's next_rising / next_setting instead; the screens read the same Data. Header-only.
#pragma once
#include <cmath>
#include <cstdio>
#include <ctime>
#include <string>

namespace fw::sun {
constexpr double RAD = 3.14159265358979 / 180, DAY = 86400000.0, J2000 = 2451545;
struct Times { double rise, set, noon; };
// sunrise and sunset of the local day holding ms, for the sun at alt degrees
inline Times times(double ms, double lat, double lon, double alt = -0.833) {
  const time_t t = (time_t) (ms / 1000);
  struct tm tm;
  localtime_r(&t, &tm);
  tm.tm_hour = 12, tm.tm_min = 0, tm.tm_sec = 0;
  const double noon = (double) mktime(&tm) * 1000;
  const double n = std::round(noon / DAY + 2440587.5 - J2000);
  const double js = n - lon / 360;
  const double mDeg = std::fmod(357.5291 + 0.98560028 * js, 360.0), m = mDeg * RAD;
  const double c = 1.9148 * std::sin(m) + 0.02 * std::sin(2 * m) + 0.0003 * std::sin(3 * m);
  const double lam = std::fmod(mDeg + c + 180 + 102.9372, 360.0) * RAD;
  const double jt = J2000 + js + 0.0053 * std::sin(m) - 0.0069 * std::sin(2 * lam);
  const double dec = std::asin(std::sin(lam) * std::sin(23.4397 * RAD));
  const double cosW = (std::sin(alt * RAD) - std::sin(lat * RAD) * std::sin(dec)) / (std::cos(lat * RAD) * std::cos(dec));
  auto from_jd = [](double jd) { return (jd - 2440587.5) * DAY; };
  if (cosW < -1 || cosW > 1) return {NAN, NAN, from_jd(jt)};
  const double w = std::acos(cosW) / RAD / 360;
  return {from_jd(jt - w), from_jd(jt + w), from_jd(jt)};
}
struct Data { double rise, set, dawn, dusk, length, delta, next_rise; };
inline Data data(double now, double lat, double lon) {
  const Times t = times(now, lat, lon), y = times(now - DAY, lat, lon), tm = times(now + DAY, lat, lon), c = times(now, lat, lon, -6);
  const double len = t.set - t.rise, ylen = y.set - y.rise;
  return {t.rise, t.set, c.rise, c.set, len, len - ylen, tm.rise};
}
// the next sunrise or sunset (kind 1 sunrise, 2 sunset, 0 none) and the day's progress 0..1 (NAN while it's down)
struct Now { int kind; double t, frac; };
inline Now now_of(const Data &s, double now) {
  Now r{0, NAN, NAN};
  if (!std::isnan(s.rise) && now < s.rise) r = {1, s.rise, NAN};
  else if (!std::isnan(s.set) && now < s.set) r = {2, s.set, NAN};
  else if (!std::isnan(s.next_rise)) r = {1, s.next_rise, NAN};
  if (!std::isnan(s.rise) && !std::isnan(s.set) && now >= s.rise && now < s.set) r.frac = (now - s.rise) / (s.set - s.rise);
  return r;
}
// "10 h 42 min"
inline std::string day_length(double ms) {
  const long m = std::lround(ms / 60000);
  char b[24];
  snprintf(b, sizeof b, "%ld h %02ld min", m / 60, m % 60);
  return b;
}
// "in 25 min", "in 2 h 14 min"
inline std::string in_time(double ms) {
  const long m = std::max(0L, std::lround(ms / 60000));
  char b[24];
  if (m < 60) snprintf(b, sizeof b, "in %ld min", m);
  else snprintf(b, sizeof b, "in %ld h %ld min", m / 60, m % 60);
  return b;
}
// "2 min 10 s shorter" / "longer" than yesterday
inline std::string day_delta(double ms) {
  const long s = std::lround(std::fabs(ms) / 1000), m = s / 60;
  char b[40];
  if (m) snprintf(b, sizeof b, "%ld min %ld s %s", m, s % 60, ms < 0 ? "shorter" : "longer");
  else snprintf(b, sizeof b, "%ld s %s", s % 60, ms < 0 ? "shorter" : "longer");
  return b;
}
}  // namespace fw::sun
