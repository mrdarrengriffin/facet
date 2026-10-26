// Device runtime: core/device.h (screen stack, navigation, the screen transition). Same file on the panels and in WASM.
#include <cmath>
#include <functional>
#include "device.h"
#include <algorithm>
#include <vector>
#include "spec.h"
#include "toasts.h"
#include "settings_gen.h"

namespace fw {
namespace dev {
namespace {
enum Op : uint8_t { NONE, GO, BACK, HOME };
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
struct LargeScope {
  bool was;
  explicit LargeScope(bool on) : was(text_large()) { set_text_large(on); }
  ~LargeScope() { set_text_large(was); }
};
bool show(Device *d, const std::string &id, std::vector<lv_obj_t *> &items) {
  LargeScope large(text_large_setting());
  struct RootScope {  // the stack's root screen is built without Back (spec::at_root)
    bool was;
    explicit RootScope(bool on) : was(spec::at_root) { spec::at_root = on; }
    ~RootScope() { spec::at_root = was; }
  } root(d->stack.size() <= 1);
  const Component *c = find(base_of(id).c_str());
  const bool from_spec = !c && spec::screen_json(base_of(id));  // a screen from its spec (core/spec.h)
  if (!c && !from_spec) return false;
  ArduinoJson::JsonDocument none;
  const size_t q = id.find('?');
  if (q == std::string::npos || deserializeJson(none, id.c_str() + q + 1) || !none.is<ArduinoJson::JsonObject>()) none.to<ArduinoJson::JsonObject>();
  if (d->screen) lv_obj_delete(d->screen);
  d->builds++;
  d->screen = box(d->root);
  lv_obj_set_size(d->screen, lay::screen_w, lay::screen_h);
  lv_obj_set_style_bg_color(d->screen, T->bg, 0);
  lv_obj_set_style_bg_opa(d->screen, LV_OPA_COVER, 0);
  if (from_spec || !strcmp(c->section, "screens")) {
    lv_obj_t *s = from_spec ? spec::build(d->screen, base_of(id), none.as<Props>()) : build_into(d->screen, c, none.as<Props>());
    if (!s) return true;
    lv_obj_update_layout(d->screen);
    // header = the screen's first child; blocks = the rest, a scrolling content column = its children on screen
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
    return true;
  }
  // a page that is not a screen (a foundation): the Header (back + its name) and the page scrolling under it
  ArduinoJson::JsonDocument hd;
  hd["label"] = c->name;
  lv_obj_t *h = build_into(d->screen, find("header"), hd.as<Props>());
  if (h) lv_obj_set_pos(h, lay::inset, lay::inset);
  lv_obj_t *sc = box(d->screen);
  lv_obj_add_flag(sc, (lv_obj_flag_t) (LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE));  // clickable: a drag on plain text scrolls too
  lv_obj_set_scroll_dir(sc, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(sc, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_pos(sc, 0, lay::top);
  lv_obj_set_size(sc, lay::screen_w, lay::screen_h - lay::top);
  lv_obj_set_flex_flow(sc, LV_FLEX_FLOW_COLUMN);
  // a content-width view (Weather 5 days) sits centred, inset to inset (facet-185: it hugged the left edge)
  lv_obj_set_flex_align(sc, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  build_into(sc, c, none.as<Props>());
  if (h) items.push_back(h);
  items.push_back(sc);
  return true;
}

// Navigation (facet-188, Darren 2026-10-26): a hierarchy, Home (the stack's root) at the bottom, never a long back
// chain. Back = up one level (Home > Weather > Weather view), hold Back = straight Home ("home" event).
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
void run(lv_timer_t *t) {
  auto *d = (Device *) lv_timer_get_user_data(t);
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
      // Kitchen > Timer > Back = Kitchen (facet-188)
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
  } else return;
  const Theme *was = T;
  if (d->theme) T = d->theme;
  std::vector<lv_obj_t *> items;
  show(d, d->stack.back(), items);
  const char *tr = setting("transition");
  if (!items.empty() && !(tr && !strcmp(tr, "instant"))) rise(items.data(), (int) items.size(), home_dir);
  T = devices.size() == 1 ? d->theme : was;  // a panel (one device): fw::T stays its look for taps / fades
  emit(d->root, "screen", base_of(d->stack.back()).c_str());  // the host may mirror it (URL, HA); not a tap
}
void queue(lv_obj_t *in, Op op, const char *id) {
  Device *d = of(in);
  if (!d) return;
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
  lv_obj_t *root = d->screen && lv_obj_get_child_count(d->screen) ? lv_obj_get_child(d->screen, 0) : nullptr;
  int32_t idx = -1, y = 0;
  for (uint32_t i = 0; root && i < lv_obj_get_child_count(root); i++) {
    lv_obj_t *k = lv_obj_get_child(root, (int32_t) i);
    if (lv_obj_has_flag(k, LV_OBJ_FLAG_SCROLLABLE)) { idx = (int32_t) i; y = lv_obj_get_scroll_y(k); break; }
  }
  const Theme *was = T;
  T = d->theme;
  std::vector<lv_obj_t *> items;
  show(d, d->stack.back(), items);
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
  const bool look_ = !strcmp(id, "theme") || !strcmp(id, "accent") || text_;
  if (look_ && store().look_hook) store().look_hook();
  for (Device *d : devices) {
    if (text_) {
      if (!d->restyle) d->restyle = lv_timer_create(restyle_tick, 30, d);
      d->restyle_at = lv_tick_get();
      continue;
    }
    if (look_) {
      const Theme *t = look(d->stage);
      if (t != d->theme) {
        d->theme = t;
        if (devices.size() == 1) T = t;  // taps / fades paint from fw::T after the build: the panel's look now
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
    if (d->toasts) lv_obj_delete(d->toasts);  // its DELETE clears d->toasts
    devices.erase(std::remove(devices.begin(), devices.end(), d), devices.end());
    delete d;
  }, LV_EVENT_DELETE, d);
  listen(d->root, on_emit);
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
void toast(Props t) {
  const bool top = store().v[index_of("toastPosition")] != "bottom";
  for (Device *d : devices) {
    if (!d->toasts) continue;
    const Theme *was = T;
    T = d->theme;  // built in the device's look (fw::T is global)
    toasts::push(d->toasts, t, top);
    T = devices.size() == 1 ? d->theme : was;  // a panel (one device): fw::T stays its look for taps / fades
  }
}
void reload(const char *id) {
  for (Device *d : devices) {
    if (d->stack.empty()) continue;
    const std::string top = base_of(d->stack.back());
    if (!strcmp(id, "deck")) {  // the deck changed: screens it no longer has leave the stack; Home shows the new cards
      while (d->stack.size() > 1 && !known(d->stack.back())) d->stack.pop_back();
    } else if (top != id) {  // a sent screen that isn't showing stays where it is (facet-188: a send never takes
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
Timer &timer() {
  static Timer t;
  return t;
}
bool in_device(lv_obj_t *obj) { return of(obj) != nullptr; }
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
  // Allow AI agents (facet-163): only the panel itself or Home Assistant can turn it on, never an HTTP request (an
  // agent must not be able to let itself in); a host may still turn it off
  if (from == HOST && v == "true" && !strcmp(id, "aiAgents")) return "turn Allow AI agents on on the panel (Settings) or in Home Assistant";
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
