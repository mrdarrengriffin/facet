// Calendar month: the Calendar screen's Month view (docs @facet/ui screens/calendar-month.ts monthView; was inside
// calendar_view), on the contract (renderer/CONTRACT.md, "contract": 2). Specific to the Calendar. A Grid can't draw
// it: the day columns and the weeks are different gaps apart (layout:cal-cell-gx / cal-cell-gy), the weekday row lines
// up with the columns and six weeks must fit the content height at 480 (6 x 48 + 5 x 4 ends at 380 of 388); 42 cells x
// (disc + number + dots) as objects would also be ~250 objects on the panel. So the days are drawn (the Pen, C24).
//   bar      a row (gutter apart): the month (type:title, the free width, "..."), then the Today Button (C8b: secondary,
//            small; only while another month shows), previous / next = Icon buttons (C8b: square, chevron-left /
//            chevron); gap:5 under it (docs weekday row at cal-wk-y = row + 16 / 12)
//   days     a strip (structure style cal-month: the parent's width, (cal-grid-y - cal-wk-y) + 6 cal-cell-h + 5
//            cal-cell-gy tall), the 7 columns cal-cell-w wide, cal-cell-gx apart, centred: the weekday names (type:label,
//            text-dim) on the top line, then six weeks of days from cal-grid-y - cal-wk-y: the selected day on a
//            surface plate (radius:square), the number (type:body; text, other months text-dim) centred in a cal-disc
//            disc (accent today, on-accent number) space:1 from the cell top, under it up to three event dots
//            (layout:dot, space:2 apart, a cal-dots-h row; other months line-hi) or two and "+N" (type:label, text-dim)
// Props (bound to panel.calendar, core/calendar_source.cpp): title (month_title), here (month_here: hides Today), days
// (month_days: a JSON list of 42 {d, n, out, today, sel, dots, more}).
// Touch: a day = event "pick" detail "day YYYY-MM-DD" + sound open; Today / previous / next = event "month" detail
// today / previous / next (their own press, tap and sound). States: today, selected, other month (per day); the
// buttons' own. n/a: pressed (a day has no press look: the docs plate is the selection), disabled, unavailable,
// loading, unknown, empty, error (the feed's: an empty days list draws the weekday row only).
// Update path: new days = one redraw of the strip; the title in place; Today shows / hides (snap). Silent.
// Contrast: label + body on the canvas, on-accent on accent, text on surface (+ the Button's and Icon button's).
#include <map>
#include "fw.h"
#include "fx.h"

namespace {
using Doc = ArduinoJson::JsonDocument;
struct Day { std::string d, n, dots, more; bool out, today, sel; };
struct M { std::vector<Day> days; lv_obj_t *title = nullptr, *today = nullptr, *strip = nullptr; };
std::map<lv_obj_t *, M *> by_strip;  // the strip's draw finds its days (no user data on core objects)
const char *const WK[] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};

const char *ink_of(const std::string &c, bool out) {  // a dot's ink from a short closed list
  static const char *const OK[] = {"blue", "ok", "warn", "steel", "text-dim", "text", "error", "accent"};
  static const char *const FG[] = {"fg:blue", "fg:ok", "fg:warn", "fg:steel", "fg:text-dim", "fg:text", "fg:error", "fg:accent"};
  if (out) return "fg:line-hi";
  for (int i = 0; i < 8; i++) if (c == OK[i]) return FG[i];
  return "fg:text-dim";
}
void take(M *m, fw::Props p) {
  m->days.clear();
  Doc d;
  const char *s = p["days"] | "[]";
  if (deserializeJson(d, s) || !d.is<ArduinoJson::JsonArray>()) return;
  for (ArduinoJson::JsonObjectConst o : d.as<ArduinoJson::JsonArrayConst>()) {
    if (m->days.size() >= 42) break;
    char n[8];
    snprintf(n, sizeof n, "%d", o["n"] | 0);
    m->days.push_back({o["d"] | "", n, o["dots"] | "", o["more"] | "", (o["out"] | 0) != 0, (o["today"] | 0) != 0, (o["sel"] | 0) != 0});
  }
}
// the geometry, live (the Layout lab tunes it): column x of cell i's left edge, row y of its top (centre-relative)
struct Geo { int cw, ch, gx, gy, top, ox, oy; };
Geo geo(int w, int h) {
  Geo g{fx::size("layout:cal-cell-w"), fx::size("layout:cal-cell-h"), fx::size("layout:cal-cell-gx"), fx::size("layout:cal-cell-gy"),
        fx::size("layout:cal-grid-y") - fx::size("layout:cal-wk-y"), 0, 0};
  g.ox = -w / 2 + (w - (7 * g.cw + 6 * g.gx)) / 2;
  g.oy = -h / 2;
  return g;
}
void draw(const fx::Pen &pen) {
  auto it = by_strip.find(pen.obj);
  if (it == by_strip.end()) return;
  const M &m = *it->second;
  const Geo g = geo(pen.w, pen.h);
  const int lb = fx::type_box("type:label"), s1 = fx::size("space:1"), disc = fx::size("layout:cal-disc"),
            dot = fx::size("layout:dot"), s2 = fx::size("space:2"), dh = fx::size("layout:cal-dots-h");
  for (int c = 0; c < 7; c++) pen.text(g.ox + c * (g.cw + g.gx) + g.cw / 2, g.oy + lb / 2, WK[c], "type:label", "fg:text-dim");
  for (size_t i = 0; i < m.days.size(); i++) {
    const Day &d = m.days[i];
    const int x = g.ox + (int) (i % 7) * (g.cw + g.gx), y = g.oy + g.top + (int) (i / 7) * (g.ch + g.gy), mx = x + g.cw / 2;
    if (!pen.sees(x, y, x + g.cw, y + g.ch)) continue;
    if (d.sel) pen.rect(x, y, g.cw, g.ch, "fg:surface", fx::size("radius:square"));
    const int dy = y + s1 + disc / 2;
    if (d.today) pen.dot(mx, dy, "layout:cal-disc", "fg:accent");
    pen.text(mx, dy, d.n.c_str(), "type:body", d.today ? "fg:on-accent" : d.out ? "fg:text-dim" : "fg:text");
    // dots: the colours (up to three, or two + "+N"), centred in a cal-dots-h row space:1 under the disc
    std::vector<std::string> cs;
    for (size_t a = 0; a <= d.dots.size() && cs.size() < 3;) {
      size_t b = d.dots.find(',', a);
      if (b == std::string::npos) b = d.dots.size();
      if (b > a) cs.push_back(d.dots.substr(a, b - a));
      a = b + 1;
    }
    const int more_w = d.more.empty() ? 0 : pen.text_w(d.more.c_str(), "type:label");
    const int n = (int) cs.size(), w = n * dot + (n > 0 ? n - 1 : 0) * s2 + (more_w ? (n ? s2 : 0) + more_w : 0);
    const int ry = dy + disc / 2 + s1 + dh / 2;
    int cx = mx - w / 2;
    for (const std::string &c : cs) { pen.dot(cx + dot / 2, ry, "layout:dot", ink_of(c, d.out)); cx += dot + s2; }
    if (more_w) pen.text(cx + more_w / 2, ry, d.more.c_str(), "type:label", "fg:text-dim");
  }
  (void) dot;
}
void redraw_all(lv_obj_t *s) {
  const int w = lv_obj_get_content_width(s), h = lv_obj_get_content_height(s);
  fx::redraw(s, -w / 2, -h / 2, w, h);
}
lv_obj_t *build(const char *id, lv_obj_t *parent, Doc &d) {
  const uint32_t n = lv_obj_get_child_count(parent);
  if (const fw::Component *c = fw::find(id)) c->build(parent, d.as<fw::Props>());
  return lv_obj_get_child_count(parent) > n ? lv_obj_get_child(parent, -1) : nullptr;
}
void month(lv_obj_t *b, const char *what) {
  lv_obj_add_event_cb(b, [](lv_event_t *e) {
    auto *o = (lv_obj_t *) lv_event_get_current_target(e);
    if (lv_obj_has_state(o, fw::st::OFF)) return;
    fw::emit(lv_obj_get_parent(lv_obj_get_parent(o)), "month", (const char *) lv_event_get_user_data(e));
  }, LV_EVENT_CLICKED, (void *) what);
}
void here(M *m, fw::Props p) {
  const bool h = p["here"].is<const char *>() ? *(p["here"].as<const char *>()) != 0 : (p["here"] | true);
  if (!m->today) return;
  if (h) lv_obj_add_flag(m->today, LV_OBJ_FLAG_HIDDEN); else lv_obj_remove_flag(m->today, LV_OBJ_FLAG_HIDDEN);
}
}  // namespace

FW_COMPONENT(calendar_month, "Calendar month", "components", R"({
  "about": "A month of the Calendar: the month with Today, previous and next, the weekday names, then six weeks of days, each with its number (today in the accent) and dots for its events. Tap a day to see it.",
  "contract": 2,
  "parts": [],
  "frame": [688, 0],
  "icons": ["chevron@glyph", "chevron-left@glyph"],
  "props": [
    {"name": "title", "about": "The month shown, such as October 2026.", "type": "text", "default": "October 2026"},
    {"name": "here", "about": "Whether this month is the one shown: Today is offered only for another month.", "type": "bool", "default": true},
    {"name": "days", "about": "The 42 days of the six weeks shown, a JSON list: each with its date d, number n, out (another month), today, sel (the day open), dots (up to three calendar colours, comma separated) and more (+2 when there are more).", "type": "text", "default": ""}
  ],
  "variants": [
    {"label": "this month", "props": {"days": "[{\"d\":\"2026-09-28\",\"n\":28,\"out\":1},{\"d\":\"2026-09-29\",\"n\":29,\"out\":1},{\"d\":\"2026-09-30\",\"n\":30,\"out\":1},{\"d\":\"2026-10-01\",\"n\":1},{\"d\":\"2026-10-02\",\"n\":2},{\"d\":\"2026-10-03\",\"n\":3},{\"d\":\"2026-10-04\",\"n\":4},{\"d\":\"2026-10-05\",\"n\":5},{\"d\":\"2026-10-06\",\"n\":6},{\"d\":\"2026-10-07\",\"n\":7},{\"d\":\"2026-10-08\",\"n\":8},{\"d\":\"2026-10-09\",\"n\":9},{\"d\":\"2026-10-10\",\"n\":10},{\"d\":\"2026-10-11\",\"n\":11},{\"d\":\"2026-10-12\",\"n\":12},{\"d\":\"2026-10-13\",\"n\":13},{\"d\":\"2026-10-14\",\"n\":14},{\"d\":\"2026-10-15\",\"n\":15},{\"d\":\"2026-10-16\",\"n\":16},{\"d\":\"2026-10-17\",\"n\":17},{\"d\":\"2026-10-18\",\"n\":18},{\"d\":\"2026-10-19\",\"n\":19},{\"d\":\"2026-10-20\",\"n\":20},{\"d\":\"2026-10-21\",\"n\":21},{\"d\":\"2026-10-22\",\"n\":22,\"today\":1,\"dots\":\"warn,blue\"},{\"d\":\"2026-10-23\",\"n\":23,\"dots\":\"ok,blue\",\"sel\":1},{\"d\":\"2026-10-24\",\"n\":24},{\"d\":\"2026-10-25\",\"n\":25,\"dots\":\"steel,blue\"},{\"d\":\"2026-10-26\",\"n\":26},{\"d\":\"2026-10-27\",\"n\":27,\"dots\":\"blue,blue,ok\",\"more\":\"+2\"},{\"d\":\"2026-10-28\",\"n\":28},{\"d\":\"2026-10-29\",\"n\":29},{\"d\":\"2026-10-30\",\"n\":30,\"dots\":\"warn\"},{\"d\":\"2026-10-31\",\"n\":31},{\"d\":\"2026-11-01\",\"n\":1,\"out\":1},{\"d\":\"2026-11-02\",\"n\":2,\"out\":1,\"dots\":\"blue\"},{\"d\":\"2026-11-03\",\"n\":3,\"out\":1},{\"d\":\"2026-11-04\",\"n\":4,\"out\":1},{\"d\":\"2026-11-05\",\"n\":5,\"out\":1},{\"d\":\"2026-11-06\",\"n\":6,\"out\":1},{\"d\":\"2026-11-07\",\"n\":7,\"out\":1},{\"d\":\"2026-11-08\",\"n\":8,\"out\":1}]"}},
    {"label": "another month · Today shows", "props": {"title": "November 2026", "here": false, "days": ""}}
  ]
})") {
  auto *m = new M;
  take(m, p);
  lv_obj_t *root = fx::box(parent, "col");
  fx::name(root, "calendar_month");
  fx::use(root, "gap:5");
  lv_obj_add_event_cb(root, [](lv_event_t *e) {
    auto *m = (M *) lv_event_get_user_data(e);
    by_strip.erase(m->strip);
    delete m;
  }, LV_EVENT_DELETE, m);
  lv_obj_t *bar = fx::box(root, "row");
  m->title = fx::text(bar, p["title"] | "", "type:title", "fg:text");
  fx::use(m->title, "grow");
  Doc tb;
  tb["kind"] = "secondary", tb["size"] = "small", tb["label"] = "today";
  if ((m->today = build("button", bar, tb))) month(m->today, "today");
  for (int k = 0; k < 2; k++) {
    Doc ib;
    ib["style"] = "square";
    ib["icon"] = k ? "chevron" : "chevron-left";
    if (lv_obj_t *b = build("icon_button", bar, ib)) month(b, k ? "next" : "previous");
  }
  here(m, p);
  m->strip = fx::strip(root, draw, "cal-month");
  by_strip[m->strip] = m;
  lv_obj_add_flag(m->strip, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(m->strip, [](lv_event_t *e) {  // a day: which cell is under the finger
    auto *s = (lv_obj_t *) lv_event_get_current_target(e);
    auto it = by_strip.find(s);
    lv_indev_t *in = lv_indev_active();
    if (it == by_strip.end() || !in) return;
    lv_point_t pt;
    lv_indev_get_point(in, &pt);
    lv_area_t a;
    lv_obj_get_content_coords(s, &a);
    const int w = lv_area_get_width(&a), h = lv_area_get_height(&a);
    const Geo g = geo(w, h);
    const int x = pt.x - a.x1 - w / 2 - g.ox, y = pt.y - a.y1 - h / 2 - g.oy - g.top;
    if (x < 0 || y < 0) return;
    const int c = x / (g.cw + g.gx), r = y / (g.ch + g.gy);
    if (c > 6 || r > 5 || x % (g.cw + g.gx) >= g.cw) return;
    const size_t i = (size_t) (r * 7 + c);
    if (i >= it->second->days.size()) return;
    fw::emit(lv_obj_get_parent(s), "pick", ("day " + it->second->days[i].d).c_str());
    fw::sound(s, "open");
  }, LV_EVENT_CLICKED, nullptr);
}

FW_UPDATE(calendar_month) {
  lv_obj_t *s = lv_obj_get_child(root, 1);
  auto it = by_strip.find(s);
  if (it == by_strip.end()) return false;
  M *m = it->second;
  take(m, p);
  fx::set_text(m->title, p["title"] | "", "type:title");
  here(m, p);
  redraw_all(s);
  return true;
}
