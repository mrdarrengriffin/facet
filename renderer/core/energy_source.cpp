// The energy feed (CONTRACT.md C51): the reserved entity panel.energy (core/spec.h "local sources") in the HA store,
// the one shape the Energy screen binds to, whatever measures the use. Same file on the panels and in WASM.
//   state ok | loading (no data yet) | error (the source is unavailable); attributes: cost ("£2.42"), line ("4.2 kWh ·
//   12% less than yesterday by now"), rate ("Normal 26.2p until 16:00, then 39.4p"), tone (the band as a status tone:
//   peak working, cheap info, normal idle), values (48 half-hours of kWh, "-" = not yet), bands (48 band ids), now (the
//   half-hour holding now), watts ("420 W now").
//   The week (the screen's Week view, docs energy.ts weekView): week_cost (this week's cost, "£27.10"), week_line
//   ("112.4 kWh · 6% less a day than the week before": compared per complete day, today left out), week (7 days of
//   kWh, today last), week_before (the 7 days before), days ("Fri,Sat,...,Today": the axis words).
// Adapters (the feed's settings, the spec's "feeds"; core/ha.h feed_config): source
//   ha      two Home Assistant sensors in the panel energy shape (a live subscription like any bind):
//           sensor (today): state = kWh so far; attributes cost (£ so far, standing charge included), w (W now, -1 =
//           unknown), rate (£/kWh x 1000 now), band (0 cheap, 1 normal, 2 peak), until (unix s the rate changes),
//           next (£/kWh x 1000 after that, -1 = none), next_band, slots ("J" + JSON: 48 half-hours of [Wh | -1 not
//           yet, band, £/kWh x 1000, £ x 1000]); days (setting days): attribute days ("J" + JSON: the 14 days before
//           today, oldest first, [kWh x 100 | -1, £ x 100 | -1]) and yday_now (kWh yesterday up to this time of day,
//           -1 = unknown). Missing pieces show as "--", never an error.
//   sample  the docs time-of-use tariff day (core/energy.h sample), for previews and tests
// More adapters (a tariff integration read directly, the HA Energy dashboard, a supplier's API) = more rows here, no
// screen change. Refreshed once a minute while a bind reads it (unchanged values are no update).
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
void publish(const fw::energy::Data &d, double rate_now, double rate_next) {
  namespace en = fw::energy;
  auto put = [](const char *a, const std::string &x) { ha::put(E, a, x); };
  put("", "ok");
  put("cost", en::money(d.cost));
  char b[96];
  const double y = d.yesterday_to_now;
  if (!std::isnan(y) && y > 0) {
    const long delta = std::lround((d.kwh - y) / y * 100);
    snprintf(b, sizeof b, "%.1f kWh \xC2\xB7 %ld%% %s than yesterday by now", d.kwh, std::labs(delta), delta <= 0 ? "less" : "more");
  } else if (!std::isnan(d.kwh)) snprintf(b, sizeof b, "%.1f kWh", d.kwh);
  else snprintf(b, sizeof b, "-- kWh");
  put("line", b);
  std::string name = en::BAND_ID[d.band];
  name[0] = (char) toupper((unsigned char) name[0]);
  if (std::isnan(rate_now)) snprintf(b, sizeof b, "%s", "");
  else if (d.until <= 0 || std::isnan(rate_next)) snprintf(b, sizeof b, "%s %s", name.c_str(), en::pence(rate_now).c_str());
  else snprintf(b, sizeof b, "%s %s until %s, then %s", name.c_str(), en::pence(rate_now).c_str(), hm(d.until).c_str(), en::pence(rate_next).c_str());
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
    int na = 0, np = 0;  // the days known (a source may miss some: those count as no data, not as 0)
    std::string w, wb, dn;
    static const char *DAY[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    auto z = [](double x) { return std::isnan(x) ? 0.0 : x; };
    for (int i = 0; i < 7; i++) {
      const en::DayUse &c = d.days[7 + i], &p = d.days[i];
      wc += z(c.cost), wk += z(c.kwh);
      if (!std::isnan(p.kwh)) pv += p.kwh, np++;
      if (i < 6 && !std::isnan(c.kwh)) a += c.kwh, na++;  // complete days only (today is partial)
      char x[16];
      if (std::isnan(c.kwh)) snprintf(x, sizeof x, "0");
      else snprintf(x, sizeof x, "%.1f", c.kwh);
      w += (i ? "," : "") + std::string(x);
      if (std::isnan(p.kwh)) snprintf(x, sizeof x, "0");
      else snprintf(x, sizeof x, "%.1f", p.kwh);
      wb += (i ? "," : "") + std::string(x);
      const time_t s = (time_t) (c.t / 1000 + 43200);  // noon: the day, whatever the zone
      struct tm tm;
      localtime_r(&s, &tm);
      dn += (i ? "," : "") + std::string(i == 6 ? "Today" : DAY[tm.tm_wday]);
    }
    a = na ? a / na : 0, pv = np ? pv / np : 0;
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

// ---- adapter: ha (two sensors in the panel energy shape)
using Doc = ArduinoJson::JsonDocument;
std::string cfg_seen, today_e, days_e;
double attr_num(const std::string &e, const char *a) { return ha::num(ha::get(e, a)); }
bool parse_j(const std::string *s, Doc &j) { return s && !s->empty() && !deserializeJson(j, s->c_str() + ((*s)[0] == 'J' ? 1 : 0)); }
fw::energy::Band band_of(int b) { return b <= 0 ? fw::energy::CHEAP : b >= 2 ? fw::energy::PEAK : fw::energy::NORMAL; }
void put_empty(bool err) {
  ha::put(E, "cost", fw::energy::money(NAN)), ha::put(E, "line", err ? "energy offline" : "loading");
  ha::put(E, "rate", ""), ha::put(E, "tone", "idle"), ha::put(E, "watts", "-- W now");
  ha::put(E, "", err ? "error" : "loading");
}
void from_ha() {
  namespace en = fw::energy;
  const std::string *st = ha::get(today_e);
  if (!st) { put_empty(false); return; }
  if (*st == "unavailable" || *st == "unknown") { put_empty(true); return; }
  const double now = (double) time(nullptr) * 1000, d0 = en::midnight(now);
  en::Data d;
  d.kwh = ha::num(st);
  const double cost = attr_num(today_e, "cost");
  d.cost = cost >= 0 ? cost : NAN;
  const double w = attr_num(today_e, "w");
  d.watts = w >= 0 ? w : NAN;
  const double rate = attr_num(today_e, "rate"), next = attr_num(today_e, "next"), until = attr_num(today_e, "until");
  d.band = band_of((int) attr_num(today_e, "band"));
  d.next_band = band_of((int) attr_num(today_e, "next_band"));
  d.until = until > 0 ? until * 1000 : 0;
  Doc sl;
  const bool have = parse_j(ha::get(today_e, "slots"), sl);
  for (int i = 0; i < 48; i++) {
    const double start = d0 + i * 30 * 60000.0;
    d.slots[i] = {start, NAN, en::band_at(i / 2.0)};
    if (have && i < (int) sl.size()) {
      const double wh = sl[i][0] | -1.0;
      d.slots[i].band = band_of(sl[i][1] | 1);
      if (wh >= 0) d.slots[i].kwh = std::round(wh / 10) / 100;
    }
    if (start <= now && now < start + 30 * 60000.0) d.now_slot = i;
  }
  Doc dy;
  const bool days = !days_e.empty() && parse_j(ha::get(days_e, "days"), dy);
  const size_t n = days ? dy.size() : 0;
  for (int k = 0; k < 14; k++) {
    const double t = d0 - (13 - k) * 24 * 3600000.0;
    d.days[k] = {t, NAN, NAN};
    if (k == 13) { d.days[k] = {d0, d.kwh, d.cost}; continue; }
    const long at = (long) n - 13 + k;  // the 13 days before today = the last 13 of the list
    if (at >= 0 && at < (long) n) {
      const double kw = dy[at][0] | -1.0, c = dy[at][1] | -1.0;
      d.days[k].kwh = kw >= 0 ? kw / 100 : NAN;
      d.days[k].cost = c >= 0 ? c / 100 : NAN;
    }
  }
  const double y = days_e.empty() ? NAN : attr_num(days_e, "yday_now");
  d.yesterday_to_now = y >= 0 ? y : NAN;
  publish(d, rate >= 0 ? rate / 1000 : NAN, next >= 0 ? next / 1000 : NAN);
}
void entity_put(const std::string &e, const std::string &, const std::string &) {
  if (!today_e.empty() && (e == today_e || e == days_e) && ha::watched(E)) from_ha();
}
void fresh() {
  if (!tick) tick = lv_timer_create([](lv_timer_t *) { if (ha::watched(E)) fresh(); }, 60000, nullptr);
  static const bool hooked = (ha::on_put(entity_put), true);
  (void) hooked;
  const std::string &cfg = ha::feed_config(E);
  Doc c;
  deserializeJson(c, cfg.empty() ? "{}" : cfg.c_str());
  const std::string src = c["source"] | "sample", ent = c["sensor"] | "", dent = c["days"] | "";
  const bool changed = cfg != cfg_seen;
  cfg_seen = cfg;
  if (src == "ha" && ent.rfind("sensor.", 0) == 0) {
    if (changed || ent != today_e || dent != days_e) {
      today_e = ent, days_e = dent.rfind("sensor.", 0) == 0 ? dent : "";
      for (const char *a : {"", "cost", "w", "rate", "band", "until", "next", "next_band", "slots"}) ha::want(today_e, a);
      if (!days_e.empty()) for (const char *a : {"days", "yday_now"}) ha::want(days_e, a);
    }
    from_ha();
  } else if (src == "ha") { today_e.clear(), days_e.clear(); put_empty(true); }  // no sensor picked
  else {
    today_e.clear(), days_e.clear();
    const fw::energy::Data d = fw::energy::sample((double) time(nullptr) * 1000);
    publish(d, fw::energy::RATE[d.band], fw::energy::RATE[d.next_band]);
  }
}
[[maybe_unused]] const bool registered = (ha::on_local(E, fresh), true);  // binds on panel.energy never reach a host
}  // namespace
}  // namespace fw::dev::energy_source
