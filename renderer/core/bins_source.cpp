// The bins feed (CONTRACT.md C51, the Feed base core/feed.h): panel.bins, the one shape the Bins screen and its Home
// card bind to, whatever says when the bins go out. Same file on the panels and in WASM.
//   (state)  the feed's state (core/feed.h): ok | empty (no collections) | missing (no bin calendar) | loading | error
//   moment   tonight (collection tomorrow, after 12:00) | today (collection today, before 12:00) | collected (today, after
//            12:00) | later | none
//   caption  "put out tonight" / "collection today" / "collected today \xC2\xB7 bring it in" / "next collection"
//   accent   "on" for tonight / today (the moments that matter), else "off"; tone: the same as a status tone
//   when     "Tomorrow" / "Today" / the weekday ("Thursday")
//   bins     the next collection's bins: "Green + Black bins" (one: "Green bin"); colours: their colours, "green,black"
//            (bins colour tokens: black green brown blue grey purple red yellow)
//   line     "Fri 24 Oct \xC2\xB7 every 2 weeks" (the date, and how often its first bin comes when the calendar shows it)
//   coming   the collections after it, a JSON list of List rows ({title, sub, trailing: "value", value: "in 8 days"})
//   more     "1" when the weeks after the next collection are known (sample, or a get_events reply), else ""
//   yours    every bin seen, a JSON list of List rows ({title, sub: "every 2 weeks", value: "next Fri 24 Oct"})
// Adapters (sources.json bins; the feed's settings, the spec's "feeds"): source
//   calendar  the Waste Collection Schedule integration's calendar (HACS; setting calendar = its entity, else the first
//             calendar of that integration in Home Assistant's registry). The next collection = the calendar entity
//             itself (its message + start_time, read live); the six weeks after it come from calendar.get_events
//             (core/ha.h ask) on a change, every 30 minutes and on a new day. Without action responses, or when the ask
//             fails, Bins shows the next collection only, never an error. The bin's colour from its name (a colour word,
//             else "general / refuse / rubbish" black, "recycl" blue, "garden / food" green / brown, else grey).
//   sample    the docs week (today Fri 23 Oct 2026; setting sample: tonight | today | collected | later | two | none):
//             docs, workbench and tests only (a panel never offers it, #156)
// Other bin integrations and councils = more adapters onto the same model (docs: Inside > Adapters).
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <strings.h>
#include <ctime>
#include "fw.h"
#include "ha.h"
#include "feed.h"
#include "device.h"

namespace fw::dev::bins_source {
namespace {
struct Bin { std::string name, colour; };
struct Coll { long day; std::vector<int> bins; };  // day = days since 1970-01-01 (civil)
struct Model { std::vector<Bin> bins; std::vector<Coll> colls; long today = 0; int hour = 12; bool more = true; };
const char *const WD[] = {"Thursday", "Friday", "Saturday", "Sunday", "Monday", "Tuesday", "Wednesday"};  // day 0 = Thu 1 Jan 1970
const char *const WD3[] = {"Thu", "Fri", "Sat", "Sun", "Mon", "Tue", "Wed"};
const char *const MON[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
long fetched_day = -1;
uint32_t fetched_at = 0;
bool waiting = false;
std::string cal_now;    // the calendar the adapter reads ("" = none)
Model asked;            // the last get_events reply's collections (empty until one lands)
bool asked_ok = false;  // a get_events reply has landed for this calendar
void put(const char *a, const std::string &x);
void put_state(feed::State s);
void run_again();

long days_from_civil(int y, int m, int d) {  // H. Hinnant's algorithm: days since 1970-01-01
  y -= m <= 2;
  const long era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = (unsigned) (y - era * 400), doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + (long) doe - 719468;
}
void civil(long z, int &y, int &m, int &d) {
  z += 719468;
  const long era = (z >= 0 ? z : z - 146096) / 146097;
  const unsigned doe = (unsigned) (z - era * 146097), yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100), mp = (5 * doy + 2) / 153;
  d = (int) (doy - (153 * mp + 2) / 5 + 1);
  m = (int) (mp < 10 ? mp + 3 : mp - 9);
  y = (int) (yoe + era * 400 + (m <= 2));
}
std::string date(long day) {  // "Fri 24 Oct"
  int y, m, d;
  civil(day, y, m, d);
  return std::string(WD3[((day % 7) + 7) % 7]) + " " + std::to_string(d) + " " + MON[m - 1];
}
std::string lower(std::string s) {
  for (char &c : s) c = (char) tolower((unsigned char) c);
  return s;
}
std::string trim(std::string s) {
  while (!s.empty() && isspace((unsigned char) s.front())) s.erase(0, 1);
  while (!s.empty() && isspace((unsigned char) s.back())) s.pop_back();
  return s;
}
const char *colour_of(const std::string &name) {
  const std::string n = lower(name);
  static const char *const C[] = {"black", "green", "brown", "blue", "grey", "purple", "red", "yellow"};
  for (const char *c : C) if (n.find(c) != std::string::npos) return c;
  if (n.find("gray") != std::string::npos || n.find("silver") != std::string::npos) return "grey";
  if (n.find("general") != std::string::npos || n.find("refuse") != std::string::npos || n.find("rubbish") != std::string::npos || n.find("residual") != std::string::npos || n.find("landfill") != std::string::npos || n.find("trash") != std::string::npos) return "black";
  if (n.find("recycl") != std::string::npos) return "blue";
  if (n.find("garden") != std::string::npos || n.find("yard") != std::string::npos) return "green";
  if (n.find("food") != std::string::npos || n.find("compost") != std::string::npos || n.find("organic") != std::string::npos) return "brown";
  return "grey";
}
std::string tidy(std::string name) {  // "GREEN BIN" / "green bin" -> "Green bin"
  name = lower(trim(name));
  if (!name.empty()) name[0] = (char) toupper((unsigned char) name[0]);
  return name;
}
int bin_index(Model &M, const std::string &raw) {
  const std::string name = tidy(raw);
  for (size_t i = 0; i < M.bins.size(); i++) if (M.bins[i].name == name) return (int) i;
  M.bins.push_back({name, colour_of(name)});
  return (int) M.bins.size() - 1;
}
void add(Model &M, long day, const std::string &summary) {  // one event: its bins on that day
  Coll *c = nullptr;
  for (Coll &x : M.colls) if (x.day == day) c = &x;
  if (!c) { M.colls.push_back({day, {}}); c = &M.colls.back(); }
  std::string s = summary;
  for (const char *sep : {" & ", " and ", " + ", ";"})
    for (size_t k; (k = s.find(sep)) != std::string::npos;) s.replace(k, strlen(sep), ",");
  for (size_t i = 0; i <= s.size();) {
    size_t j = s.find(',', i);
    if (j == std::string::npos) j = s.size();
    const std::string part = trim(s.substr(i, j - i));
    if (!part.empty()) {
      const int b = bin_index(M, part);
      if (std::find(c->bins.begin(), c->bins.end(), b) == c->bins.end()) c->bins.push_back(b);
    }
    i = j + 1;
  }
}
// "Green bin"; two or more = "Green + Brown bins" (each " bin" dropped, "bins" once at the end: it fits a theme's
// wider font on one line) unless a name does not end in " bin" ("Green bin + Garden waste")
std::string names(const Model &M, const std::vector<int> &b) {
  const auto bin = [](const std::string &n) { return n.size() > 4 && !strcasecmp(n.c_str() + n.size() - 4, " bin"); };
  bool all = b.size() > 1;
  for (int i : b) all = all && bin(M.bins[i].name);
  std::string s;
  for (int i : b) s += (s.empty() ? "" : " + ") + (all ? M.bins[i].name.substr(0, M.bins[i].name.size() - 4) : M.bins[i].name);
  return all ? s + " bins" : s;
}
std::string every(const Model &M, int b) {  // the gap between a bin's first two collections
  long first = -1;
  for (const Coll &c : M.colls)
    if (std::find(c.bins.begin(), c.bins.end(), b) != c.bins.end()) {
      if (first < 0) { first = c.day; continue; }
      const long g = c.day - first;
      return g == 7 ? "every week" : g % 7 == 0 ? "every " + std::to_string(g / 7) + " weeks" : "every " + std::to_string(g) + " days";
    }
  return "";
}
std::string in_days(long d) { return d == 0 ? "today" : d == 1 ? "tomorrow" : "in " + std::to_string(d) + " days"; }

void put_model(Model M) {
  std::sort(M.colls.begin(), M.colls.end(), [](const Coll &a, const Coll &b) { return a.day < b.day; });
  M.colls.erase(std::remove_if(M.colls.begin(), M.colls.end(), [&](const Coll &c) { return c.day < M.today || c.bins.empty(); }), M.colls.end());
  if (M.colls.size() > 8) M.colls.resize(8);
  put("more", M.more && !M.colls.empty() ? "1" : "");
  if (M.colls.empty()) {
    for (const char *a : {"caption", "when", "bins", "colours", "line"}) put(a, "");
    put("accent", "off");
    put("tone", "idle");
    put("coming", "[]");
    put("yours", "[]");
    put("moment", "none");
    put_state(feed::EMPTY);
    return;
  }
  const Coll &next = M.colls[0];
  const long dd = next.day - M.today;
  const bool tonight = dd == 1 && M.hour >= 12, today = dd == 0 && M.hour < 12, collected = dd == 0 && M.hour >= 12;
  put("caption", tonight ? "put out tonight" : today ? "collection today" : collected ? "collected today \xC2\xB7 bring it in" : "next collection");
  put("accent", tonight || today ? "on" : "off");
  put("tone", tonight || today ? "working" : "idle");
  put("when", dd == 0 ? "Today" : dd == 1 ? "Tomorrow" : WD[((next.day % 7) + 7) % 7]);
  put("bins", names(M, next.bins));
  std::string cols;
  for (int b : next.bins) cols += (cols.empty() ? "" : ",") + M.bins[b].colour;
  put("colours", cols);
  const std::string ev = next.bins.empty() ? "" : every(M, next.bins[0]);
  put("line", date(next.day) + (ev.empty() ? "" : " \xC2\xB7 " + ev));
  ArduinoJson::JsonDocument cd;
  ArduinoJson::JsonArray ca = cd.to<ArduinoJson::JsonArray>();
  for (size_t i = 1; i < M.colls.size(); i++) {
    ArduinoJson::JsonObject r = ca.add<ArduinoJson::JsonObject>();
    r["title"] = names(M, M.colls[i].bins), r["sub"] = date(M.colls[i].day), r["trailing"] = "value", r["value"] = in_days(M.colls[i].day - M.today);
  }
  std::string js;
  serializeJson(cd, js);
  put("coming", js);
  ArduinoJson::JsonDocument yd;
  ArduinoJson::JsonArray ya = yd.to<ArduinoJson::JsonArray>();
  for (size_t b = 0; b < M.bins.size(); b++) {
    long nd = -1;
    for (const Coll &c : M.colls)
      if (nd < 0 && std::find(c.bins.begin(), c.bins.end(), (int) b) != c.bins.end()) nd = c.day;
    if (nd < 0) continue;
    ArduinoJson::JsonObject r = ya.add<ArduinoJson::JsonObject>();
    r["title"] = M.bins[b].name, r["sub"] = every(M, (int) b), r["trailing"] = "value", r["value"] = "next " + date(nd);
  }
  js.clear();
  serializeJson(yd, js);
  put("yours", js);
  put("moment", tonight ? "tonight" : today ? "today" : collected ? "collected" : "later");
  put_state(feed::OK);
}

// ---- adapter: sample (the docs week, today = Fri 23 Oct 2026)
void sample_week(const std::string &s) {
  Model M;
  M.today = days_from_civil(2026, 10, 23);
  M.hour = s == "today" ? 7 : s == "collected" ? 15 : 19;
  M.bins = {{"Black bin", "black"}, {"Green bin", "green"}, {"Brown bin", "brown"}};
  if (s != "none") {
    const int first = s == "tonight" || s == "two" ? 1 : s == "later" ? 6 : 0;
    for (int w = 0; w < 4; w++) {
      std::vector<int> b{w % 2 ? 1 : 0};
      if (w % 2 == (s == "two" ? 0 : 1)) b.push_back(2);
      M.colls.push_back({M.today + first + 7 * w, b});
    }
  }
  put_model(M);
}

// ---- adapter: calendar (the calendar entity's next event + calendar.get_events for the weeks after it)
long today_local(int *hour) {
  const time_t t = time(nullptr);
  struct tm tm;
  localtime_r(&t, &tm);
  if (hour) *hour = tm.tm_hour;
  return days_from_civil(tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
}
void events_into(Model &M, ArduinoJson::JsonVariantConst v, int depth = 0) {  // finds "events" lists anywhere in the reply
  if (depth > 4) return;
  if (v.is<ArduinoJson::JsonObjectConst>()) {
    for (ArduinoJson::JsonPairConst kv : v.as<ArduinoJson::JsonObjectConst>()) {
      if (!strcmp(kv.key().c_str(), "events") && kv.value().is<ArduinoJson::JsonArrayConst>()) {
        for (ArduinoJson::JsonVariantConst e : kv.value().as<ArduinoJson::JsonArrayConst>()) {
          const char *st = e["start"].is<const char *>() ? e["start"].as<const char *>() : (e["start"]["date"] | (e["start"]["dateTime"] | ""));
          int y, m, d;
          if (strlen(st) < 10 || sscanf(st, "%d-%d-%d", &y, &m, &d) != 3) continue;
          add(M, days_from_civil(y, m, d), e["summary"] | "");
        }
      } else events_into(M, kv.value(), depth + 1);
    }
  }
}
// the feed from what is known: the entity's next event (attributes) + the asked weeks
void rebuild() {
  Model M = asked;
  M.more = asked_ok;
  M.today = today_local(&M.hour);
  const std::string *state = ha::get(cal_now), *st = ha::get(cal_now, "start_time"), *msg = ha::get(cal_now, "message");
  const feed::State s = feed::of(state);
  if (s == feed::MISSING || s == feed::ERROR) { put_state(s); return; }
  int y, m, d;
  if (st && msg && st->size() >= 10 && sscanf(st->c_str(), "%d-%d-%d", &y, &m, &d) == 3) add(M, days_from_civil(y, m, d), *msg);
  if (s == feed::LOADING && M.colls.empty()) { put_state(feed::LOADING); return; }
  put_model(M);
}
void answered(void *, bool ok, const std::string &json) {
  waiting = false;
  ArduinoJson::JsonDocument d;
  if (ok && !deserializeJson(d, json)) {
    Model M;
    events_into(M, d.as<ArduinoJson::JsonVariantConst>());
    asked = M, asked_ok = true;
  }
  run_again();  // a failed ask keeps what the entity says
}
void fetch(const std::string &cal) {
  int hour;
  const long today = today_local(&hour);
  if (waiting && lv_tick_elaps(fetched_at) < 60000) return;
  char a[24], b[24];
  int y, m, d;
  civil(today, y, m, d);
  snprintf(a, sizeof a, "%04d-%02d-%02d 00:00:00", y, m, d);
  civil(today + 42, y, m, d);
  snprintf(b, sizeof b, "%04d-%02d-%02d 00:00:00", y, m, d);
  ArduinoJson::JsonDocument q;
  q["entity_id"] = cal, q["start_date_time"] = a, q["end_date_time"] = b;
  std::string qs;
  serializeJson(q, qs);
  waiting = true;  // before the ask: a host may answer at once
  fetched_day = today, fetched_at = lv_tick_get();
  if (*ha::ask("calendar.get_events", qs.c_str(), answered, nullptr)) waiting = false;
}
// no calendar picked: the first Waste Collection Schedule calendar in Home Assistant's registry ("" = none yet)
std::string wcs_calendar() {
  namespace hl = fw::dev::ha_link;
  hl::reg_fetch("calendar");
  if (const std::vector<hl::RegRow> *r = hl::reg("calendar"))
    for (const hl::RegRow &e : *r) if (e.platform == "waste_collection_schedule") return e.id;
  return "";
}
std::vector<feed::Field> bins_fields() {
  return {{"moment", false, "none"}, {"caption", false, ""}, {"accent", false, "off"}, {"tone", false, "idle"}, {"when", true, ""},
          {"bins", true, ""}, {"colours", false, ""}, {"line", false, ""}, {"coming", false, "[]"}, {"more", false, ""}, {"yours", false, "[]"}};
}
class Bins : public feed::Feed {
 public:
  Bins() : Feed("panel.bins", 60000, "calendar", bins_fields()) {}
  using Feed::put;
  using Feed::state;

 protected:
  void sample() override { cal_now.clear(), reads({}); sample_week(cfg()["sample"] | "tonight"); }
  void fill() override {
    std::string cal = cfg()["calendar"] | "";
    if (cal.rfind("calendar.", 0) != 0) cal = wcs_calendar();
    if (cal.empty()) {  // no bin calendar: Home Assistant has no Waste Collection Schedule calendar (or not read yet)
      cal_now.clear(), reads({});
      namespace hl = fw::dev::ha_link;
      state(hl::reg("calendar") || !hl::ready() ? feed::MISSING : feed::LOADING);
      return;
    }
    if (changed() || cal != cal_now) {
      fetched_day = -1, asked = Model{}, asked_ok = false, waiting = false, cal_now = cal;
      reads({{cal, ""}, {cal, "start_time"}, {cal, "message"}});  // the next event, live
    }
    if (back()) waiting = false;  // asks sent before Home Assistant dropped never come back
    rebuild();
    if (changed() || back() || today_local(nullptr) != fetched_day || lv_tick_elaps(fetched_at) > 30 * 60000u) fetch(cal);
  }
};
Bins F;
void put(const char *a, const std::string &x) { F.put(a, x); }
void put_state(feed::State s) { F.state(s); }
void run_again() { F.run(); }
}  // namespace
}  // namespace fw::dev::bins_source
