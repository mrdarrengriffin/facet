// Calendar row: one line of the Calendar screen (docs @facet/ui screens/calendar.ts agendaRow + the calendars sheet's
// rows; was inside calendar_view), on the contract (renderer/CONTRACT.md, "contract": 2). Specific to the Calendar: the
// calendar's colour dot is what a List row can't draw. A spec repeats it per item of a Grid (items = panel.calendar
// agenda / day_events / calendars, core/calendar_source.cpp).
//   day       a day's label: the Section header component (C8b, size label: "TODAY · THU 22 OCT", its box + space 2),
//             then layout:cal-sec-gap to its first row (together = layout section-gap); above:cal-day-gap unless first
//   event     tall:cal-row-h, a hairline on top (rule-top), a row gutter apart: the time (type:body, w:cal-time, text-dim;
//             tone now = accent-text; "all day"), the calendar's dot (layout:dot, round, bg:<colour>; tone past =
//             bg:line-hi, the docs 40 % without opacity: RGB565), the title (type:title, text; past = text-dim, the free
//             width, "...")
//   calendar  the calendars sheet's row: tall:list-row-h (= sheet-row-h), rule-top: the dot (pad-x:3 round it; off =
//             bg:line-hi), the name (type:title) over its status (type:label, text-dim: "5 events" / "hidden"), the
//             Checkbox component (C8b, read-only: done = on), pad-x:3 from the end
// Touch: none of its own (the Grid cell around it takes the tap: pick = the item's id). Static: no sound.
// States: tone now / past (event), on / off (calendar). n/a: pressed (the Grid cell's), disabled, unavailable, loading,
// unknown, empty, error (the feed's: the screen shows its words). Update path: the same kind = texts, tones, dot and
// the checkbox in place (the checkbox through its own update: it animates); another kind or tone = made again (faded like a tap). Silent.
// Contrast: text, label, body (text-dim / accent-text on the canvas) + the Section header's and the Checkbox's.
#include "fw.h"
#include "fx.h"

namespace {
using Doc = ArduinoJson::JsonDocument;
const char *dot_bg(const char *c) {  // the calendar's colour token, from a short closed list (feed data never aborts fx::use)
  static const char *const OK[] = {"blue", "ok", "warn", "steel", "text-dim", "text", "error", "accent"};
  static const char *const BG[] = {"bg:blue", "bg:ok", "bg:warn", "bg:steel", "bg:text-dim", "bg:text", "bg:error", "bg:accent"};
  for (int i = 0; i < 8; i++) if (!strcmp(c, OK[i])) return BG[i];
  return "bg:text-dim";
}
bool is_on(fw::Props p, const char *k) { return p[k].is<const char *>() ? !strcmp(p[k].as<const char *>(), "on") : (p[k] | false); }
std::string kind(fw::Props p) { return p["kind"] | "event"; }
std::string sig(fw::Props p) { return kind(p) + "|" + (is_on(p, "first") ? "f" : "-") + "|" + (p["tone"] | ""); }
lv_obj_t *build(const char *id, lv_obj_t *parent, Doc &d) {
  const uint32_t n = lv_obj_get_child_count(parent);
  if (const fw::Component *c = fw::find(id)) c->build(parent, d.as<fw::Props>());
  return lv_obj_get_child_count(parent) > n ? lv_obj_get_child(parent, -1) : nullptr;
}
void sec_props(fw::Props p, Doc &d) { d["title"] = p["title"] | ""; d["size"] = "label"; }
void check_props(fw::Props p, Doc &d) { d["check"] = is_on(p, "on") ? "done" : "open"; d["label"] = ""; d["interactive"] = false; }
const char *time_ink(fw::Props p) { return !strcmp(p["tone"] | "", "now") ? "fg:accent-text" : "fg:text-dim"; }
const char *title_ink(fw::Props p) { return !strcmp(p["tone"] | "", "past") ? "fg:text-dim" : "fg:text"; }
const char *ev_dot(fw::Props p) { return !strcmp(p["tone"] | "", "past") ? "bg:line-hi" : dot_bg(p["colour"] | ""); }
const char *cal_dot(fw::Props p) { return is_on(p, "on") ? dot_bg(p["colour"] | "") : "bg:line-hi"; }
lv_obj_t *dot(lv_obj_t *parent, const char *bg) {
  lv_obj_t *d = fx::box(parent, "free");
  for (const char *t : {"w:dot", "h:dot", "radius:round"}) fx::use(d, t);
  fx::use(d, bg);
  return d;
}
void fill(lv_obj_t *root, fw::Props p) {
  const std::string k = kind(p);
  fw::shape(root, sig(p).c_str());
  if (k == "day") {
    for (const char *t : {"box", "col"}) fx::use(root, t);
    fx::use(root, is_on(p, "first") ? "above:group" : "above:cal-day-gap");  // the first: layout group under the hero (docs)
    Doc d;
    sec_props(p, d);
    build("section_header", root, d);
    fx::use(fx::box(root, "free"), "h:cal-sec-gap");
    return;
  }
  for (const char *t : {"box", "row", "fill-w", "line:line", "rule-top"}) fx::use(root, t);
  if (k == "calendar") {
    fx::use(root, "tall:list-row-h");
    lv_obj_t *dw = fx::box(root, "centre");  // the dot, space:3 each side (docs 14 / 12)
    for (const char *t : {"hug", "pad-x:3"}) fx::use(dw, t);
    dot(dw, cal_dot(p));
    lv_obj_t *col = fx::box(root, "col");
    fx::use(col, "grow");
    fx::use(fx::text(col, p["title"] | "", "type:title", "fg:text"), "fill-w");
    fx::use(fx::text(col, p["status"] | "", "type:label", "fg:text-dim"), "fill-w");
    lv_obj_t *cp = fx::box(root, "centre");  // space:3 from the end (docs 14 / 12) round a box the Checkbox's size (it fills what it is in)
    for (const char *t : {"hug", "pad-x:3"}) fx::use(cp, t);
    lv_obj_t *cw = fx::box(cp, "centre");
    for (const char *t : {"hug", "w:checkbox"}) fx::use(cw, t);
    Doc d;
    check_props(p, d);
    build("checkbox", cw, d);
    return;
  }
  for (const char *t : {"tall:cal-row-h"}) fx::use(root, t);
  lv_obj_t *tm = fx::text(root, p["time"] | "", "type:body", time_ink(p));
  fx::use(tm, "w:cal-time");
  dot(root, ev_dot(p));
  fx::use(fx::text(root, p["title"] | "", "type:title", title_ink(p)), "grow");
}
}  // namespace

FW_COMPONENT(calendar_row, "Calendar row", "components", R"({
  "about": "One line of the Calendar: a day's label, an event (its time, its calendar's colour dot and its title) or a calendar in the calendars sheet with its checkbox.",
  "contract": 2,
  "parts": [],
  "audit": {"snaps": {"kind": "a day line, an event and a calendar are different rows (structure): a list's item never turns into another kind"}},
  "frame": [448, 0],
  "props": [
    {"name": "kind", "about": "What the row is: day is a day's label over its events, event is one event, calendar is one calendar you can show or hide.", "type": "select", "options": ["day", "event", "calendar"], "default": "event"},
    {"name": "title", "about": "The words: the day (today · Thu 22 Oct), the event's title or the calendar's name.", "type": "text", "default": "Pottery class"},
    {"name": "time", "about": "An event's time, such as 16:50, now or all day.", "type": "text", "default": "16:50"},
    {"name": "colour", "about": "The calendar's colour for its dot: blue, ok, warn, steel, text-dim, text, error or accent.", "type": "text", "default": "blue"},
    {"name": "tone", "about": "An event's moment: empty for later, now while it runs (the time in the accent), past when it is over (quieter).", "type": "select", "options": ["", "now", "past"], "default": ""},
    {"name": "first", "about": "A day's label at the top of the list, with no space above it.", "type": "bool", "default": false},
    {"name": "status", "about": "A calendar's quieter line, such as 5 events or hidden.", "type": "text", "default": "5 events"},
    {"name": "on", "about": "Whether a calendar is shown: its checkbox is ticked and its dot takes its colour.", "type": "bool", "default": true}
  ],
  "variants": [
    {"label": "event", "props": {}},
    {"label": "event · now", "props": {"time": "now", "tone": "now"}},
    {"label": "event · past", "props": {"time": "08:30", "title": "School run", "colour": "warn", "tone": "past"}},
    {"label": "event · all day", "props": {"time": "all day", "title": "Sam's birthday", "colour": "steel"}},
    {"label": "day", "props": {"kind": "day", "title": "tomorrow · Fri 23 Oct"}},
    {"label": "calendar · on", "props": {"kind": "calendar", "title": "Family"}},
    {"label": "calendar · hidden", "props": {"kind": "calendar", "title": "Bins", "colour": "text", "status": "hidden", "on": false}}
  ]
})") {
  lv_obj_t *root = fx::box(parent, "col");
  fx::name(root, "calendar_row");
  fill(root, p);
}

FW_UPDATE(calendar_row) {
  const std::string k = kind(p);
  if (!fw::shape(root, sig(p).c_str())) {  // another kind or tone: made again, faded like a tap
    fw::st::refill_fade(root, [&] { fw::refill(root); fill(root, p); }, true, true);
    return true;
  }
  if (k == "day") {
    Doc d;
    sec_props(p, d);
    fw::update_child(fw::find("section_header"), lv_obj_get_child(root, 0), d.as<fw::Props>());
  } else if (k == "calendar") {
    fx::use(lv_obj_get_child(lv_obj_get_child(root, 0), 0), cal_dot(p));
    lv_obj_t *col = lv_obj_get_child(root, 1);
    fx::set_text(lv_obj_get_child(col, 0), p["title"] | "", "type:title");
    fx::set_text(lv_obj_get_child(col, 1), p["status"] | "", "type:label");
    Doc d;
    check_props(p, d);
    fw::update_child(fw::find("checkbox"), lv_obj_get_child(lv_obj_get_child(lv_obj_get_child(root, 2), 0), 0), d.as<fw::Props>());
  } else {
    lv_obj_t *tm = lv_obj_get_child(root, 0), *ti = lv_obj_get_child(root, 2);
    fx::set_text(tm, p["time"] | "", "type:body");
    fx::use(lv_obj_get_child(root, 1), ev_dot(p));
    fx::set_text(ti, p["title"] | "", "type:title");
  }
  return true;
}
