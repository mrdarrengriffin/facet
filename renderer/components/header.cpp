// Header: the row at the top of every screen, on the contract (renderer/CONTRACT.md C1: Screen > Page > Header slot +
// Body). Structure (Darren 2026-10-27, Header round 3):
//   [Back] [Title] .......... [END slot] [Time]
//   Back   fixed, built by the Header: the Back button component (an Icon button, back icon), shown only when the
//          screen has somewhere to go back to (prop back; a spec cannot set it: the renderer owns Back). A back-gap
//          (layout:back-gap) to the title; no back = no room, no gap.
//   title  a SLOT: the screen's label (caps, mono, text-dim, one line). A node a spec puts in slot "title" REPLACES the
//          label (a Segmented, a picker) and HUGS its natural width, left-aligned after Back. The title takes the room
//          the end slot leaves: the label ellipsises and never pushes an end item off the row (flex grow from width
//          0); a title node too wide for the row is overfull and reported by the bounds proof (C11).
//   end    a SLOT: up to 4 items in spec order, Icon buttons, Status chips or a Segmented (facet-205; the header's own
//          settings / shortcut / timer items count). Code (C++ screens, Home) is not capped: an overfull row = the
//          bounds proof reports it. "in": "header" with no slot = end. Home's settings button
//          and a Home's header actions are END items too (props settings + actions, mapped here: shortcut square,
//          settings square, the running timer's time left), after the spec's items.
//   Time   fixed, always last: the Time component, size "header" (prop clock off = none; prop time = a fixed preview
//          time, empty = the live clock). With a shortcut, a long press on the time opens Settings too.
// There is NO start slot (Darren 2026-10-27; a spec naming slot "start" is refused by core/spec.h). Each slot is declared
// ONCE, in the meta "slots" (name, accepts, max: CONTRACT.md C8a; core/spec.h enforces it at spec load, the editor reads
// it); SLOTS below only maps a slot name to the child that holds it (header.test.mjs keeps the two equal), so a start
// slot later is one row here + one declaration.
// Fit: the row is layout:row tall (>= touch:min, tools/gen_layout.mjs), up to layout:header-max-w wide, items centred
// on it, layout:header-item-gap apart (all through tok(): the /wasm Layout lab moves them live; C20).
// Steps: none (one row; the title is what gives way). Bleed: none. States: n/a (its children have theirs). Events:
// Back's "back" / "home", the end squares' "navigate".
// Slots for code: fw::spec::building_header() = the END slot (C++ screens place their controls there with
// header_place: before the header's own end items); fw::spec::header_slot(root, "title" | "end");
// fw::spec::header_hide_time(header or end slot) = a control takes the time's place (Media, sport).
// Update path: label and time in place (a title replaced by a node keeps it); back, clock, settings, actions or the
// preview sample = the row again (fw::refill, snap; nodes a spec placed go with it, as before). Silent.
// Contrast pairs: tokens/colour.json section "header" (+ icon_button, status_chip, clock).
#include <ctime>
#include <map>
#include "fw.h"
#include "fx.h"
#include "device.h"

namespace {
// slot name -> the header child that holds it (what each accepts and how many: the meta "slots")
struct Slot { const char *name; int child; };
constexpr Slot SLOTS[] = {{"title", 1}, {"end", 2}};
enum Child { BACK = 0, TITLE = 1, END = 2, TIME = 3, N_CHILD = 4 };

std::map<lv_obj_t *, int> own_tail;  // end slot -> how many of its last items the header itself built (settings, ...)

lv_obj_t *build(const char *id, lv_obj_t *parent, ArduinoJson::JsonDocument &props) {
  const fw::Component *c = fw::find(id);
  if (!c) return nullptr;
  const uint32_t before = lv_obj_get_child_count(parent);
  c->build(parent, props.as<fw::Props>());
  return lv_obj_get_child_count(parent) > before ? lv_obj_get_child(parent, -1) : nullptr;
}
std::string sample(fw::Props p) {
  const std::string s = p["sample"] | "none";
  for (const char *o : {"end-chip", "end-buttons", "title-segmented", "full-end"}) if (s == o) return s;
  return "none";
}
bool timer_status(fw::Props p) { return !strcmp(p["actions"]["status"] | "none", "timer"); }
std::string sig(fw::Props p) {
  return std::string((p["back"] | true) ? "b" : "") + ((p["clock"] | true) ? "c" : "") + ((p["settings"] | false) ? "s" : "") +
         (timer_status(p) ? "t" : "") + "|" + (p["actions"]["shortcut"] | "") + "|" + (p["actions"]["to"] | "") + "|" + sample(p);
}
lv_obj_t *slot(lv_obj_t *h, int i) { return lv_obj_get_child(h, i); }
void show(lv_obj_t *s, bool on) {  // a part with nothing in it takes no room (no gap either)
  if (on) lv_obj_remove_flag(s, LV_OBJ_FLAG_HIDDEN); else lv_obj_add_flag(s, LV_OBJ_FLAG_HIDDEN);
}
lv_obj_t *label_of(lv_obj_t *h) {  // the title's own label, or nullptr when a node replaced it
  lv_obj_t *t = slot(h, TITLE), *l = t ? lv_obj_get_child(t, 0) : nullptr;
  return l && lv_obj_check_type(l, &lv_label_class) ? l : nullptr;
}
void time_props(fw::Props p, ArduinoJson::JsonDocument &tp) { tp["size"] = "header"; tp["time"] = p["time"] | ""; }

// the running timer's time left ("4:32", "1:02:05") as an end item; hidden while none runs (no gap)
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
  fx::set_text(l, b, "type:label");
  show(l, *b);
}
// Home's settings button + a Home's header actions (screen model §10), mapped onto END items
int own_items(lv_obj_t *en, fw::Props p) {
  int n = 0;
  const char *sc = p["actions"]["shortcut"] | "";
  if (*sc) {
    ArduinoJson::JsonDocument q;
    q["style"] = "square"; q["role"] = "shortcut"; q["icon"] = std::string("screen-") + sc; q["to"] = p["actions"]["to"] | "";
    n += build("icon_button", en, q) != nullptr;
  }
  if (p["settings"] | false) {
    ArduinoJson::JsonDocument q;
    q["style"] = "square"; q["role"] = "settings"; q["icon"] = "settings";
    n += build("icon_button", en, q) != nullptr;
  }
  if (timer_status(p)) {
    lv_obj_t *st = fx::text(en, "", "type:label", "fg:accent-text");
    lv_timer_t *tm = lv_timer_create(status_tick, 1000, st);
    status_tick(tm);
    lv_obj_add_event_cb(st, [](lv_event_t *e) { lv_timer_delete((lv_timer_t *) lv_event_get_user_data(e)); }, LV_EVENT_DELETE, tm);
    n++;
  }
  return n;
}
// the preview-only samples: real components in the slots, to see how the row fits them (a spec places its own)
void samples(lv_obj_t *h, const std::string &s) {
  lv_obj_t *en = slot(h, END);
  auto first = [&](lv_obj_t *o) { if (o) lv_obj_move_to_index(o, (int32_t) lv_obj_get_child_count(en) - 1 - own_tail[en]); };
  if (s == "title-segmented") {
    ArduinoJson::JsonDocument q;
    lv_obj_clean(slot(h, TITLE)); q["options"] = "Rooms, Map"; q["selected"] = "rooms"; build("segmented", slot(h, TITLE), q);
  }
  if (s == "end-buttons")  // several items in the end slot (Darren 2026-10-27: any number, any component)
    for (const char *ic : {"plus", "close", "check"}) {
      ArduinoJson::JsonDocument q;
      q["style"] = "square"; q["icon"] = ic;
      first(build("icon_button", en, q));
    }
  if (s == "end-chip" || s == "full-end" || s == "end-buttons") {
    ArduinoJson::JsonDocument q;
    q["tone"] = "working"; q["text"] = s == "full-end" ? "2 working" : "working";
    first(build("status_chip", en, q));
  }
}

void fill(lv_obj_t *h, fw::Props p) {
  fx::name(h, "header");
  fw::shape(h, sig(p));
  fx::use(h, "part:header");  // the theme's header (C46): face, edges, the text ink of its title
  fx::use(h, "header-move");  // a flush header bar (C45): the page's insets inside the bar (empty in Facet)
  lv_obj_t *bk = fx::box(h, "header-back"), *ti = fx::box(h, "header-title"), *en = fx::box(h, "header-end"),
           *tm = fx::box(h, "header-time");
  for (const Slot &s : SLOTS) fx::slot(lv_obj_get_child(h, s.child), s.name);  // the workbench outlines them
  lv_obj_add_event_cb(en, [](lv_event_t *e) { own_tail.erase((lv_obj_t *) lv_event_get_current_target(e)); }, LV_EVENT_DELETE, nullptr);
  lv_obj_add_event_cb(en, [](lv_event_t *e) {  // a C++ screen adding to the end slot (building_header()) shows it
    auto *o = (lv_obj_t *) lv_event_get_current_target(e);
    if (lv_obj_get_child_count(o)) lv_obj_remove_flag(o, LV_OBJ_FLAG_HIDDEN);
  }, LV_EVENT_CHILD_CHANGED, nullptr);
  if (p["back"] | true) { ArduinoJson::JsonDocument bp; build("back_button", bk, bp); }
  fx::use(fx::text(ti, p["label"] | "", "type:label", "fg:text-dim"), "header-label");
  own_tail[en] = own_items(en, p);
  if (p["clock"] | true) {
    ArduinoJson::JsonDocument tp;
    time_props(p, tp);
    build("time", tm, tp);
    if (*(p["actions"]["shortcut"] | "")) {  // with a shortcut, a long press on the time opens Settings too
      lv_obj_add_flag(tm, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_add_event_cb(tm, [](lv_event_t *e) {
        auto *o = (lv_obj_t *) lv_event_get_current_target(e);
        if (lv_indev_t *in = lv_indev_active()) lv_indev_wait_release(in);
        fw::emit(o, "navigate", "settings");
        fw::sound(o, "open");
      }, LV_EVENT_LONG_PRESSED, nullptr);
    }
  }
  samples(h, sample(p));
  show(bk, lv_obj_get_child_count(bk)); show(en, lv_obj_get_child_count(en)); show(tm, lv_obj_get_child_count(tm));
}
lv_obj_t *header_of(lv_obj_t *o) {  // the Header row from itself or its end slot
  if (o && own_tail.count(o)) o = lv_obj_get_parent(o);
  return o && lv_obj_get_child_count(o) == N_CHILD && own_tail.count(lv_obj_get_child(o, END)) ? o : nullptr;
}
}  // namespace

namespace fw::spec {
lv_obj_t *header_slot(lv_obj_t *header, const char *name) {
  if (!header || lv_obj_get_child_count(header) != N_CHILD) return nullptr;
  for (const Slot &s : SLOTS) if (!strcmp(s.name, name)) return lv_obj_get_child(header, s.child);
  return nullptr;
}
int header_tail(lv_obj_t *end) { auto it = own_tail.find(end); return it == own_tail.end() ? 0 : it->second; }
void header_hide_time(lv_obj_t *o) {  // and shows the end slot: the control that takes the time's place goes there
  if (lv_obj_t *h = header_of(o)) {
    lv_obj_add_flag(lv_obj_get_child(h, TIME), LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(lv_obj_get_child(h, END), LV_OBJ_FLAG_HIDDEN);
  }
}
}  // namespace fw::spec

FW_COMPONENT(header, "Header", "components", R"({
  "about": "The row at the top of every screen: the back button (when there is somewhere to go back to), the screen's name, the end slot, then the time. A screen can put any component in the title slot (it replaces the name and keeps its own width) and any number of components in the end slot (Home puts its settings button there); a long name shortens with an ellipsis, so the end slot always fits.",
  "contract": 2,
  "parts": ["header"],
  "frame": [480, 80],
  "min": ["layout:ibtn-square", "layout:ibtn-square"],
  "slots": [
    {"name": "title", "about": "Replaces the screen's name; keeps its own width, just after the back button.", "accepts": ["segmented"], "max": 1},
    {"name": "end", "about": "Up to four items before the time, in order: buttons, status chips or a segmented control. The header's own settings button, shortcut and timer count as items.", "accepts": ["icon_button", "status_chip", "segmented"], "max": 4}
  ],
  "icons": ["back@glyph", "screen-settings@icon", "screen-timer@icon", "plus@glyph", "close@glyph", "check@glyph"],
  "props": [
    {"name": "label", "about": "The screen's name, in the title slot; a long one ends in an ellipsis. A component a screen puts in the title replaces it.", "type": "text", "default": "Weather"},
    {"name": "back", "about": "Shows the back button at the start, to return to the screen before.", "type": "bool", "default": true},
    {"name": "clock", "about": "Shows the time at the end of the row.", "type": "bool", "default": true},
    {"name": "time", "about": "For previews only: a fixed time to show, like 21:42. Leave empty for the real time.", "type": "text", "default": "21:42"},
    {"name": "settings", "about": "Puts the settings button in the end slot, before the time, as on Home.", "type": "bool", "default": false},
    {"name": "sample", "about": "For previews only: fills a slot with real components to see how the row fits them (a screen places its own).", "type": "select", "options": ["none", "end-chip", "end-buttons", "title-segmented", "full-end"], "default": "none"}
  ],
  "variants": [
    {"label": "back + label + time", "props": {}},
    {"label": "Home · no back, settings button", "props": {"label": "Home", "back": false, "settings": true}},
    {"label": "a status chip in the end slot", "props": {"label": "Agents", "sample": "end-chip"}},
    {"label": "several items in the end slot", "props": {"label": "Photo", "sample": "end-buttons"}},
    {"label": "a segmented control in the title slot", "props": {"label": "Vacuum", "sample": "title-segmented"}},
    {"label": "long title, full end slot", "props": {"label": "Living room ceiling and the sofa lamps", "settings": true, "sample": "full-end"}},
    {"label": "live clock", "props": {"label": "Lights", "time": ""}},
    {"label": "no clock", "props": {"label": "Settings", "clock": false}}
  ]
})") {
  lv_obj_t *h = fx::box(parent, "header");
  fill(h, p);
}

FW_UPDATE(header) {
  if (lv_obj_get_child_count(root) != N_CHILD || !fw::shape(root, sig(p))) {  // a part on / off: the row again (snap)
    fw::refill(root);
    for (const char *t : {"box", "header"}) fx::use(root, t);
    fill(root, p);
    return true;
  }
  if (lv_obj_t *l = label_of(root)) fx::set_text(l, p["label"] | "", "type:label");
  if (lv_obj_t *t = lv_obj_get_child(slot(root, TIME), 0)) {
    ArduinoJson::JsonDocument tp;
    time_props(p, tp);
    fw::update_child(fw::find("time"), t, tp.as<fw::Props>());
  }
  return true;
}
