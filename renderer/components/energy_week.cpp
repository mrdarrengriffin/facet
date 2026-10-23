// Energy week: the Energy screen's "last 7 days" body (docs screens/energy.ts weekView; the factory spec
// renderer/specs/energy_week.json adds the header "Last 7 days"): this week's cost as the hero, use per day against
// the week before, and pairs of bars per day.
//   top      "this week" (type label, text-dim; a tapped bar's day in its place: "Tuesday · 16.2 kWh · £3.88"), the
//            cost (hero), "112.4 kWh · 6% less a day than the week before" (body, text-dim; compared only with at
//            least 3 days on each side), the legend (this week text-dim, week before an outline in line-hi)
//   chart    the Charts foundation's bars (fw::chart::bars): 7 pairs, the week before as compare outlines (line-hi)
//            and this week solid (text-dim), lay::gutter between days, the content width x ch::chart_h; scrub = tap
//            a bar; the day names under them (axis: Mon .. Today)
// Feed (core/energy.h: the last 14 days, today last); now = a fixed time (ms, tests). Static to touch apart from the
// scrub; update path: rebuilt in place (the bars play again). Contrast: "energy" + "chart".
#include <ctime>
#include "chart.h"
#include "energy.h"
#include "fw.h"

namespace {
struct WK { double fixed = 0; lv_obj_t *lbl = nullptr; };
WK *of(lv_obj_t *o) { return (WK *) lv_obj_get_user_data(o); }
std::string day_name(double t, bool longer) {
  static const char *S[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  static const char *L[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
  const time_t s = (time_t) (t / 1000 + 43200);  // noon: the day, whatever the zone
  struct tm tm;
  localtime_r(&s, &tm);
  return longer ? L[tm.tm_wday] : S[tm.tm_wday];
}
void fill(lv_obj_t *root) {
  namespace en = fw::energy;
  WK *w = of(root);
  const fw::Theme &t = *fw::T;
  const double now = w->fixed > 0 ? w->fixed : (double) time(nullptr) * 1000;
  lv_obj_clean(root);
  const en::Data d = en::sample(now);
  const en::DayUse *prev = d.days, *cur = d.days + 7;
  double cost = 0, kwh = 0, a = 0, p = 0;
  for (int i = 0; i < 7; i++) { cost += cur[i].cost; kwh += cur[i].kwh; p += prev[i].kwh; }
  for (int i = 0; i < 6; i++) a += cur[i].kwh;  // complete days only (today is partial)
  a /= 6, p /= 7;
  w->lbl = fw::text(root, fw::ty::label, "this week", t.text_dim, fw::lay::content_w);
  fw::text(root, fw::ty::hero, en::money(cost).c_str(), t.text);
  char b[96];
  const long delta = std::lround((a - p) / p * 100);
  snprintf(b, sizeof b, "%.1f kWh \xC2\xB7 %ld%% %s a day than the week before", kwh, std::labs(delta), delta <= 0 ? "less" : "more");
  fw::text(root, fw::ty::body, b, t.text_dim, fw::lay::content_w);
  const lv_color_t cols[2] = {t.text_dim, t.line_hi};
  const bool outline[2] = {false, true};
  const char *const names[2] = {"this week", "week before"};
  fw::chart::legend_colours(root, t, cols, outline, names, 2);
  fw::chart::BarOpts o;
  o.w = fw::lay::content_w;
  o.h = fw::ch::chart_h;
  o.group = 2, o.group_gap = fw::lay::gutter;
  o.scrub = true, o.readout = false;
  for (int i = 0; i < 7; i++) {
    o.v.push_back((float) prev[i].kwh), o.kind.push_back(2);  // the week before: a compare outline
    o.v.push_back((float) cur[i].kwh), o.kind.push_back(0);
    o.col.push_back(t.line_hi), o.col.push_back(t.text_dim);
  }
  fw::chart::Bars *B = fw::chart::bars(root, t, o);
  lv_obj_set_style_margin_top(B->root, fw::sp::s7, 0);
  const char *labels[7];
  std::string keep[7];
  float at[7];
  for (int i = 0; i < 7; i++) {
    keep[i] = i == 6 ? "Today" : day_name(cur[i].t, false);
    labels[i] = keep[i].c_str();
    at[i] = (i + 0.5f) / 7;
  }
  fw::chart::axis(root, t, fw::lay::content_w, labels, at, 7);
  fw::listen(B->root, [](lv_obj_t *, const char *name, const char *detail, void *user) {  // a tapped bar: its day
    if (strcmp(name, "scrub")) return;
    auto *root = (lv_obj_t *) user;
    WK *w = of(root);
    const int i = atoi(detail);
    const fw::energy::Data d = fw::energy::sample(w->fixed > 0 ? w->fixed : (double) time(nullptr) * 1000);
    if (i < 0 || i >= 14) { fw::set_text(w->lbl, fw::ty::label, "this week"); return; }
    const fw::energy::DayUse &x = d.days[i % 2 ? 7 + i / 2 : i / 2];
    std::string day = i % 2 && i / 2 == 6 ? "today" : day_name(x.t, true);
    if (!(i % 2)) day += " before";
    char b[80];
    snprintf(b, sizeof b, "%s \xC2\xB7 %.1f kWh \xC2\xB7 %s", day.c_str(), x.kwh, fw::energy::money(x.cost).c_str());
    fw::set_text(w->lbl, fw::ty::label, b);
  }, root);
  fw::chart::bars_play(B);
}
}  // namespace

FW_COMPONENT(energy_week, "Energy week", "components", R"({
  "about": "The Energy screen's last 7 days: this week's cost, use per day against the week before, and pairs of bars per day.",
  "frame": [480, 0],
  "props": [],
  "variants": [
    {"label": "to Thursday", "props": {"now": 1792926000000}}
  ]
})") {
  auto *w = new WK;
  lv_obj_t *root = lv_obj_create(parent);
  lv_obj_remove_style_all(root);
  lv_obj_remove_flag(root, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_user_data(root, w);
  lv_obj_add_event_cb(root, [](lv_event_t *e) { delete (WK *) lv_event_get_user_data(e); }, LV_EVENT_DELETE, w);
  lv_obj_set_size(root, fw::lay::content_w, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(root, fw::sp::s2, 0);
  w->fixed = p["now"] | 0.0;
  fill(root);
}

FW_UPDATE(energy_week) {
  WK *w = of(root);
  if (!w) return false;
  w->fixed = p["now"] | 0.0;
  fill(root);
  return true;
}
