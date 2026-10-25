// Screen chrome: the top-right corner every screen shares (Layout rule "The clock always top right"; docs header clock
// + Home's settingsButton()). Darren 2026-10-22: the clock on every screen, the settings button on Home only for now
// (it may move later). The Header component ends its row with it, so a screen gets it by placing its Header at
// (inset, inset) and never places it itself.
//   box      the time only: lay::header_time_w x lay::row; with the button: lay::chrome_w (136 x 48 / 96 x 32; derived
//            in layout.json = chrome-btn + gutter + header-time-w); flex row, a gutter apart, ending on the content edge
//   settings prop settings none (default on screens via the Header) | button (Home) | here (an option: the active
//            border, line-hi, the icon keeps its colours; unused). The Icon button (fw::find("icon_button"), internal
//            role "settings"): a lay::chrome_btn square (= the header row, touch min), the screen-settings icon at
//            ic::icon (24 / 16, centred: offset 12 / 8, on the grid), resting look as Darren's pick (surface square,
//            line border, ink lines, accent knob); pressed = the press tint; tap = event "navigate" (detail settings)
//            + sound open (the host ignores a navigate to the screen it shows)
//   time     screen type label (caps, mono), text-dim, lay::header_time_w wide, right-aligned on the content edge.
//            Prop time = a fixed text (docs, tests, the Header playground); empty = the live clock (localtime, HH:MM,
//            checked every second, redrawn only when the minute changes; "--:--" until the device has the time)
//   header actions (Darren 2026-10-25, per Home; screen model §10): internal props shortcut (a screen kind: the button
//            holds that screen's icon and opens page `to` instead of Settings; Settings is then a long press on the
//            time) and status timer (the panel's timer, time left in accent-text, in a fixed header-time-w box before
//            the time; empty while none runs). The chrome grows by a gutter + header-time-w for it.
// Contrast pairs: tokens/colour.json section "screen_chrome" (+ icon_button).
#include <ctime>
#include "fw.h"
#include "device.h"

static_assert(fw::lay::chrome_btn == fw::lay::row, "the settings button is a square in the header row");
static_assert(fw::lay::chrome_w == fw::lay::chrome_btn + fw::lay::gutter + fw::lay::header_time_w, "chrome width");

namespace {
struct Clock { lv_timer_t *timer = nullptr; int minute = -1; };

lv_obj_t *build(const char *id, lv_obj_t *parent, ArduinoJson::JsonDocument &props) {
  const fw::Component *c = fw::find(id);
  if (!c) return nullptr;
  const uint32_t before = lv_obj_get_child_count(parent);
  c->build(parent, props.as<fw::Props>());
  return lv_obj_get_child_count(parent) > before ? lv_obj_get_child(parent, -1) : nullptr;
}
bool has_shortcut(fw::Props p) { return *(p["shortcut"] | ""); }
bool has_status(fw::Props p) { return !strcmp(p["status"] | "none", "timer"); }
void button_props(fw::Props p, ArduinoJson::JsonDocument &bp) {
  bp["style"] = "square";
  bp["role"] = has_shortcut(p) ? "shortcut" : "settings";
  if (has_shortcut(p)) { bp["icon"] = std::string("screen-") + (p["shortcut"] | ""); bp["to"] = p["to"] | ""; }
  bp["state"] = !strcmp(p["settings"] | "button", "here") ? "active" : "default";
}
// the live clock: HH:MM from localtime, "--:--" before the device has the time (year < 2020)
void tick(lv_timer_t *t) {
  auto *l = (lv_obj_t *) lv_timer_get_user_data(t);
  auto *c = (Clock *) lv_obj_get_user_data(l);
  const time_t now = time(nullptr);
  struct tm tm {};
  localtime_r(&now, &tm);
  const bool set = tm.tm_year + 1900 >= 2020;
  const int m = set ? tm.tm_hour * 60 + tm.tm_min : -2;
  if (!c || m == c->minute) return;
  c->minute = m;
  char b[8];
  if (set) snprintf(b, sizeof b, "%02d:%02d", tm.tm_hour, tm.tm_min);
  else snprintf(b, sizeof b, "--:--");
  fw::set_text(l, fw::ty::label, b);
}
// the time slot: fixed text, or the live clock (a timer owned by the label, freed with it)
void time_text(lv_obj_t *l, fw::Props p) {
  auto *c = (Clock *) lv_obj_get_user_data(l);
  const char *fixed = p["time"] | "";
  if (*fixed) {
    if (c && c->timer) { lv_timer_delete(c->timer); c->timer = nullptr; }
    if (c) c->minute = -1;
    fw::set_text(l, fw::ty::label, fixed);
    return;
  }
  if (!c) return;
  if (!c->timer) c->timer = lv_timer_create(tick, 1000, l);
  c->minute = -1;
  tick(c->timer);
}
bool has_button(fw::Props p) { return strcmp(p["settings"] | "none", "none") || has_shortcut(p); }
std::string sig(fw::Props p) { return std::string(has_button(p) ? "b" : "") + (has_status(p) ? "s" : "") + (p["shortcut"] | "") + "|" + (p["to"] | ""); }
// the status beside the time: the panel's timer, time left ("4:32", "1:02:05"), "" when none runs (the box stays)
void status_tick(lv_timer_t *tm) {
  auto *l = (lv_obj_t *) lv_timer_get_user_data(tm);
  const fw::dev::Timer &g = fw::dev::timer();
  const long long left = g.state == 1 ? g.ends - (long long) time(nullptr) : 0;
  char b[16] = "";
  if (left > 0) {
    const int x = (int) left;
    if (x >= 3600) snprintf(b, sizeof b, "%d:%02d:%02d", x / 3600, x / 60 % 60, x % 60);
    else snprintf(b, sizeof b, "%d:%02d", x / 60, x % 60);
  }
  if (strcmp(lv_label_get_text(l), b)) fw::set_text(l, fw::ty::label, b);
}
void fill(lv_obj_t *r, fw::Props p) {
  lv_obj_remove_style_all(r);
  lv_obj_remove_flag(r, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  fw::shape(r, sig(p));
  const int w = fw::lay::header_time_w + (has_button(p) ? fw::lay::chrome_btn + fw::lay::gutter : 0) + (has_status(p) ? fw::lay::header_time_w + fw::lay::gutter : 0);
  lv_obj_set_size(r, w, fw::lay::row);
  lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(r, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(r, fw::lay::gutter, 0);

  if (has_button(p)) {
    ArduinoJson::JsonDocument bp;
    button_props(p, bp);
    build("icon_button", r, bp);
  }

  if (has_status(p)) {  // a fixed box: the text changes in place, nothing moves
    lv_obj_t *st = fw::text(r, fw::ty::label, "", fw::T->accent_text);
    lv_obj_remove_flag(st, LV_OBJ_FLAG_CLICKABLE);
    lv_label_set_long_mode(st, LV_LABEL_LONG_MODE_CLIP);
    lv_obj_set_size(st, fw::lay::header_time_w, fw::style(fw::ty::label).box);
    lv_obj_set_style_text_align(st, LV_TEXT_ALIGN_RIGHT, 0);
    lv_timer_t *tm = lv_timer_create(status_tick, 1000, st);
    status_tick(tm);
    lv_obj_add_event_cb(st, [](lv_event_t *e) { lv_timer_delete((lv_timer_t *) lv_event_get_user_data(e)); }, LV_EVENT_DELETE, tm);
  }
  lv_obj_t *l = fw::text(r, fw::ty::label, "", fw::T->text_dim);
  lv_obj_remove_flag(l, LV_OBJ_FLAG_CLICKABLE);
  if (has_shortcut(p)) {  // the settings square holds a shortcut: Settings is a long press on the time
    lv_obj_add_flag(l, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(l, (fw::lay::row - fw::style(fw::ty::label).box) / 2);
    lv_obj_add_event_cb(l, [](lv_event_t *e) {
      auto *o = (lv_obj_t *) lv_event_get_current_target(e);
      if (lv_indev_t *in = lv_indev_active()) lv_indev_wait_release(in);
      fw::emit(o, "navigate", "settings");
      fw::sound(o, "open");
    }, LV_EVENT_LONG_PRESSED, nullptr);
  }
  lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_CLIP);
  lv_obj_set_size(l, fw::lay::header_time_w, fw::style(fw::ty::label).box);
  lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_RIGHT, 0);
  auto *c = new Clock;
  lv_obj_set_user_data(l, c);
  lv_obj_add_event_cb(l, [](lv_event_t *e) {
    auto *c = (Clock *) lv_event_get_user_data(e);
    if (c->timer) lv_timer_delete(c->timer);
    delete c;
  }, LV_EVENT_DELETE, c);
  time_text(l, p);
}
}  // namespace

FW_COMPONENT(screen_chrome, "Screen chrome", "components", R"({
  "about": "The top-right corner of every screen: the time, with the settings button to its left on Home. The Header ends with it.",
  "frame": [240, 80],
  "icons": ["screen-settings@icon", "screen-timer@icon", "screen-media@icon", "screen-weather@icon", "screen-calendar@icon"],
  "props": [
    {"name": "time", "type": "text", "default": "21:42"},
    {"name": "settings", "type": "select", "options": ["none", "button", "here"], "default": "button"},
    {"name": "shortcut", "type": "select", "options": ["", "timer", "media", "weather", "calendar"], "default": ""},
    {"name": "to", "type": "text", "default": "timer"},
    {"name": "status", "type": "select", "options": ["none", "timer"], "default": "none"}
  ],
  "variants": [
    {"label": "Home · settings button", "props": {}},
    {"label": "other screens · time only", "props": {"settings": "none"}},
    {"label": "on Settings · here", "props": {"settings": "here"}},
    {"label": "live clock", "props": {"time": ""}},
    {"label": "header actions · timer shortcut + time left", "props": {"shortcut": "timer", "status": "timer"}}
  ]
})") {
  fill(lv_obj_create(parent), p);
}

// Update path: the time text in place (fixed <-> live clock too); button <-> here = the Icon button's active look
// fades in place; the button / shortcut / status on / off = the contents again, faded in. Silent.
FW_UPDATE(screen_chrome) {
  if (!fw::shape(root, sig(p))) {  // a part arriving / leaving (button, shortcut, status): the contents fade in, like a tap
    fw::refill(root);
    fill(root, p);
    fw::st::fade_in_children(root);
    return true;
  }
  const bool btn = has_button(p);
  lv_obj_t *b = btn ? lv_obj_get_child(root, 0) : nullptr, *l = lv_obj_get_child(root, -1);
  if (!l || (btn && !b)) return false;
  if (b) {
    ArduinoJson::JsonDocument bp;
    button_props(p, bp);
    fw::update_child(fw::find("icon_button"), b, bp.as<fw::Props>());
  }
  time_text(l, p);
  return true;
}
