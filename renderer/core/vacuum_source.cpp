// The vacuum feed (CONTRACT.md C51): the reserved entity panel.vacuum (core/ha.h "local sources") in the HA store, the
// one shape the Robot vacuum screen and its Home card bind to, whatever robot it is (the panel lab's Vacuum,
// apps/hub panel/vacuum/screen.ts: vacWord, vacLine, vacuumCard). Words and units are Facet's.
//   state (attr "")  docked | cleaning | returning | paused | idle | error | unavailable
//   word         the state in words: Docked | Cleaning | Going home | Paused | Idle | Stuck | Offline
//   tone         accent (cleaning) | error (stuck) | dim
//   battery      "74%" ("" when not known); bat_line "battery 74%"
//   rest         the idle line's second part: charged | charging | back to the dock | the error in words | ""
//   last         "last run yesterday 18:42" ("" when not known)
//   room         the room it is in now (a Roborock status worth saying wins: "emptying the bin", "washing the mop")
//   here         the room it is cleaning now, "" when no run is on (the tiles' and the plan's lit room)
//   where        the Map view's second value: the room while a run is on, else the battery
//   progress     "46%" (+ " done" when paused); progress_n "46" (the level bar); minutes "9 min"; area "7.6 m²"
//   mode / fan / mop   the cleaning mode (vacuum | vac_and_mop | mop), suction (quiet .. max_plus), water (low .. high)
//   pos          the robot's position "x,y" (the source's units: the screen's transform turns them into plan metres)
//   trail        the run's path "x,y x,y ..." (same units)
//   idle_show / run_show / pick_show   "1" | "": the idle words (not cleaning or paused), the run's words and bar
//                (cleaning or paused), the room picking + mode (no run on: not cleaning, returning or paused)
//   clean_show / pause_show / resume_show / dock_show   which buttons the bar has (the lab's rules)
//   tile_state   default | disabled (a run is on: picking rooms is for the next run)
//   card_mode value | text; card_value "46%"; card_text the word; card_sub "lounge · 9 min" | "100% · charged";
//   card_accent  "true" while cleaning (or stuck); card_ring 0-100 (progress while a run is on, else the battery);
//   card_run     run | idle | off  (the Vacuum robot preview)
// Adapters (the feed's settings, the spec's "feeds"; core/ha.h feed_config): source
//   ha      a Home Assistant vacuum entity (setting vacuum) + its sensors: battery, room, progress, time (min), area
//           (m²), last_clean (a timestamp), status (the robot's own status words), error, position ("x,y"), path
//           (attribute points), mode + mop (select entities); fan = the vacuum's fan_speed attribute. The controls
//           go out through vacuum.control (core/spec.h run_action: the one checked path) as vacuum / select services.
//   sample  the docs robot in the docs house (Lounge 1, Dining 2, Hall 3): starts in setting "sample" (docked,
//           cleaning, paused, returning, idle, error); a Clean really runs (rows across the picked rooms, about
//           0.5 m a second), then it goes home and charges; nothing leaves the panel
// Controls (control(); the screen's taps through vacuum.control on e panel.vacuum): clean [rooms] | resume | pause |
// dock | mode <id> | fan <id> | mop <id>.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include "fw.h"
#include "ha.h"

namespace fw::dev::vacuum_source {
namespace {
constexpr const char *E = "panel.vacuum";
const char *const KEYS[] = {"vacuum", "battery", "room", "progress", "time", "area", "last_clean", "status", "error", "position", "path", "mode", "mop"};
constexpr int NK = sizeof KEYS / sizeof *KEYS;
struct Box { const char *name, *seg; float x0, z0, x1, z1; };
constexpr Box ROOMS[] = {{"Lounge", "1", 0.3f, 3.4f, 3.6f, 6.9f}, {"Dining", "2", 0.3f, 0.3f, 3.3f, 2.7f}, {"Hall", "3", 4.3f, 3.4f, 6.1f, 5.3f}};
constexpr float DOCK_X = 0.6f, DOCK_Z = 0.7f, STEP = 0.5f;
struct V {  // the robot, whatever the source
  std::string state = "unavailable", status, error, room, mode, fan, mop, pos, trail, last;
  double battery = -1, progress = -1, minutes = -1, area = -1;
};
struct St {
  std::string cfg_seen, src = "sample", start = "docked";
  std::string ent[NK];
  V s;                              // sample: the robot now
  std::vector<std::pair<float, float>> route;  // sample: the run's rows still to go (metres)
  std::vector<std::pair<float, float>> done;   // sample: the path so far
  float walked = 0, total = 1, bx = DOCK_X, bz = DOCK_Z;
  int ticks = 0;
};
St &S() { static St s; return s; }
lv_timer_t *motor = nullptr;

void put(const char *a, const std::string &v) { ha::put(E, a, v); }
std::string num(double v, int dp = 0) { char b[24]; snprintf(b, sizeof b, "%.*f", dp, v); return b; }
std::string pct(double v) { return v < 0 ? "" : num(std::round(v)) + "%"; }
bool running(const std::string &s) { return s == "cleaning" || s == "returning" || s == "paused"; }
std::string lower(std::string s) { for (char &c : s) c = (char) tolower((unsigned char) c); return s; }
std::string words(std::string s) { std::replace(s.begin(), s.end(), '_', ' '); return s; }
const char *word(const V &v) {
  static const char *const W[][2] = {{"docked", "Docked"}, {"cleaning", "Cleaning"}, {"returning", "Going home"}, {"paused", "Paused"},
                                     {"idle", "Idle"}, {"error", "Stuck"}};
  for (auto &w : W) if (v.state == w[0]) return w[1];
  return "Offline";
}
const char *status_text(const std::string &s) {  // the Roborock status words worth saying while it is out
  static const char *const T[][2] = {{"washing_the_mop", "washing the mop"}, {"going_to_wash_the_mop", "going to wash the mop"},
                                     {"emptying_the_bin", "emptying the bin"}, {"docking", "docking"}, {"returning_home", "going home"},
                                     {"charging_problem", "charging problem"}, {"air_drying_stopping", "drying the mop"}};
  for (auto &t : T) if (s == t[0]) return t[1];
  return nullptr;
}
std::string join(std::initializer_list<std::string> l) {
  std::string o;
  for (const std::string &x : l) if (!x.empty()) o += (o.empty() ? "" : " \xC2\xB7 ") + x;
  return o;
}

// ---- adapter: ha (one vacuum entity + its sensors)
const std::string *got(int k, const char *a = "") { const std::string &e = S().ent[k]; return e.empty() ? nullptr : ha::get(e, a); }
double gnum(int k) { const std::string *v = got(k); return v && !v->empty() && *v != "unknown" && *v != "unavailable" ? ha::num(v) : -1; }
std::string gstr(int k, const char *a = "") { const std::string *v = got(k, a); return v && *v != "unknown" && *v != "unavailable" ? *v : ""; }
V read_ha() {
  V v;
  const std::string *st = got(0);
  if (!st) return v;
  v.state = *st == "unknown" || st->empty() ? "unavailable" : *st;
  v.battery = gnum(1);
  if (v.battery < 0) if (const std::string *b = got(0, "battery_level"); b && !b->empty()) v.battery = ha::num(b);
  v.room = gstr(2), v.progress = gnum(3), v.minutes = gnum(4), v.area = gnum(5);
  if (const std::string l = gstr(6); !l.empty()) v.last = ha::when(l, true);
  v.status = gstr(7), v.error = gstr(8);
  if (v.error == "none") v.error.clear();
  v.pos = gstr(9), v.trail = gstr(10, "points");
  v.mode = gstr(11), v.mop = gstr(12), v.fan = gstr(0, "fan_speed");
  return v;
}

void publish() {
  St &s = S();
  const V v = s.src == "ha" ? read_ha() : s.s;
  const bool run = running(v.state), out = v.state == "unavailable", stuck = v.state == "error";
  const std::string bat = pct(v.battery);
  const char *said = status_text(v.status);
  const std::string room = said ? said : v.room;
  put("word", word(v));
  put("tone", v.state == "cleaning" ? "accent" : stuck ? "error" : "dim");
  put("battery", bat);
  put("bat_line", bat.empty() ? "" : "battery " + bat);
  const bool charging = v.status == "charging" && v.battery >= 0 && v.battery < 100;
  put("rest", stuck ? words(v.error.empty() ? "stuck" : v.error) : v.state == "returning" ? "back to the dock"
              : v.state == "docked" ? (charging ? "charging" : "charged") : "");
  put("last", v.last.empty() ? "" : "last run " + v.last);
  put("room", v.state == "returning" ? "" : room);
  put("here", run ? v.room : "");
  put("where", run ? (v.state == "returning" ? "" : room) : bat);
  put("progress", v.progress < 0 ? "" : pct(v.progress) + (v.state == "paused" ? " done" : ""));
  put("progress_n", v.progress < 0 ? "-1" : num(std::round(v.progress)));
  put("minutes", v.minutes > 0 ? num(std::round(v.minutes)) + " min" : "");
  put("area", v.area > 0 ? num(v.area, 1) + " m\xC2\xB2" : "");
  put("mode", v.mode), put("fan", v.fan), put("mop", v.mop);
  put("pos", v.pos), put("trail", v.trail);
  put("idle_show", v.state == "cleaning" || v.state == "paused" ? "" : "1");
  put("run_show", v.state == "cleaning" || v.state == "paused" ? "1" : "");
  put("pick_show", run ? "" : "1");
  put("tile_state", run ? "disabled" : "default");
  put("clean_show", run || out ? "" : "1");
  put("resume_show", v.state == "paused" ? "1" : "");
  put("pause_show", v.state == "cleaning" || v.state == "returning" ? "1" : "");
  put("dock_show", v.state == "docked" || v.state == "returning" || out ? "" : "1");
  const bool card_val = run && v.state != "returning";
  put("card_mode", card_val ? "value" : "text");
  put("card_value", card_val ? (v.progress < 0 ? "--" : pct(v.progress)) : "--");
  put("card_text", word(v));
  put("card_sub", card_val ? join({v.state == "paused" ? "paused" : lower(room), v.minutes > 0 ? num(std::round(v.minutes)) + " min" : ""})
                           : out ? "not reachable" : join({bat, stuck ? words(v.error.empty() ? "stuck" : v.error) : v.state == "returning" ? "back to the dock"
                                                                     : v.state == "docked" ? (charging ? "charging" : "charged") : ""}));
  put("card_accent", v.state == "cleaning" || stuck ? "true" : "false");
  put("card_ring", num(out ? 0 : run ? std::max(0.0, v.progress) : std::max(0.0, v.battery)));
  put("card_run", out ? "off" : run ? "run" : "idle");
  put("", v.state);
}

// ---- adapter: sample (the docs robot; a run walks rows across the picked rooms)
std::string xy(float x, float z) { return num(x, 1) + "," + num(z, 1); }
void sample_trail() {
  St &s = S();
  std::string t;
  for (auto &p : s.done) t += (t.empty() ? "" : " ") + xy(p.first, p.second);
  s.s.trail = t, s.s.pos = xy(s.bx, s.bz);
}
void plan_run(const std::string &rooms) {  // rows 0.6 m apart across each picked room (none = all three)
  St &s = S();
  s.route.clear(), s.done.clear(), s.walked = 0;
  std::vector<const Box *> pick;
  for (const Box &b : ROOMS) {
    const std::string seg = std::string(":") + b.seg;
    if (rooms.empty() || ("," + rooms + ",").find(seg + ",") != std::string::npos) pick.push_back(&b);
  }
  if (pick.empty()) for (const Box &b : ROOMS) pick.push_back(&b);
  for (const Box *b : pick) {
    bool right = true;
    for (float z = b->z0; z <= b->z1 + 0.01f; z += 0.6f, right = !right) {
      s.route.push_back({right ? b->x0 : b->x1, z});
      s.route.push_back({right ? b->x1 : b->x0, z});
    }
  }
  float tot = 0, px = s.bx, pz = s.bz;
  for (auto &p : s.route) tot += std::hypot(p.first - px, p.second - pz), px = p.first, pz = p.second;
  s.total = std::max(tot, 1.0f);
  s.done.push_back({s.bx, s.bz});
}
const char *room_at(float x, float z) {
  for (const Box &b : ROOMS) if (x >= b.x0 - 0.3f && x <= b.x1 + 0.3f && z >= b.z0 - 0.3f && z <= b.z1 + 0.3f) return b.name;
  return "";
}
bool walk_to(float tx, float tz) {  // one step towards a point; true = there
  St &s = S();
  const float d = std::hypot(tx - s.bx, tz - s.bz);
  if (d <= STEP) { s.walked += d, s.bx = tx, s.bz = tz; return true; }
  s.bx += (tx - s.bx) * STEP / d, s.bz += (tz - s.bz) * STEP / d, s.walked += STEP;
  return false;
}
void sample_set(const std::string &state) {  // a start state, as the docs show it
  St &s = S();
  V &v = s.s;
  v = V{};
  v.state = state, v.mode = "vac_and_mop", v.fan = "turbo", v.mop = "high";
  const time_t t = time(nullptr) - 86400;
  struct tm tm;
  localtime_r(&t, &tm);
  char b[32];
  snprintf(b, sizeof b, "%04d-%02d-%02dT18:42:00", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
  v.last = ha::when(b, true);
  s.route.clear(), s.done.clear(), s.bx = DOCK_X, s.bz = DOCK_Z;
  const bool out = state == "cleaning" || state == "paused";
  v.battery = state == "docked" ? 100 : state == "cleaning" ? 74 : state == "paused" ? 71 : state == "returning" ? 62 : state == "idle" ? 88 : 40;
  v.status = state == "docked" ? "charging" : "";
  if (state == "error") v.error = "main_brush_jammed";
  if (out) {  // halfway through the Lounge
    plan_run("Lounge:1");
    for (int i = 0; i < 9 && !s.route.empty(); i++)
      if (walk_to(s.route.front().first, s.route.front().second)) s.done.push_back(s.route.front()), s.route.erase(s.route.begin());
    s.done.push_back({s.bx, s.bz});
    s.walked = 0.46f * s.total;  // the docs' 46 %
    v.room = "Lounge", v.progress = 46, v.minutes = 9, v.area = 7.6;
  } else if (state == "returning") {
    s.bx = 2.0f, s.bz = 2.6f, s.done = {{0.5f, 3.6f}, {3.4f, 3.6f}, {3.4f, 4.2f}, {0.5f, 4.2f}, {2.0f, 2.6f}};
  } else if (state == "docked" || state == "idle") {
    s.done = {{0.6f, 0.7f}, {3.1f, 0.7f}, {3.1f, 1.3f}, {0.6f, 1.3f}, {0.6f, 1.9f}, {3.1f, 1.9f}, {3.1f, 2.5f}, {0.6f, 2.5f}};
    if (state == "idle") s.bx = 0.6f, s.bz = 2.5f;
  }
  sample_trail();
}
void motor_step() {  // one second of the sample robot
  St &s = S();
  V &v = s.s;
  s.ticks++;
  if (v.state == "cleaning") {
    if (s.route.empty()) { v.state = "returning"; v.progress = 100; }
    else {
      if (walk_to(s.route.front().first, s.route.front().second)) s.route.erase(s.route.begin());
      s.done.push_back({s.bx, s.bz});
      v.progress = std::min(99.0, 100.0 * s.walked / s.total);
      v.area += 0.15, v.room = room_at(s.bx, s.bz);
      if (s.ticks % 6 == 0) v.minutes += 1;
      if (s.ticks % 5 == 0 && v.battery > 5) v.battery -= 1;
    }
  } else if (v.state == "returning") {
    if (walk_to(DOCK_X, DOCK_Z)) {
      v.state = "docked", v.status = "charging", v.room.clear(), v.progress = v.minutes = v.area = -1;
      const time_t t = time(nullptr);
      struct tm tm;
      localtime_r(&t, &tm);
      char b[8];
      snprintf(b, sizeof b, "%02d:%02d", tm.tm_hour, tm.tm_min);
      v.last = std::string("today ") + b;
    }
  } else if (v.state == "docked" && v.battery < 100) {
    if (s.ticks % 2 == 0) v.battery += 1;
  } else return;
  sample_trail();
  publish();
}

// ---- wiring
void entity_put(const std::string &e, const std::string &, const std::string &) {
  St &s = S();
  if (s.src != "ha" || e.empty() || e.rfind("panel.", 0) == 0) return;
  for (const std::string &x : s.ent) if (x == e) { publish(); return; }
}
void fresh() {
  St &s = S();
  const std::string &cfg = ha::feed_config(E);
  static const bool hooked = (ha::on_put(entity_put), true);
  (void) hooked;
  if (cfg != s.cfg_seen) {
    s.cfg_seen = cfg;
    ArduinoJson::JsonDocument c;
    deserializeJson(c, cfg.empty() ? "{}" : cfg.c_str());
    s.src = c["source"] | "sample";
    for (int k = 0; k < NK; k++) s.ent[k] = c[KEYS[k]] | "";
    if (s.ent[0].rfind("vacuum.", 0) != 0) s.ent[0].clear();
    if (s.src == "ha") {
      for (int k = 0; k < NK; k++) if (!s.ent[k].empty()) ha::want(s.ent[k]);
      if (!s.ent[0].empty()) ha::want(s.ent[0], "fan_speed"), ha::want(s.ent[0], "battery_level");
      if (!s.ent[10].empty()) ha::want(s.ent[10], "points");
    } else {
      static const char *const OK[] = {"docked", "cleaning", "paused", "returning", "idle", "error"};
      s.start = c["sample"] | "docked";
      if (std::none_of(std::begin(OK), std::end(OK), [&](const char *x) { return s.start == x; })) s.start = "docked";
      sample_set(s.start);
    }
  }
  if (!motor) motor = lv_timer_create([](lv_timer_t *) { if (S().src != "ha") motor_step(); }, 1000, nullptr);
  publish();
}
[[maybe_unused]] const bool registered = (ha::on_local(E, fresh), true);  // binds on panel.vacuum never reach a host
}  // namespace

// vacuum.control (core/spec.h run_action): what a tap on the Robot vacuum screen does; "" = done, else why not. svc =
// the service to call on ent with data ("" = none: the sample robot does it itself)
const char *control(const std::string &v, const std::string &rooms, std::string *svc, std::string *ent, std::string *data) {
  St &s = S();
  svc->clear(), ent->clear(), data->clear();
  const bool ha = s.src == "ha";
  const std::string state = ha ? read_ha().state : s.s.state;
  if (state == "unavailable") return "the vacuum is unavailable";
  const size_t sp = v.find(' ');
  const std::string key = lower(v.substr(0, sp)), opt = sp == std::string::npos ? "" : v.substr(sp + 1);
  if (key == "mode" || key == "fan" || key == "mop") {
    if (opt.empty() || opt.size() > 32) return "a setting needs an option";
    if (!ha) { (key == "mode" ? s.s.mode : key == "fan" ? s.s.fan : s.s.mop) = opt; publish(); return ""; }
    ArduinoJson::JsonDocument d;
    if (key == "fan") { *svc = "vacuum.set_fan_speed", *ent = s.ent[0], d["fan_speed"] = opt; }
    else {
      *ent = s.ent[key == "mode" ? 11 : 12];
      if (ent->empty()) return "no setting chosen for that";
      *svc = "select.select_option", d["option"] = opt;
    }
    serializeJson(d, *data);
    return "";
  }
  if (key.rfind("clean", 0) == 0 || key == "start") {
    if (running(state)) return "a run is on";
    std::string segs;  // "Kitchen:3,Corridor:1" -> 3,1 (the id after an item's last colon)
    for (size_t i = 0; i < rooms.size();) {
      size_t j = rooms.find(',', i);
      if (j == std::string::npos) j = rooms.size();
      std::string it = rooms.substr(i, j - i);
      const size_t c = it.rfind(':');
      if (c != std::string::npos) it = it.substr(c + 1);
      const char *q = it.c_str();
      while (*q && !isdigit((unsigned char) *q)) q++;
      if (*q) segs += (segs.empty() ? "" : ",") + std::to_string(atoi(q));
      i = j + 1;
    }
    if (ha) {
      *ent = s.ent[0];
      if (segs.empty()) *svc = "vacuum.start";
      else *svc = "vacuum.send_command", *data = "{\"command\":\"app_segment_clean\",\"params\":[{\"segments\":[" + segs + "],\"repeat\":1}]}";
      return "";
    }
    plan_run(rooms);
    s.s.state = "cleaning", s.s.status.clear(), s.s.error.clear(), s.s.progress = 0, s.s.minutes = 0, s.s.area = 0;
    s.s.room = room_at(s.bx, s.bz);
    sample_trail();
    publish();
    return "";
  }
  const char *hsvc = key == "resume" ? "vacuum.start" : key == "pause" ? "vacuum.pause" : key == "dock" ? "vacuum.return_to_base" : nullptr;
  if (!hsvc) return "not a vacuum control";
  if (ha) { *svc = hsvc, *ent = s.ent[0]; return ""; }
  if (key == "resume" && s.s.state == "paused") s.s.state = "cleaning";
  else if (key == "pause" && (s.s.state == "cleaning" || s.s.state == "returning")) s.s.state = "paused";
  else if (key == "dock" && s.s.state != "docked") s.s.state = "returning", s.s.error.clear();
  publish();
  return "";
}
}  // namespace fw::dev::vacuum_source
