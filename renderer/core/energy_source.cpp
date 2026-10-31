// The energy feed (CONTRACT.md C51): the reserved entity panel.energy (core/spec.h "local sources") in the HA store,
// the one shape the Energy screen binds to, whatever measures the use. Same file on the panels and in WASM.
//   state ok | loading (no data yet) | error (the source is unavailable); attributes: cost ("£2.42"), line ("4.2 kWh ·
//   12% less than yesterday by now"), rate ("Normal 26.2p until 16:00, then 39.4p"), tone (the band as a status tone:
//   peak working, cheap info, normal idle), values (48 half-hours of kWh, "-" = not yet), bands (48 band ids), now (the
//   half-hour holding now), watts ("420 W now").
//   The week (the screen's Week view, docs energy.ts weekView): week_cost (this week's cost, "£27.10"), week_line
//   ("112.4 kWh · 6% less a day than the week before": compared per complete day, today left out), week (7 days of
//   kWh, today last), week_before (the 7 days before), days ("Fri,Sat,...,Today": the axis words).
//   found (what the ha adapter found in HA's Energy dashboard: "Grid, Solar · 3 devices"; Settings > Screens > Energy).
//   The extra's words (empty / the plain ones without an extra): live (the header pill: "536 W · 43.2p now → 28.8p at
//   19:00", else = watts), card + card_tone (Home: "536 W · 43.2p", ok when cheap, warn at peak, else dim), cheapest
//   ("Cheapest 22:00–00:00"), event + event_tone ("Saving session tomorrow 17:30–18:30", warn; a greener night ok).
// Adapters (the feed's settings, the spec's "feeds"; core/ha.h feed_config): source
//   ha      Home Assistant's Energy dashboard (2026-10-31: nothing to pick, energy is already standard in HA), read over
//           the panel's own HA link (core/device.h ha_link::ws, reads only): energy/get_prefs (the grid's import /
//           export / cost statistics, solar, battery, gas, water, the devices) every 10 minutes, then
//           recorder/statistics_during_period: 5-minute changes from yesterday's midnight (today's half-hours and
//           yesterday up to now) every minute, daily changes for the 13 days before today every 15 minutes. Use = grid
//           in + solar - grid out + battery out - battery in; cost = the grid's cost statistics (else use x the
//           dashboard's price). The rate = the dashboard's price (entity or fixed), else the last half-hour's cost /
//           kWh; no tariff bands (all "normal"). Live W = the grid's power sensor when the dashboard has one. Each
//           reply is streamed through a small reader (Sax: a key path, never the whole reply). Not linked / no grid or
//           solar in the dashboard = an error state saying so.
//   sample  the docs time-of-use tariff day (core/energy.h sample), for previews and tests
//   fixtures the sample day + the extras' test data (sources/fixtures/<extra>.json, compiled in as fw::spec::FIXTURES:
//           served as if HA answered), so an extra can be tried without owning it
// Extras (Darren 2026-10-31: the base = HA's Energy dashboard + ONE optional extra integration; sources.json
// screens.energy.extras): each = a matcher (installed? its entities, from the dashboard's grid statistics) + a mapper
// (live watts, the rate now / next, the day's rate bands, events) + a rank. The feed setting "extra": auto (the
// best-ranked one found) | an extra's id | none. Found automatically, nothing to pick.
//   octopus  the Octopus Energy integration (BottlecapDave): the grid stat sensor.octopus_energy_electricity_<serial>_
//            <mpan>_current_accumulative_consumption gives the meter; <meter>_current_demand (live W, an HA want),
//            GET states/event.<meter>_current_day_rates + _next_day_rates (attribute rates: start, end, value_inc_vat)
//            every 30 minutes, GET calendars (the account's Octoplus calendars: _octoplus_saving_sessions,
//            _octoplus_power_down, _greener_nights) hourly, their events today + tomorrow every 15 minutes. Bands
//            relative to the day: cheap = the lowest third of the day's range, peak = the top third, a flat day normal.
// More adapters (a supplier's API) = more rows here, no screen change. Refreshed once a minute while a bind reads it
// (unchanged values are no update).
#include <algorithm>
#include <cctype>
#include <cstring>
#include <cstdio>
#include <ctime>
#include "energy.h"
#include "fw.h"
#include "ha.h"
#include "device.h"
#include "spec.h"
#include <functional>
#include <memory>
#include <vector>

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
// what the extra adds to the feed (empty = no extra: the plain words)
struct ExtOut { bool on = false; std::string live, card, tone = "dim", cheapest, event, event_tone = "idle"; };
void publish(const fw::energy::Data &d, double rate_now, double rate_next, bool banded = true, const ExtOut *x = nullptr) {
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
  else if (!banded) snprintf(b, sizeof b, "%s a kWh", en::pence(rate_now).c_str());
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
  if (std::isnan(d.watts)) snprintf(b, sizeof b, "-- W now");
  else snprintf(b, sizeof b, "%ld W now", std::lround(d.watts));
  const bool on = x && x->on;
  put("watts", b);
  put("live", on ? x->live : b);
  put("card", on ? x->card : b);
  put("card_tone", on ? x->tone : "dim");
  put("cheapest", on ? x->cheapest : "");
  put("event", on ? x->event : "");
  put("event_tone", on ? x->event_tone : "idle");
}

// ---- adapter: ha (HA's Energy dashboard over the panel's own link)
namespace hl = fw::dev::ha_link;
using Doc = ArduinoJson::JsonDocument;
constexpr double MIN = 60000, H = 3600000, DAY = 24 * H;
// Sax: a streaming JSON reader for one WS reply. leaf() for every scalar (strings capped at CAP), close() at an
// object's end; the path = each open container's key (object) / index (array), held per depth, nothing else
struct Sax {
  static constexpr int DEPTH = 10;
  static constexpr size_t CAP = 200;
  struct F { bool obj; bool want_key; int idx; std::string key; };
  F st[DEPTH];
  int depth = 0;
  bool in_str = false, esc = false, is_key = false, bad = false, ok = true;
  int hex = -1;
  std::string buf, lit;
  std::function<void(Sax &, const std::string &, bool)> leaf;  // (value, a string?)
  std::function<void(Sax &)> close;                             // the object at depth - 1 ends
  const std::string &key(int d) const { static const std::string none; return d < depth && st[d].obj ? st[d].key : none; }
  int idx(int d) const { return d < depth && !st[d].obj ? st[d].idx : -1; }
  void scalar(const std::string &v, bool str) {
    if (depth == 1 && key(0) == "success" && v == "false") ok = false;
    if (leaf) leaf(*this, v, str);
  }
  void end_lit() { if (!lit.empty()) { scalar(lit, false); lit.clear(); } }
  void feed(const char *p, size_t n) {
    for (size_t i = 0; i < n && !bad; i++) {
      const char c = p[i];
      if (in_str) {
        if (hex >= 0) { if (++hex == 4) hex = -1; continue; }  // \uXXXX: ids and keys are ASCII (dropped)
        if (esc) { esc = false; if (c == 'u') hex = 0; else if (buf.size() < CAP) buf += c; continue; }
        if (c == '\\') { esc = true; continue; }
        if (c != '"') { if (buf.size() < CAP) buf += c; continue; }
        in_str = false;
        if (is_key) st[depth - 1].key = buf;
        else scalar(buf, true);
        continue;
      }
      switch (c) {
        case '"':
          in_str = true, buf.clear();
          is_key = depth > 0 && st[depth - 1].obj && st[depth - 1].want_key;
          break;
        case '{': case '[':
          if (depth >= DEPTH) { bad = true; break; }
          st[depth] = {c == '{', c == '{', 0, std::string()};
          depth++;
          break;
        case '}': case ']':
          end_lit();
          if (!depth || st[depth - 1].obj != (c == '}')) { bad = true; break; }
          if (c == '}' && close) close(*this);
          depth--;
          break;
        case ':':
          if (depth) st[depth - 1].want_key = false;
          break;
        case ',':
          end_lit();
          if (depth) { if (st[depth - 1].obj) st[depth - 1].want_key = true; else st[depth - 1].idx++; }
          break;
        case ' ': case '\n': case '\r': case '\t':
          break;
        default:
          if (lit.size() < 32) lit += c;
      }
    }
  }
};
enum Role { R_IN, R_OUT, R_SOLAR, R_BOUT, R_BIN, R_COST };
struct Prefs {
  bool read = false;
  std::vector<std::pair<std::string, Role>> stats;
  std::string price_e, power_e, found;
  double price = NAN;
  bool grid = false, solar = false, battery = false, gas = false, water = false;
  int devices = 0;
};
struct Src { std::string type, from, to, cost, price_e, power_e; double price = NAN; std::vector<std::string> flow_in, flow_out, flow_cost; };
Prefs prefs, reading;
Src src_now;
std::unique_ptr<Sax> sax;
enum Step { S_IDLE, S_PREFS, S_TODAY, S_DAYS, S_EXTRA } step = S_IDLE;
void choose_ha();
double prefs_at = -1e18, today_at = -1e18, days_at = -1e18, days_for = 0;
std::string why_now;  // the last failure, "" = fine
bool want_data = false;  // source ha: today + days too (else only the prefs, for Settings' "found")
double slot_k[48], slot_c[48], yday_k, day_k[13], day_c[13];
bool slot_any[48], yday_any, day_any[13];
// the rows of a statistics reply: start + change per statistic id, summed into the buckets by role
double row_start = NAN, row_change = NAN;
Role role_of(const std::string &id, bool &found) {
  for (auto &p : prefs.stats) if (p.first == id) { found = true; return p.second; }
  found = false;
  return R_IN;
}
double sign(Role r) { return r == R_OUT || r == R_BIN ? -1 : 1; }

// the extra (below): chosen from the grid statistics, then laid over the base data before publish
ExtOut overlay(fw::energy::Data &d, double &rate_now, double &rate_next, bool &banded);
struct Extra;
const Extra *ext = nullptr;
const char *ext_name();
bool fixtures = false;   // source fixtures: the sample day + the extras' test data
std::string extra_cfg = "auto";
void show();

void put_state(const char *st, const char *line) {
  ha::put(E, "cost", fw::energy::money(NAN)), ha::put(E, "line", line);
  ha::put(E, "rate", ""), ha::put(E, "tone", "idle"), ha::put(E, "watts", "-- W now");
  ha::put(E, "live", "-- W now"), ha::put(E, "card", "-- W now"), ha::put(E, "card_tone", "dim");
  ha::put(E, "cheapest", ""), ha::put(E, "event", ""), ha::put(E, "event_tone", "idle");
  ha::put(E, "", st);
}
void put_found() {
  const char *f = !hl::ready() ? "Home Assistant not connected" : !prefs.read ? (why_now.empty() ? "Reading Home Assistant" : why_now.c_str())
                  : prefs.found.empty() ? "Not set up in Home Assistant" : prefs.found.c_str();
  ha::put(E, "found", ext && !fixtures && prefs.read && !prefs.found.empty() ? prefs.found + " \xC2\xB7 " + ext_name() : std::string(f));
}
void from_ha() {
  namespace en = fw::energy;
  if (!hl::ready()) { put_state("error", "Home Assistant not connected"); return; }
  if (!prefs.read) { if (why_now.empty()) put_state("loading", "loading"); else put_state("error", "Home Assistant not reached"); return; }
  if (!prefs.grid && !prefs.solar) { put_state("error", "Energy dashboard not set up in Home Assistant"); return; }
  if (today_at < 0) { put_state("loading", "loading"); return; }
  const double now = (double) time(nullptr) * 1000, d0 = en::midnight(now);
  bool costed = false;
  for (auto &p : prefs.stats) costed |= p.second == R_COST;
  double price = prefs.price;
  if (!prefs.price_e.empty()) { const double x = ha::num(ha::get(prefs.price_e)); if (std::isfinite(x) && x >= 0) price = x; }
  en::Data d;
  double kwh = 0, cost = 0, last_rate = NAN;
  bool any = false;
  for (int i = 0; i < 48; i++) {
    const double st = d0 + i * 30 * MIN;
    d.slots[i] = {st, slot_any[i] ? std::max(0.0, slot_k[i]) : NAN, en::NORMAL};
    if (slot_any[i]) {
      kwh += std::max(0.0, slot_k[i]), any = true;
      const double c = costed ? slot_c[i] : std::isnan(price) ? NAN : std::max(0.0, slot_k[i]) * price;
      cost += std::isnan(c) ? 0 : c;
      if (costed && slot_k[i] > 0.01 && slot_c[i] > 0) last_rate = slot_c[i] / slot_k[i];
    }
    if (st <= now && now < st + 30 * MIN) d.now_slot = i;
  }
  d.kwh = any ? std::round(kwh * 10) / 10 : 0;
  d.cost = costed || !std::isnan(price) ? cost : NAN;
  d.yesterday_to_now = yday_any ? yday_k : NAN;
  d.watts = NAN;
  if (!prefs.power_e.empty()) { const double w = ha::num(ha::get(prefs.power_e)); if (std::isfinite(w)) d.watts = w; }
  for (int k = 0; k < 14; k++) {
    if (k == 13) { d.days[k] = {d0, d.kwh, d.cost}; continue; }
    const double t = en::midnight(d0 - (13 - k) * DAY + 12 * H);
    d.days[k] = {t, day_any[k] ? std::max(0.0, day_k[k]) : NAN, day_any[k] && costed ? day_c[k] : day_any[k] && !std::isnan(price) ? std::max(0.0, day_k[k]) * price : NAN};
  }
  double rn = std::isnan(price) ? last_rate : price, rx = NAN;
  bool banded = false;
  const ExtOut x = overlay(d, rn, rx, banded);
  publish(d, rn, rx, banded, &x);
}
void next();
// one read (a WS command or a REST GET): a fresh Sax with these handlers, then done(ok) and the next step. soft: an
// extra's read (its failure is not the feed's: the next read goes on, why_now untouched)
using Feed = std::function<void(const char *, size_t)>;
using End = std::function<void(bool, const std::string &)>;
using Issue = std::function<uint32_t(Feed, End)>;
Issue ws_of(const std::string &cmd) { return [cmd](Feed f, End e) { return hl::ws(cmd, std::move(f), std::move(e)); }; }
uint32_t ext_get(const std::string &path, Feed f, End e);
Issue get_of(const std::string &path) { return [path](Feed f, End e) { return ext_get(path, std::move(f), std::move(e)); }; }
void start(Step s, Issue issue, std::function<void(Sax &, const std::string &, bool)> leaf, std::function<void(Sax &)> close, std::function<void(bool)> done, bool soft = false) {
  sax.reset(new Sax());
  sax->leaf = std::move(leaf), sax->close = std::move(close);
  step = s;
  const uint32_t id = issue([](const char *p, size_t n) { if (sax) sax->feed(p, n); }, [done, soft](bool ok, const std::string &why) {
    const bool good = ok && sax && !sax->bad && sax->ok;
    if (!soft) why_now = good ? "" : !ok ? "Home Assistant not reached: " + why : "Home Assistant said no";
    sax.reset();
    step = S_IDLE;
    done(good);
    put_found();
    show();
    if (good || soft) next();
  });
  if (!id) { step = S_IDLE, sax.reset(); }
}
std::string iso(double ms) {
  const time_t s = (time_t) (ms / 1000);
  struct tm tm;
  gmtime_r(&s, &tm);
  char b[32];
  strftime(b, sizeof b, "%Y-%m-%dT%H:%M:%SZ", &tm);
  return b;
}
std::string stats_cmd(double from, const char *period) {
  Doc c;
  c["type"] = "recorder/statistics_during_period", c["start_time"] = iso(from), c["period"] = period;
  for (auto &p : prefs.stats) c["statistic_ids"].add(p.first);
  c["types"].add("change");
  c["units"]["energy"] = "kWh";
  std::string out;
  serializeJson(c, out);
  return out;
}
void read_prefs() {
  reading = Prefs();
  start(S_PREFS, ws_of("{\"type\":\"energy/get_prefs\"}"),
        [](Sax &x, const std::string &v, bool str) {
          const std::string &list = x.key(1);
          if (list == "device_consumption" && x.depth == 4 && x.key(3) == "stat_consumption" && str) reading.devices++;
          if (list != "energy_sources" || !str) return;
          const std::string &k = x.depth == 4 ? x.key(3) : x.depth == 6 ? x.key(5) : std::string();
          if (x.depth == 4) {
            if (k == "type") src_now.type = v;
            else if (k == "stat_energy_from") src_now.from = v;
            else if (k == "stat_energy_to") src_now.to = v;
            else if (k == "stat_cost") src_now.cost = v;
            else if (k == "entity_energy_price") src_now.price_e = v;
            else if (k == "stat_rate" || k == "stat_power") src_now.power_e = v;
          } else if (x.depth == 6) {  // the older grid shape: flow_from / flow_to lists
            if (k == "stat_energy_from") src_now.flow_in.push_back(v);
            else if (k == "stat_energy_to") src_now.flow_out.push_back(v);
            else if (k == "stat_cost") src_now.flow_cost.push_back(v);
            else if (k == "entity_energy_price" && src_now.price_e.empty()) src_now.price_e = v;
          }
        },
        [](Sax &x) {
          if (x.key(1) != "energy_sources" || x.depth != 4) return;  // one energy source's object ends
          Src s = std::move(src_now);
          src_now = Src();
          auto add = [](const std::string &id, Role r) { if (!id.empty()) reading.stats.push_back({id, r}); };
          if (s.type == "grid") {
            reading.grid = true;
            add(s.from, R_IN), add(s.to, R_OUT), add(s.cost, R_COST);
            for (auto &i : s.flow_in) add(i, R_IN);
            for (auto &i : s.flow_out) add(i, R_OUT);
            for (auto &i : s.flow_cost) add(i, R_COST);
            if (!s.price_e.empty()) reading.price_e = s.price_e;
            if (!s.power_e.empty()) reading.power_e = s.power_e;
          } else if (s.type == "solar") reading.solar = true, add(s.from, R_SOLAR);
          else if (s.type == "battery") reading.battery = true, add(s.from, R_BOUT), add(s.to, R_BIN);
          else if (s.type == "gas") reading.gas = true;
          else if (s.type == "water") reading.water = true;
        },
        [](bool ok) {
          if (!ok) return;
          reading.read = true;
          std::string f;
          const std::pair<bool, const char *> parts[] = {{reading.grid, "Grid"}, {reading.solar, "Solar"}, {reading.battery, "Battery"}, {reading.gas, "Gas"}, {reading.water, "Water"}};
          for (auto &p : parts) if (p.first) f += (f.empty() ? "" : ", ") + std::string(p.second);
          if (reading.devices) f += (f.empty() ? "" : " \xC2\xB7 ") + std::to_string(reading.devices) + (reading.devices == 1 ? " device" : " devices");
          reading.found = f;
          const bool changed = reading.stats != prefs.stats;
          prefs = std::move(reading);
          prefs_at = (double) time(nullptr) * 1000;
          if (changed) today_at = days_at = -1e18;
          if (!prefs.price_e.empty()) ha::want(prefs.price_e);
          if (!prefs.power_e.empty()) ha::want(prefs.power_e);
          if (want_data) choose_ha();
        });
}
void stat_leaf(Sax &x, const std::string &v, bool) {
  if (x.depth != 4) return;
  if (x.key(3) == "start") row_start = v.find('-') != std::string::npos ? NAN : strtod(v.c_str(), nullptr);  // ms
  else if (x.key(3) == "change") row_change = v == "null" ? NAN : strtod(v.c_str(), nullptr);
}
void read_today() {
  const double now = (double) time(nullptr) * 1000, d0 = fw::energy::midnight(now), y0 = fw::energy::midnight(d0 - 12 * H);
  for (int i = 0; i < 48; i++) slot_k[i] = slot_c[i] = 0, slot_any[i] = false;
  yday_k = 0, yday_any = false;
  start(S_TODAY, ws_of(stats_cmd(y0, "5minute")), stat_leaf,
        [d0, y0, now](Sax &x) {
          if (x.depth != 4) return;  // one row ends
          const double st = row_start, ch = row_change;
          row_start = row_change = NAN;
          bool known;
          const Role r = role_of(x.key(1), known);
          if (!known || std::isnan(st) || std::isnan(ch)) return;
          if (st >= d0) {
            const int i = (int) ((st - d0) / (30 * MIN));
            if (i < 0 || i >= 48) return;
            if (r == R_COST) slot_c[i] += ch;
            else slot_k[i] += sign(r) * ch, slot_any[i] = true;
          } else if (st >= y0 && st + 5 * MIN <= now - DAY && r != R_COST) yday_k += sign(r) * ch, yday_any = true;
        },
        [](bool ok) { if (ok) today_at = (double) time(nullptr) * 1000; });
}
void read_days() {
  const double now = (double) time(nullptr) * 1000, d0 = fw::energy::midnight(now), first = fw::energy::midnight(d0 - 13 * DAY + 12 * H);
  for (int k = 0; k < 13; k++) day_k[k] = day_c[k] = 0, day_any[k] = false;
  start(S_DAYS, ws_of(stats_cmd(first, "day")), stat_leaf,
        [d0](Sax &x) {
          if (x.depth != 4) return;
          const double st = row_start, ch = row_change;
          row_start = row_change = NAN;
          bool known;
          const Role r = role_of(x.key(1), known);
          if (!known || std::isnan(st) || std::isnan(ch)) return;
          const long k = 13 + std::lround((fw::energy::midnight(st + 12 * H) - d0) / DAY);
          if (k < 0 || k >= 13) return;  // today comes from the 5-minute rows
          if (r == R_COST) day_c[k] += ch;
          else day_k[k] += sign(r) * ch, day_any[k] = true;
        },
        [d0](bool ok) { if (ok) days_at = (double) time(nullptr) * 1000, days_for = d0; });
}
// ---- the extras (one optional integration on top of the base; sources.json screens.energy.extras)
// an ISO time ("2026-10-31T16:00:00+00:00", "...Z", a local "2026-10-31T16:00:00" or a date "2026-11-01") -> ms
double iso_ms(const std::string &s) {
  int Y = 0, M = 0, D = 0, h = 0, m = 0;
  double sec = 0;
  const int n = sscanf(s.c_str(), "%d-%d-%dT%d:%d:%lf", &Y, &M, &D, &h, &m, &sec);
  if (n < 3 || M < 1 || M > 12) return NAN;
  const size_t tp = s.find('T');
  const size_t z = tp == std::string::npos ? std::string::npos : s.find_first_of("Z+-", tp);
  if (z == std::string::npos) {  // no zone: the panel's local time
    struct tm tm = {};
    tm.tm_year = Y - 1900, tm.tm_mon = M - 1, tm.tm_mday = D, tm.tm_hour = h, tm.tm_min = m, tm.tm_sec = (int) sec, tm.tm_isdst = -1;
    return (double) mktime(&tm) * 1000;
  }
  long off = 0;
  if (s[z] != 'Z') { int oh = 0, om = 0; sscanf(s.c_str() + z + 1, "%d:%d", &oh, &om); off = (oh * 3600L + om * 60L) * (s[z] == '-' ? -1 : 1); }
  const int y = Y - (M <= 2), era = (y >= 0 ? y : y - 399) / 400, yoe = y - era * 400, mp = (M + 9) % 12;
  const long days = era * 146097L + yoe * 365L + yoe / 4 - yoe / 100 + (153 * mp + 2) / 5 + D - 1 - 719468L;
  return ((double) days * 86400 + h * 3600 + m * 60 + sec - off) * 1000;
}
bool ends_with(const std::string &s, const char *t) { const size_t n = strlen(t); return s.size() >= n && !s.compare(s.size() - n, n, t); }
// the grid's import statistics the dashboard (or a fixture's prefs) names: what a matcher looks at
std::vector<std::string> grid_ins() {
  std::vector<std::string> g;
  for (auto &p : prefs.stats) if (p.second == R_IN) g.push_back(p.first);
  return g;
}
// Octopus Energy: the meter "octopus_energy_electricity_<serial>_<mpan>" from its accumulative consumption statistic
std::string octopus_match(const std::vector<std::string> &grid) {
  static const char *P = "sensor.octopus_energy_electricity_", *S = "_current_accumulative_consumption";
  for (auto &g : grid)
    if (!g.rfind(P, 0) && ends_with(g, S) && g.size() > strlen(P) + strlen(S)) return g.substr(7, g.size() - 7 - strlen(S));
  return "";
}
struct Extra { const char *id, *name; int rank; std::string (*match)(const std::vector<std::string> &grid); };
const Extra EXTRAS[] = {{"octopus", "Octopus Energy", 10, octopus_match}};  // more extras = more rows (+ their reads)
const char *ext_name() { return ext ? ext->name : ""; }
struct Rate { double s, e, v; };
struct Ev { double s, e; int kind; };  // kind: 0 saving session, 1 power-down, 2 greener night
struct Oct {
  std::string meter;
  std::vector<Rate> day[2];  // the current day's and the next day's rates (absolute times: a stale list is harmless)
  double rates_at[2] = {-1e18, -1e18};
  std::vector<std::pair<std::string, int>> cals;  // the account's Octoplus calendars (id, kind)
  double cals_at = -1e18, ev_at = -1e18, fixture_w = NAN;
  std::vector<Ev> evs, pend;
  size_t ev_i = 0;
} oct;
void choose(const std::vector<std::string> &grid) {
  const Extra *best = nullptr;
  std::string meter;
  for (const Extra &x : EXTRAS) {
    if (extra_cfg == "none" || (extra_cfg != "auto" && extra_cfg != x.id)) continue;
    const std::string m = x.match(grid);
    if (!m.empty() && (!best || x.rank > best->rank)) best = &x, meter = m;
  }
  if (best == ext && meter == oct.meter) return;
  ext = best, oct = Oct(), oct.meter = meter;
  if (ext && !fixtures) ha::want("sensor." + oct.meter + "_current_demand");
}
void choose_ha() { choose(grid_ins()); }
// source fixtures: every extra's test data (fw::spec::FIXTURES {extra: {prefs, states, calendars}}); {today},
// {tomorrow}, {after} = the panel's local dates
void choose_fixtures() {
  Doc f;
  std::vector<std::string> grid;
  if (!deserializeJson(f, fw::spec::FIXTURES))
    for (ArduinoJson::JsonPairConst kv : f.as<ArduinoJson::JsonObjectConst>())
      for (ArduinoJson::JsonVariantConst src : kv.value()["prefs"]["energy_sources"].as<ArduinoJson::JsonArrayConst>())
        if (src["type"] == "grid" && src["stat_energy_from"].is<const char *>()) grid.push_back(src["stat_energy_from"].as<const char *>());
  choose(grid);
  if (ext) oct.fixture_w = strtod(f[ext->id]["states"]["sensor." + oct.meter + "_current_demand"]["state"] | "nan", nullptr);
}
struct Served { std::string body; bool ok; Feed feed; End end; };
uint32_t serve(const std::string &path, Feed feed, End end) {
  Doc f;
  if (!ext || deserializeJson(f, fw::spec::FIXTURES)) return 0;
  ArduinoJson::JsonVariantConst x = f[ext->id];
  std::string body;
  bool ok = true;
  const size_t q = path.find('?');
  if (path == "calendars") {
    Doc l;
    l.to<ArduinoJson::JsonArray>();
    for (ArduinoJson::JsonPairConst kv : x["calendars"].as<ArduinoJson::JsonObjectConst>()) {
      ArduinoJson::JsonObject o = l.add<ArduinoJson::JsonObject>();
      o["entity_id"] = kv.key().c_str(), o["name"] = kv.key().c_str();
    }
    serializeJson(l, body);
  } else if (!path.rfind("calendars/", 0)) {
    ArduinoJson::JsonVariantConst v = x["calendars"][path.substr(10, q == std::string::npos ? std::string::npos : q - 10)];
    if (v.isNull()) ok = false; else serializeJson(v, body);
  } else if (!path.rfind("states/", 0)) {
    ArduinoJson::JsonVariantConst v = x["states"][path.substr(7)];
    if (v.isNull()) ok = false;
    else { Doc st; st["entity_id"] = path.substr(7), st["state"] = v["state"], st["attributes"] = v["attributes"]; serializeJson(st, body); }
  } else ok = false;
  const double d0 = fw::energy::midnight((double) time(nullptr) * 1000);
  const char *keys[] = {"{today}", "{tomorrow}", "{after}"};
  for (int k = 0; k < 3; k++) {
    const time_t t = (time_t) ((d0 + (k * 24 + 12) * 3600000.0) / 1000);
    struct tm tm;
    localtime_r(&t, &tm);
    char day[16];
    strftime(day, sizeof day, "%Y-%m-%d", &tm);
    for (size_t at; (at = body.find(keys[k])) != std::string::npos;) body.replace(at, strlen(keys[k]), day);
  }
  lv_timer_t *t = lv_timer_create([](lv_timer_t *t) {  // answered on the next turn, like a reply
    Served *sv = (Served *) lv_timer_get_user_data(t);
    lv_timer_delete(t);
    if (sv->ok) sv->feed(sv->body.data(), sv->body.size());
    sv->end(sv->ok, sv->ok ? "" : "not in the fixture");
    delete sv;
  }, 0, new Served{std::move(body), ok, std::move(feed), std::move(end)});
  (void) t;
  return 1;
}
uint32_t ext_get(const std::string &path, Feed f, End e) { return fixtures ? serve(path, std::move(f), std::move(e)) : hl::get(path, std::move(f), std::move(e)); }
// the reads (one at a time, through next())
std::vector<Rate> rates_reading;
Rate rate_now_reading = {NAN, NAN, NAN};
void read_rates(int k) {
  rates_reading.clear(), rate_now_reading = {NAN, NAN, NAN};
  oct.rates_at[k] = (double) time(nullptr) * 1000;
  start(S_EXTRA, get_of("states/event." + oct.meter + (k ? "_next_day_rates" : "_current_day_rates")),
        [](Sax &x, const std::string &v, bool) {
          if (x.depth != 4 || x.key(0) != "attributes" || x.key(1) != "rates") return;
          Rate &r = rate_now_reading;
          if (x.key(3) == "start") r.s = iso_ms(v);
          else if (x.key(3) == "end") r.e = iso_ms(v);
          else if (x.key(3) == "value_inc_vat") r.v = strtod(v.c_str(), nullptr);
        },
        [](Sax &x) {
          if (x.depth != 4 || x.key(0) != "attributes" || x.key(1) != "rates") return;
          const Rate r = rate_now_reading;
          rate_now_reading = {NAN, NAN, NAN};
          if (std::isfinite(r.s) && std::isfinite(r.e) && std::isfinite(r.v) && r.e > r.s && rates_reading.size() < 200) rates_reading.push_back(r);
        },
        [k](bool ok) { if (ok) oct.day[k] = std::move(rates_reading); }, true);
}
std::vector<std::pair<std::string, int>> cals_reading;
void read_cals() {
  cals_reading.clear();
  oct.cals_at = (double) time(nullptr) * 1000;
  start(S_EXTRA, get_of("calendars"),
        [](Sax &x, const std::string &v, bool str) {
          if (x.depth != 2 || x.key(1) != "entity_id" || !str || v.rfind("calendar.octopus_energy_", 0)) return;
          static const std::pair<const char *, int> K[] = {{"_octoplus_saving_sessions", 0}, {"_octoplus_power_down", 1}, {"_greener_nights", 2}};
          for (auto &k : K) if (ends_with(v, k.first) && cals_reading.size() < 8) cals_reading.push_back({v, k.second});
        },
        nullptr, [](bool ok) { if (ok) oct.cals = std::move(cals_reading), oct.ev_i = 0, oct.pend.clear(); }, true);
}
Ev ev_reading = {NAN, NAN, 0};
void read_events() {
  const double now = (double) time(nullptr) * 1000, d0 = fw::energy::midnight(now);
  if (!oct.ev_i) oct.ev_at = now, oct.pend.clear();
  const std::pair<std::string, int> c = oct.cals[oct.ev_i];
  ev_reading = {NAN, NAN, c.second};
  start(S_EXTRA, get_of("calendars/" + c.first + "?start=" + iso(now) + "&end=" + iso(d0 + 2 * DAY)),
        [](Sax &x, const std::string &v, bool) {
          if (x.depth != 3 || (x.key(2) != "dateTime" && x.key(2) != "date")) return;
          if (x.key(1) == "start") ev_reading.s = iso_ms(v);
          else if (x.key(1) == "end") ev_reading.e = iso_ms(v);
        },
        [](Sax &x) {
          if (x.depth != 2) return;
          const Ev e = ev_reading;
          ev_reading.s = ev_reading.e = NAN;
          if (std::isfinite(e.s) && std::isfinite(e.e) && oct.pend.size() < 32) oct.pend.push_back(e);
        },
        [](bool) {  // a calendar that fails (unavailable) adds nothing; the rest go on
          if (++oct.ev_i >= oct.cals.size()) oct.evs = std::move(oct.pend), oct.pend.clear(), oct.ev_i = 0;
        }, true);
}
// the extra's next read due (false = none)
bool ext_next(double now) {
  if (!ext || oct.meter.empty() || (!fixtures && !hl::ready())) return false;
  if (now - oct.rates_at[0] > 30 * MIN) { read_rates(0); return true; }
  if (now - oct.rates_at[1] > 30 * MIN) { read_rates(1); return true; }
  if (!oct.ev_i && now - oct.cals_at > 60 * MIN) { read_cals(); return true; }
  if (!oct.cals.empty() && (oct.ev_i || now - oct.ev_at > 15 * MIN)) { read_events(); return true; }
  return false;
}
// the mapper: the extra's rates, live watts and events over the base data
ExtOut overlay(fw::energy::Data &d, double &rate_now, double &rate_next, bool &banded) {
  namespace en = fw::energy;
  ExtOut o;
  if (!ext || oct.meter.empty()) return o;
  o.on = true;
  const double now = (double) time(nullptr) * 1000, d0 = en::midnight(now);
  const double w = fixtures ? oct.fixture_w : ha::num(ha::get("sensor." + oct.meter + "_current_demand"));
  if (std::isfinite(w)) d.watts = w;
  std::vector<Rate> all(oct.day[0]);
  all.insert(all.end(), oct.day[1].begin(), oct.day[1].end());
  std::sort(all.begin(), all.end(), [](const Rate &a, const Rate &b) { return a.s < b.s; });
  all.erase(std::unique(all.begin(), all.end(), [](const Rate &a, const Rate &b) { return a.s == b.s; }), all.end());
  auto range = [&](double day, double &lo, double &hi) {  // the day's lowest and highest rate
    lo = INFINITY, hi = -INFINITY;
    for (auto &r : all) if (r.s >= day && r.s < day + DAY) lo = std::min(lo, r.v), hi = std::max(hi, r.v);
  };
  double lo, hi;
  range(d0, lo, hi);
  const bool spread = std::isfinite(lo) && hi - lo > 0.005;
  auto band = [&](double v) { return !spread ? en::NORMAL : v <= lo + (hi - lo) / 3 ? en::CHEAP : v >= hi - (hi - lo) / 3 ? en::PEAK : en::NORMAL; };
  const Rate *cur = nullptr;
  for (auto &r : all) if (r.s <= now && now < r.e) cur = &r;
  if (std::isfinite(lo)) {
    banded = true;
    for (int i = 0; i < 48; i++) {
      const double st = d0 + i * 30 * MIN + 1;
      for (auto &r : all) if (r.s <= st && st < r.e) { d.slots[i].band = band(r.v); break; }
    }
  }
  char b[96];
  std::string at;
  rate_next = NAN, d.until = 0;
  if (cur) {
    rate_now = cur->v, d.band = band(cur->v);
    for (auto &r : all) if (r.s >= cur->e && std::fabs(r.v - cur->v) > 1e-6) { rate_next = r.v, d.until = r.s, at = hm(r.s); break; }
  }
  const std::string ws = std::isfinite(d.watts) ? std::to_string(std::lround(d.watts)) + " W" : "-- W";
  o.live = ws, o.card = ws;
  if (cur) {
    o.live += " \xC2\xB7 " + en::pence(cur->v) + " now", o.card += " \xC2\xB7 " + en::pence(cur->v);
    if (!at.empty()) o.live += " \xE2\x86\x92 " + en::pence(rate_next) + " at " + at;
    o.tone = d.band == en::CHEAP ? "ok" : d.band == en::PEAK ? "warn" : "dim";
  } else o.live += " now";
  // the cheapest run still to come: today's lowest rate, else tomorrow's
  for (int k = 0; k < 2 && o.cheapest.empty(); k++) {
    const double day = d0 + k * DAY;
    double l, h;
    range(day, l, h);
    if (!std::isfinite(l) || h - l <= 0.005) continue;
    for (size_t i = 0; i < all.size(); i++) {
      if (all[i].s < day || all[i].s >= day + DAY || all[i].v > l + 1e-6) continue;
      size_t j = i;
      while (j + 1 < all.size() && all[j + 1].s == all[j].e && all[j + 1].v <= l + 1e-6) j++;
      if (all[j].e <= now) { i = j; continue; }
      if (all[i].s <= now) snprintf(b, sizeof b, "Cheapest now until %s", hm(all[j].e).c_str());
      else snprintf(b, sizeof b, "Cheapest %s%s\xE2\x80\x93%s", k ? "tomorrow " : "", hm(all[i].s).c_str(), hm(all[j].e).c_str());
      o.cheapest = b;
      break;
    }
  }
  // the next Octoplus event today or tomorrow
  const Ev *next_ev = nullptr;
  for (auto &e : oct.evs) if (e.e > now && e.s < d0 + 2 * DAY && (!next_ev || e.s < next_ev->s)) next_ev = &e;
  if (next_ev) {
    static const char *NAME[] = {"Saving session", "Power-down", "Greener night"};
    const Ev &e = *next_ev;
    if (e.s <= now) snprintf(b, sizeof b, "%s now until %s", NAME[e.kind], hm(e.e).c_str());
    else snprintf(b, sizeof b, "%s %s %s\xE2\x80\x93%s", NAME[e.kind], e.s < d0 + DAY ? "today" : "tomorrow", hm(e.s).c_str(), hm(e.e).c_str());
    o.event = b, o.event_tone = e.kind == 2 ? "ok" : "warn";
  }
  return o;
}
void show() {
  if (want_data) { from_ha(); return; }
  fw::energy::Data d = fw::energy::sample((double) time(nullptr) * 1000);
  double rn = fw::energy::RATE[d.band], rx = fw::energy::RATE[d.next_band];
  bool banded = true;
  ExtOut x;
  if (fixtures) x = overlay(d, rn, rx, banded);
  publish(d, rn, rx, banded, &x);
}
// the next read due, one at a time
void next() {
  if (step != S_IDLE) return;
  const double now = (double) time(nullptr) * 1000;
  if (hl::ready() && now - prefs_at > 10 * MIN) { read_prefs(); return; }
  if (want_data && hl::ready() && !prefs.stats.empty()) {
    if (now - today_at > 55000) { read_today(); return; }
    if (now - days_at > 15 * MIN || days_for != fw::energy::midnight(now)) { read_days(); return; }
  }
  ext_next(now);
}
void entity_put(const std::string &e, const std::string &, const std::string &) {
  if (e.empty() || !want_data || !ha::watched(E)) return;
  if (e == prefs.price_e || e == prefs.power_e || (ext && e == "sensor." + oct.meter + "_current_demand")) from_ha();
}
void fresh() {
  if (!tick) tick = lv_timer_create([](lv_timer_t *) { if (ha::watched(E)) fresh(); }, 60000, nullptr);
  static const bool hooked = (ha::on_put(entity_put), true);
  (void) hooked;
  const std::string &cfg = ha::feed_config(E);
  Doc c;
  deserializeJson(c, cfg.empty() ? "{}" : cfg.c_str());
  const std::string src = c["source"] | "sample";
  const bool was_fixtures = fixtures;
  want_data = src == "ha", fixtures = src == "fixtures", extra_cfg = c["extra"] | "auto";
  if (fixtures != was_fixtures) ext = nullptr, oct = Oct();
  if (fixtures) choose_fixtures();
  else if (want_data && prefs.read) choose_ha();
  else ext = nullptr, oct = Oct();
  put_found();
  next();  // the prefs whatever the source (Settings shows what HA's Energy dashboard has); the data for source ha
  show();
}
[[maybe_unused]] const bool registered = (ha::on_local(E, fresh), true);  // binds on panel.energy never reach a host
}  // namespace
}  // namespace fw::dev::energy_source
