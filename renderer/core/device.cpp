// Device runtime: core/device.h (screen stack, navigation, the screen transition). Same file on the panels and in WASM.
#include "device.h"

namespace fw {
namespace dev {
namespace {
enum Op : uint8_t { NONE, GO, BACK, HOME };
struct Device {
  lv_obj_t *root = nullptr, *screen = nullptr;  // screen = the root's one child (rebuilt on every cut)
  const Theme *theme = nullptr;                 // the theme the device was mounted with (fw::T is global)
  std::vector<std::string> stack;
  Op op = NONE;
  std::string target;
  lv_timer_t *timer = nullptr;
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
bool show(Device *d, const std::string &id, std::vector<lv_obj_t *> &items) {
  const Component *c = find(id.c_str());
  if (!c) return false;
  ArduinoJson::JsonDocument none;
  none.to<ArduinoJson::JsonObject>();
  if (d->screen) lv_obj_delete(d->screen);
  d->screen = box(d->root);
  lv_obj_set_size(d->screen, lay::screen_w, lay::screen_h);
  lv_obj_set_style_bg_color(d->screen, T->bg, 0);
  lv_obj_set_style_bg_opa(d->screen, LV_OPA_COVER, 0);
  if (!strcmp(c->section, "screens")) {
    lv_obj_t *s = build_into(d->screen, c, none.as<Props>());
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
  lv_obj_add_flag(sc, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(sc, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(sc, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_pos(sc, 0, lay::top);
  lv_obj_set_size(sc, lay::screen_w, lay::screen_h - lay::top);
  lv_obj_set_flex_flow(sc, LV_FLEX_FLOW_COLUMN);
  build_into(sc, c, none.as<Props>());
  if (h) items.push_back(h);
  items.push_back(sc);
  return true;
}

void run(lv_timer_t *t) {
  auto *d = (Device *) lv_timer_get_user_data(t);
  d->timer = nullptr;  // one-shot: LVGL deletes it after this call
  const Op op = d->op;
  d->op = NONE;
  std::string id;
  bool home_dir = false;
  if (op == GO) {
    if (!find(d->target.c_str()) || (!d->stack.empty() && d->stack.back() == d->target)) return;
    auto at = std::find(d->stack.begin(), d->stack.end(), d->target);
    home_dir = at != d->stack.end();  // already on the stack: back to it
    if (home_dir) d->stack.erase(at + 1, d->stack.end());
    else d->stack.push_back(d->target);
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
  if (!items.empty()) rise(items.data(), (int) items.size(), home_dir);
  T = was;
  emit(d->root, "screen", d->stack.back().c_str());  // the host may mirror it (URL, HA); not a tap
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
}
}  // namespace

lv_obj_t *mount(lv_obj_t *parent, const char *start) {
  auto *d = new Device;
  d->theme = T;
  d->root = box(parent);
  lv_obj_set_size(d->root, lay::screen_w, lay::screen_h);
  devices.push_back(d);
  lv_obj_add_event_cb(d->root, [](lv_event_t *e) {
    auto *d = (Device *) lv_event_get_user_data(e);
    if (d->timer) lv_timer_delete(d->timer);
    devices.erase(std::remove(devices.begin(), devices.end(), d), devices.end());
    delete d;
  }, LV_EVENT_DELETE, d);
  listen(d->root, on_emit);
  const char *s = start && find(start) ? start : "home";
  d->stack.push_back(s);
  std::vector<lv_obj_t *> items;
  show(d, s, items);  // the first screen cut in (no rise: the panel just shows it)
  return d->root;
}
void go(lv_obj_t *in, const char *id) { queue(in, GO, id); }
void back(lv_obj_t *in) { queue(in, BACK, nullptr); }
void home(lv_obj_t *in) { queue(in, HOME, nullptr); }
const char *current(lv_obj_t *in) {
  Device *d = of(in);
  return d && !d->stack.empty() ? d->stack.back().c_str() : "";
}
}  // namespace dev
}  // namespace fw
