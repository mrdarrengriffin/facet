// Weather days: the Weather screen's "next 5 days" body (docs screens/weather.ts daysView; the factory spec
// renderer/specs/weather_days.json adds the header "5 days" and back): one row per day, lay::list_row_h tall with a
// hairline between: the day's name (title, text: Today, Saturday, ...), its weather icon (ic::feature), the condition
// and rain (body, text-dim: "Rain · 4.2 mm"), then at the right the low (body, text-dim) and the high (title, text).
// Data = the weather feed's daily forecast (core/weather.h; the sample until a panel fetches forecasts); now = a fixed
// time (ms, tests). Static to touch; update path: texts and icons in place. Contrast: "weather" + "list_row".
#include <ctime>
#include "fw.h"
#include "weather.h"

namespace {
struct Row { lv_obj_t *name, *ic, *cond, *lo, *hi; };
struct WD { Row rows[5]; };
WD *of(lv_obj_t *o) { return (WD *) lv_obj_get_user_data(o); }
lv_obj_t *box(lv_obj_t *parent) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  return o;
}
void show(lv_obj_t *root, fw::Props p) {
  WD *w = of(root);
  const double f = p["now"] | 0.0, now = f > 0 ? f : (double) time(nullptr) * 1000;
  const fw::wx::Data d = fw::wx::sample(now);
  static const char *WD_NAMES[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
  for (int i = 0; i < 5 && i < d.n_daily; i++) {
    const fw::wx::Day &x = d.daily[i];
    Row &r = w->rows[i];
    const time_t t = (time_t) (x.t / 1000);
    struct tm tm;
    localtime_r(&t, &tm);
    fw::set_text(r.name, fw::ty::title, i == 0 ? "Today" : WD_NAMES[tm.tm_wday]);
    fw::weather_icon_set(r.ic, fw::wx::icon(x.condition, true, x.precip), false);
    char b[48];
    if (x.precip > 0) snprintf(b, sizeof b, "%s \xC2\xB7 %.1f mm", fw::wx::text(x.condition), x.precip);
    else snprintf(b, sizeof b, "%s", fw::wx::text(x.condition));
    fw::set_text(r.cond, fw::ty::body, b);
    fw::set_text(r.lo, fw::ty::body, fw::wx::deg(x.lo).c_str());
    fw::set_text(r.hi, fw::ty::title, fw::wx::deg(x.hi).c_str());
  }
}
}  // namespace

FW_COMPONENT(weather_days, "Weather days", "components", R"({
  "about": "The Weather screen's next five days: one row a day with its icon, the condition and rain, the low and the high.",
  "frame": [480, 400],
  "props": [],
  "variants": [
    {"label": "from Thursday", "props": {"now": 1792926000000}}
  ]
})") {
  const fw::Theme &t = *fw::T;
  auto *w = new WD;
  lv_obj_t *root = lv_obj_create(parent);
  lv_obj_remove_style_all(root);
  lv_obj_remove_flag(root, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_user_data(root, w);
  lv_obj_add_event_cb(root, [](lv_event_t *e) { delete (WD *) lv_event_get_user_data(e); }, LV_EVENT_DELETE, w);
  // the screen content width, inset to inset, like a List row (facet-185: 100% of the column ran edge to edge)
  lv_obj_set_size(root, 2 * fw::lay::card_w + fw::lay::gutter, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
  for (int i = 0; i < 5; i++) {
    lv_obj_t *row = box(root);
    lv_obj_set_size(row, LV_PCT(100), fw::lay::list_row_h);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, fw::sp::s5, 0);
    lv_obj_set_style_pad_hor(row, fw::sp::s4, 0);  // the List row inset
    if (i) {  // the hairline between rows (List row rule)
      lv_obj_set_style_border_side(row, LV_BORDER_SIDE_TOP, 0);
      lv_obj_set_style_border_width(row, fw::stroke::hair, 0);
      lv_obj_set_style_border_color(row, t.line, 0);
    }
    Row &r = w->rows[i];
    r.name = fw::text(row, fw::ty::title, "--", t.text, fw::lay::wx_day_name);
    r.ic = fw::weather_icon(row, "cloudy", fw::ic::feature, false, t.text, t.bg);
    r.cond = fw::text(row, fw::ty::body, "", t.text_dim);
    lv_obj_set_flex_grow(r.cond, 1);  // takes the room between, cut with an ellipsis when a long one meets the temperatures
    lv_obj_set_width(r.cond, 0);
    lv_label_set_long_mode(r.cond, LV_LABEL_LONG_DOT);
    r.lo = fw::text(row, fw::ty::body, "--", t.text_dim);
    r.hi = fw::text(row, fw::ty::title, "--", t.text);
  }
  show(root, p);
}

FW_UPDATE(weather_days) {
  if (!of(root)) return false;
  show(root, p);
  return true;
}
