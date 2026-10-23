// Energy feed (docs @facet/ui EnergyData + @facet/demo-data energy()): today's half-hours (kWh, rate, price band), the
// cost so far, the current rate and when it changes, use against yesterday, and the last 14 days. A panel fills the
// same Data from HA (an energy sensor + the tariff's rate sensors, a host job); sample() is the docs time-of-use tariff
// until then: cheap 04-07, 13-16 and 22-24, peak 16-19, normal the rest. Header-only.
#pragma once
#include <cmath>
#include <cstdio>
#include <ctime>
#include <string>

namespace fw::energy {
enum Band { CHEAP, NORMAL, PEAK };
constexpr const char *BAND_ID[3] = {"cheap", "normal", "peak"};
constexpr double RATE[3] = {0.135, 0.262, 0.394};  // £ per kWh
inline Band band_at(double hr) { return (hr >= 4 && hr < 7) || (hr >= 13 && hr < 16) || hr >= 22 ? CHEAP : hr >= 16 && hr < 19 ? PEAK : NORMAL; }
struct Slot { double start; double kwh; Band band; };  // kwh NAN = not yet
struct DayUse { double t; double kwh, cost; };
struct Data {
  Slot slots[48];
  double kwh = 0, cost = 0, yesterday_to_now = NAN, watts = NAN;
  Band band = NORMAL, next_band = NORMAL;
  double until = 0;         // when the rate changes (ms)
  int now_slot = -1;        // the half-hour holding now
  DayUse days[14];          // oldest first, today last
};
inline double midnight(double t) {
  const time_t s = (time_t) (t / 1000);
  struct tm tm;
  localtime_r(&s, &tm);
  tm.tm_hour = tm.tm_min = tm.tm_sec = 0;
  tm.tm_isdst = -1;
  return (double) mktime(&tm) * 1000;
}
inline Data sample(double now, double watts = 420) {
  Data d;
  const double d0 = midnight(now), H = 3600000;
  double kwh = 0, cost = 0;
  for (int i = 0; i < 48; i++) {
    const double st = d0 + i * 30 * 60000.0;
    const Band b = band_at(i / 2.0);
    const bool past = st + 30 * 60000.0 <= now;
    const double base = 0.12 + 0.18 * std::fabs(std::sin(i * 0.61)) + (b == CHEAP ? 0.32 : 0) + (i >= 14 && i <= 17 ? 0.25 : 0);
    const double k = past ? std::round(base * 100) / 100 : NAN;
    d.slots[i] = {st, k, b};
    if (past) { kwh += k; cost += k * RATE[b]; }
    if (st <= now && now < st + 30 * 60000.0) d.now_slot = i;
  }
  d.kwh = std::round(kwh * 10) / 10;
  d.cost = cost + 0.48;  // + a standing charge
  const time_t s = (time_t) (now / 1000);
  struct tm tm;
  localtime_r(&s, &tm);
  const double hr = tm.tm_hour + tm.tm_min / 60.0;
  d.band = band_at(hr);
  const double edges[7] = {4, 7, 13, 16, 19, 22, 24};
  double until = 24;
  for (double e : edges) if (e > hr) { until = e; break; }
  d.until = d0 + until * H;
  d.next_band = band_at(std::fmod(until, 24.0));
  d.yesterday_to_now = std::round(kwh * 1.12 * 10) / 10;
  d.watts = watts;
  for (int k = 0; k < 14; k++) {
    const int i = 13 - k;
    if (i == 0) { d.days[k] = {d0, d.kwh, d.cost}; continue; }
    const double dk = 15.4 + 3.2 * std::fabs(std::sin(i * 1.7)) + (i > 7 ? 1.1 : 0);  // the week before used a bit more
    d.days[k] = {d0 - i * 24 * H, std::round(dk * 10) / 10, dk * 0.21 + 0.48};
  }
  return d;
}
inline std::string money(double gbp) {
  if (std::isnan(gbp)) return "\xC2\xA3--";
  char b[24];
  snprintf(b, sizeof b, "\xC2\xA3%.2f", gbp);
  return b;
}
inline std::string pence(double gbp) {
  if (std::isnan(gbp)) return "--";
  char b[16];
  snprintf(b, sizeof b, "%.1fp", gbp * 100);
  return b;
}
}  // namespace fw::energy
