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
// More adapters (a tariff integration read directly, a supplier's API) = more rows here, no screen change. Refreshed
// once a minute while a bind reads it (unchanged values are no update).
#include <cctype>
#include <cstdio>
#include <ctime>
#include "energy.h"
#include "fw.h"
#include "ha.h"
#include "device.h"
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
void publish(const fw::energy::Data &d, double rate_now, double rate_next, bool banded = true) {
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
  if (std::isnan(d.watts)) put("watts", "-- W now");
  else { snprintf(b, sizeof b, "%ld W now", std::lround(d.watts)); put("watts", b); }
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
enum Step { S_IDLE, S_PREFS, S_TODAY, S_DAYS } step = S_IDLE;
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

void put_state(const char *st, const char *line) {
  ha::put(E, "cost", fw::energy::money(NAN)), ha::put(E, "line", line);
  ha::put(E, "rate", ""), ha::put(E, "tone", "idle"), ha::put(E, "watts", "-- W now");
  ha::put(E, "", st);
}
void put_found() {
  const char *f = !hl::ready() ? "Home Assistant not connected" : !prefs.read ? (why_now.empty() ? "Reading Home Assistant" : why_now.c_str())
                  : prefs.found.empty() ? "Not set up in Home Assistant" : prefs.found.c_str();
  ha::put(E, "found", f);
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
  publish(d, std::isnan(price) ? last_rate : price, NAN, false);
}
void next();
// one WS read: a fresh Sax with these handlers, then done(ok) and the next step
void start(Step s, const std::string &cmd, std::function<void(Sax &, const std::string &, bool)> leaf, std::function<void(Sax &)> close, std::function<void(bool)> done) {
  sax.reset(new Sax());
  sax->leaf = std::move(leaf), sax->close = std::move(close);
  step = s;
  const uint32_t id = hl::ws(cmd, [](const char *p, size_t n) { if (sax) sax->feed(p, n); }, [done](bool ok, const std::string &why) {
    const bool good = ok && sax && !sax->bad && sax->ok;
    why_now = good ? "" : !ok ? "Home Assistant not reached: " + why : "Home Assistant said no";
    sax.reset();
    step = S_IDLE;
    done(good);
    put_found();
    if (want_data) from_ha();
    if (good) next();
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
  start(S_PREFS, "{\"type\":\"energy/get_prefs\"}",
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
  start(S_TODAY, stats_cmd(y0, "5minute"), stat_leaf,
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
  start(S_DAYS, stats_cmd(first, "day"), stat_leaf,
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
// the next read due, one at a time
void next() {
  if (step != S_IDLE || !hl::ready()) return;
  const double now = (double) time(nullptr) * 1000;
  if (now - prefs_at > 10 * MIN) { read_prefs(); return; }
  if (!want_data || prefs.stats.empty()) return;
  if (now - today_at > 55000) { read_today(); return; }
  if (now - days_at > 15 * MIN || days_for != fw::energy::midnight(now)) read_days();
}
void entity_put(const std::string &e, const std::string &, const std::string &) {
  if (want_data && (e == prefs.price_e || e == prefs.power_e) && !e.empty() && ha::watched(E)) from_ha();
}
void fresh() {
  if (!tick) tick = lv_timer_create([](lv_timer_t *) { if (ha::watched(E)) fresh(); }, 60000, nullptr);
  static const bool hooked = (ha::on_put(entity_put), true);
  (void) hooked;
  const std::string &cfg = ha::feed_config(E);
  Doc c;
  deserializeJson(c, cfg.empty() ? "{}" : cfg.c_str());
  const std::string src = c["source"] | "sample";
  want_data = src == "ha";
  put_found();
  next();  // the prefs whatever the source (Settings shows what HA's Energy dashboard has); the data for source ha
  if (want_data) from_ha();
  else {
    const fw::energy::Data d = fw::energy::sample((double) time(nullptr) * 1000);
    publish(d, fw::energy::RATE[d.band], fw::energy::RATE[d.next_band]);
  }
}
[[maybe_unused]] const bool registered = (ha::on_local(E, fresh), true);  // binds on panel.energy never reach a host
}  // namespace
}  // namespace fw::dev::energy_source
