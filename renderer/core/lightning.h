// Lightning: the strike data the Lightning radar and the lightning card preview draw (docs @facet/ui radar.ts +
// @facet/demo-data lightning()). Strikes carry a distance (km from home), a bearing (degrees from north) and a time (ms
// since the epoch). Age classes (docs ageClass): 0 < 1 min, 1 < 5, 2 < 15, 3 older. sample() = the docs demo storm, a
// cell crossing the area once an hour from the west at about 50 km/h (deterministic per strike, for the workbench and
// tests; a panel feeds real strikes from Home Assistant into the same Data). Header-only (the firmware bundle picks up
// core/*.h by itself).
#pragma once
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>
#include "units.h"

namespace fw::lx {
struct Strike { uint32_t id; float d, az; double t; };  // km, degrees, ms
struct Data {
  int radius = 100;                 // km the feed covers (the outer ring)
  std::vector<Strike> strikes;      // newest first
  int m5 = 0, m15 = 0, m60 = 0, near25 = 0;
  bool has_nearest = false;         // nearest strike of the last 15 minutes
  Strike nearest{};
  int trend = 0;                    // 1 approaching, -1 receding, 0 none
  int kmh = 0;                      // approach speed (km/h)
  bool storm = false;               // more than 10 strikes in 15 minutes
};
constexpr const char *DIRS[8] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
inline const char *compass(double az) { return DIRS[(int) std::floor(std::fmod(std::fmod(az, 360.0) + 360 + 22.5, 360.0) / 45) % 8]; }
inline int age_class(double t, double now) {
  const double m = (now - t) / 60000;
  return m < 1 ? 0 : m < 5 ? 1 : m < 15 ? 2 : 3;
}
// counts, nearest and the storm flag for a list of strikes (newest first; docs summariseStrikes)
inline void summarise(Data &d, double now) {
  d.m5 = d.m15 = d.m60 = d.near25 = 0;
  d.has_nearest = false;
  for (const Strike &s : d.strikes) {
    const double age = now - s.t;
    if (age < 5 * 60000.0) d.m5++;
    if (age < 60 * 60000.0) d.m60++;
    if (age >= 15 * 60000.0) continue;
    d.m15++;
    if (s.d <= 25) d.near25++;
    if (!d.has_nearest || s.d < d.nearest.d) { d.nearest = s; d.has_nearest = true; }
  }
  d.storm = d.m15 > 10;
}
// the docs demo storm (lightning(now, quiet)): up to 90 strikes over the last hour, one per 40 s slot. Each strike is
// worked out from its slot alone (a hash, not a running seed: the docs re-roll every strike every 20 s, fine for one
// still picture, wrong for a live feed), so it keeps its place and id while time moves on: a later `now` adds new
// strikes at the front and ages the rest, like the real feed.
inline uint32_t hash32(uint32_t x) {
  x += 0x9e3779b9u; x ^= x >> 16; x *= 0x85ebca6bu; x ^= x >> 13; x *= 0xc2b2ae35u; x ^= x >> 16;
  return x;
}
inline Data sample(double now, bool quiet = false) {
  Data d;
  if (quiet) return d;
  const double HOUR = 3600000, SLOT = 40000;
  const int64_t s0 = (int64_t) std::floor(now / SLOT);
  for (int k = 0; k < 92; k++) {
    const int64_t s = s0 - k;
    const uint32_t h = hash32((uint32_t) s);
    auto rnd = [&](int i) { return (double) (hash32(h + (uint32_t) i * 0x632be5abu) & 0xffff) / 0xffff; };
    const double t = s * SLOT + rnd(0) * 20000;
    if (t > now) continue;  // this slot's strike has not happened yet
    const double phase = std::fmod(t, HOUR) / HOUR;
    const double cx = -90 + phase * 160, cy = -10 + phase * 18;  // km east / north of home
    const double x = cx + (rnd(1) - 0.5) * 22, y = cy + (rnd(2) - 0.5) * 22;
    const double dist = std::hypot(x, y);
    if (dist > 100) continue;
    const double az = std::fmod(std::atan2(x, y) * 180 / 3.14159265358979 + 360, 360.0);
    d.strikes.push_back({(uint32_t) s, (float) std::lround(dist), (float) std::lround(az), std::round(t)});
  }
  const bool approaching = std::fmod(now, HOUR) / HOUR < 0.55;
  d.trend = approaching ? 1 : -1;
  d.kmh = approaching ? 48 : -44;
  summarise(d, now);
  return d;
}
// one line for a Home card or chip (docs lightningSummary): quiet unless there were strikes in the last 15 minutes.
// tone: 0 dim, 1 info, 2 warn.
struct Summary { std::string status; int tone; bool active; };
inline Summary summary(const Data &d, units::Distance u = units::Distance::km, units::Speed v = units::Speed::kmh) {
  char b[96];
  if (!d.has_nearest) {
    if (d.m60) snprintf(b, sizeof b, "quiet · %d strikes in the last hour", d.m60);
    else snprintf(b, sizeof b, "quiet · nothing within %s", units::distance(d.radius, u).c_str());
    return {b, 0, false};
  }
  const std::string where = units::distance(d.nearest.d, u) + " " + compass(d.nearest.az);
  if (d.trend > 0) {
    snprintf(b, sizeof b, "approaching · %s · ~%s", where.c_str(), units::speed_kmh(d.kmh, v).c_str());
    return {b, 2, true};
  }
  snprintf(b, sizeof b, "%s%s · %d in 15 min", d.nearest.d <= 25 ? "nearby · " : "", where.c_str(), d.m15);
  return {b, d.nearest.d <= 25 ? 2 : 1, d.nearest.d <= 25 || d.storm};
}
}  // namespace fw::lx
