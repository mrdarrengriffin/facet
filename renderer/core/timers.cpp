// The panel's timers: core/device.h "the panel's timers" (the list, the shown one, done = the sound once, observers)
// and core/spec.h "local sources" (the reserved entity panel.timer the binds read). Same file on the panels and in WASM.
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <ctime>
#include "device.h"
#include "ha.h"

namespace fw {
namespace dev {
namespace timers {
namespace {
constexpr int MAX_S = 24 * 3600;  // a timer is at most a day
constexpr int NAME_CHARS = 24;      // characters kept of a given name (HA's Timers list fits 4 in 255)
struct List {
  std::vector<TimerItem> items;  // in start order
  int next = 1, sel = 0;
  std::vector<Observer> obs;
  lv_timer_t *tick = nullptr;
};
List &L() {
  static List l;
  return l;
}
int32_t ms_left(const TimerItem &t) { return t.state == 1 ? (int32_t) (t.ends - lv_tick_get()) : 0; }
TimerItem *mut(int id) {
  for (TimerItem &t : L().items)
    if (t.id == id) return &t;
  return nullptr;
}
bool same(const std::string &a, const std::string &b) {
  if (a.size() != b.size()) return false;
  for (size_t i = 0; i < a.size(); i++)
    if (tolower((unsigned char) a[i]) != tolower((unsigned char) b[i])) return false;
  return true;
}
bool taken(const std::string &n) {
  for (const TimerItem &t : L().items)
    if (same(t.name, n)) return true;
  return false;
}
void fresh();
void tell(Change what, const TimerItem &t) {
  fresh();
  const TimerItem copy = t;  // an observer may start / cancel (the list moves)
  for (Observer cb : std::vector<Observer>(L().obs)) cb(what, copy);
}
// done detection (always, whatever screen shows) + the values that move each second (only while something reads them)
void tick_cb(lv_timer_t *) {
  List &l = L();
  for (TimerItem &t : l.items)
    if (t.state == 1 && ms_left(t) <= 0) {
      t.state = 2;
      sound(lv_screen_active(), "timer-done");
      tell(DONE, t);
    }
  if (ha::watched(TIMER_ENTITY)) fresh();
  if (l.items.empty() && l.tick) { lv_timer_delete(l.tick); l.tick = nullptr; }
}
void run_tick() {
  if (!L().tick) L().tick = lv_timer_create(tick_cb, 250, nullptr);
}
// the shown timer + the others into the HA store (pair (panel.timer, attribute)); unchanged values are no update
void fresh() {
  static bool reg = false;
  if (!reg) { reg = true; ha::on_local(TIMER_ENTITY, fresh); }
  const std::vector<TimerItem> v = all();
  const TimerItem *s = shown();
  auto put = [](const char *a, const std::string &x) { ha::put(TIMER_ENTITY, a, x); };
  if (s) {
    const int lf = left(*s);
    put("", s->state == 1 ? "running" : "done");
    put("id", std::to_string(s->id));
    put("name", s->name);
    put("left", countdown(lf));
    put("left_s", std::to_string(lf));
    put("total", std::to_string(s->total));
    put("share", std::to_string(s->total > 0 ? (int) std::min<long long>(1000, (1000LL * std::max<int32_t>(0, ms_left(*s)) + s->total * 500LL) / (s->total * 1000LL)) : 0));
    std::string line = "of " + length_name(s->total);
    if (s->state == 2) line += " \xC2\xB7 done";
    else {
      const time_t end = time(nullptr) + lf;
      struct tm tm;
      localtime_r(&end, &tm);
      char b[24];
      snprintf(b, sizeof b, " \xC2\xB7 ends %02d:%02d", tm.tm_hour, tm.tm_min);
      line += b;
    }
    put("line", line);
  } else {
    for (const char *a : {"id", "name", "left_s", "total", "share", "line"}) put(a, "");
    put("", "idle");
    put("left", "0:00");
  }
  put("count", std::to_string(v.size()));
  put("running", std::to_string(running()));
  put("free", std::to_string(TIMERS_MAX - (int) v.size()));
  ArduinoJson::JsonDocument d;
  ArduinoJson::JsonArray o = d.to<ArduinoJson::JsonArray>();
  for (const TimerItem &t : v) {
    if (s && t.id == s->id) continue;
    ArduinoJson::JsonObject j = o.add<ArduinoJson::JsonObject>();
    j["id"] = t.id;
    j["name"] = t.name;
    j["left"] = left(t);
    j["total"] = t.total;
    j["state"] = t.state == 1 ? "running" : "done";
    j["time"] = t.state == 1 ? countdown(left(t)) : std::string("done");
  }
  std::string js;
  serializeJson(d, js);
  put("others", js);
}
[[maybe_unused]] const bool registered = (ha::on_local(TIMER_ENTITY, fresh), true);  // binds on panel.timer never reach a host
}  // namespace

std::string countdown(int s) {
  s = std::max(0, s);
  char b[16];
  if (s >= 3600) snprintf(b, sizeof b, "%d:%02d:%02d", s / 3600, s / 60 % 60, s % 60);
  else snprintf(b, sizeof b, "%d:%02d", s / 60, s % 60);
  return b;
}
std::string length_name(int s) {
  const int h = s / 3600, m = s / 60 % 60, x = s % 60;
  std::string r;
  if (h) r += std::to_string(h) + " h";
  if (m) r += (r.empty() ? "" : " ") + std::to_string(m) + " min";
  if (x || r.empty()) r += (r.empty() ? "" : " ") + std::to_string(x) + " s";
  return r;
}
int left(const TimerItem &t) {
  const int32_t ms = ms_left(t);
  return ms <= 0 ? 0 : (int) ((ms + 999) / 1000);
}
long long ends_at(const TimerItem &t) {
  const time_t now = time(nullptr);
  if (t.state != 1 || now < 1700000000) return 0;  // done, or the clock is not set yet
  return (long long) now + (ms_left(t) + 500) / 1000;
}
std::vector<TimerItem> all() {
  std::vector<TimerItem> v = L().items;
  std::stable_sort(v.begin(), v.end(), [](const TimerItem &a, const TimerItem &b) {
    if (a.state != b.state) return a.state < b.state;  // running first, then the done ones (in start order)
    return a.state == 1 && ms_left(a) < ms_left(b);
  });
  return v;
}
const TimerItem *get(int id) { return mut(id); }
const TimerItem *shown() {
  List &l = L();
  if (const TimerItem *t = mut(l.sel)) return t;
  const TimerItem *best = nullptr;
  for (const TimerItem &t : l.items)
    if (t.state == 1 && (!best || ms_left(t) < ms_left(*best))) best = &t;
  if (best) return best;
  for (const TimerItem &t : l.items)
    if (t.state == 2) return &t;
  return nullptr;
}
int running() {
  int n = 0;
  for (const TimerItem &t : L().items) n += t.state == 1;
  return n;
}
int find(const char *q) {
  if (!q || !*q) return 0;
  for (const TimerItem &t : L().items)
    if (same(t.name, q)) return t.id;
  for (const char *c = q; *c; c++)
    if (!isdigit((unsigned char) *c)) return 0;
  return mut(atoi(q)) ? atoi(q) : 0;
}
const char *start(const char *name, int seconds, int *id_out) {
  List &l = L();
  if (id_out) *id_out = 0;
  if (seconds < 1 || seconds > MAX_S) return "a timer is 1 second to 24 hours long";
  if ((int) l.items.size() >= TIMERS_MAX) return "4 timers already: stop one first";
  std::string n = name ? name : "";
  while (!n.empty() && isspace((unsigned char) n.back())) n.pop_back();
  while (!n.empty() && isspace((unsigned char) n[0])) n.erase(0, 1);
  if (n.empty()) n = length_name(seconds);
  if (n.size() > NAME_CHARS) n.resize(NAME_CHARS);
  if (taken(n)) {  // "5 min" twice = "5 min 2": every timer has its own name (HA cancels by name)
    for (int k = 2;; k++) {
      const std::string c = n + " " + std::to_string(k);
      if (!taken(c)) { n = c; break; }
    }
  }
  TimerItem t;
  t.id = l.next++;
  t.name = n;
  t.total = seconds;
  t.ends = lv_tick_get() + (uint32_t) seconds * 1000;
  l.items.push_back(t);
  if (id_out) *id_out = t.id;
  run_tick();
  tell(START, t);
  return "";
}
const char *extend(int id, int s) {
  TimerItem *t = mut(id);
  if (!t) return "no such timer";
  if (s < 1 || s > MAX_S) return "extend by 1 second to 24 hours";
  if (t->state == 2) {  // +1 min after it rang: it runs again for s
    t->state = 1;
    t->total = s;
    t->ends = lv_tick_get() + (uint32_t) s * 1000;
  } else {
    t->total = std::min(MAX_S, t->total + s);
    t->ends += (uint32_t) s * 1000;
  }
  run_tick();
  tell(EXTEND, *t);
  return "";
}
namespace {
const char *remove(int id, Change why) {
  List &l = L();
  auto it = std::find_if(l.items.begin(), l.items.end(), [id](const TimerItem &t) { return t.id == id; });
  if (it == l.items.end()) return "no such timer";
  const TimerItem t = *it;
  l.items.erase(it);
  if (l.sel == id) l.sel = 0;
  tell(why, t);
  return "";
}
}  // namespace
const char *cancel(int id) { return remove(id, CANCEL); }
const char *stop(int id) {
  const TimerItem *t = mut(id);
  if (!t) return "no such timer";
  if (t->state != 2) return "not done";
  return remove(id, STOP);
}
const char *select(int id) {
  const TimerItem *t = mut(id);
  if (!t) return "no such timer";
  L().sel = id;
  tell(SELECT, *t);
  return "";
}
void list_json(std::string &out) {
  ArduinoJson::JsonDocument d;
  ArduinoJson::JsonArray a = d.to<ArduinoJson::JsonArray>();
  for (const TimerItem &t : all()) {
    ArduinoJson::JsonArray r = a.add<ArduinoJson::JsonArray>();
    r.add(t.name);
    r.add(ends_at(t));
    r.add(t.total);
    r.add(t.state == 1 ? "running" : "done");
  }
  out.clear();
  serializeJson(d, out);
}
void observe(Observer cb) { L().obs.push_back(cb); }
void reset() {
  List &l = L();
  l.items.clear();
  l.sel = 0;
  l.next = 1;
  fresh();
}
}  // namespace timers

Timer &timer() {
  static Timer g;
  const std::vector<TimerItem> v = timers::all();
  if (v.empty()) { g = Timer(); return g; }
  const TimerItem &t = v[0];  // the soonest running, else the first done
  g.state = t.state;
  g.total = t.total;
  g.label = t.name;
  g.ends = t.state == 1 ? (long long) time(nullptr) + timers::left(t) : 0;
  return g;
}
}  // namespace dev
}  // namespace fw
