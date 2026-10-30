// The calendar feed (CONTRACT.md C51): the reserved entity panel.calendar (core/spec.h "local sources") in the HA store,
// the one shape the Calendar screen and its Home card bind to, whatever holds the events. Same file on the panels and in
// WASM. Words are Facet's (core/agenda.h: "16:50", "Tmrw", "today · Thu 22 Oct", "October 2026").
//   state        ok | loading (no calendar has answered yet) | error (every calendar unavailable, or none picked)
//   hero_n       the next event's when: "16:50" (today), "Now" (running), "Tmrw", "Thu", "24 Oct"; "--" for none
//   hero_label   its title as written ("Nothing coming up" / "Loading" / "Offline"); hero_line "until 17:50 · Larkfield
//                studio · Family" (the parts it knows); hero_accent "on" while it runs or starts within 15 minutes
//   agenda       the days from today, a JSON list of Calendar rows (components/calendar_row.cpp): a day {"kind": "day",
//                "title": "today · Thu 22 Oct", "first": "on" | "off"} then its events {"kind": "event", "time": "16:50" |
//                "now" | "all day", "colour": "blue" (a colour token), "title": "Pottery class", "tone": "" | "now" |
//                "past"}; empty = no events on the calendars that are on (the screen's empty words)
//   month_title  "October 2026"; month_here "1" while this month shows (else the Today button shows); month_days the
//                six weeks from the Monday on or before the 1st, a JSON list of 42 Calendar month days: {"d":
//                "2026-10-23", "n": 23, "out": 0 | 1 (another month), "today": 0 | 1, "sel": 0 | 1 (the day sheet's),
//                "dots": "blue,ok" (up to three, or two when there are more), "more": "+2" | ""}
//   sheet        which sheet is open: "calendars" | "day" | ""
//   calendars    the calendars sheet's rows, on first: [{"id": "toggle family", "kind": "calendar", "colour": "blue",
//                "title": "Family", "status": "5 events" | "hidden", "on": "on" | "off"}] (the id is the row's pick:
//                calendar.control toggles it)
//   shown        how many calendars are on ("5")
//   day_title    the day sheet's title ("Friday 23 October"); day_events its rows (Calendar rows, kind event)
//   card         the Home card's line: the next event's title ("Pottery class", "Nothing coming up"; the card's value
//                is hero_n, accent hero_accent, as on the screen)
// What the person does (calendar.control, core/spec.h ACTIONS): previous | next | today (the month shown), day
// <YYYY-MM-DD> (that day's sheet), calendars (the calendars sheet), close (the open sheet), toggle <id> (a calendar
// shown / hidden; kept while the panel runs).
// Adapters (the feed's settings, the spec's "feeds"; core/ha.h feed_config): source
//   ha      Home Assistant calendars (setting calendars: a comma list of calendar entities). Each entity's own event
//           (state on while one runs; message, start_time, end_time, all_day, location) is a live subscription like any
//           bind; the days around come from calendar.get_events (core/ha.h ask, one per calendar) on build, every 15
//           minutes, on a new day and when the month shown leaves the days fetched. Without action responses (or a
//           failed ask) the screen shows each calendar's own event only, never an error. Names = friendly_name; colours
//           in order: blue, ok, warn, steel, text-dim.
//   sample  the docs story (core/agenda.h sample: two weeks from today, five calendars, Bins hidden); setting now = a
//           fixed time (ms, tests)
// More adapters (a calendar link read by the panel itself, CalDAV ...) = more rows here, no screen change.
#include <algorithm>
#include <cstdio>
#include <ctime>
#include <set>
#include "agenda.h"
#include "fw.h"
#include "ha.h"

namespace fw::dev::calendar_source {
namespace {
namespace ag = fw::agenda;
constexpr const char *E = "panel.calendar";
constexpr const char *COLOURS[] = {"blue", "ok", "warn", "steel", "text-dim"};
constexpr int AGENDA_MAX = 60;  // rows (Grid ITEMS_MAX 64)
struct Cal { std::string id, entity, name, colour; bool on; };
lv_timer_t *tick = nullptr;
std::string cfg_seen, src_now;
std::vector<Cal> cals;
std::vector<ag::Event> asked;      // get_events replies (ha), every calendar
std::set<std::string> hidden;      // calendars turned off (ids)
std::set<std::string> answered_ok; // calendars whose get_events reply landed
int waiting = 0;
double fixed = 0, from_ms = 0, to_ms = 0;  // the days fetched
long fetched_day = -1;
uint32_t fetched_at = 0;
int my = -1, mm = 0;  // the month shown (-1 = this month)
double sel = 0;       // the day sheet's day (0 = none)
std::string sheet;

double now_ms() { return fixed > 0 ? fixed : (double) time(nullptr) * 1000; }
long day_no(double t) { return std::lround(ag::midnight(t) / 86400000.0); }
std::string ymd(double t) {
  const time_t s = (time_t) (t / 1000);
  struct tm tm;
  localtime_r(&s, &tm);
  char b[16];
  snprintf(b, sizeof b, "%04d-%02d-%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
  return b;
}
double local_day(int y, int m, int d) {  // local midnight of y-m-d
  struct tm tm = {};
  tm.tm_year = y - 1900, tm.tm_mon = m - 1, tm.tm_mday = d, tm.tm_isdst = -1;
  return (double) mktime(&tm) * 1000;
}
long days_from_civil(int y, int m, int d) {  // days since 1970-01-01 (H. Hinnant)
  y -= m <= 2;
  const long era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = (unsigned) (y - era * 400), doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  return era * 146097 + (long) (yoe * 365 + yoe / 4 - yoe / 100 + doy) - 719468;
}
// an HA time -> ms: "2026-10-29" (a date: local midnight, *date = true), "2026-10-29T14:00:00+01:00" / "...Z" (an
// offset), "2026-10-29 14:00:00" (local, the entity's attributes); NAN = not one
double ha_ms(const char *s, bool *date = nullptr) {
  int y, mo, d, h = 0, mi = 0, se = 0;
  if (date) *date = false;
  if (!s || sscanf(s, "%d-%d-%d", &y, &mo, &d) != 3) return NAN;
  if (strlen(s) <= 10) { if (date) *date = true; return local_day(y, mo, d); }
  if (sscanf(s + 11, "%d:%d:%d", &h, &mi, &se) < 2) return NAN;
  const char *z = strlen(s) > 19 ? strpbrk(s + 19, "+-Z") : nullptr;
  if (!z) return local_day(y, mo, d) + (h * 3600.0 + mi * 60 + se) * 1000;  // local wall time
  double t = ((double) days_from_civil(y, mo, d) * 86400 + h * 3600 + mi * 60 + se) * 1000;
  if (*z != 'Z') {
    int oh = 0, om = 0;
    if (sscanf(z + 1, "%d:%d", &oh, &om) >= 1) t -= (*z == '-' ? -1 : 1) * (oh * 3600 + om * 60) * 1000.0;
  }
  return t;
}
const Cal *cal_of(const std::string &id) {
  for (const Cal &c : cals) if (c.id == id) return &c;
  return nullptr;
}
bool on(const std::string &id) { return cal_of(id) && !hidden.count(id); }
const char *colour(const std::string &id) { const Cal *c = cal_of(id); return c ? c->colour.c_str() : "text-dim"; }
void sort_events(std::vector<ag::Event> &v) {
  std::stable_sort(v.begin(), v.end(), [](const ag::Event &a, const ag::Event &b) {
    return a.start < b.start || (a.start == b.start && a.all_day && !b.all_day);
  });
}
std::string ser(ArduinoJson::JsonDocument &d) {
  std::string s;
  if (d.isNull()) return "[]";
  serializeJson(d, s);
  return s;
}
void event_row(ArduinoJson::JsonArray arr, const ag::Event &x, double now) {
  ArduinoJson::JsonObject o = arr.add<ArduinoJson::JsonObject>();
  const bool past = x.end <= now, run = ag::running(x, now);
  o["kind"] = "event";
  o["time"] = x.all_day ? "all day" : run ? "now" : ag::hm(x.start);
  o["colour"] = colour(x.calendar);
  o["title"] = x.summary;
  o["tone"] = past ? "past" : run && !x.all_day ? "now" : "";
}

// the feed from every event known (all calendars; the hidden ones drop out here)
void put_events(const std::vector<ag::Event> &all, double now) {
  auto put = [](const char *a, const std::string &v) { ha::put(E, a, v); };
  std::vector<ag::Event> ev;
  for (const ag::Event &e : all) if (on(e.calendar)) ev.push_back(e);
  sort_events(ev);
  put("", "ok");
  // hero (+ the card): the running or next event
  const ag::Event *e = ag::next(ev, now);
  if (!e) {
    put("hero_n", "--"), put("hero_label", "Nothing coming up"), put("hero_line", "in the next 14 days"), put("hero_accent", "off");
    put("card", "Nothing coming up");
  } else {
    const long dd = ag::day_diff(e->start, now);
    const bool run = ag::running(*e, now);
    std::string line = run ? "until " + ag::hm(e->end) : dd == 0 ? "today" : e->all_day ? ag::time_text(*e) : ag::hm(e->start);
    if (!e->location.empty()) line += " \xC2\xB7 " + e->location;
    if (const Cal *c = cal_of(e->calendar)) line += " \xC2\xB7 " + c->name;
    const std::string n = run ? "Now" : dd == 0 && !e->all_day ? ag::hm(e->start) : dd == 1 ? "Tmrw" : dd < 7 ? ag::wd(e->start) : ag::dm(e->start);
    put("hero_n", n), put("hero_label", e->summary), put("hero_line", line);
    put("hero_accent", run || e->start - now <= ag::SOON ? "on" : "off");
    put("card", e->summary);
  }
  // agenda: a day line, then its events
  {
    ArduinoJson::JsonDocument d;
    ArduinoJson::JsonArray arr = d.to<ArduinoJson::JsonArray>();
    int n = 0;
    for (const ag::DayGroup &g : ag::days(ev, now)) {
      if (n + 1 + (int) g.ev.size() > AGENDA_MAX) break;
      ArduinoJson::JsonObject o = arr.add<ArduinoJson::JsonObject>();
      const long dd = ag::day_diff(g.day, now);
      o["kind"] = "day";
      o["title"] = dd == 0 ? "today \xC2\xB7 " + ag::long_day(g.day) : dd == 1 ? "tomorrow \xC2\xB7 " + ag::long_day(g.day) : ag::long_day(g.day);
      o["first"] = n == 0 ? "on" : "off";
      n++;
      for (const ag::Event *x : g.ev) { event_row(arr, *x, now); n++; }
    }
    put("agenda", ser(d));
  }
  // month
  int ty = 0, tm = 0;
  ag::ym(now, ty, tm);
  const int y = my < 0 ? ty : my, m = my < 0 ? tm : mm;
  put("month_title", ag::month_title(y, m));
  put("month_here", y == ty && m == tm ? "1" : "");
  {
    ArduinoJson::JsonDocument d;
    ArduinoJson::JsonArray arr = d.to<ArduinoJson::JsonArray>();
    int yy = 0, mo = 0;
    ag::ym(ag::month_first(y, m), yy, mo);
    const double today = ag::midnight(now);
    double day = ag::grid_start(y, m);
    for (int i = 0; i < 42; i++, day = ag::day_add(day, 1)) {
      int dy = 0, dm = 0;
      ag::ym(day, dy, dm);
      ArduinoJson::JsonObject o = arr.add<ArduinoJson::JsonObject>();
      o["d"] = ymd(day);
      o["n"] = ag::mday(day);
      o["out"] = dm != mo ? 1 : 0;
      o["today"] = day == today ? 1 : 0;
      o["sel"] = day == sel ? 1 : 0;
      const std::vector<const ag::Event *> at = ag::on_day(ev, day);
      const int more = at.size() > 3 ? (int) at.size() - 2 : 0;
      std::string dots;
      for (size_t k = 0; k < at.size() && (int) k < (more ? 2 : 3); k++) dots += (k ? "," : "") + std::string(colour(at[k]->calendar));
      o["dots"] = dots;
      o["more"] = more ? "+" + std::to_string(more) : "";
    }
    put("month_days", ser(d));
  }
  // the calendars sheet: on first, then the rest, each in the feed's order
  {
    ArduinoJson::JsonDocument d;
    ArduinoJson::JsonArray arr = d.to<ArduinoJson::JsonArray>();
    int shown = 0;
    for (int pass = 0; pass < 2; pass++)
      for (const Cal &c : cals) {
        const bool o = on(c.id);
        if (o != (pass == 0)) continue;
        shown += o;
        int n = 0;
        for (const ag::Event &x : all) n += x.calendar == c.id;  // every event known (the docs count)
        ArduinoJson::JsonObject r = arr.add<ArduinoJson::JsonObject>();
        r["id"] = "toggle " + c.id;
        r["kind"] = "calendar";
        r["colour"] = c.colour;
        r["title"] = c.name;
        r["status"] = o ? std::to_string(n) + (n == 1 ? " event" : " events") : "hidden";
        r["on"] = o ? "on" : "off";
      }
    put("calendars", ser(d));
    put("shown", std::to_string(shown));
  }
  // the day sheet
  {
    ArduinoJson::JsonDocument d;
    ArduinoJson::JsonArray arr = d.to<ArduinoJson::JsonArray>();
    if (sel > 0) for (const ag::Event *x : ag::on_day(ev, sel)) event_row(arr, *x, now);
    put("day_title", sel > 0 ? ag::day_title(sel) : "day");
    put("day_events", ser(d));
  }
  put("sheet", sheet);
}
void put_empty(const char *state) {  // loading / error: the words only
  const bool err = !strcmp(state, "error");
  ha::put(E, "", state);
  ha::put(E, "hero_n", "--");
  ha::put(E, "hero_label", err ? "Offline" : "Loading");
  ha::put(E, "hero_line", err ? (cals.empty() ? "no calendars picked" : "the calendars can't be reached") : "");
  ha::put(E, "hero_accent", "off");
  ha::put(E, "card", err ? "Offline" : "Loading");
  ha::put(E, "agenda", "[]");
  for (const char *a : {"calendars", "day_events", "month_days"}) ha::put(E, a, "[]");
  ha::put(E, "month_title", ""), ha::put(E, "month_here", "1"), ha::put(E, "shown", "0");
  ha::put(E, "day_title", "day"), ha::put(E, "sheet", "");
}

// ---- sample adapter
void sample_cals() {
  cals.clear();
  hidden.clear();
  for (const ag::Cal &c : ag::CALENDARS) {
    cals.push_back({c.id, "", c.name, c.colour, c.on});
    if (!c.on) hidden.insert(c.id);  // Bins: hidden until turned on
  }
}
void put_sample() {
  bool all[ag::N_CALENDARS];
  for (bool &x : all) x = true;
  put_events(ag::sample(now_ms(), all), now_ms());
}

// ---- ha adapter
// what each calendar entity says itself (its running or next event): used until its get_events reply lands
void rebuild() {
  if (src_now != "ha") return;
  if (cals.empty()) { put_empty("error"); return; }
  bool any = false, all_down = true;
  std::vector<ag::Event> ev = asked;
  for (const Cal &c : cals) {
    const std::string *st = ha::get(c.entity);
    if (st && !st->empty()) any = true;
    if (!st || (*st != "unavailable" && *st != "unknown")) all_down = false;
    if (answered_ok.count(c.id)) continue;
    const std::string *msg = ha::get(c.entity, "message"), *s = ha::get(c.entity, "start_time"), *e = ha::get(c.entity, "end_time"),
                      *ad = ha::get(c.entity, "all_day"), *loc = ha::get(c.entity, "location");
    const double a = s ? ha_ms(s->c_str()) : NAN, b = e ? ha_ms(e->c_str()) : NAN;
    if (msg && !msg->empty() && !std::isnan(a))
      ev.push_back({*msg, loc ? *loc : "", c.id, a, std::isnan(b) ? a + ag::HOUR : b, ad && (*ad == "on" || *ad == "true")});
  }
  if (!any && ev.empty()) { put_empty("loading"); return; }
  if (all_down && ev.empty()) { put_empty("error"); return; }
  put_events(ev, now_ms());
}
void events_into(const std::string &cid, ArduinoJson::JsonVariantConst v, int depth = 0) {  // "events" lists anywhere
  if (depth > 4 || !v.is<ArduinoJson::JsonObjectConst>()) return;
  for (ArduinoJson::JsonPairConst kv : v.as<ArduinoJson::JsonObjectConst>()) {
    if (strcmp(kv.key().c_str(), "events") || !kv.value().is<ArduinoJson::JsonArrayConst>()) { events_into(cid, kv.value(), depth + 1); continue; }
    for (ArduinoJson::JsonVariantConst e : kv.value().as<ArduinoJson::JsonArrayConst>()) {
      if (asked.size() >= 400) return;
      auto txt = [](ArduinoJson::JsonVariantConst t) -> const char * {
        return t.is<const char *>() ? t.as<const char *>() : (t["dateTime"] | (t["date"] | ""));
      };
      bool date = false, date2 = false;
      const double a = ha_ms(txt(e["start"]), &date), b = ha_ms(txt(e["end"]), &date2);
      if (std::isnan(a)) continue;
      asked.push_back({e["summary"] | "", e["location"] | "", cid, a, std::isnan(b) ? a + ag::HOUR : b, date});
    }
  }
}
void answered(void *ctx, bool ok, const std::string &json) {
  waiting = std::max(0, waiting - 1);
  const std::string cid = (const char *) ctx;
  ArduinoJson::JsonDocument d;
  if (ok && cal_of(cid) && !deserializeJson(d, json)) {
    asked.erase(std::remove_if(asked.begin(), asked.end(), [&](const ag::Event &x) { return x.calendar == cid; }), asked.end());
    events_into(cid, d.as<ArduinoJson::JsonVariantConst>());
    answered_ok.insert(cid);
  }
  rebuild();  // a failed ask keeps what the entity says
}
// the days to fetch: today .. 14 days on, and the month shown's six weeks
void want_range(double &a, double &b) {
  const double now = now_ms();
  int ty = 0, tm = 0;
  ag::ym(now, ty, tm);
  const double g = ag::grid_start(my < 0 ? ty : my, my < 0 ? tm : mm);
  a = std::min(ag::midnight(now), g);
  b = std::max(ag::day_add(now, 15), ag::day_add(g, 42));
}
void fetch() {
  if (waiting) return;
  double a, b;
  want_range(a, b);
  from_ms = a, to_ms = b;
  fetched_day = day_no(now_ms()), fetched_at = lv_tick_get();
  static std::vector<std::string> ids;  // the ask contexts (stable while asks wait)
  ids.clear();
  ids.reserve(cals.size());
  for (const Cal &c : cals) ids.push_back(c.id);
  for (size_t i = 0; i < cals.size(); i++) {
    ArduinoJson::JsonDocument q;
    q["entity_id"] = cals[i].entity, q["start_date_time"] = ymd(a) + " 00:00:00", q["end_date_time"] = ymd(b) + " 00:00:00";
    std::string qs;
    serializeJson(q, qs);
    waiting++;  // before the ask: a host may answer at once
    if (*ha::ask("calendar.get_events", qs.c_str(), answered, (void *) ids[i].c_str())) waiting--;
  }
}
void entity_put(const std::string &e, const std::string &a, const std::string &) {
  if (src_now != "ha") return;
  for (const Cal &c : cals)
    if (c.entity == e) {
      if (a == "friendly_name") { for (Cal &x : cals) if (x.entity == e) { const std::string *n = ha::get(e, "friendly_name"); if (n && !n->empty()) x.name = *n; } }
      rebuild();
      return;
    }
}

void fresh() {
  if (!tick) tick = lv_timer_create([](lv_timer_t *) { fresh(); }, 60000, nullptr);
  const std::string &cfg = ha::feed_config(E);
  ArduinoJson::JsonDocument c;
  deserializeJson(c, cfg.empty() ? "{}" : cfg.c_str());
  const std::string src = c["source"] | "sample", list = c["calendars"] | "";
  const char *fx_ = c["now"] | "";
  fixed = *fx_ ? strtod(fx_, nullptr) : 0;
  const bool changed = cfg != cfg_seen;
  cfg_seen = cfg;
  static const bool hooked = (ha::on_put(entity_put), true);
  (void) hooked;
  if (changed) { my = -1, sel = 0, sheet.clear(); }
  if (src == "ha") {
    if (changed || src_now != "ha") {
      cals.clear(), asked.clear(), answered_ok.clear(), hidden.clear(), fetched_day = -1;
      for (size_t i = 0, k = 0; i < list.size() && cals.size() < 12;) {
        size_t j = list.find(',', i);
        if (j == std::string::npos) j = list.size();
        std::string e = list.substr(i, j - i);
        i = j + 1;
        while (!e.empty() && e[0] == ' ') e.erase(0, 1);
        while (!e.empty() && e.back() == ' ') e.pop_back();
        if (e.rfind("calendar.", 0) != 0 || e.size() > 96) continue;
        const std::string id = e.substr(9);
        const std::string *n = ha::get(e, "friendly_name");
        cals.push_back({id, e, n && !n->empty() ? *n : id, COLOURS[k++ % 5], true});
        for (const char *a : {"", "message", "start_time", "end_time", "all_day", "location", "friendly_name"}) ha::want(e, a);
      }
    }
    src_now = "ha";
    rebuild();
    double a, b;
    want_range(a, b);
    if (!cals.empty() && (changed || day_no(now_ms()) != fetched_day || lv_tick_elaps(fetched_at) > 15 * 60000u || a < from_ms || b > to_ms)) fetch();
  } else {
    if (src_now != "sample" || changed) sample_cals();
    src_now = "sample";
    put_sample();
  }
}
[[maybe_unused]] const bool registered = (ha::on_local(E, fresh), true);  // binds on panel.calendar never reach a host
}  // namespace

// calendar.control (core/spec.h ACTIONS): what a tap on the Calendar screen does; "" = done, else why not
const char *control(const std::string &v) {
  const double now = now_ms();
  int ty = 0, tm = 0;
  ag::ym(now, ty, tm);
  if (v == "previous" || v == "next") {
    int y = my < 0 ? ty : my, m = my < 0 ? tm : mm;
    ag::ym(ag::month_first(y, m + (v == "next" ? 1 : -1)), y, m);
    if (y == ty && m == tm) my = -1; else my = y, mm = m;
    sel = 0;
    if (sheet == "day") sheet.clear();
  } else if (v == "today") { my = -1, sel = 0; if (sheet == "day") sheet.clear(); }
  else if (v == "calendars") sheet = "calendars";
  else if (v == "close") { sheet.clear(); sel = 0; }
  else if (v.rfind("day ", 0) == 0) {
    int y, m, d;
    if (sscanf(v.c_str() + 4, "%d-%d-%d", &y, &m, &d) != 3 || m < 1 || m > 12 || d < 1 || d > 31) return "day: YYYY-MM-DD";
    sel = local_day(y, m, d), sheet = "day";
  } else if (v.rfind("toggle ", 0) == 0) {
    const std::string id = v.substr(7);
    if (!cal_of(id)) return "no calendar by that id";
    if (hidden.count(id)) hidden.erase(id); else hidden.insert(id);
  } else return "previous, next, today, calendars, close, day <YYYY-MM-DD> or toggle <id>";
  fresh();  // the month may need fetching; the feed again
  return "";
}
}  // namespace fw::dev::calendar_source
