// Calendar view: the Calendar screen's agenda (docs screens/calendar.ts calendarScreen, view Agenda; the factory spec
// renderer/specs/calendar.json adds the header and scrolls it, layout list): the hero is the next event, then the
// days.
//   hero     a real Hero block: the number = when ("16:50" today, "Now" while it runs, "Tmrw", "Thu", "24 Oct"; accent
//            while it runs or starts within 15 minutes), the event's title, one quiet line ("until 17:50 · Larkfield
//            studio · Family")
//   days     per day from today: a section line (a Status dot working + type label, text-dim: "TODAY · TUE 20 OCT",
//            "TOMORROW · ...", "THU 22 OCT"), then a row per event, lay::list_row_h tall with a hairline on top: the
//            time (body, text-dim, lay::cal_time wide: "16:50", "now" in accent-text, "all day"), the calendar's dot
//            (sp::s2 round, its colour token), the title (title, text, one line, ellipsis); a finished event goes
//            text-mute with its dot at half opacity
// Feed (core/agenda.h): the events (the docs story until a panel fetches calendar.get_events); now = a fixed time
// (ms, tests), 0 = live; the view re-renders once a minute. Not ported yet (noted for Darren): the Month view and the
// Agenda | Month switch, the calendars sheet (show / hide each calendar), tapping an event.
// Update path: rebuilt in place on a new minute or new props (the agenda's rows change shape; snap, like the docs).
// Contrast: "calendar" (+ hero_block, status_dot, text).
#include <ctime>
#include "agenda.h"
// audit deps: hero_block, status_dot
#include "fw.h"

namespace {
lv_color_t token(const char *name) {  // a colour token by its name (the calendar's colour)
  for (int i = 0; i < fw::tokens::N_COLOURS; i++)
    if (!strcmp(fw::tokens::COLOURS[i].name, name)) return *(const lv_color_t *) ((const char *) fw::T + fw::tokens::COLOURS[i].off);
  return fw::T->text_dim;
}
struct CV { double fixed = 0; long shown_min = -1; lv_timer_t *tick = nullptr; };
CV *of(lv_obj_t *o) { return (CV *) lv_obj_get_user_data(o); }
lv_obj_t *box(lv_obj_t *parent) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  return o;
}
lv_obj_t *build(const char *id, lv_obj_t *parent, ArduinoJson::JsonDocument &d) {
  const uint32_t n = lv_obj_get_child_count(parent);
  if (const fw::Component *c = fw::find(id)) c->build(parent, d.as<fw::Props>());
  return lv_obj_get_child_count(parent) > n ? lv_obj_get_child(parent, -1) : nullptr;
}
void fill(lv_obj_t *root) {
  namespace ag = fw::agenda;
  CV *c = of(root);
  const fw::Theme &t = *fw::T;
  const double now = c->fixed > 0 ? c->fixed : (double) time(nullptr) * 1000;
  c->shown_min = (long) (now / 60000);
  lv_obj_clean(root);
  const std::vector<ag::Event> ev = ag::sample(now);
  // hero
  ArduinoJson::JsonDocument hp;
  const ag::Event *e = ag::next(ev, now);
  if (!e) { hp["n"] = "--", hp["label"] = "Nothing coming up", hp["line"] = "in the next 14 days", hp["accent"] = false; }
  else {
    const long dd = ag::day_diff(e->start, now);
    const bool on = ag::running(*e, now);
    const std::string when = on ? "until " + ag::hm(e->end) : dd == 0 ? "today" : e->all_day ? ag::time_text(*e) : ag::hm(e->start);
    std::string line = when;
    if (!e->location.empty()) line += " \xC2\xB7 " + e->location;
    if (*ag::cal_name(e->calendar)) line += std::string(" \xC2\xB7 ") + ag::cal_name(e->calendar);
    hp["n"] = on ? "Now" : dd == 0 && !e->all_day ? ag::hm(e->start) : dd == 1 ? "Tmrw" : dd < 7 ? ag::wd(e->start) : ag::dm(e->start);
    hp["label"] = e->summary;
    hp["line"] = line;
    hp["accent"] = on || e->start - now <= ag::SOON;
  }
  build("hero_block", root, hp);
  // days
  for (const ag::DayGroup &g : ag::days(ev, now)) {
    lv_obj_t *sec = box(root);
    lv_obj_set_size(sec, LV_PCT(100), fw::lay::section_h);
    lv_obj_set_flex_flow(sec, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(sec, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(sec, fw::sp::s3, 0);
    lv_obj_set_style_margin_top(sec, fw::sp::s7, 0);
    ArduinoJson::JsonDocument dp;
    dp["tone"] = "working";
    build("status_dot", sec, dp);
    const long dd = ag::day_diff(g.day, now);
    const std::string name = dd == 0 ? "today \xC2\xB7 " + ag::long_day(g.day) : dd == 1 ? "tomorrow \xC2\xB7 " + ag::long_day(g.day) : ag::long_day(g.day);
    fw::text(sec, fw::ty::label, name.c_str(), t.text_dim);
    for (const ag::Event *x : g.ev) {
      const bool past = x->end <= now, on = ag::running(*x, now);
      lv_obj_t *row = box(root);
      lv_obj_set_size(row, LV_PCT(100), fw::lay::list_row_h);
      lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
      lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
      lv_obj_set_style_pad_column(row, fw::lay::gutter, 0);
      lv_obj_set_style_border_side(row, LV_BORDER_SIDE_TOP, 0);
      lv_obj_set_style_border_width(row, fw::stroke::hair, 0);
      lv_obj_set_style_border_color(row, t.line, 0);
      const std::string tm = x->all_day ? "all day" : on ? "now" : ag::hm(x->start);
      fw::text(row, fw::ty::body, tm.c_str(), past ? t.text_mute : on ? t.accent_text : t.text_dim, fw::lay::cal_time);
      lv_obj_t *dot = box(row);
      lv_obj_set_size(dot, fw::sp::s2, fw::sp::s2);
      lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
      lv_obj_set_style_bg_color(dot, token(ag::colour_of(x->calendar)), 0);
      lv_obj_set_style_bg_opa(dot, past ? LV_OPA_50 : LV_OPA_COVER, 0);
      lv_obj_t *ti = fw::text(row, fw::ty::title, x->summary.c_str(), past ? t.text_mute : t.text);
      lv_obj_set_flex_grow(ti, 1);
      lv_obj_set_width(ti, 0);
      lv_label_set_long_mode(ti, LV_LABEL_LONG_DOT);
    }
  }
}
}  // namespace

FW_COMPONENT(calendar_view, "Calendar view", "components", R"({
  "about": "The Calendar screen's agenda: the next event as the hero, then the days with a row per event.",
  "frame": [480, 0],
  "props": [],
  "variants": [
    {"label": "a Thursday afternoon", "props": {"now": 1792926000000}},
    {"label": "during the pottery class", "props": {"now": 1792948800000}}
  ]
})") {
  auto *c = new CV;
  lv_obj_t *root = lv_obj_create(parent);
  lv_obj_remove_style_all(root);
  lv_obj_remove_flag(root, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_user_data(root, c);
  lv_obj_add_event_cb(root, [](lv_event_t *e) {
    auto *c = (CV *) lv_event_get_user_data(e);
    if (c->tick) lv_timer_delete(c->tick);
    delete c;
  }, LV_EVENT_DELETE, c);
  lv_obj_set_size(root, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
  c->fixed = p["now"] | 0.0;
  fill(root);
  if (c->fixed <= 0)
    c->tick = lv_timer_create([](lv_timer_t *tm) {
      auto *root = (lv_obj_t *) lv_timer_get_user_data(tm);
      if ((long) (time(nullptr) / 60) != of(root)->shown_min) fill(root);
    }, fw::mo::ms::clock_tick, root);
}

FW_UPDATE(calendar_view) {
  CV *c = of(root);
  if (!c) return false;
  c->fixed = p["now"] | 0.0;
  fill(root);
  return true;
}
