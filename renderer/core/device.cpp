// Device runtime: core/device.h (screen stack, navigation, the screen transition). Same file on the panels and in WASM.
#include <cmath>
#include <functional>
#include <map>
#include "device.h"
#include "fx.h"
#include "colour_values_gen.h"
#include <algorithm>
#include <vector>
#include "spec.h"
#include "toasts.h"
#include "theme_slot.h"
#include "settings_gen.h"

namespace fw {
namespace dev {
namespace {
enum Op : uint8_t { NONE, GO, BACK, HOME, IDLE };
struct Device {
  lv_obj_t *root = nullptr, *screen = nullptr;  // screen = the root's one child (rebuilt on every cut)
  const Theme *stage = nullptr;                 // the theme the device was mounted with (fw::T is global)
  const Theme *theme = nullptr;                 // the theme it shows (look(stage): the Theme / Accent settings)
  lv_timer_t *restyle = nullptr;                // a restyle waiting for the running animations
  uint32_t builds = 0;    // screens built (cuts + restyles + spec reloads): hello diagnostics
  std::string reloaded;  // the spec id a pending rebuild is for ("" = a restyle)
  uint32_t restyle_at = 0;
  std::vector<std::string> stack;
  Op op = NONE;
  std::string target;
  lv_timer_t *timer = nullptr;
  lv_obj_t *toasts = nullptr;  // the Toast layer over every screen (the display's top layer; core/toasts.h)
  struct Kept { lv_obj_t *scr; std::string entry; bool stale; uint32_t bytes, objs, ms; };  // a kept page (show())
  std::map<std::string, Kept> kept;
  lv_timer_t *pre = nullptr;  // the kept pages' prebuild (idle gaps)
  uint32_t shown_at = 0;      // the last screen change (the prebuild waits 300 ms after one)
  uint32_t pre_done = 0;      // the last prebuild's end (the next waits 250 ms from it)
  lv_obj_t *park = nullptr;   // the kept pages not shown: a never-loaded screen of the same display (no refresh walks it)
  lv_timer_t *idle = nullptr; // the idle check (1 s; device.h "idle")
  bool idled = false;         // the idle clock shows (dimmed)
};

std::vector<Device *> devices;  // every mounted device (usually one; a workbench page may show several)
Device *of(lv_obj_t *in) {
  for (lv_obj_t *o = in; o; o = lv_obj_get_parent(o))
    for (Device *d : devices)
      if (d->root == o) return d;
  return nullptr;
}
lv_obj_t *box(lv_obj_t *parent) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  return o;
}
// the screen's root after a build into `host` (its last child), or nullptr
lv_obj_t *build_into(lv_obj_t *host, const Component *c, Props p) {
  const uint32_t before = lv_obj_get_child_count(host);
  c->build(host, p);
  return lv_obj_get_child_count(host) > before ? lv_obj_get_child(host, -1) : nullptr;
}
bool skip(lv_obj_t *o) { return lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN) || lv_obj_has_flag(o, LV_OBJ_FLAG_FLOATING); }

// build screen `id` as the device's screen; returns the items for the rise (header first), or false if unknown
// an id may carry the screen's props: "light?{\"entity\":\"light.x\"}" (one Light screen for every light)
std::string base_of(const std::string &id) { return id.substr(0, id.find('?')); }
bool known(const std::string &id) { return find(base_of(id).c_str()) || spec::screen_json(base_of(id)); }
bool text_large_setting();
// the device's screens are built in its Text size (Settings > Text size: Large = core/fonts.cpp large fonts)
// navigation timing (device.h nav_timing): the change being measured, from its tap to its first frame
struct Nav { bool on = false; std::vector<NavTime> done; NavTime cur; int stage = 0; uint32_t touch = 0; };  // stage 0 idle, 1 queued, 2 built
Nav &nav() { static Nav n; return n; }
struct LargeScope {
  bool was;
  explicit LargeScope(bool on) : was(text_large()) { set_text_large(on); }
  ~LargeScope() { set_text_large(was); }
};
// the items the rise transition moves: the header (the screen's first child) and the blocks (a scrolling column =
// its children on screen)
void collect(lv_obj_t *s, std::vector<lv_obj_t *> &items) {
  for (uint32_t i = 0; i < lv_obj_get_child_count(s); i++) {
    lv_obj_t *k = lv_obj_get_child(s, (int32_t) i);
    if (skip(k)) continue;
    if (!i) { items.push_back(k); continue; }
    if (lv_obj_has_flag(k, LV_OBJ_FLAG_SCROLLABLE) && lv_obj_get_child_count(k) > 1) {
      for (uint32_t j = 0; j < lv_obj_get_child_count(k); j++) {
        lv_obj_t *b = lv_obj_get_child(k, (int32_t) j);
        lv_area_t a;
        lv_obj_get_coords(b, &a);
        if (!skip(b) && a.y1 < lay::screen_h) items.push_back(b);
      }
    } else items.push_back(k);
  }
}
// screen `id` made into a new screen box under the device root (at_root: the stack's root, built without Back);
// nullptr = no such screen
lv_obj_t *make(Device *d, const std::string &id, bool at_root, std::vector<lv_obj_t *> *items) {
  LargeScope large(text_large_setting());
  struct RootScope {  // the stack's root screen is built without Back (spec::at_root)
    bool was;
    explicit RootScope(bool on) : was(spec::at_root) { spec::at_root = on; }
    ~RootScope() { spec::at_root = was; }
  } root(at_root);
  const Component *c = find(base_of(id).c_str());
  const bool from_spec = !c && spec::screen_json(base_of(id));  // a screen from its spec (core/spec.h)
  if (!c && !from_spec) return nullptr;
  ArduinoJson::JsonDocument none;
  const size_t q = id.find('?');
  if (q == std::string::npos || deserializeJson(none, id.c_str() + q + 1) || !none.is<ArduinoJson::JsonObject>()) none.to<ArduinoJson::JsonObject>();
  d->builds++;
  lv_obj_t *scr = box(d->root);
  lv_obj_set_size(scr, lay::screen_w, lay::screen_h);
  // the panel's own canvas: the live theme's bg under a contract or spec screen (fx::bg, Facet's bg for the Facet
  // Skin), Facet's under a legacy screen (no half-themed screens: its parts keep Facet's palette)
  const bool themed = from_spec || (c && c->meta && (strstr(c->meta, "\"contract\": 2") || strstr(c->meta, "\"contract\":2")));
  lv_obj_set_style_bg_color(scr, themed ? fx::bg(lv_obj_get_display(d->root)) : T->bg, 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
  if (from_spec || !strcmp(c->section, "screens")) {
    lv_obj_t *s = from_spec ? spec::build(scr, base_of(id), none.as<Props>()) : build_into(scr, c, none.as<Props>());
    if (!s) return scr;
    if (nav().on && nav().stage == 1) nav().cur.made = lv_tick_get();
    lv_obj_update_layout(scr);
    if (nav().on && nav().stage == 1) nav().cur.laid = lv_tick_get();
    if (items) collect(s, *items);
    return scr;
  }
  // a page that is not a screen (a foundation): the Header (back + its name) and the page scrolling under it
  ArduinoJson::JsonDocument hd;
  hd["label"] = c->name;
  const fx::Page pg = fx::page(scr, "page");  // Page > Header slot + Body (CONTRACT.md): the inset from the structure
  lv_obj_t *h = build_into(pg.head, find("header"), hd.as<Props>());
  build_into(pg.body, c, none.as<Props>());
  if (items) {
    if (h) items->push_back(h);
    items->push_back(pg.body);
  }
  return scr;
}

// Kept pages (facet-243): Home and the deck's pages are built once, in the idle gaps (prebuild), and parked when
// another screen shows (moved to a never-loaded screen of the same display: the panel's refresh never walks them; hidden
// on the shown screen they cost ~80 ms before every render on the Waveshare), so a tap shows a page instead of building
// it. A parked page does no drawing; its binds keep the data and repaint when it shows again (core/ha.h asleep / wake). It keeps its scroll and state. A settings or look
// change makes every kept page stale; a deck change the pages whose entry changed (and Home: its cards); a spec change
// that page. A stale page is built again when it shows, or in the idle gaps. keep_pages(n): at most n kept (0 = off:
// every screen built on tap); a page over the cap is built on tap. The host's mem hook (free PSRAM) measures each.
size_t (*&mem_hook())() { static size_t (*f)() = nullptr; return f; }
int &keep_max() { static int n = 0; return n; }
uint32_t &kept_all_at() { static uint32_t t = 0; return t; }  // lv_tick when the kept pages first reached the cap (0 = not yet)
bool keepable(const std::string &id) { return id == "home" || (id.find('?') == std::string::npos && spec::deck_kinds().count(id)); }
std::string entry_of(const std::string &id) {
  ArduinoJson::JsonDocument e;
  if (!spec::deck_entry(id, e)) return "";
  std::string out;
  serializeJson(e, out);
  return out;
}
bool is_kept(Device *d, lv_obj_t *o) {
  for (auto &k : d->kept) if (k.second.scr == o) return true;
  return false;
}
lv_obj_t *park_of(Device *d) {
  if (d->park) return d->park;
  lv_display_t *was = lv_display_get_default();
  lv_display_set_default(lv_obj_get_display(d->root));  // the device's own display (a workbench page has several)
  d->park = lv_obj_create(nullptr);
  lv_display_set_default(was);
  ha::parks().insert(d->park);
  lv_obj_add_event_cb(d->park, [](lv_event_t *e) { ha::parks().erase((lv_obj_t *) lv_event_get_current_target(e)); }, LV_EVENT_DELETE, nullptr);
  return d->park;
}
bool show(Device *d, const std::string &id, std::vector<lv_obj_t *> &items) {
  lv_obj_t *old = d->screen;
  d->shown_at = lv_tick_get();
  auto leave = [&](lv_obj_t *o) {
    if (!o) return;
    if (is_kept(d, o)) lv_obj_set_parent(o, park_of(d));  // parked: off the shown screen, nothing walks it
    else lv_obj_delete(o);
  };
  auto it = d->kept.find(id);
  if (it != d->kept.end() && !it->second.stale) {  // kept: shown again as it was left, its binds caught up first
    lv_obj_t *scr = it->second.scr;
    if (old != scr) leave(old);
    if (nav().on && nav().stage == 1) nav().cur.del = lv_tick_get();
    lv_obj_set_parent(scr, d->root);  // back on the shown screen
    d->screen = scr;
    ha::wake(scr);
    if (nav().on && nav().stage == 1) nav().cur.made = lv_tick_get();  // made = shown + its waiting binds repainted
    lv_obj_update_layout(scr);  // its layout now, inside the change (not in the first frame)
    if (nav().on && nav().stage == 1) nav().cur.laid = lv_tick_get();
    if (lv_obj_get_child_count(scr)) collect(lv_obj_get_child(scr, 0), items);
    return true;
  }
  if (it != d->kept.end()) {  // stale: built again
    if (it->second.scr == old) old = nullptr;
    lv_obj_delete(it->second.scr);
    d->kept.erase(it);
  }
  leave(old);
  d->screen = nullptr;
  if (nav().on && nav().stage == 1) nav().cur.del = lv_tick_get();
  lv_obj_t *scr = make(d, id, d->stack.size() <= 1, &items);
  if (!scr) return false;
  d->screen = scr;
  if (keep_max() > 0 && keepable(id) && (int) d->kept.size() < keep_max()) d->kept[id] = {scr, entry_of(id), false, 0, 0, 0};
  return true;
}
bool pointer_down() {
  for (lv_indev_t *i = lv_indev_get_next(nullptr); i; i = lv_indev_get_next(i))
    if (lv_indev_get_type(i) == LV_INDEV_TYPE_POINTER && lv_indev_get_state(i) == LV_INDEV_STATE_PRESSED) return true;
  return false;
}
int objects(lv_obj_t *x) { int n = 1; for (uint32_t i = 0; i < lv_obj_get_child_count(x); i++) n += objects(lv_obj_get_child(x, (int32_t) i)); return n; }
// the idle gaps: one kept page built (or built again) per tick, never while a finger is down or a screen change waits
void prebuild(lv_timer_t *t) {
  auto *d = (Device *) lv_timer_get_user_data(t);
  // idle: no finger for 500 ms, no screen change in the last 300 ms, none waiting (Home always has something turning,
  // so a running animation does not count)
  lv_display_t *disp = lv_obj_get_display(d->root);
  if (keep_max() <= 0 || d->stack.empty() || d->timer || d->restyle || pointer_down()) return;
  if ((disp && lv_display_get_inactive_time(disp) < 500) || lv_tick_elaps(d->shown_at) < 300) return;
  // 250 ms from the END of the last build: building makes timers, LVGL's handler then starts its list again and would
  // run this one again in the same call, page after page, and never give the loop back (the Guition's task watchdog)
  if (d->pre_done && lv_tick_elaps(d->pre_done) < 250) return;
  std::vector<std::string> want{"home"};
  ArduinoJson::JsonDocument deck;
  if (!deserializeJson(deck, spec::deck_json()))
    for (ArduinoJson::JsonVariantConst pg : deck["pages"].as<ArduinoJson::JsonArrayConst>()) {
      const std::string kind = pg["kind"] | "";
      if (kind != "home" && keepable(kind) && std::find(want.begin(), want.end(), kind) == want.end()) want.push_back(kind);
    }
  for (const std::string &id : want) {
    auto it = d->kept.find(id);
    if (it == d->kept.end() && d->screen && d->stack.back() == id && (int) d->kept.size() < keep_max()) {  // the shown one: kept as it is
      d->kept[id] = {d->screen, entry_of(id), false, 0, (uint32_t) objects(d->screen), 0};
      return;
    }
    if (it != d->kept.end() && !it->second.stale) continue;
    if (it != d->kept.end() && it->second.scr == d->screen) continue;  // the shown page: built again by its restyle
    if (it == d->kept.end() && (int) d->kept.size() >= keep_max()) return;
    if (it != d->kept.end()) { lv_obj_delete(it->second.scr); d->kept.erase(it); }
    const size_t m0 = mem_hook() ? mem_hook()() : 0;
    const uint32_t t0 = lv_tick_get();
    const Theme *was = T;
    if (d->theme) T = d->theme;
    lv_obj_t *scr = make(d, id, id == "home", nullptr);
    T = devices.size() == 1 ? d->theme : was;
    if (!scr) continue;
    lv_obj_set_parent(scr, park_of(d));
    const size_t m1 = mem_hook() ? mem_hook()() : 0;
    d->kept[id] = {scr, entry_of(id), false, m0 > m1 ? (uint32_t) (m0 - m1) : 0u, (uint32_t) objects(scr), lv_tick_elaps(t0)};
    d->pre_done = lv_tick_get();
    if (!kept_all_at() && (int) d->kept.size() >= std::min<int>(keep_max(), (int) want.size())) kept_all_at() = d->pre_done;
    return;  // one per tick
  }
}

// Navigation: a hierarchy, Home (the stack's root) at the bottom, never a long back
// chain. Back = up one level (Home > Weather > 5 days), hold Back = straight Home ("home" event).
//   pages       a page (a deck page id or page kind, from a shortcut or a page card) and a sent screen (spec::sent:
//               an agent's / the AI's) sit one level above Home: opening one cuts the stack to the root first, so a
//               sent screen replaces the previous sent screen and its Back = Home. A page opened from a second
//               Home page (Kitchen's header Timer) sits above that Home page: Kitchen > Timer, Back = Kitchen.
//   the rest    a card's detail ("light?{...}"), Settings, a sub-view: one level above where it was opened from.
//   cap         Home + 2 (MAX_DEPTH 3): at the cap the deepest screen is replaced (Back = still up one level). The
//               Settings tree (built-in "system" screens: Settings > Screens > one screen's set) is exempt.
//   loops       a screen already on the stack is gone back to (the stack is cut after it).
// A deck page id becomes what shows it: a Home = "home" (the start page) or "home?{"page":"<id>"}", another page =
// its kind. Sending a spec never opens it (core/spec.h apply -> reload): only a show (/fw/show, facet_show) does.
constexpr size_t MAX_DEPTH = 3;
std::string target_of(const std::string &id, bool &page) {
  const std::string base = base_of(id);
  page = false;
  ArduinoJson::JsonDocument deck;
  if (id.find('?') == std::string::npos && !deserializeJson(deck, spec::deck_json())) {
    const std::string start = deck["start"] | "";
    for (ArduinoJson::JsonVariantConst pg : deck["pages"].as<ArduinoJson::JsonArrayConst>()) {
      if (base != (pg["id"] | "")) continue;
      page = true;
      const std::string kind = pg["kind"] | "";
      if (kind != "home") return kind;
      return base == start ? std::string("home") : "home?{\"page\":\"" + base + "\"}";
    }
  }
  if (base == "home") { page = true; return id; }
  if (id.find('?') == std::string::npos && spec::page_role(base) && find(base.c_str()) == nullptr) page = true;
  return id;
}
// Idle (device.h "idle"): the host's dim hook, else the "idle" event (the workbench dims its canvas)
void (*&idle_hook())(bool, int) { static void (*f)(bool, int) = nullptr; return f; }
void idle_dim(Device *d, bool on) {
  d->idled = on;
  const char *v = setting("idleBrightness");
  const int pct = on ? (v ? atoi(v) : 70) : 100;
  if (idle_hook()) idle_hook()(on, pct);
  emit(d->root, "idle", on ? std::to_string(pct).c_str() : "");
}
void queue(lv_obj_t *in, Op op, const char *id);
void idle_tick(lv_timer_t *t) {
  auto *d = (Device *) lv_timer_get_user_data(t);
  if (d->stack.empty() || d->stack[0] != "home" || d->op != NONE || d->restyle || !known("idle")) return;
  const std::string top = base_of(d->stack.back());
  if (top == "idle" || top == "setup" || top == "ha_link" || top == "report" || pointer_down()) return;
  const char *v = setting("idleAfter");
  const uint32_t ms = (uint32_t) (v ? atol(v) : 0) * 1000u;
  if (!ms) return;
  for (const TimerItem &x : timers::all()) if (x.state == 2) return;  // a timer ringing stays in view
  if (lv_display_get_inactive_time(lv_obj_get_display(d->root)) < ms || lv_tick_elaps(d->shown_at) < ms) return;
  queue(d->root, IDLE, nullptr);
}
void run(lv_timer_t *t) {
  auto *d = (Device *) lv_timer_get_user_data(t);
  if (nav().on && nav().stage == 1) nav().cur.run = lv_tick_get();
  d->timer = nullptr;  // one-shot: LVGL deletes it after this call
  const Op op = d->op;
  d->op = NONE;
  std::string id;
  bool home_dir = false;
  if (op == GO) {
    bool page = false;
    const std::string tgt = target_of(d->target, page);
    if (!known(tgt) || (!d->stack.empty() && d->stack.back() == tgt)) return;
    auto at = std::find(d->stack.begin(), d->stack.end(), tgt);
    home_dir = at != d->stack.end();  // already on the stack: back to it
    if (home_dir) d->stack.erase(at + 1, d->stack.end());
    else {
      // one level above Home; a second Home page (Kitchen) is the parent of its own header shortcuts / pages:
      // Kitchen > Timer > Back = Kitchen
      const bool sub_home = d->stack.size() == 2 && d->stack.back().rfind("home?", 0) == 0 && tgt.rfind("home", 0) != 0;
      if (spec::sent(base_of(tgt))) d->stack.resize(1);
      else if (page && !sub_home) d->stack.resize(1);
      else if (d->stack.size() >= MAX_DEPTH && spec::role_of(base_of(tgt)) != "system") d->stack.pop_back();  // the cap: the deepest is replaced
      d->stack.push_back(tgt);
    }
  } else if (op == BACK) {
    if (d->stack.size() < 2) return;
    d->stack.pop_back();
    home_dir = true;
  } else if (op == HOME) {
    if (d->stack.size() < 2) return;
    d->stack.resize(1);
    home_dir = true;
  } else if (op == IDLE) {
    if (d->stack.back() == "idle") return;
    d->stack.resize(1);
    d->stack.push_back("idle");
  } else return;
  const Theme *was = T;
  if (d->theme) T = d->theme;
  std::vector<lv_obj_t *> items;
  if (nav().on && nav().stage == 1) nav().cur.build0 = lv_tick_get(), nav().cur.screen = d->stack.back();
  show(d, d->stack.back(), items);
  if (nav().on && nav().stage == 1) {  // timing: the whole display screen's layout too (what LVGL's next refresh does first)
    nav().cur.screen_layout0 = lv_tick_get();
    lv_obj_update_layout(lv_obj_get_screen(d->root));
    nav().cur.build1 = lv_tick_get(), nav().stage = 2;
  }
  const char *tr = setting("transition");
  const bool idle = d->stack.back() == "idle";
  if (idle && d->screen) lv_obj_fade_in(d->screen, 600, 0);  // the idle clock fades in, no rise
  else if (!items.empty() && !(tr && !strcmp(tr, "instant"))) rise(items.data(), (int) items.size(), home_dir);
  T = devices.size() == 1 ? d->theme : was;  // a panel (one device): fw::T stays its look for taps / fades
  emit(d->root, "screen", base_of(d->stack.back()).c_str());  // the host may mirror it (URL, HA); not a tap
  if (idle != d->idled) idle_dim(d, idle);
}
void queue(lv_obj_t *in, Op op, const char *id) {
  Device *d = of(in);
  if (!d) return;
  if (nav().on) {
    Nav &n = nav();
    n.cur = NavTime{}, n.cur.tap = lv_tick_get(), n.stage = 1;
    n.cur.touch = n.touch && lv_tick_diff(n.cur.tap, n.touch) < 1000 ? n.touch : n.cur.tap;  // its finger's lift, when one was
  }
  d->op = op;
  d->target = id ? id : "";
  if (!d->timer) {
    d->timer = lv_timer_create(run, 0, d);
    lv_timer_set_repeat_count(d->timer, 1);
  }
}
void on_emit(lv_obj_t *from, const char *name, const char *detail, void *) {
  if (!strcmp(name, "navigate")) queue(from, GO, detail);
  else if (!strcmp(name, "back")) queue(from, BACK, nullptr);
  else if (!strcmp(name, "home")) queue(from, HOME, nullptr);  // hold Back (components/back_button.cpp)
}

// ---- settings (device.h "settings")
struct Store {
  std::string v[set::N_SETTINGS];
  bool given[set::N_SETTINGS] = {};  // set since start (else the schema default; accent then follows the stage)
  std::vector<Observer> obs;
  void (*look_hook)() = nullptr;
  int day = -1;
  Store() {
    for (int i = 0; i < set::N_SETTINGS; i++) v[i] = set::SETTINGS[i].def;
  }
};
Store &store() {
  static Store s;
  return s;
}
int index_of(const char *id);
bool text_large_setting() { return store().v[index_of("textSize")] == "large"; }
int index_of(const char *id) {
  for (int i = 0; id && i < set::N_SETTINGS; i++)
    if (!strcmp(set::SETTINGS[i].id, id)) return i;
  return -1;
}
std::string num(double v) {
  char b[24];
  snprintf(b, sizeof b, "%g", v);
  return b;
}
bool same_word(const char *a, const char *b) {
  for (; *a && *b; a++, b++)
    if (tolower((unsigned char) *a) != tolower((unsigned char) *b)) return false;
  return !*a && !*b;
}
// value -> the stored form, or "" = not valid for s
std::string normal(const set::Setting &s, const char *value) {
  if (!value) return "";
  if (s.kind == set::SELECT) {
    for (int i = 0; i < s.n_options; i++)
      if (same_word(s.options[i].id, value) || same_word(s.options[i].label, value)) return s.options[i].id;
    return "";
  }
  if (s.kind == set::SWITCH) {
    if (same_word(value, "true") || same_word(value, "on") || !strcmp(value, "1")) return "true";
    if (same_word(value, "false") || same_word(value, "off") || !strcmp(value, "0")) return "false";
    return "";
  }
  char *end = nullptr;
  const double x = strtod(value, &end);
  if (end == value || !std::isfinite(x)) return "";
  const double st = s.step > 0 ? s.step : 1;
  return num(LV_CLAMP(s.min, s.min + std::lround((x - s.min) / st) * st, s.max));
}
// the Settings screen's props: every value (its update path moves the controls; the sheet stays as it is)
void settings_props(ArduinoJson::JsonDocument &d) {
  const Store &st = store();
  for (int i = 0; i < set::N_SETTINGS; i++) {
    const set::Setting &s = set::SETTINGS[i];
    if (s.kind == set::SWITCH) d[s.id] = st.v[i] == "true";
    else if (s.kind == set::NUMBER && s.control == set::SLIDER) d[s.id] = atof(st.v[i].c_str());
    else d[s.id] = st.v[i];
  }
}
// restyle: the current screen built again with d->theme (no rise), its scrolled column kept where it was. Waits for
// the running animations (the Segmented / swatch slide that made the change), at most restyle_wait ms.
constexpr uint32_t restyle_wait = 600;
void restyle_now(Device *d) {
  if (d->stack.empty()) return;
  if (d->reloaded.empty()) for (auto &k : d->kept) k.second.stale = true;  // a settings / look change: every kept page
  else {  // a spec / deck change: only the pages it touched (reload() marked them); the shown one may be untouched
    auto it = d->kept.find(d->stack.back());
    if (it != d->kept.end() && it->second.scr == d->screen && !it->second.stale) {
      emit(d->root, "spec-applied", d->reloaded.c_str());
      d->reloaded.clear();
      return;
    }
  }
  lv_obj_t *root = d->screen && lv_obj_get_child_count(d->screen) ? lv_obj_get_child(d->screen, 0) : nullptr;
  int32_t idx = -1, y = 0;
  for (uint32_t i = 0; root && i < lv_obj_get_child_count(root); i++) {
    lv_obj_t *k = lv_obj_get_child(root, (int32_t) i);
    if (lv_obj_has_flag(k, LV_OBJ_FLAG_SCROLLABLE)) { idx = (int32_t) i; y = lv_obj_get_scroll_y(k); break; }
  }
  const Theme *was = T;
  T = d->theme;
  std::vector<lv_obj_t *> items;
  if (nav().on && nav().stage == 1) nav().cur.build0 = lv_tick_get(), nav().cur.screen = d->stack.back();
  show(d, d->stack.back(), items);
  if (nav().on && nav().stage == 1) nav().cur.build1 = lv_tick_get(), nav().stage = 2;
  T = devices.size() == 1 ? d->theme : was;  // a panel (one device): fw::T stays its look for taps / fades
  root = d->screen && lv_obj_get_child_count(d->screen) ? lv_obj_get_child(d->screen, 0) : nullptr;
  if (root && idx >= 0 && idx < (int32_t) lv_obj_get_child_count(root) && y > 0) {
    lv_obj_t *k = lv_obj_get_child(root, idx);
    lv_obj_update_layout(root);
    lv_obj_scroll_to_y(k, y, LV_ANIM_OFF);
  }
  emit(d->root, "restyle", d->theme->light ? "light" : "dark");
  if (!d->reloaded.empty()) { emit(d->root, "spec-applied", d->reloaded.c_str()); d->reloaded.clear(); }  // the host may say so
}
void restyle_tick(lv_timer_t *t) {
  auto *d = (Device *) lv_timer_get_user_data(t);
  if (lv_anim_count_running() > 0 && lv_tick_elaps(d->restyle_at) < restyle_wait) return;
  lv_timer_delete(t);
  d->restyle = nullptr;
  restyle_now(d);
}
// every device after a change: its theme, the Settings screen's controls (a change from outside the screen)
void apply(int k, From from) {
  const char *id = set::SETTINGS[k].id;
  const bool text_ = !strcmp(id, "textSize");  // Large text: other fonts, every device builds its screen again
  if (text_) set_text_large_default(store().v[k] == "large");
  // look (theme engine T4, core/theme_slot.h): the theme file live first (refused / missing = Facet, said in the
  // status), then every device rebuilt in it (legacy parts keep Facet's palette, contract parts follow the Skin)
  const bool file_ = !strcmp(id, "look");
  if (file_) {
    std::string why;
    if (!fx::theme_select(store().v[k], why)) fprintf(stderr, "facet look %s: %s\n", store().v[k].c_str(), why.c_str());
  }
  const bool look_ = !strcmp(id, "theme") || !strcmp(id, "accent") || text_ || file_;
  if (look_ && store().look_hook) store().look_hook();
  for (Device *d : devices) {
    if (text_) {
      for (auto &k : d->kept) k.second.stale = true;  // at once: a tap before the restyle builds, never shows the old page
      if (!d->restyle) d->restyle = lv_timer_create(restyle_tick, 30, d);
      d->restyle_at = lv_tick_get();
      continue;
    }
    if (look_) {
      const Theme *t = look(d->stage);
      if (t != d->theme || file_) {
        d->theme = t;
        if (devices.size() == 1) T = t;  // taps / fades paint from fw::T after the build: the panel's look now
        for (auto &k : d->kept) k.second.stale = true;  // at once (as above)
        if (!d->restyle) d->restyle = lv_timer_create(restyle_tick, 30, d);
        d->restyle_at = lv_tick_get();
        continue;  // the rebuild shows every value anyway
      }
    }
    if (from == SCREEN || d->restyle || d->stack.empty() || d->stack.back() != "settings") continue;
    lv_obj_t *root = d->screen && lv_obj_get_child_count(d->screen) ? lv_obj_get_child(d->screen, 0) : nullptr;
    ArduinoJson::JsonDocument p;
    settings_props(p);
    const Theme *was = T;
    T = d->theme;
    {
      Quiet q;
      update_child(find("settings"), root, p.as<Props>());
    }
    T = devices.size() == 1 ? d->theme : was;  // a panel (one device): fw::T stays its look for taps / fades
  }
}
}  // namespace

void toast_put(const std::string &e, const std::string &a, const std::string &v) {
  if (e != TOAST_PAIR || !a.empty() || v.empty()) return;
  ArduinoJson::JsonDocument t;
  if (deserializeJson(t, v) || !t.is<ArduinoJson::JsonObject>()) return;
  // the HA action's words (facet_toast: kind, duration in ms) -> the Toast's (tone, life): duration >= 10 s = urgent
  if (t["tone"].isNull() && !t["kind"].isNull()) t["tone"] = t["kind"].as<const char *>();
  if (t["life"].isNull() && !t["duration"].isNull()) t["life"] = t["duration"].as<float>() >= 10000 ? "urgent" : "normal";
  toast(t.as<Props>());
}
lv_obj_t *mount(lv_obj_t *parent, const char *start) {
  spec::reload_hook() = reload;  // a sent spec / deck rebuilds what shows it
  spec::kept_hook() = kept_changed;  // a quiet deck change (Settings > Screens): the kept pages it touched go stale
  spec::want_all();               // the deck's entities (idempotent; the panel also does it at boot)
  auto *d = new Device;
  d->stage = T;
  d->theme = look(T);
  const Theme *was = T;
  T = d->theme;
  d->root = box(parent);
  lv_obj_set_size(d->root, lay::screen_w, lay::screen_h);
  devices.push_back(d);
  lv_obj_add_event_cb(d->root, [](lv_event_t *e) {
    auto *d = (Device *) lv_event_get_user_data(e);
    if (d->timer) lv_timer_delete(d->timer);
    if (d->restyle) lv_timer_delete(d->restyle);
    if (d->pre) lv_timer_delete(d->pre);  // the kept pages' prebuild
    if (d->idle) lv_timer_delete(d->idle);
    if (d->park) lv_obj_delete(d->park);  // the parked kept pages (the shown one goes with the root)
    if (d->toasts) lv_obj_delete(d->toasts);  // its DELETE clears d->toasts
    devices.erase(std::remove(devices.begin(), devices.end(), d), devices.end());
    delete d;
  }, LV_EVENT_DELETE, d);
  listen(d->root, on_emit);
  if (keep_max() > 0) d->pre = lv_timer_create(prebuild, 250, d);  // kept pages on: this device keeps them too
  d->idle = lv_timer_create(idle_tick, 1000, d);
  // toasts arrive over whatever screen shows: a Toast layer on the display's top layer (above the screen stack and
  // its cuts); it goes with the device (or with the display, whichever is deleted first)
  static const bool hooked = (ha::on_put(toast_put), true);
  (void) hooked;
  d->toasts = toasts::layer(lv_display_get_layer_top(lv_obj_get_display(d->root)));
  lv_obj_add_event_cb(d->toasts, [](lv_event_t *e) { ((Device *) lv_event_get_user_data(e))->toasts = nullptr; }, LV_EVENT_DELETE, d);
  const char *s = start && known(start) ? start : "home";
  d->stack.push_back(s);
  std::vector<lv_obj_t *> items;
  show(d, s, items);  // the first screen cut in (no rise: the panel just shows it)
  T = devices.size() == 1 ? d->theme : was;  // a panel (one device): fw::T stays its look for taps / fades
  return d->root;
}
const Theme *theme_at(lv_obj_t *o) {
  for (Device *d : devices) if (d->root == o) return d->theme;
  return nullptr;
}
void toast(Props t) { toast(t, nullptr); }
void toast(Props t, std::function<void()> on_action) {
  const bool top = store().v[index_of("toastPosition")] != "bottom";
  for (Device *d : devices) {
    if (!d->toasts) continue;
    const Theme *was = T;
    T = d->theme;  // built in the device's look (fw::T is global)
    lv_obj_t *o = toasts::push(d->toasts, t, top);
    if (o && on_action) toasts::on_action(o, on_action);
    T = devices.size() == 1 ? d->theme : was;  // a panel (one device): fw::T stays its look for taps / fades
  }
}
// a deck change kept without rebuilding what shows (spec.h deck_set: Settings > Screens > a screen, a screen's own
// pick): a kept page whose entry changed is built again when it shows (or in the idle gaps), one gone from the deck is
// dropped; the shown page made the change itself, so it only takes the new entry. The changed page's feeds take its
// new settings at once (spec::page_feeds), so its Home card follows before the page is built again; not while that
// page shows (its own pick: the feed already has it).
void kept_changed(const char *id) {
  bool shown = false;
  for (Device *d : devices) {
    for (auto it = d->kept.begin(); it != d->kept.end();) {
      const std::string e = entry_of(it->first);
      const bool gone = it->first != "home" && !spec::deck_kinds().count(it->first);
      if (it->second.scr == d->screen) {
        it->second.entry = e;
        if (it->first == id) shown = true;
      } else if (gone) {
        lv_obj_delete(it->second.scr);
        it = d->kept.erase(it);
        continue;
      } else if (e != it->second.entry) {
        it->second.stale = true;
      }
      ++it;
    }
    if (!d->stack.empty() && base_of(d->stack.back()) == id) shown = true;
  }
  if (!shown) spec::page_feeds(id);
}
void reload(const char *id) {
  for (Device *d : devices)  // kept pages it touched go stale (a deck change: the pages whose entry changed, the gone ones)
    for (auto it = d->kept.begin(); it != d->kept.end();) {
      const bool deck = !strcmp(id, "deck");
      const bool gone = deck && it->first != "home" && !spec::deck_kinds().count(it->first);
      if (gone && it->second.scr != d->screen) { lv_obj_delete(it->second.scr); it = d->kept.erase(it); continue; }
      if (gone || (deck ? entry_of(it->first) != it->second.entry : it->first == id)) it->second.stale = true;
      ++it;
    }
  for (Device *d : devices) {
    if (d->stack.empty()) continue;
    const std::string top = base_of(d->stack.back());
    if (!strcmp(id, "deck")) {  // the deck changed: screens it no longer has leave the stack; Home shows the new cards
      while (d->stack.size() > 1 && !known(d->stack.back())) d->stack.pop_back();
    } else if (top != id) {  // a sent screen that isn't showing stays where it is (a send never takes
      continue;               // over the display; the sender shows it with /fw/show or facet_show)
    }
    d->reloaded = id;
    if (!d->restyle) d->restyle = lv_timer_create(restyle_tick, 30, d);
    d->restyle_at = lv_tick_get();
  }
}
void state(ArduinoJson::JsonArray out) {
  for (Device *d : devices) {
    ArduinoJson::JsonObject o = out.add<ArduinoJson::JsonObject>();
    ArduinoJson::JsonArray st = o["stack"].to<ArduinoJson::JsonArray>();
    for (const auto &x : d->stack) st.add(base_of(x));
    o["builds"] = d->builds;
    std::function<int(lv_obj_t *)> count = [&](lv_obj_t *x) { int n = 1; for (uint32_t i = 0; i < lv_obj_get_child_count(x); i++) n += count(lv_obj_get_child(x, i)); return n; };
    o["objects"] = d->screen ? count(d->screen) : 0;
  }
}
void go(lv_obj_t *in, const char *id) { queue(in, GO, id); }
void back(lv_obj_t *in) { queue(in, BACK, nullptr); }
void home(lv_obj_t *in) { queue(in, HOME, nullptr); }
void on_idle(void (*cb)(bool idle, int pct)) { idle_hook() = cb; }
static Update &update_state() { static Update u; return u; }
static void (*&install_hook())() { static void (*f)() = nullptr; return f; }
void update_set(const char *state, const char *installed, const char *latest) {
  Update &u = update_state();
  u.state = state ? state : "", u.installed = installed ? installed : "", u.latest = latest ? latest : "";
}
const Update &update_now() { return update_state(); }
void on_update_install(void (*cb)()) { install_hook() = cb; }
const char *update_install() {
  if (update_state().state != "available") return "no update";
  if (!install_hook()) return "this panel cannot install";
  install_hook()();
  return "";
}
bool idle() {
  for (Device *d : devices) if (d->idled) return true;
  return false;
}
bool in_device(lv_obj_t *obj) { return of(obj) != nullptr; }
void keep_pages(int n) {
  keep_max() = n < 0 ? 0 : n;
  for (Device *d : devices) {
    while ((int) d->kept.size() > keep_max()) {  // fewer: drop hidden ones (the shown one stays until it is left)
      auto it = std::find_if(d->kept.begin(), d->kept.end(), [&](auto &k) { return k.second.scr != d->screen; });
      if (it == d->kept.end()) break;
      lv_obj_delete(it->second.scr);
      d->kept.erase(it);
    }
    if (keep_max() > 0 && !d->pre) d->pre = lv_timer_create(prebuild, 250, d);
  }
}
int keep_pages() { return keep_max(); }
uint32_t kept_all() { return kept_all_at(); }
void on_mem(size_t (*f)()) { mem_hook() = f; }
void kept_json(ArduinoJson::JsonArray out) {
  for (Device *d : devices)
    for (auto &k : d->kept) {
      ArduinoJson::JsonObject o = out.add<ArduinoJson::JsonObject>();
      o["page"] = k.first, o["bytes"] = k.second.bytes, o["objects"] = k.second.objs, o["build_ms"] = k.second.ms;
      o["stale"] = k.second.stale, o["shown"] = k.second.scr == d->screen;
      int waiting = 0;  // binds whose data changed while the page was hidden (their widgets repaint when it shows)
      for (const ha::Bound *b : ha::S().bounds)
        if (b->dirty) for (lv_obj_t *x = b->obj; x; x = lv_obj_get_parent(x)) if (x == k.second.scr) { waiting++; break; }
      o["waiting"] = waiting;
    }
}
// ---- a tap during a build (device.h late_start)
namespace {
struct Late {
  void (*poll)() = nullptr;
  bool polling = false, down = false, pending = false;
  lv_point_t pt{};
  int phase = 0;  // the tap given: 1, 2 pressed, 3 released
  uint32_t every = 20, last = 0;
  lv_indev_t *indev = nullptr;
};
Late &late() { static Late l; return l; }
bool host_pressed() {  // a pointer other than ours is down (the host's own finger, the bench's)
  for (lv_indev_t *i = lv_indev_get_next(nullptr); i; i = lv_indev_get_next(i))
    if (i != late().indev && lv_indev_get_type(i) == LV_INDEV_TYPE_POINTER && lv_indev_get_state(i) == LV_INDEV_STATE_PRESSED) return true;
  return false;
}
}  // namespace
void late_start(void (*poll)()) {
  Late &l = late();
  l.poll = poll;
  if (l.indev) return;
  l.indev = lv_indev_create();
  lv_indev_set_type(l.indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(l.indev, [](lv_indev_t *, lv_indev_data_t *d) {
    Late &l = late();
    d->point = l.pt;
    d->state = l.phase == 1 || l.phase == 2 ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
    if (l.phase == 3) l.phase = 0, l.pending = false;
    else if (l.phase) l.phase++;
  });
  lv_timer_create([](lv_timer_t *) {  // between builds: LVGL saw the press itself, or the tap is due
    Late &l = late();
    if (l.down && host_pressed()) l.down = false;
    if (l.pending && !l.phase) {
      if (host_pressed()) l.pending = false;
      else {
        l.phase = 1;
        LV_LOG_WARN("a tap during a build, given after it at %d,%d", (int) l.pt.x, (int) l.pt.y);
      }
    }
  }, 10, nullptr);
}
void late_press(int32_t x, int32_t y) { Late &l = late(); if (l.polling && !l.down && !l.pending) l.down = true, l.pt = {x, y}; }
void late_lift() {
  Late &l = late();
  if (l.polling && l.down && !l.pending) l.pending = true;
  l.down = false;
}
void late_ms(uint32_t ms) { late().every = ms; }
void build_tick() {
  Late &l = late();
  if (!l.poll || l.polling || (l.every && lv_tick_elaps(l.last) < l.every)) return;
  l.last = lv_tick_get();
  l.polling = true;
  l.poll();
  l.polling = false;
}
void nav_timing(bool on) { nav().on = on; nav().stage = 0; if (on) nav().done.clear(); }
bool nav_timing() { return nav().on; }
void nav_touch() { if (nav().on) nav().touch = lv_tick_get(); }
const std::vector<NavTime> &nav_times() { return nav().done; }
void nav_feed(uint32_t ms) { if (nav().on && nav().stage == 1 && nav().cur.build0) nav().cur.feed_ms += ms, nav().cur.feeds++; }
void nav_prof(const char *what, uint32_t ms) { if (nav().on && nav().stage == 1 && nav().cur.prof.size() < 64) nav().cur.prof.push_back({what, ms}); }
void nav_copy(uint32_t ms) { if (nav().on && nav().stage == 2) nav().cur.copy = ms; }
void nav_render() { if (nav().on && nav().stage == 2 && !nav().cur.render0) nav().cur.render0 = lv_tick_get(); }
void nav_frame() {
  Nav &n = nav();
  if (!n.on || n.stage != 2) return;
  n.cur.frame = lv_tick_get();
  if (n.done.size() >= 32) n.done.erase(n.done.begin());
  n.done.push_back(n.cur);
  n.stage = 0;
}
lv_obj_t *first_root() { return devices.empty() ? nullptr : devices.front()->root; }
const char *current(lv_obj_t *in) {
  Device *d = of(in);
  return d && !d->stack.empty() ? d->stack.back().c_str() : "";
}
const char *setting(const char *id) {
  const int k = index_of(id);
  return k < 0 ? nullptr : store().v[k].c_str();
}
const char *set(const char *id, const char *value, From from, lv_obj_t *obj) {
  const int k = index_of(id);
  if (k < 0) return "no such setting";
  const set::Setting &s = set::SETTINGS[k];
  const std::string v = normal(s, value);
  if (v.empty()) return "not a valid value";
  // Allow API access (core/keys.h): only the glass, Home Assistant or the panel's own editor can turn it on,
  // never an agent's request (an agent must not be able to let itself in); a host may still turn it off
  if (from == HOST && v == "true" && !strcmp(id, "aiAgents")) return "turn on Allow API access on the panel (Settings > API keys), in its editor or in Home Assistant";
  Store &st = store();
  const bool was_given = st.given[k];
  st.given[k] = true;
  if (st.v[k] == v) {
    if (!was_given && !strcmp(id, "accent")) apply(k, from);  // the default set, now chosen: stop following the stage
    return "same";
  }
  st.v[k] = v;
  apply(k, from);
  const std::string detail = std::string(id) + "=" + v;
  if (obj) emit(obj, "setting", detail.c_str());
  else
    for (Device *d : devices) emit(d->root, "setting", detail.c_str());
  for (Observer cb : std::vector<Observer>(st.obs)) cb(id, st.v[k].c_str(), from);
  return "";
}
const char *set(const char *id, double value, From from) { return set(id, num(value).c_str(), from); }
const char *option_label(const char *id, const char *value) {
  const int k = index_of(id);
  if (k < 0 || !value) return value;
  const set::Setting &s = set::SETTINGS[k];
  for (int i = 0; i < s.n_options; i++)
    if (!strcmp(s.options[i].id, value)) return s.options[i].label;
  return value;
}
void observe(Observer cb) { store().obs.push_back(cb); }
void on_look(void (*cb)()) { store().look_hook = cb; }
void refresh_look() { const int k = index_of("look"); if (k >= 0) apply(k, HOST); }
void settings_json(std::string &out) {
  ArduinoJson::JsonDocument d;
  d["facet"] = "settings";
  d["version"] = 1;
  ArduinoJson::JsonObject v = d["values"].to<ArduinoJson::JsonObject>();
  const Store &st = store();
  for (int i = 0; i < set::N_SETTINGS; i++) {
    const set::Setting &s = set::SETTINGS[i];
    if (s.kind == set::SWITCH) v[s.id] = st.v[i] == "true";
    else if (s.kind == set::NUMBER) v[s.id] = atof(st.v[i].c_str());
    else v[s.id] = st.v[i];
  }
  out.clear();
  serializeJson(d, out);
}
std::string restore(const char *json, From from) {
  ArduinoJson::JsonDocument d;
  if (deserializeJson(d, json ? json : "")) return "not JSON";
  ArduinoJson::JsonObjectConst v = d["values"].is<ArduinoJson::JsonObjectConst>() ? d["values"].as<ArduinoJson::JsonObjectConst>() : d.as<ArduinoJson::JsonObjectConst>();
  if (v.isNull()) return "no values";
  std::string err;
  for (auto kv : v) {
    std::string val;
    if (kv.value().is<bool>()) val = kv.value().as<bool>() ? "true" : "false";
    else if (kv.value().is<double>()) val = num(kv.value().as<double>());
    else if (kv.value().is<const char *>()) val = kv.value().as<const char *>();
    if (from == HOST && !strcmp(kv.key().c_str(), "aiAgents")) continue;  // a backup never changes it (see set())
    const char *e = set(kv.key().c_str(), val.c_str(), from);
    if (*e && strcmp(e, "same") && err.empty()) err = std::string(kv.key().c_str()) + ": " + e;
  }
  return err;
}
void set_daylight(int day) {
  Store &st = store();
  if (st.day == day) return;
  st.day = day;
  apply(index_of("theme"), HOST);
}
const Theme *look(const Theme *stage) {
  const Store &st = store();
  const std::string &th = st.v[index_of("theme")];
  const bool stage_light = stage ? stage->light : false;
  const bool light = th == "light" ? true : th == "dark" ? false : st.day >= 0 ? st.day == 1 : stage_light;
  const int ka = index_of("accent");
  int accent = stage ? stage->accent_set : -1;
  if (st.given[ka])
    for (int i = 0; i < tokens::N_ACCENTS; i++)
      if (st.v[ka] == tokens::ACCENTS[i].id) accent = i;
  return theme(light, accent);
}
// ---------------------------------------------------------------- hardware button (device.h)
namespace {
struct Claim { lv_obj_t *obj; ButtonLevel level; ButtonFn fn; void *user; uint32_t seq; };
std::vector<Claim> &claims() { static std::vector<Claim> c; return c; }
uint32_t claim_seq = 0;
bool shown(lv_obj_t *o) {
  lv_obj_t *scr = lv_obj_get_screen(o);
  if (scr != lv_screen_active() && scr != lv_layer_top()) return false;
  for (lv_obj_t *x = o; x; x = lv_obj_get_parent(x))
    if (lv_obj_has_flag(x, LV_OBJ_FLAG_HIDDEN)) return false;
  return true;
}
}  // namespace
void claim_button(lv_obj_t *obj, ButtonLevel level, ButtonFn fn, void *user) {
  if (!obj || !fn) return;
  claims().push_back({obj, level, fn, user, ++claim_seq});
  lv_obj_add_event_cb(obj, [](lv_event_t *e) {
    lv_obj_t *o = (lv_obj_t *) lv_event_get_current_target(e);
    auto &c = claims();
    for (size_t i = c.size(); i-- > 0;) if (c[i].obj == o) c.erase(c.begin() + i);
  }, LV_EVENT_DELETE, nullptr);
}
const char *button(const char *press) {
  // highest level first, newest first; a fn may delete objects (a dismissed toast): work on a copy
  std::vector<Claim> c = claims();
  std::sort(c.begin(), c.end(), [](const Claim &a, const Claim &b) { return a.level != b.level ? a.level > b.level : a.seq > b.seq; });
  for (const Claim &x : c) {
    bool live = false;
    for (const Claim &y : claims()) if (y.seq == x.seq) { live = true; break; }
    if (!live || !lv_obj_is_valid(x.obj) || !shown(x.obj)) continue;
    if (x.fn(x.user, press)) return x.level == BUTTON_ALERT ? "alert" : x.level == BUTTON_SHEET ? "sheet" : "screen";
  }
  return "";
}
}  // namespace dev
}  // namespace fw
