// Date line: the Facet date line (docs: Components > Clock > Date line, @facet/ui clocks.ts dateLine()): short = MON 19
// OCT (type c-date: JetBrains Mono caps, tracked), long = Monday 19 October (type date-long, Inter light), text-dim by
// default (docs tone). Prop "date" = a fixed "YYYY-MM-DD" (docs, tests, the audit); empty = today, live (localtime,
// checked every minute). Its box is rounded up to the grid, the text at its left (Label fit8).
// Static. Update path: the text in place; style refills. Contrast pairs: colour.json section "clock".
#include <ctime>
#include "fw.h"

namespace {
const char *DAYS[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
const char *MONTHS[] = {"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};
struct DL { std::string fixed; bool lng = false; lv_obj_t *l = nullptr; lv_timer_t *timer = nullptr; };
DL *of(lv_obj_t *o) { return (DL *) lv_obj_get_user_data(o); }
std::string text_of(const DL *d) {
  struct tm tm = {};
  if (!d->fixed.empty()) {
    int y = 2026, m = 10, dd = 19;
    sscanf(d->fixed.c_str(), "%d-%d-%d", &y, &m, &dd);
    tm.tm_year = y - 1900, tm.tm_mon = m - 1, tm.tm_mday = dd, tm.tm_hour = 12;
    timegm(&tm);  // fills in the weekday
  } else {
    const time_t now = time(nullptr);
    localtime_r(&now, &tm);
    if (tm.tm_year + 1900 < 2020) return fw::st::PLACEHOLDER;
  }
  char b[48];
  if (d->lng) snprintf(b, sizeof b, "%s %d %s", DAYS[tm.tm_wday], tm.tm_mday, MONTHS[tm.tm_mon]);
  else snprintf(b, sizeof b, "%.3s %d %.3s", DAYS[tm.tm_wday], tm.tm_mday, MONTHS[tm.tm_mon]);  // cased by the style
  return b;
}
void set(lv_obj_t *root) {
  auto *d = of(root);
  const fw::ty::Id st = d->lng ? fw::ty::date_long : fw::ty::c_date;
  const std::string s = fw::cased(st, text_of(d).c_str());
  if (s == lv_label_get_text(d->l)) return;
  fw::set_text(d->l, st, s.c_str());
  lv_obj_set_width(d->l, LV_SIZE_CONTENT);
  lv_obj_update_layout(d->l);
  lv_obj_set_width(d->l, (lv_obj_get_width(d->l) + fw::sp::GRID - 1) / fw::sp::GRID * fw::sp::GRID);
}
void fill(lv_obj_t *root, fw::Props p) {
  auto *d = of(root);
  d->lng = !strcmp(p["style"] | "short", "long");
  d->fixed = p["date"] | "";
  lv_obj_remove_style_all(root);
  lv_obj_remove_flag(root, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_size(root, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  const fw::ty::Id st = d->lng ? fw::ty::date_long : fw::ty::c_date;
  d->l = fw::text(root, st, "", fw::T->text_dim);
  lv_obj_set_height(d->l, fw::style(st).box);
  set(root);
  if (d->fixed.empty() && !d->timer) d->timer = lv_timer_create([](lv_timer_t *t) { set((lv_obj_t *) lv_timer_get_user_data(t)); }, 60000, root);
  if (!d->fixed.empty() && d->timer) { lv_timer_delete(d->timer); d->timer = nullptr; }
  fw::shape(root, d->lng ? "long" : "short");
}
}  // namespace

FW_COMPONENT(date_line, "Date line", "components", R"({
  "about": "The date: short (MON 19 OCT, mono caps) or long (Monday 19 October).",
  "frame": [400, 64],
  "props": [
    {"name": "style", "type": "select", "options": ["short", "long"], "default": "short"},
    {"name": "date", "type": "text", "default": ""}
  ],
  "variants": [
    {"label": "short", "props": {"date": "2026-10-19"}},
    {"label": "long", "props": {"date": "2026-10-19", "style": "long"}}
  ]
})") {
  auto *d = new DL;
  lv_obj_t *root = lv_obj_create(parent);
  lv_obj_set_user_data(root, d);
  lv_obj_add_event_cb(root, [](lv_event_t *e) {
    auto *d = (DL *) lv_event_get_user_data(e);
    if (d->timer) lv_timer_delete(d->timer);
    delete d;
  }, LV_EVENT_DELETE, d);
  fill(root, p);
}

FW_UPDATE(date_line) {
  auto *d = of(root);
  if (!d) return false;
  if (!fw::shape(root, !strcmp(p["style"] | "short", "long") ? "long" : "short")) {
    fw::refill(root);
    fill(root, p);
    return true;
  }
  d->fixed = p["date"] | "";
  set(root);
  return true;
}
