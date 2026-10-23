// The moon, worked out locally (no API; docs @facet/ui moon.ts): phase, illumination, age, the next new and full
// moons, and moonrise / moonset. Phase times = Meeus (Astronomical Algorithms ch. 49, within a couple of minutes);
// positions = the main lunar terms (~0.2 deg), so rise and set are good to a few minutes. Times are ms since the Unix
// epoch (double). Header-only (the firmware bundle picks up core/*.h by itself). Used by Moon facts and Moon by day.
#pragma once
#include <cmath>
#include <ctime>
#include <vector>

namespace fw::lunar {
constexpr double RAD = 3.14159265358979 / 180, DAY = 86400000.0, SYNODIC = 29.530588861, J2000 = 2451545;
inline double to_jd(double ms) { return ms / DAY + 2440587.5; }
inline double from_jd(double jd) { return (jd - 2440587.5) * DAY; }
inline double norm(double deg) { return std::fmod(std::fmod(deg, 360.0) + 360.0, 360.0); }
enum Kind { NEW, FIRST, FULL, LAST };

// time (ms) of lunation k + the phase's offset; k = 0 is the new moon of 6 January 2000
inline double phase_time(double k, Kind kind) {
  const double OFF[4] = {0, 0.25, 0.5, 0.75};
  k += OFF[kind];
  const double T = k / 1236.85;
  double jde = 2451550.09766 + SYNODIC * k + 0.00015437 * T * T - 0.00000015 * T * T * T + 0.00000000073 * T * T * T * T;
  const double E = 1 - 0.002516 * T - 0.0000074 * T * T;
  const double M = (2.5534 + 29.1053567 * k - 0.0000014 * T * T) * RAD;
  const double Mp = (201.5643 + 385.81693528 * k + 0.0107582 * T * T + 0.00001238 * T * T * T) * RAD;
  const double F = (160.7108 + 390.67050284 * k - 0.0016118 * T * T - 0.00000227 * T * T * T) * RAD;
  const double O = (124.7746 - 1.56375588 * k + 0.0020672 * T * T) * RAD;
  auto s = [](double x) { return std::sin(x); };
  if (kind == NEW || kind == FULL) {
    const bool n = kind == NEW;
    jde += (n ? -0.4072 : -0.40614) * s(Mp) + (n ? 0.17241 : 0.17302) * E * s(M) + (n ? 0.01608 : 0.01614) * s(2 * Mp) +
           (n ? 0.01039 : 0.01043) * s(2 * F) + (n ? 0.00739 : 0.00734) * E * s(Mp - M) - (n ? 0.00514 : 0.00515) * E * s(Mp + M) +
           (n ? 0.00208 : 0.00209) * E * E * s(2 * M) - 0.00111 * s(Mp - 2 * F) - 0.00057 * s(Mp + 2 * F) +
           0.00056 * E * s(2 * Mp + M) - 0.00042 * s(3 * Mp) + 0.00042 * E * s(M + 2 * F) + 0.00038 * E * s(M - 2 * F) -
           0.00024 * E * s(2 * Mp - M) - 0.00017 * s(O) - 0.00007 * s(Mp + 2 * M) + 0.00004 * s(2 * Mp - 2 * F) +
           0.00004 * s(3 * M) + 0.00003 * s(Mp + M - 2 * F) + 0.00003 * s(2 * Mp + 2 * F) - 0.00003 * s(Mp + M + 2 * F) +
           0.00003 * s(Mp - M + 2 * F) - 0.00002 * s(Mp - M - 2 * F) - 0.00002 * s(3 * Mp + M) + 0.00002 * s(4 * Mp);
  } else {
    jde += -0.62801 * s(Mp) + 0.17172 * E * s(M) - 0.01183 * E * s(Mp + M) + 0.00862 * s(2 * Mp) + 0.00804 * s(2 * F) +
           0.00454 * E * s(Mp - M) + 0.00204 * E * E * s(2 * M) - 0.0018 * s(Mp - 2 * F) - 0.0007 * s(Mp + 2 * F) -
           0.0004 * s(3 * Mp) - 0.00034 * E * s(2 * Mp - M) + 0.00032 * E * s(M + 2 * F) + 0.00032 * E * s(M - 2 * F) -
           0.00028 * E * E * s(Mp + 2 * M) + 0.00027 * E * s(2 * Mp + M) - 0.00017 * s(O);
    const double c1 = std::cos(M), c2 = std::cos(Mp);
    const double W = 0.00306 - 0.00038 * E * c1 + 0.00026 * c2 - 0.00002 * std::cos(Mp - M) + 0.00002 * std::cos(Mp + M) + 0.00002 * std::cos(2 * F);
    jde += kind == FIRST ? W : -W;
  }
  return from_jd(jde) - 70000;  // terrestrial time -> UTC (delta T ~ 70 s in the 2020s)
}
struct Phase { Kind kind; double t; };
inline std::vector<Phase> phases_around(double from, int count) {
  const double k0 = std::floor((to_jd(from) - 2451550.09766) / SYNODIC) - 1;
  std::vector<Phase> out;
  for (double k = k0; (int) out.size() < count + 8; k++)
    for (Kind kind : {NEW, FIRST, FULL, LAST}) out.push_back({kind, phase_time(k, kind)});
  return out;
}
inline double sun_lon(double d) {
  const double M = (357.5291 + 0.98560028 * d) * RAD;
  const double C = (1.9148 * std::sin(M) + 0.02 * std::sin(2 * M) + 0.0003 * std::sin(3 * M)) * RAD;
  return M + C + 102.9372 * RAD + 3.14159265358979;
}
inline void moon_ecl(double d, double *lon, double *lat) {
  const double L = 218.316 + 13.176396 * d, Mp = (134.963 + 13.064993 * d) * RAD, M = (357.5291 + 0.98560028 * d) * RAD;
  const double F = (93.272 + 13.22935 * d) * RAD, D = (297.8502 + 12.19074912 * d) * RAD;
  *lon = (L + 6.289 * std::sin(Mp) + 1.274 * std::sin(2 * D - Mp) + 0.658 * std::sin(2 * D) + 0.214 * std::sin(2 * Mp) -
          0.186 * std::sin(M) - 0.114 * std::sin(2 * F)) * RAD;
  *lat = (5.128 * std::sin(F) + 0.28 * std::sin(Mp + F) + 0.277 * std::sin(Mp - F)) * RAD;
}
// how far through the synodic month (0..1) at ms, from the sun-moon elongation (docs moonFrac)
inline double frac(double ms) {
  const double d = to_jd(ms) - J2000;
  double lon, lat;
  moon_ecl(d, &lon, &lat);
  return norm((lon - sun_lon(d)) / RAD) / 360;
}
inline double illumination(double f) { return (1 - std::cos(2 * 3.14159265358979 * f)) / 2; }
inline double moon_alt(double ms, double lat, double lon) {
  const double d = to_jd(ms) - J2000;
  double l, b;
  moon_ecl(d, &l, &b);
  const double e = 23.4397 * RAD;
  const double ra = std::atan2(std::sin(l) * std::cos(e) - std::tan(b) * std::sin(e), std::cos(l));
  const double dec = std::asin(std::sin(b) * std::cos(e) + std::cos(b) * std::sin(e) * std::sin(l));
  const double H = (280.16 + 360.9856235 * d + lon) * RAD - ra, phi = lat * RAD;
  return std::asin(std::sin(phi) * std::sin(dec) + std::cos(phi) * std::cos(dec) * std::cos(H));
}
// local midnight (the device's time zone) of the day holding ms
inline double day_start(double ms) {
  const time_t t = (time_t) (ms / 1000);
  struct tm tm;
  localtime_r(&t, &tm);
  tm.tm_hour = tm.tm_min = tm.tm_sec = 0;
  return (double) mktime(&tm) * 1000;
}
// moonrise and moonset on the local day holding ms (NAN = none that day); altitude threshold +0.125 deg
inline void rise_set(double ms, double lat, double lon, double *rise, double *set) {
  const double t0 = day_start(ms), h0 = 0.125 * RAD, STEP = 10 * 60000.0;
  *rise = *set = NAN;
  double prev = moon_alt(t0, lat, lon) - h0;
  for (double t = t0 + STEP; t <= t0 + DAY; t += STEP) {
    const double cur = moon_alt(t, lat, lon) - h0;
    if ((prev < 0) != (cur < 0)) {
      const double at = t - STEP + STEP * prev / (prev - cur);
      if (cur > 0 && std::isnan(*rise)) *rise = at;
      if (cur < 0 && std::isnan(*set)) *set = at;
    }
    prev = cur;
  }
}
struct Data {
  double frac = 0, illumination = 0, age = 0, next_new = 0, next_full = 0, next_rise = NAN, next_set = NAN;
  int phase = 0;  // 0 new, 1 waxing crescent, 2 first quarter, 3 waxing gibbous, 4 full, 5 waning gibbous, 6 last, 7 waning crescent
};
constexpr const char *PHASE_TEXT[8] = {"New moon", "Waxing crescent", "First quarter", "Waxing gibbous", "Full moon", "Waning gibbous", "Last quarter", "Waning crescent"};
// everything the Moon screen shows, for ms at a place (docs moonData; a principal phase within 12 h of its exact time)
inline Data data(double ms, double lat, double lon) {
  Data r;
  const std::vector<Phase> all = phases_around(ms, 4);
  double last_new = 0;
  int near = -1;
  for (const Phase &p : all) {
    if (p.kind == NEW && p.t <= ms) last_new = p.t;
    if (std::fabs(p.t - ms) < 12 * 3600000.0 && near < 0) near = p.kind;
  }
  for (const Phase &p : all) if (p.kind == NEW && p.t > ms) { r.next_new = p.t; break; }
  for (const Phase &p : all) if (p.kind == FULL && p.t > ms) { r.next_full = p.t; break; }
  r.frac = frac(ms);
  r.illumination = std::round(illumination(r.frac) * 1000) / 10;
  r.age = std::round((ms - last_new) / DAY * 10) / 10;
  r.phase = near >= 0 ? near * 2 : r.frac < 0.25 ? 1 : r.frac < 0.5 ? 3 : r.frac < 0.75 ? 5 : 7;
  for (int k = 0; k < 3 && (std::isnan(r.next_rise) || std::isnan(r.next_set)); k++) {  // the next rise / set from now
    double rs, st;
    rise_set(ms + k * DAY, lat, lon, &rs, &st);
    if (std::isnan(r.next_rise) && !std::isnan(rs) && rs > ms) r.next_rise = rs;
    if (std::isnan(r.next_set) && !std::isnan(st) && st > ms) r.next_set = st;
  }
  return r;
}
}  // namespace fw::lunar
