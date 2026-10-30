// The covers feed (CONTRACT.md C51): the reserved entity panel.covers (core/ha.h "local sources") in the HA store, the
// one shape the Blinds and curtains screen and its Home card bind to, whatever moves the blinds. Same file on the
// panels and in WASM. Positions are Home Assistant's: 0 = closed, 100 = open (docs @facet/ui covers.ts).
//   state        open | closed | opening | closing | unavailable | none (no window chosen)
//   name         the window's name (the screen's header title), "Blinds" when none is chosen
//   cap          over the value (docs .fs-cv-cap): "42% open" | "open" | "closed" | "opening" | "closing" | "offline" |
//                "set to" (a command just went out) | "no window chosen"
//   value        the value (docs coverValue): "42%" | "Open" | "Closed" | "--"; value_state default | unavailable
//   kind         roller | venetian | curtain (the Cover visual: docs coverKind: curtain device class = curtains, a
//                tilt position = Venetian, else roller); kind_text "roller blind" | "Venetian blind" | "curtains"
//   position     0-100, the real one (the Cover visual draws it; a drag only moves its ghost); tilt 0-100
//   heading      where a moving cover is going (-1 = none: the Cover visual's target line)
//   vis_state    default | disabled (a cover with no position, or offline) | unavailable; drag "true" | "false"
//   tilt_show    "1" for a Venetian blind that tilts to a position, else "" (the slats Slider row shows); tilt_text
//                "60%" | "--"; tilt_state default | unavailable
//   open_show / stop_show / close_show  "1" when the cover can (its supported features), else ""; open_state /
//                stop_state / close_state default | disabled (docs: open at 100, close at 0, stop unless moving)
//   windows      the windows sheet's rows, a JSON list of {id, name, status, icon, tone, selected}; windows_show
//                "1" with more than one window; sheet open | closed
//   card_value / card_sub / card_accent / card_pos  the Home card (docs home.ts covers): the value, "study blind ·
//                1 of 3 open" (one window: its status), accent while any moves, the preview's position
// Adapters (the feed's settings, the spec's "feeds"; core/ha.h feed_config): source
//   ha      Home Assistant cover entities (setting cover = the one shown first; covers = a comma list for the sheet),
//           read live like any bind (state, current_position, current_tilt_position, device_class,
//           supported_features, friendly_name). The controls go out through the action cover.control (core/spec.h
//           run_action: the one checked path) as cover services on the window shown.
//   sample  the docs mock house (demo-data covers.ts): a roller blind in the study, curtains in the lounge and a
//           Venetian blind in the kitchen, their motors moving about 5 % a second; nothing leaves the panel
// Controls (control(); the screen's taps through cover.control on e panel.covers): open | close | stop, position N (a
// drag on the window, released), tilt N (the slats, released), windows / sheet_close (the sheet), pick <name> (a sheet
// row). A command holds HA's echoes off for 3 s (docs holdUntil): the words say "set to" while it is on its way.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include "fw.h"
#include "ha.h"

namespace fw::dev::covers_source {
namespace {
constexpr const char *E = "panel.covers";
const char *const ATTRS[] = {"", "current_position", "current_tilt_position", "device_class", "supported_features", "friendly_name"};
enum { F_OPEN = 1, F_CLOSE = 2, F_POS = 4, F_STOP = 8, F_TILT = 128 };
struct Win {
  std::string id, name, dc, state = "closed";
  int pos = -1, tilt = -1, feat = 15, goal = -1;  // -1 = unknown / none; goal = the sample motor's target
  int goal_tilt = -1;
};
struct St {
  std::string cfg_seen, src = "sample", first, picked, sheet = "closed";
  std::vector<std::string> ids;   // ha: the covers for the sheet
  std::vector<Win> sample;        // sample: the mock house
  int sel = 0;                    // sample: the window shown
  int held = -1, held_tilt = -1, heading = -1;
  uint32_t hold_until = 0, tilt_until = 0;
};
St &S() { static St s; return s; }
lv_timer_t *motor = nullptr;

void put(const char *a, const std::string &v) { ha::put(E, a, v); }
std::string num(int v) { char b[16]; snprintf(b, sizeof b, "%d", v); return b; }
bool moving(const Win &w) { return w.state == "opening" || w.state == "closing"; }
int pos_of(const Win &w) { return w.pos >= 0 ? w.pos : (w.state == "open" || w.state == "closing") ? 100 : 0; }
const char *kind_of(const Win &w) {
  if (w.dc == "curtain") return "curtain";
  return w.feat & F_TILT ? "venetian" : "roller";
}
std::string status(const Win &w) {
  if (w.state == "unavailable") return "offline";
  if (moving(w)) return w.state;
  const int p = pos_of(w);
  return p <= 0 ? "closed" : p >= 100 ? "open" : num(p) + "% open";
}
std::string value(const Win &w, int p) {
  if (w.state == "unavailable") return "--";
  return p <= 0 ? "Closed" : p >= 100 ? "Open" : num(p) + "%";
}
std::string lower(std::string s) { for (char &c : s) c = (char) tolower((unsigned char) c); return s; }
std::string esc(const std::string &s) {
  ArduinoJson::JsonDocument d;
  d.set(s);
  std::string o;
  serializeJson(d, o);
  return o;
}

// ha: one cover entity as a window (nullptr when the store has nothing for it yet)
bool read(const std::string &id, Win &w) {
  const std::string *st = ha::get(id);
  w = Win{};
  w.id = id;
  const std::string *fn = ha::get(id, "friendly_name");
  w.name = fn && !fn->empty() ? *fn : id.substr(id.find('.') + 1);
  if (!st) { w.state = "unavailable"; return false; }
  w.state = *st == "unknown" ? "unavailable" : *st;
  if (const std::string *v = ha::get(id, "current_position"); v && !v->empty()) w.pos = (int) std::lround(ha::num(v));
  if (const std::string *v = ha::get(id, "current_tilt_position"); v && !v->empty()) w.tilt = (int) std::lround(ha::num(v));
  if (const std::string *v = ha::get(id, "device_class")) w.dc = *v;
  if (const std::string *v = ha::get(id, "supported_features"); v && !v->empty()) w.feat = (int) ha::num(v);
  return true;
}
std::vector<Win> windows(int *shown) {
  St &s = S();
  std::vector<Win> out;
  *shown = 0;
  if (s.src != "ha") { *shown = s.sel; return s.sample; }
  const std::string cur = s.picked.empty() ? s.first : s.picked;
  std::vector<std::string> ids = s.ids;
  if (!cur.empty() && std::find(ids.begin(), ids.end(), cur) == ids.end()) ids.insert(ids.begin(), cur);
  for (const std::string &id : ids) {
    Win w;
    read(id, w);
    if (id == cur) *shown = (int) out.size();
    out.push_back(w);
  }
  return out;
}
std::string current() {
  int i;
  std::vector<Win> ws = windows(&i);
  return ws.empty() ? "" : ws[i].id;
}

void publish() {
  St &s = S();
  int i;
  const std::vector<Win> ws = windows(&i);
  const uint32_t now = lv_tick_get();
  const bool hold_on = (int32_t) (s.hold_until - now) > 0, holding = s.held >= 0 && hold_on;
  if (!holding) s.held = -1;
  if (s.held_tilt >= 0 && (int32_t) (s.tilt_until - now) <= 0) s.held_tilt = -1;
  if (ws.empty()) {
    put("name", "Blinds"), put("cap", "no window chosen"), put("value", "--"), put("value_state", "unavailable");
    put("kind", "roller"), put("kind_text", ""), put("position", "0"), put("tilt", "50"), put("heading", "-1");
    put("vis_state", "unavailable"), put("drag", "false"), put("tilt_show", ""), put("tilt_text", "--");
    put("tilt_state", "unavailable");
    for (const char *a : {"open", "stop", "close"}) put((std::string(a) + "_show").c_str(), "1"), put((std::string(a) + "_state").c_str(), "disabled");
    put("windows", "[]"), put("windows_show", ""), put("sheet", s.sheet);
    put("card_value", "--"), put("card_sub", "no window chosen"), put("card_accent", "false"), put("card_pos", "0");
    put("", "none");
    return;
  }
  const Win &w = ws[i];
  const bool off = w.state == "unavailable", mv = moving(w);
  const int p = pos_of(w);
  if (!mv && !hold_on) s.heading = -1;  // the motor never started (or it got there): no target line
  put("name", w.name);
  put("cap", holding ? "set to" : status(w));
  put("value", holding ? value(w, s.held) : value(w, p));
  put("value_state", off ? "unavailable" : "default");
  put("kind", kind_of(w));
  put("kind_text", !strcmp(kind_of(w), "curtain") ? "curtains" : !strcmp(kind_of(w), "venetian") ? "Venetian blind" : "roller blind");
  put("position", num(p));
  const int tl = s.held_tilt >= 0 ? s.held_tilt : w.tilt >= 0 ? w.tilt : 50;
  put("tilt", num(tl));
  put("heading", num(mv ? s.heading : -1));
  put("vis_state", off ? "unavailable" : (w.feat & F_POS) ? "default" : "disabled");
  put("drag", (w.feat & F_POS) ? "true" : "false");
  put("tilt_show", !strcmp(kind_of(w), "venetian") && (w.feat & F_TILT) ? "1" : "");
  put("tilt_text", off || w.tilt < 0 ? "--" : num(tl));
  put("tilt_state", off ? "unavailable" : "default");
  put("open_show", w.feat & F_OPEN ? "1" : ""), put("close_show", w.feat & F_CLOSE ? "1" : ""), put("stop_show", w.feat & F_STOP ? "1" : "");
  put("open_state", off || (!mv && p >= 100) ? "disabled" : "default");
  put("close_state", off || (!mv && p <= 0) ? "disabled" : "default");
  put("stop_state", !off && mv ? "default" : "disabled");
  std::string rows = "[";
  int open = 0;
  bool any_moving = false;
  for (size_t k = 0; k < ws.size(); k++) {
    const Win &x = ws[k];
    if (x.state != "unavailable" && status(x) != "closed") open++;
    any_moving = any_moving || moving(x);
    rows += (k ? "," : "") + std::string("{\"id\":") + esc(x.id) + ",\"name\":" + esc(x.name) + ",\"status\":" + esc(status(x)) +
            ",\"icon\":\"" + kind_of(x) + "\",\"tone\":\"" + (moving(x) ? "working" : "dim") + "\",\"selected\":" + ((int) k == i ? "true" : "false") + "}";
  }
  put("windows", rows + "]");
  put("windows_show", ws.size() > 1 ? "1" : "");
  put("sheet", s.sheet);
  put("card_value", value(w, p));
  put("card_sub", ws.size() > 1 ? lower(w.name) + " \xC2\xB7 " + num(open) + " of " + num((int) ws.size()) + " open" : lower(w.name) + " \xC2\xB7 " + status(w));
  put("card_accent", any_moving ? "true" : "false");
  put("card_pos", num(p));
  put("", off ? "unavailable" : w.state);
}

// ---- adapter: sample (the docs mock house; motors about 5 % a second)
void sample_init() {
  St &s = S();
  if (!s.sample.empty()) return;
  s.sample = {{"sample.study_blind", "Study blind", "shade", "open", 60, -1, 15},
              {"sample.lounge_curtains", "Lounge curtains", "curtain", "open", 100, -1, 15},
              {"sample.kitchen_blind", "Kitchen blind", "blind", "open", 100, 60, 255}};
}
void motor_step() {  // one step of the motors: covers heading somewhere move 1 % towards it (5 steps a second)
  St &s = S();
  bool any = false;
  for (Win &w : s.sample) {
    if (w.goal_tilt >= 0) { w.tilt = w.goal_tilt; w.goal_tilt = -1; any = true; }
    if (w.goal < 0) continue;
    any = true;
    const int p = w.pos;
    const int next = std::abs(w.goal - p) <= 1 ? w.goal : p + (w.goal > p ? 1 : -1);
    w.pos = next;
    if (next == w.goal) { w.state = next <= 0 ? "closed" : "open"; w.goal = -1; }
    else w.state = w.goal > p ? "opening" : "closing";
  }
  if (any) publish();
}
void sample_go(Win &w, int goal) {
  goal = LV_CLAMP(0, goal, 100);
  if (goal == w.pos) { w.goal = -1; w.state = goal <= 0 ? "closed" : "open"; return; }
  w.goal = goal;
  w.state = goal > w.pos ? "opening" : "closing";
  S().heading = goal;
}

// ---- adapter: ha
void entity_put(const std::string &e, const std::string &a, const std::string &) {
  St &s = S();
  if (s.src != "ha" || e.rfind("cover.", 0) != 0) return;
  if (std::find(std::begin(ATTRS), std::end(ATTRS), a) == std::end(ATTRS)) return;
  if (e == s.first || e == s.picked || std::find(s.ids.begin(), s.ids.end(), e) != s.ids.end()) publish();
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
    s.first = c["cover"] | "";
    if (s.first.rfind("cover.", 0) != 0) s.first.clear();
    s.ids.clear(), s.picked.clear(), s.sheet = "closed", s.held = s.held_tilt = s.heading = -1;
    const std::string list = c["covers"] | "";
    for (size_t a = 0; a < list.size();) {
      size_t b = list.find(',', a);
      if (b == std::string::npos) b = list.size();
      std::string id = list.substr(a, b - a);
      id.erase(0, id.find_first_not_of(' '));
      id.erase(id.find_last_not_of(' ') + 1);
      if (id.rfind("cover.", 0) == 0) s.ids.push_back(id);
      a = b + 1;
    }
    if (s.src == "ha") {
      std::vector<std::string> all = s.ids;
      if (!s.first.empty()) all.push_back(s.first);
      for (const std::string &id : all)
        for (const char *a : ATTRS) ha::want(id, a);
    } else sample_init();
  }
  if (!motor) motor = lv_timer_create([](lv_timer_t *) {
    if (S().src != "ha") motor_step();
    else if (S().held >= 0 || S().held_tilt >= 0 || S().heading >= 0) publish();  // a hold runs out: the words go back to HA's
  }, 200, nullptr);
  publish();
}
[[maybe_unused]] const bool registered = (ha::on_local(E, fresh), true);  // binds on panel.covers never reach a host
}  // namespace

// cover.control (core/spec.h run_action): what a tap on the Covers screen does; "" = done, else why not. svc = a cover
// service to call on ent with data ("" = none: the sample adapter moves its own motors)
const char *control(const std::string &v, std::string *svc, std::string *ent, std::string *data) {
  St &s = S();
  svc->clear(), data->clear(), ent->clear();
  const bool ha = s.src == "ha";
  if (v == "windows" || v == "sheet_close") { s.sheet = v == "windows" ? "open" : "closed"; put("sheet", s.sheet); return ""; }
  if (v.rfind("pick ", 0) == 0) {  // a sheet row by its name (a Picker row's tap says the name) or its id
    const std::string id = v.substr(5);
    int i;
    const std::vector<Win> ws = windows(&i);
    s.sheet = "closed";
    for (size_t k = 0; k < ws.size(); k++)
      if (ws[k].name == id || ws[k].id == id) {
        if (ha) s.picked = ws[k].id; else s.sel = (int) k;
        s.held = s.held_tilt = s.heading = -1;
      }
    publish();
    return "";
  }
  int i;
  std::vector<Win> ws = windows(&i);
  if (ws.empty()) return "no window chosen";
  if (ws[i].state == "unavailable") return "the cover is unavailable";
  *ent = ha ? ws[i].id : "";
  const uint32_t until = lv_tick_get() + 3000;
  int goal = -1;
  char b[48];
  if (v == "open" || v == "close") {
    goal = v == "open" ? 100 : 0;
    if (ha) *svc = v == "open" ? "cover.open_cover" : "cover.close_cover";
  } else if (v == "stop") {
    if (ha) *svc = "cover.stop_cover";
    else { Win &w = s.sample[s.sel]; w.goal = -1; w.state = w.pos <= 0 ? "closed" : "open"; }
    s.heading = -1, s.held = -1;
    publish();
    return "";
  } else if (v.rfind("position ", 0) == 0) {
    goal = LV_CLAMP(0, atoi(v.c_str() + 9), 100);
    s.held = goal, s.hold_until = until;
    if (ha) { *svc = "cover.set_cover_position"; snprintf(b, sizeof b, "{\"position\":%d}", goal); *data = b; }
  } else if (v.rfind("tilt ", 0) == 0) {
    const int t = LV_CLAMP(0, atoi(v.c_str() + 5), 100);
    s.held_tilt = t, s.tilt_until = until;
    if (ha) { *svc = "cover.set_cover_tilt_position"; snprintf(b, sizeof b, "{\"tilt_position\":%d}", t); *data = b; }
    else s.sample[s.sel].goal_tilt = t;
    publish();
    return "";
  } else if (v.rfind("live ", 0) == 0) {
    return "";  // the slats under a finger: the Slider row shows it, the release sends it
  } else return "not a cover control";
  s.heading = goal;
  if (!ha) sample_go(s.sample[s.sel], goal);
  else s.hold_until = until;
  publish();
  return "";
}
}  // namespace fw::dev::covers_source
