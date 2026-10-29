// The energy feed (CONTRACT.md C51): the reserved entity panel.energy (core/spec.h "local sources") in the HA store,
// the one shape the Energy screen binds to. Adapters fill it; the only one so far is the sample (core/energy.h: the
// docs time-of-use tariff day); HA energy sensors + tariff rates (Octopus etc.) come as more adapters, no screen
// change. Same file on the panels and in WASM.
//   state "ok"; attributes: cost ("£2.42"), line ("4.2 kWh · 12% less than yesterday by now"), rate ("Normal 26.2p
//   until 16:00, then 39.4p"), tone (the band as a status tone: peak working, cheap info, normal idle), values (48
//   half-hours of kWh, "-" = not yet), bands (48 band ids), now (the half-hour holding now), watts ("420 W now").
//   The week (the screen's Week view, docs energy.ts weekView): week_cost (this week's cost, "£27.10"), week_line
//   ("112.4 kWh · 6% less a day than the week before": compared per complete day, today left out), week (7 days of
//   kWh, today last), week_before (the 7 days before), days ("Fri,Sat,...,Today": the axis words).
// Refreshed once a minute once a bind has read it (unchanged values are no update: the chart moves on a new half hour).
#include <cctype>
#include <cstdio>
#include <ctime>
#include "energy.h"
#include "fw.h"
#include "ha.h"

namespace fw::dev::energy_source {
namespace {
constexpr const char *E = "panel.energy";
lv_timer_t *tick = nullptr;
std::string hm(double t) {
  const time_t s = (time_t) (t / 1000);
  struct tm tm;
  localtime_r(&s, &tm);
  char b[8];
  snprintf(b, sizeof b, "%02d:%02d", tm.tm_hour, tm.tm_min);
  return b;
}
void fresh() {
  namespace en = fw::energy;
  if (!tick) tick = lv_timer_create([](lv_timer_t *) { fresh(); }, 60000, nullptr);
  const en::Data d = en::sample((double) time(nullptr) * 1000);
  auto put = [](const char *a, const std::string &x) { ha::put(E, a, x); };
  put("", "ok");
  put("cost", en::money(d.cost));
  char b[96];
  const double y = d.yesterday_to_now;
  if (!std::isnan(y) && y > 0) {
    const long delta = std::lround((d.kwh - y) / y * 100);
    snprintf(b, sizeof b, "%.1f kWh \xC2\xB7 %ld%% %s than yesterday by now", d.kwh, std::labs(delta), delta <= 0 ? "less" : "more");
  } else snprintf(b, sizeof b, "%.1f kWh", d.kwh);
  put("line", b);
  std::string name = en::BAND_ID[d.band];
  name[0] = (char) toupper((unsigned char) name[0]);
  snprintf(b, sizeof b, "%s %s until %s, then %s", name.c_str(), en::pence(en::RATE[d.band]).c_str(), hm(d.until).c_str(), en::pence(en::RATE[d.next_band]).c_str());
  put("rate", b);
  put("tone", d.band == en::PEAK ? "working" : d.band == en::CHEAP ? "info" : "idle");
  std::string v, bands;
  for (int i = 0; i < 48; i++) {
    char x[16];
    if (std::isnan(d.slots[i].kwh)) snprintf(x, sizeof x, "-");
    else snprintf(x, sizeof x, "%.2f", d.slots[i].kwh);
    v += (i ? "," : "") + std::string(x);
    bands += (i ? "," : "") + std::string(en::BAND_ID[d.slots[i].band]);
  }
  put("values", v);
  put("bands", bands);
  put("now", std::to_string(d.now_slot));
  {  // the week: this week against the week before (days[0..6] = the week before, days[7..13] = this week)
    double wc = 0, wk = 0, a = 0, pv = 0;
    std::string w, wb, dn;
    static const char *DAY[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    for (int i = 0; i < 7; i++) {
      const en::DayUse &c = d.days[7 + i], &p = d.days[i];
      wc += c.cost, wk += c.kwh, pv += p.kwh;
      if (i < 6) a += c.kwh;  // complete days only (today is partial)
      char x[16];
      snprintf(x, sizeof x, "%.1f", c.kwh);
      w += (i ? "," : "") + std::string(x);
      snprintf(x, sizeof x, "%.1f", p.kwh);
      wb += (i ? "," : "") + std::string(x);
      const time_t s = (time_t) (c.t / 1000 + 43200);  // noon: the day, whatever the zone
      struct tm tm;
      localtime_r(&s, &tm);
      dn += (i ? "," : "") + std::string(i == 6 ? "Today" : DAY[tm.tm_wday]);
    }
    a /= 6, pv /= 7;
    put("week_cost", en::money(wc));
    const long delta = pv > 0 ? std::lround((a - pv) / pv * 100) : 0;
    if (pv > 0) snprintf(b, sizeof b, "%.1f kWh \xC2\xB7 %ld%% %s a day than the week before", wk, std::labs(delta), delta <= 0 ? "less" : "more");
    else snprintf(b, sizeof b, "%.1f kWh", wk);
    put("week_line", b);
    put("week", w);
    put("week_before", wb);
    put("days", dn);
  }
  if (std::isnan(d.watts)) put("watts", "-- W now");
  else { snprintf(b, sizeof b, "%ld W now", std::lround(d.watts)); put("watts", b); }
}
[[maybe_unused]] const bool registered = (ha::on_local(E, fresh), true);  // binds on panel.energy never reach a host
}  // namespace
}  // namespace fw::dev::energy_source
