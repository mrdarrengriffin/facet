// Calendar view: the Calendar screen's body (docs @facet/ui screens/calendar.ts calendarScreen + calendar-month.ts
// monthView; the factory spec renderer/specs/calendar.json puts it on a screen, layout center). Agenda | Month in the
// header switch the two views; the calendars button (icon + how many are on) opens a sheet to show or hide each one.
//   header   on a spec screen the view puts its controls in the screen's own Header row before the chrome
//            (fw::spec::building_header(), docs header({right: [seg, button]})): the Segmented component (Agenda,
//            Month; hug) and the calendars button (lay::row tall, surface, rad::button, side padding lay::cal_btn_pad;
//            the screen-calendar icon at ic::icon + the count, label type, text-dim; pressed surface-press; tap = event
//            "open" calendars + sound open). Built alone (the workbench component page) it is the whole screen: its
//            own Header (label Calendar) at (inset, inset) and the body at (inset, lay::top).
//   body     lay::content_w x the content height (screen - top - inset), one of:
//   agenda   scrolls (vertical): the hero = the Hero block component, label_type title (the next event: the number =
//            when, "16:50" today, "Now" while it runs, "Tmrw", "Thu", "24 Oct"; accent while it runs or starts within
//            15 minutes; the title as written; one quiet line "until 17:50 · Larkfield studio · Family"); then from
//            lay::group under it the days: a section line (label type, text-dim: "TODAY · FRI 23 OCT") lay::section_gap
//            above its rows, a row per event lay::cal_row_h tall with a hairline on top: the time (body, lay::cal_time
//            wide, text-dim; "now" accent-text, "all day"), the calendar's dot (sp::s2 round, its colour token), the
//            title (title, one line, ellipsis); a finished event goes text-mute with its dot at 40 %. Days
//            lay::cal_day_gap apart (docs 32: a list to scan). Nothing on the shown calendars = one body line.
//   month    the month bar (lay::row: the month title in title type, then Today when another month shows (the
//            calendars button's look), then previous / next = Icon buttons, square, chevron-left / chevron at
//            ic::glyph); the weekday row (label type, text-dim, centred per column) at lay::cal_wk_y; six weeks of day
//            cells from lay::cal_grid_y, lay::cal_cell_w x lay::cal_cell_h, lay::cal_cell_gx / gy apart (7 across =
//            the content width): the number (body) centred in a lay::cal_disc disc (today: accent disc, on-accent
//            number), under it up to three event dots (sp::s2, sp::s2 apart, lay::cal_dots_h row) or two and "+N"
//            (label, text-dim); days of other months text-mute, dots at 40 %; the selected day on a surface plate
//            (rad::square). A cell tap = sound open, the plate, and the day sheet.
//   sheets   two Bottom sheet components (overlay, host rows), over the whole screen: "calendars" = a row per
//            calendar (lay::sheet_row_h, hairline on top: its dot (text-mute when hidden), the name (title) over
//            "N events" / "hidden" (label, text-dim), a read-only Checkbox at the right); a row tap shows / hides
//            that calendar (sound tap, the checkbox animates through its update path, event "calendar" "<id> on|off",
//            the agenda and the month follow); the day sheet = the day ("FRIDAY 23 OCTOBER") with its agenda rows,
//            or "Nothing on this day."; closing it clears the selected day.
// Feed (core/agenda.h): the events (the docs story until a panel fetches calendar.get_events); now = a fixed time (ms,
// tests), 0 = live (re-renders once a minute); view agenda | month; month = "YYYY-MM" (default this month).
// Update path: a new minute or new props = the agenda / month again in place (rows change shape; snap, like the docs);
// view switches like a tap on the segmented (the segmented's own update path), sheets close. Silent.
// Contrast: "calendar" (+ hero_block, segmented, checkbox, bottom_sheet, icon_button, header).
#include <ctime>
#include "agenda.h"
#include "spec.h"
// audit deps: hero_block, header, segmented, checkbox, bottom_sheet, icon_button
#include "fw.h"

namespace {
namespace ag = fw::agenda;
using Doc = ArduinoJson::JsonDocument;

lv_color_t token(const char *name) {  // a colour token by its name (the calendar's colour)
  for (int i = 0; i < fw::tokens::N_COLOURS; i++)
    if (!strcmp(fw::tokens::COLOURS[i].name, name)) return *(const lv_color_t *) ((const char *) fw::T + fw::tokens::COLOURS[i].off);
  return fw::T->text_dim;
}
struct CV {
  double fixed = 0;
  long shown_min = -1;
  lv_timer_t *tick = nullptr;
  bool month = false;
  bool show[ag::N_CALENDARS];
  int y = 0, m = 0;   // the month shown
  double sel = 0;     // the selected day (month view), 0 = none
  lv_obj_t *root = nullptr, *agenda = nullptr, *mbox = nullptr, *mtitle = nullptr, *today = nullptr, *grid = nullptr;
  lv_obj_t *seg = nullptr, *count = nullptr, *cal_sheet = nullptr, *day_sheet = nullptr;
};
CV *of(lv_obj_t *o) { return (CV *) lv_obj_get_user_data(o); }
double now_of(const CV *c) { return c->fixed > 0 ? c->fixed : (double) time(nullptr) * 1000; }
lv_obj_t *box(lv_obj_t *parent) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  return o;
}
lv_obj_t *build(const char *id, lv_obj_t *parent, Doc &d) {
  const uint32_t n = lv_obj_get_child_count(parent);
  if (const fw::Component *c = fw::find(id)) c->build(parent, d.as<fw::Props>());
  return lv_obj_get_child_count(parent) > n ? lv_obj_get_child(parent, -1) : nullptr;
}
const lv_style_prop_t TR_PROPS[] = {LV_STYLE_BG_COLOR, LV_STYLE_PROP_INV};
const lv_style_transition_dsc_t *tr(bool pressed) {
  static lv_style_transition_dsc_t d[2];
  static bool ready = false;
  if (!ready) { fw::st::press_transitions(d, TR_PROPS); ready = true; }
  return &d[pressed];
}
// the calendars button / Today: a boxed control on the surface (states.json pressed surface), lay::row tall
lv_obj_t *pill(lv_obj_t *parent) {
  const fw::Theme &t = *fw::T;
  lv_obj_t *b = box(parent);
  lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_size(b, LV_SIZE_CONTENT, fw::lay::row);
  lv_obj_set_style_pad_hor(b, fw::lay::cal_btn_pad, 0);
  lv_obj_set_style_radius(b, fw::rad::button, 0);
  lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(b, t.surface, 0);
  lv_obj_set_style_bg_color(b, fw::st::pressed_surface(t), LV_STATE_PRESSED);
  lv_obj_set_style_transition(b, tr(false), 0);
  lv_obj_set_style_transition(b, tr(true), LV_STATE_PRESSED);
  lv_obj_set_flex_flow(b, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(b, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(b, fw::sp::s2, 0);
  return b;
}
// a hugging pill's width up to the grid (its content centred), so what follows it stays on the grid
// (measured from its parts: on a spec screen the header is not laid out yet while the view builds)
void snap_w(lv_obj_t *b) {
  int w = 2 * fw::lay::cal_btn_pad, n = 0;
  for (uint32_t i = 0; i < lv_obj_get_child_count(b); i++) {
    lv_obj_t *o = lv_obj_get_child(b, (int32_t) i);
    if (lv_obj_check_type(o, &lv_label_class)) {
      lv_point_t sz;
      lv_text_get_size(&sz, lv_label_get_text(o), lv_obj_get_style_text_font(o, LV_PART_MAIN), lv_obj_get_style_text_letter_space(o, LV_PART_MAIN), 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
      w += sz.x;
    } else w += lv_obj_get_style_width(o, LV_PART_MAIN);
    n++;
  }
  w += (n > 1 ? n - 1 : 0) * fw::sp::s2;
  const int g = fw::sp::GRID;
  lv_obj_set_width(b, (w + g - 1) / g * g);
}
lv_obj_t *dot(lv_obj_t *parent, lv_color_t c, lv_opa_t opa = LV_OPA_COVER) {
  lv_obj_t *d = box(parent);
  lv_obj_set_size(d, fw::sp::s2, fw::sp::s2);
  lv_obj_set_style_radius(d, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(d, c, 0);
  lv_obj_set_style_bg_opa(d, opa, 0);
  return d;
}
constexpr lv_opa_t PAST_OPA = LV_OPA_40;  // docs .is-past / .is-out dots: opacity 0.4

// an agenda row (docs agendaRow): time · dot · title, a hairline on top
void agenda_row(lv_obj_t *parent, const ag::Event &x, double now) {
  const fw::Theme &t = *fw::T;
  const bool past = x.end <= now, on = ag::running(x, now);
  lv_obj_t *row = box(parent);
  lv_obj_set_size(row, LV_PCT(100), fw::lay::cal_row_h);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(row, fw::lay::gutter, 0);
  lv_obj_set_style_border_side(row, LV_BORDER_SIDE_TOP, 0);
  lv_obj_set_style_border_width(row, fw::stroke::hair, 0);
  lv_obj_set_style_border_color(row, t.line, 0);
  const std::string tm = x.all_day ? "all day" : on ? "now" : ag::hm(x.start);
  fw::text(row, fw::ty::body, tm.c_str(), past ? t.text_mute : on ? t.accent_text : t.text_dim, fw::lay::cal_time);
  dot(row, token(ag::colour_of(x.calendar)), past ? PAST_OPA : LV_OPA_COVER);
  lv_obj_t *ti = fw::text(row, fw::ty::title, x.summary.c_str(), past ? t.text_mute : t.text);
  lv_obj_set_flex_grow(ti, 1);
  lv_obj_set_width(ti, 0);
  lv_label_set_long_mode(ti, LV_LABEL_LONG_MODE_DOTS);
  lv_obj_set_height(ti, fw::style(fw::ty::title).box);
}

void fill_agenda(CV *c) {
  const fw::Theme &t = *fw::T;
  const double now = now_of(c);
  lv_obj_t *root = c->agenda;
  const int32_t sy = lv_obj_get_scroll_y(root);
  lv_obj_clean(root);
  const std::vector<ag::Event> ev = ag::sample(now, c->show);
  Doc hp;
  hp["label_type"] = "title";
  const ag::Event *e = ag::next(ev, now);
  if (!e) { hp["n"] = "--", hp["label"] = "Nothing coming up", hp["line"] = "in the next 14 days", hp["accent"] = false; }
  else {
    const long dd = ag::day_diff(e->start, now);
    const bool on = ag::running(*e, now);
    std::string line = on ? "until " + ag::hm(e->end) : dd == 0 ? "today" : e->all_day ? ag::time_text(*e) : ag::hm(e->start);
    if (!e->location.empty()) line += " \xC2\xB7 " + e->location;
    if (*ag::cal_name(e->calendar)) line += std::string(" \xC2\xB7 ") + ag::cal_name(e->calendar);
    hp["n"] = on ? "Now" : dd == 0 && !e->all_day ? ag::hm(e->start) : dd == 1 ? "Tmrw" : dd < 7 ? ag::wd(e->start) : ag::dm(e->start);
    hp["label"] = e->summary;
    hp["line"] = line;
    hp["accent"] = on || e->start - now <= ag::SOON;
  }
  build("hero_block", root, hp);
  bool first = true;
  const std::vector<ag::DayGroup> groups = ag::days(ev, now);
  for (const ag::DayGroup &g : groups) {
    lv_obj_t *sec = box(root);
    lv_obj_set_size(sec, LV_PCT(100), fw::style(fw::ty::label).box);  // docs .fs-sec 24
    lv_obj_set_flex_flow(sec, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(sec, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_margin_top(sec, first ? fw::lay::group : fw::lay::cal_day_gap, 0);
    lv_obj_set_style_margin_bottom(sec, fw::lay::section_gap, 0);
    first = false;
    const long dd = ag::day_diff(g.day, now);
    const std::string name = dd == 0 ? "today \xC2\xB7 " + ag::long_day(g.day) : dd == 1 ? "tomorrow \xC2\xB7 " + ag::long_day(g.day) : ag::long_day(g.day);
    fw::text(sec, fw::ty::label, name.c_str(), t.text_dim);
    for (const ag::Event *x : g.ev) agenda_row(root, *x, now);
  }
  if (groups.empty()) {
    lv_obj_t *l = fw::text(root, fw::ty::body, "Nothing on the calendars that are on.", t.text_dim);
    lv_obj_set_style_margin_top(l, fw::lay::group, 0);
  }
  lv_obj_update_layout(root);
  lv_obj_scroll_to_y(root, sy, LV_ANIM_OFF);  // a minute tick keeps where you are
}

void open_day(CV *c, double day);
void fill_month(CV *c) {
  const fw::Theme &t = *fw::T;
  const double now = now_of(c), today = ag::midnight(now);
  int ty = 0, tm = 0;
  ag::ym(now, ty, tm);
  fw::set_text(c->mtitle, fw::ty::title, ag::month_title(c->y, c->m).c_str());
  const bool here = c->y == ty && c->m == tm;
  if (here) lv_obj_add_flag(c->today, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_remove_flag(c->today, LV_OBJ_FLAG_HIDDEN);
  const std::vector<ag::Event> ev = ag::sample(now, c->show);
  lv_obj_clean(c->grid);
  double d = ag::grid_start(c->y, c->m);
  int yy = 0, mm = 0;
  ag::ym(ag::month_first(c->y, c->m), yy, mm);
  for (int i = 0; i < 42; i++, d = ag::day_add(d, 1)) {
    int dy = 0, dmo = 0;
    ag::ym(d, dy, dmo);
    const bool out = dmo != mm, is_today = d == today, sel = d == c->sel;
    lv_obj_t *cell = box(c->grid);
    lv_obj_add_flag(cell, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(cell, fw::lay::cal_cell_w, fw::lay::cal_cell_h);
    lv_obj_set_pos(cell, (i % 7) * (fw::lay::cal_cell_w + fw::lay::cal_cell_gx), (i / 7) * (fw::lay::cal_cell_h + fw::lay::cal_cell_gy));
    lv_obj_set_style_radius(cell, fw::rad::square, 0);
    lv_obj_set_style_bg_color(cell, t.surface, 0);
    lv_obj_set_style_bg_opa(cell, sel ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_opa(cell, LV_OPA_COVER, LV_STATE_PRESSED);  // docs :active = the surface plate
    lv_obj_set_style_pad_top(cell, fw::sp::s1, 0);
    lv_obj_set_flex_flow(cell, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cell, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(cell, fw::sp::s1, 0);
    lv_obj_t *disc = box(cell);
    lv_obj_set_size(disc, fw::lay::cal_disc, fw::lay::cal_disc);
    lv_obj_set_style_radius(disc, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(disc, t.accent, 0);
    lv_obj_set_style_bg_opa(disc, is_today ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    const std::string n = std::to_string(ag::mday(d));
    lv_obj_t *nl = fw::text(disc, fw::ty::body, n.c_str(), is_today ? t.on_accent : out ? t.text_mute : t.text);
    lv_obj_center(nl);
    // dots: up to three, or two and "+N"
    lv_obj_t *dots = box(cell);
    lv_obj_set_size(dots, LV_SIZE_CONTENT, fw::lay::cal_dots_h);
    lv_obj_set_flex_flow(dots, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(dots, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(dots, fw::sp::s2, 0);
    const std::vector<const ag::Event *> on = ag::on_day(ev, d);
    const int more = on.size() > 3 ? (int) on.size() - 2 : 0;
    for (size_t k = 0; k < on.size() && (int) k < (more ? 2 : 3); k++)
      dot(dots, token(ag::colour_of(on[k]->calendar)), out ? PAST_OPA : LV_OPA_COVER);
    if (more) fw::text(dots, fw::ty::label, ("+" + std::to_string(more)).c_str(), out ? t.text_mute : t.text_dim);
    lv_obj_set_user_data(cell, (void *) (intptr_t) i);
    lv_obj_add_event_cb(cell, [](lv_event_t *e) {
      auto *c = (CV *) lv_event_get_user_data(e);
      auto *o = (lv_obj_t *) lv_event_get_current_target(e);
      const int i = (int) (intptr_t) lv_obj_get_user_data(o);
      double day = ag::grid_start(c->y, c->m);
      for (int k = 0; k < i; k++) day = ag::day_add(day, 1);
      fw::emit(o, "open", ag::day_title(day).c_str());
      fw::sound(o, "open");
      open_day(c, day);
    }, LV_EVENT_CLICKED, c);
  }
}

// ---- sheets
void cal_sheet_props(const CV *c, bool open, Doc &d) {
  d["overlay"] = true;
  d["title"] = "calendars";
  d["open"] = open ? "open" : "closed";
  d["rows"] = ag::N_CALENDARS;
  d["row_h"] = fw::lay::sheet_row_h;
}
void fill_cal_rows(CV *c, lv_obj_t *list) {
  const fw::Theme &t = *fw::T;
  const double now = now_of(c);
  bool all[ag::N_CALENDARS];
  for (bool &x : all) x = true;
  const std::vector<ag::Event> ev = ag::sample(now, all);
  // on first (docs: sorted by on), then the rest, each in the feed's order
  for (int pass = 0; pass < 2; pass++)
    for (int i = 0; i < ag::N_CALENDARS; i++) {
      if (c->show[i] != (pass == 0)) continue;
      const ag::Cal &k = ag::CALENDARS[i];
      int n = 0;
      for (const ag::Event &e : ev) n += e.calendar == k.id;
      lv_obj_t *row = box(list);
      lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_set_size(row, LV_PCT(100), fw::lay::sheet_row_h);
      lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
      lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
      lv_obj_set_style_pad_column(row, fw::lay::gutter, 0);
      lv_obj_set_style_border_side(row, LV_BORDER_SIDE_TOP, 0);
      lv_obj_set_style_border_width(row, fw::stroke::hair, 0);
      lv_obj_set_style_border_color(row, t.line, 0);
      lv_obj_set_style_bg_color(row, fw::st::pressed_tint(t), 0);
      lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
      lv_obj_set_style_bg_opa(row, LV_OPA_COVER, LV_STATE_PRESSED);
      lv_obj_t *dt = dot(row, c->show[i] ? token(k.colour) : t.text_mute);
      lv_obj_set_style_margin_left(dt, fw::sp::s3, 0);
      lv_obj_set_style_margin_right(dt, fw::sp::s3, 0);
      lv_obj_t *col = box(row);
      lv_obj_set_size(col, 0, LV_SIZE_CONTENT);
      lv_obj_set_flex_grow(col, 1);
      lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
      lv_obj_t *nm = fw::text(col, fw::ty::title, k.name, t.text);
      lv_obj_set_width(nm, LV_PCT(100));
      lv_label_set_long_mode(nm, LV_LABEL_LONG_MODE_DOTS);
      lv_obj_set_height(nm, fw::style(fw::ty::title).box);
      const std::string st = c->show[i] ? std::to_string(n) + (n == 1 ? " event" : " events") : "hidden";
      fw::text(col, fw::ty::label, st.c_str(), t.text_dim);
      Doc cb;
      cb["check"] = c->show[i] ? "done" : "open";
      cb["label"] = "";
      cb["interactive"] = false;
      if (lv_obj_t *x = build("checkbox", row, cb)) lv_obj_set_style_margin_right(x, fw::sp::s3, 0);  // docs 14
      lv_obj_set_user_data(row, (void *) (intptr_t) i);
      lv_obj_add_event_cb(row, [](lv_event_t *e) {
        auto *c = (CV *) lv_event_get_user_data(e);
        auto *o = (lv_obj_t *) lv_event_get_current_target(e);
        const int i = (int) (intptr_t) lv_obj_get_user_data(o);
        c->show[i] = !c->show[i];
        fw::emit(o, "calendar", (std::string(ag::CALENDARS[i].id) + (c->show[i] ? " on" : " off")).c_str());
        fw::sound(o, "tap");
        // the row in place: its checkbox animates (update path), dot + status follow; the list keeps its order while open
        Doc cb;
        cb["check"] = c->show[i] ? "done" : "open";
        cb["label"] = "";
        cb["interactive"] = false;
        lv_obj_t *box_ = lv_obj_get_child(o, -1);
        fw::update_child(fw::find("checkbox"), box_, cb.as<fw::Props>());
        lv_obj_set_style_bg_color(lv_obj_get_child(o, 0), c->show[i] ? token(ag::CALENDARS[i].colour) : fw::T->text_mute, 0);
        lv_obj_t *st = lv_obj_get_child(lv_obj_get_child(o, 1), 1);
        if (!c->show[i]) fw::set_text(st, fw::ty::label, "hidden");
        else {
          bool all[ag::N_CALENDARS];
          for (bool &x : all) x = true;
          int n = 0;
          for (const ag::Event &x : ag::sample(now_of(c), all)) n += x.calendar == ag::CALENDARS[i].id;
          fw::set_text(st, fw::ty::label, (std::to_string(n) + (n == 1 ? " event" : " events")).c_str());
        }
        int on = 0;
        for (bool x : c->show) on += x;
        fw::set_text(c->count, fw::ty::label, std::to_string(on).c_str());
        snap_w(lv_obj_get_parent(c->count));
        fill_agenda(c);
        fill_month(c);
      }, LV_EVENT_CLICKED, c);
    }
}
void day_sheet_props(const CV *c, bool open, int rows, Doc &d) {
  d["overlay"] = true;
  d["title"] = c->sel > 0 ? ag::day_title(c->sel) : "day";
  d["open"] = open ? "open" : "closed";
  d["rows"] = rows;
  d["row_h"] = fw::lay::cal_row_h;
}
void fill_day_rows(CV *c, lv_obj_t *list) {
  if (c->sel <= 0) return;
  const double now = now_of(c);
  const std::vector<ag::Event> ev = ag::sample(now, c->show);
  const std::vector<const ag::Event *> on = ag::on_day(ev, c->sel);
  for (const ag::Event *x : on) agenda_row(list, *x, now);
  if (on.empty()) {
    lv_obj_t *l = fw::text(list, fw::ty::body, "Nothing on this day.", fw::T->text_dim);
    lv_obj_set_style_margin_top(l, fw::lay::section_gap, 0);
  }
}
void open_day(CV *c, double day) {
  c->sel = day;
  fill_month(c);
  if (!c->day_sheet) return;
  const std::vector<ag::Event> ev = ag::sample(now_of(c), c->show);
  const int n = (int) ag::on_day(ev, day).size();
  Doc d;
  day_sheet_props(c, true, n ? n : 1, d);
  fw::update_child(fw::find("bottom_sheet"), c->day_sheet, d.as<fw::Props>());
}

lv_obj_t *sheet(CV *c, lv_obj_t *root, Doc &d, int ox, int oy, bool day) {
  lv_obj_t *s = build("bottom_sheet", root, d);
  if (!s) return nullptr;
  lv_obj_set_pos(s, ox, oy);
  lv_obj_set_size(s, fw::lay::screen_w, fw::lay::screen_h);
  lv_obj_add_event_cb(s, day ? [](lv_event_t *e) { auto *c = (CV *) lv_event_get_user_data(e); fill_day_rows(c, (lv_obj_t *) lv_event_get_param(e)); }
                             : [](lv_event_t *e) { auto *c = (CV *) lv_event_get_user_data(e); fill_cal_rows(c, (lv_obj_t *) lv_event_get_param(e)); },
                      LV_EVENT_REFRESH, c);
  return s;
}

void set_view(CV *c, bool month) {
  c->month = month;
  if (month) { lv_obj_add_flag(c->agenda, LV_OBJ_FLAG_HIDDEN); lv_obj_remove_flag(c->mbox, LV_OBJ_FLAG_HIDDEN); }
  else { lv_obj_remove_flag(c->agenda, LV_OBJ_FLAG_HIDDEN); lv_obj_add_flag(c->mbox, LV_OBJ_FLAG_HIDDEN); }
}
void go_month(CV *c, int dm) {
  int y = 0, m = 0;
  ag::ym(ag::month_first(c->y, c->m + dm), y, m);
  c->y = y, c->m = m, c->sel = 0;
  fill_month(c);
}
void take(CV *c, fw::Props p) {
  c->fixed = p["now"] | 0.0;
  ag::ym(now_of(c), c->y, c->m);
  const char *mo = p["month"] | "";
  int y = 0, m = 0;
  if (sscanf(mo, "%d-%d", &y, &m) == 2 && m >= 1 && m <= 12) c->y = y, c->m = m - 1;
  c->sel = 0;
}

// the header controls: Agenda | Month + the calendars button, into `row` before its last child (the screen chrome)
void header_controls(CV *c, lv_obj_t *row) {
  const fw::Theme &t = *fw::T;
  const uint32_t n = lv_obj_get_child_count(row);
  lv_obj_t *chrome = n ? lv_obj_get_child(row, (int32_t) n - 1) : nullptr;
  Doc sp;
  sp["options"] = "Agenda, Month";
  sp["selected"] = c->month ? "month" : "agenda";
  c->seg = build("segmented", row, sp);
  if (c->seg)
    fw::listen(c->seg, [](lv_obj_t *, const char *name, const char *detail, void *user) {
      auto *c = (CV *) user;
      if (!strcmp(name, "change")) set_view(c, !strcmp(detail, "month"));
    }, c);
  lv_obj_t *b = pill(row);
  fw::icon(b, "screen-calendar", fw::ic::icon, t.text_dim);
  int on = 0;
  for (bool x : c->show) on += x;
  c->count = fw::text(b, fw::ty::label, std::to_string(on).c_str(), t.text_dim);
  snap_w(b);
  lv_obj_add_event_cb(b, [](lv_event_t *e) {
    auto *c = (CV *) lv_event_get_user_data(e);
    auto *o = (lv_obj_t *) lv_event_get_current_target(e);
    fw::emit(o, "open", "calendars");
    fw::sound(o, "open");
    if (!c->cal_sheet) return;
    Doc d;
    cal_sheet_props(c, true, d);
    fw::update_child(fw::find("bottom_sheet"), c->cal_sheet, d.as<fw::Props>());
  }, LV_EVENT_CLICKED, c);
  if (chrome) lv_obj_move_to_index(chrome, -1);  // the time stays at the right edge
}

void build_month_box(CV *c, lv_obj_t *mb) {
  const fw::Theme &t = *fw::T;
  lv_obj_t *bar = box(mb);
  lv_obj_set_size(bar, fw::lay::content_w, fw::lay::row);
  lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(bar, fw::lay::gutter, 0);
  c->mtitle = fw::text(bar, fw::ty::title, "", t.text);
  lv_obj_set_flex_grow(c->mtitle, 1);
  lv_obj_set_width(c->mtitle, 0);
  lv_label_set_long_mode(c->mtitle, LV_LABEL_LONG_MODE_DOTS);
  lv_obj_set_height(c->mtitle, fw::style(fw::ty::title).box);
  c->today = pill(bar);
  fw::text(c->today, fw::ty::label, "today", t.text_dim);
  snap_w(c->today);
  lv_obj_add_event_cb(c->today, [](lv_event_t *e) {
    auto *c = (CV *) lv_event_get_user_data(e);
    auto *o = (lv_obj_t *) lv_event_get_current_target(e);
    fw::emit(o, "tap", "today");
    fw::sound(o, "tap");
    int y = 0, m = 0;
    ag::ym(now_of(c), y, m);
    c->y = y, c->m = m, c->sel = 0;
    fill_month(c);
  }, LV_EVENT_CLICKED, c);
  for (int k = 0; k < 2; k++) {
    Doc ib;
    ib["style"] = "square";
    ib["icon"] = k ? "chevron" : "chevron-left";
    lv_obj_t *b = build("icon_button", bar, ib);
    if (b) lv_obj_add_event_cb(b, k ? [](lv_event_t *e) { go_month((CV *) lv_event_get_user_data(e), 1); }
                                    : [](lv_event_t *e) { go_month((CV *) lv_event_get_user_data(e), -1); }, LV_EVENT_CLICKED, c);
  }
  // weekday row
  lv_obj_t *wk = box(mb);
  lv_obj_set_pos(wk, 0, fw::lay::cal_wk_y);
  lv_obj_set_size(wk, fw::lay::content_w, fw::style(fw::ty::label).box);
  static const char *W[] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
  for (int i = 0; i < 7; i++) {
    lv_obj_t *l = fw::text(wk, fw::ty::label, W[i], t.text_dim, fw::lay::cal_cell_w);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(l, i * (fw::lay::cal_cell_w + fw::lay::cal_cell_gx), 0);
  }
  c->grid = box(mb);
  lv_obj_set_pos(c->grid, 0, fw::lay::cal_grid_y);
  lv_obj_set_size(c->grid, fw::lay::content_w, 6 * fw::lay::cal_cell_h + 5 * fw::lay::cal_cell_gy);
}
static_assert(7 * fw::lay::cal_cell_w + 6 * fw::lay::cal_cell_gx == fw::lay::content_w, "7 day cells fill the content width");
static_assert(fw::lay::cal_grid_y + 6 * fw::lay::cal_cell_h + 5 * fw::lay::cal_cell_gy <= fw::lay::screen_h - fw::lay::top - fw::lay::inset,
              "the month grid fits the content height");
}  // namespace

FW_COMPONENT(calendar_view, "Calendar view", "components", R"({
  "about": "The Calendar screen: Agenda (the next event as the hero, then the days) and Month (six weeks of days with their events), switched in the header, with a sheet to show or hide each calendar.",
  "frame": [480, 480],
  "icons": ["screen-calendar@icon", "chevron@glyph", "chevron-left@glyph", "close@glyph", "check@icon", "back@glyph"],
  "props": [
    {"name": "view", "type": "select", "options": ["agenda", "month"], "default": "agenda"},
    {"name": "month", "type": "text", "default": ""}
  ],
  "variants": [
    {"label": "a Thursday afternoon", "props": {"now": 1792926000000}},
    {"label": "during the pottery class", "props": {"now": 1792948800000}},
    {"label": "month", "props": {"now": 1792926000000, "view": "month"}},
    {"label": "month · next month", "props": {"now": 1792926000000, "view": "month", "month": "2026-11"}}
  ]
})") {
  auto *c = new CV;
  for (int i = 0; i < ag::N_CALENDARS; i++) c->show[i] = ag::CALENDARS[i].on;
  take(c, p);
  c->month = !strcmp(p["view"] | "agenda", "month");
  lv_obj_t *hdr = fw::spec::building_header();
  const bool alone = !hdr;
  const int CH = fw::lay::screen_h - fw::lay::top - fw::lay::inset;
  lv_obj_t *root = box(parent);
  c->root = root;
  lv_obj_set_user_data(root, c);
  lv_obj_add_event_cb(root, [](lv_event_t *e) {
    auto *c = (CV *) lv_event_get_user_data(e);
    if (c->tick) lv_timer_delete(c->tick);
    delete c;
  }, LV_EVENT_DELETE, c);
  int ox = 0, oy = 0;  // the body's corner on the root
  if (alone) {  // the whole screen: its own Header (the spec screen's look) + the body under it
    lv_obj_set_size(root, fw::lay::screen_w, fw::lay::screen_h);
    lv_obj_set_style_bg_color(root, fw::T->bg, 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
    Doc hd;
    hd["label"] = "Calendar";
    hdr = build("header", root, hd);
    if (hdr) lv_obj_set_pos(hdr, fw::lay::inset, fw::lay::inset);
    ox = fw::lay::inset, oy = fw::lay::top;
  } else {
    lv_obj_set_size(root, fw::lay::content_w, CH);
    // the sheets cover the whole screen from the body's corner (drawn and touched; the scrim covers the header too):
    // LVGL lets a child out of its parents only by their ext draw size, so the view bleeds by lay::top up to the screen
    fw::bleed(root, fw::lay::top);
    fw::qa::free(root, "overlay: hosts the calendars and day sheets, which cover the whole screen");
  }
  c->agenda = box(root);
  lv_obj_set_pos(c->agenda, ox, oy);
  lv_obj_set_size(c->agenda, fw::lay::content_w, CH);
  lv_obj_add_flag(c->agenda, (lv_obj_flag_t) (LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE));  // clickable: a drag on a day row or its text scrolls too (BF-008)
  lv_obj_set_scroll_dir(c->agenda, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(c->agenda, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_flex_flow(c->agenda, LV_FLEX_FLOW_COLUMN);
  c->mbox = box(root);
  lv_obj_set_pos(c->mbox, ox, oy);
  lv_obj_set_size(c->mbox, fw::lay::content_w, CH);
  build_month_box(c, c->mbox);
  if (hdr) header_controls(c, hdr);
  set_view(c, c->month);
  fill_agenda(c);
  fill_month(c);
  // the sheets: over the whole screen from the body's corner (the scrim covers the header too)
  Doc cs;
  cal_sheet_props(c, false, cs);
  c->cal_sheet = sheet(c, root, cs, alone ? 0 : -fw::lay::inset, alone ? 0 : -fw::lay::top, false);
  Doc ds;
  day_sheet_props(c, false, 1, ds);
  c->day_sheet = sheet(c, root, ds, alone ? 0 : -fw::lay::inset, alone ? 0 : -fw::lay::top, true);
  if (c->day_sheet)
    fw::listen(c->day_sheet, [](lv_obj_t *, const char *name, const char *, void *user) {
      auto *c = (CV *) user;
      if (!strcmp(name, "close") && c->sel > 0) { c->sel = 0; fill_month(c); }
    }, c);
  if (c->fixed <= 0)
    c->tick = lv_timer_create([](lv_timer_t *tm) {
      auto *c = (CV *) lv_timer_get_user_data(tm);
      if ((long) (time(nullptr) / 60) != c->shown_min) { c->shown_min = (long) (time(nullptr) / 60); fill_agenda(c); fill_month(c); }
    }, fw::mo::ms::clock_tick, c);
  c->shown_min = (long) (now_of(c) / 60000);
}

FW_UPDATE(calendar_view) {
  CV *c = of(root);
  if (!c) return false;
  take(c, p);
  const bool month = !strcmp(p["view"] | "agenda", "month");
  if (month != c->month) {
    set_view(c, month);
    if (c->seg) {  // the tabs move like a tap (the segmented's own update path)
      Doc sp;
      sp["options"] = "Agenda, Month";
      sp["selected"] = month ? "month" : "agenda";
      fw::update_child(fw::find("segmented"), c->seg, sp.as<fw::Props>());
    }
  }
  if (c->cal_sheet) {  // new props: the sheets close (their own close motion)
    Doc d;
    cal_sheet_props(c, false, d);
    fw::update_child(fw::find("bottom_sheet"), c->cal_sheet, d.as<fw::Props>());
  }
  if (c->day_sheet) {
    Doc d;
    day_sheet_props(c, false, 1, d);
    fw::update_child(fw::find("bottom_sheet"), c->day_sheet, d.as<fw::Props>());
  }
  fill_agenda(c);
  fill_month(c);
  return true;
}
