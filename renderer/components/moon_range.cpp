// Moon by day: the Moon screen's long-range chart (docs: Components > Charts > Moon by day, @facet/ui screens/moon.ts
// moonRangeChart(), moon.ts moonDays()): one bar per evening, its height = how much of the moon is lit; full moons in the
// highlight, past days in the compare style, a dotted line on today, a moon above every new and full moon, and month
// names under the 1st. Press and drag to read a day. Built with the Charts foundation's bars (core/chart.h): the bars,
// the now marker and the scrub are the Bar chart's; marks = the Moon phase glyph (core fw::moon_icon) at ic::icon.
//   days    one sp::s1 (4) bar + ch::bar_gap (4) per day = 8 px a day at both densities (docs: 8 px a day at 720, bar 6 +
//           gap 2 off the grid), across lay::content_w (the Moon screen's content width; docs width control)
//   moon    the evening moon (21:00 UTC) from the sun-moon elongation (core/lunar.h, docs moonFrac),
//           illumination (1 - cos 2 pi f) / 2; new / first / full / last = the day the phase crosses 0 / .25 / .5 / .75
//           (docs: Meeus moonFrac; a real screen gets its days from the moon entity, this is the workbench sample)
//   colours full = the chart highlight (accent), before today = compare (line-hi, decor: the readout and the month
//           names carry the days), the rest steel (series 2)
//   rows    marks (ic::icon) sp::s5 chart sp::s5 axis (type label, caps, text-dim: the 1st of each month under its bar,
//           none that would run off the right edge)
// Motion: bars grow on show (chart motion bars). Scrub: "scrub" (detail "index value") + ticks. Update path: refills
// (snap: a new start day redraws the chart). Contrast pairs: colour.json sections "chart" + "moon".
#include "fw.h"
#include "chart.h"
#include "lunar.h"

namespace {
// the moon's place in its month from the sun-moon elongation (core/lunar.h = docs moonFrac; was the mean month)
double frac_at(double unix_days) { return fw::lunar::frac(unix_days * fw::lunar::DAY); }
// days since 1970-01-01 of a civil date (Howard Hinnant's days_from_civil)
long days_from_civil(int y, unsigned m, unsigned d) {
  y -= m <= 2;
  const long era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = (unsigned) (y - era * 400), doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  return era * 146097 + (long) (yoe * 365 + yoe / 4 - yoe / 100 + doy) - 719468;
}
void civil_from_days(long z, int *y, unsigned *m, unsigned *d) {
  z += 719468;
  const long era = (z >= 0 ? z : z - 146096) / 146097;
  const unsigned doe = (unsigned) (z - era * 146097), yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100), mp = (5 * doy + 2) / 153;
  *d = doy - (153 * mp + 2) / 5 + 1;
  *m = mp < 10 ? mp + 3 : mp - 9;
  *y = (int) (yoe + era * 400) + (*m <= 2);
}
const char *MONTHS[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

void fill(lv_obj_t *root, fw::Props p, bool opening) {
  const fw::Theme &t = *fw::T;
  lv_obj_remove_style_all(root);
  lv_obj_remove_flag(root, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  const int W = fw::lay::content_w, slot = fw::sp::s1 + fw::ch::bar_gap, n = (W + fw::ch::bar_gap) / slot;
  lv_obj_set_size(root, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(root, fw::sp::s5, 0);
  const char *start = p["start"] | "2026-10-15";
  int y = 2026, mo = 10, da = 15;
  sscanf(start, "%d-%d-%d", &y, &mo, &da);
  const long d0 = days_from_civil(y, (unsigned) mo, (unsigned) da);
  const int today = p["today"] | 7;
  const bool south = p["south"] | false;
  fw::chart::BarOpts o;
  o.w = W, o.h = fw::px((float) (p["height"] | 160));
  o.max = 100, o.now = today, o.unit = "%", o.dec = 0, o.first_hour = -1, o.scrub = true;
  std::vector<int> ev(n, 0);  // 1 new, 2 full
  for (int i = 0; i < n; i++) {
    const double f0 = frac_at(d0 + i), f1 = frac_at(d0 + i + 1), f = frac_at(d0 + i + 21.0 / 24);
    const auto across = [&](double x) { return f0 < x && f1 >= x; };
    ev[i] = f1 < f0 ? 1 : across(0.5) ? 2 : 0;
    o.v.push_back((float) std::lround(fw::lunar::illumination(f) * 100));
    o.col.push_back(ev[i] == 2 ? fw::ch::ink::highlight(t) : i < today ? fw::ch::series(t, 3) : fw::ch::series(t, 1));
    int yy; unsigned mm, dd;
    civil_from_days(d0 + i, &yy, &mm, &dd);
    char b[16];
    snprintf(b, sizeof b, "%u %s", dd, MONTHS[mm - 1]);
    o.labels.push_back(b);
  }
  // marks row: a moon over every new and full moon
  lv_obj_t *marks = lv_obj_create(root);
  lv_obj_remove_style_all(marks);
  lv_obj_remove_flag(marks, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  const int M = fw::ic::PX[fw::ic::icon];
  lv_obj_set_size(marks, W, M);
  fw::chart::Bars *B = fw::chart::bars(root, t, o);
  for (int i = 0; i < n; i++) {
    if (!ev[i]) continue;
    lv_obj_t *g = fw::moon_icon(marks, ev[i] == 2 ? 0.5f : 0.0f, fw::ic::icon, south);
    if (!g) continue;
    lv_obj_set_pos(g, LV_CLAMP(0, fw::chart::bar_x(B, i) + B->bw / 2 - M / 2, W - M), 0);
  }
  // axis row: month names under the 1st
  lv_obj_t *ax = lv_obj_create(root);
  lv_obj_remove_style_all(ax);
  lv_obj_remove_flag(ax, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_size(ax, W, fw::style(fw::ty::label).box);
  for (int i = 0; i < n; i++) {
    int yy; unsigned mm, dd;
    civil_from_days(d0 + i, &yy, &mm, &dd);
    if (dd != 1) continue;
    lv_obj_t *l = fw::text(ax, fw::ty::label, MONTHS[mm - 1], fw::ch::ink::axis(t));
    lv_obj_update_layout(l);
    const int x = fw::chart::bar_x(B, i);
    if (x + lv_obj_get_width(l) > W) { lv_obj_delete(l); continue; }  // would run off the edge (docs: none in the last 48)
    lv_obj_set_x(l, x);
  }
  if (opening) fw::chart::bars_play(B);
}
}  // namespace

FW_COMPONENT(moon_range, "Moon by day", "components", R"({
  "about": "One bar per evening, its height how much of the moon is lit: full moons in the accent, past days dimmed, a dotted line on today, a moon over every new and full moon and month names under the 1st. Press and drag to read a day.",
  "frame": [448, 240],
  "props": [
    {"name": "start", "type": "text", "default": "2026-10-15"},
    {"name": "today", "type": "range", "min": 0, "max": 30, "step": 1, "default": 7},
    {"name": "height", "type": "range", "min": 80, "max": 160, "step": 8, "default": 120},
    {"name": "south", "type": "bool", "default": false}
  ],
  "opening": "Showing it grows the bars (chart motion bars).",
  "variants": [
    {"label": "from a week ago", "props": {}},
    {"label": "southern hemisphere", "props": {"south": true}},
    {"label": "from 1 December", "props": {"start": "2026-12-01", "today": 0}}
  ]
})") {
  lv_obj_t *root = lv_obj_create(parent);
  fill(root, p, p["opening"] | true);
}

FW_UPDATE(moon_range) {
  fw::refill(root);
  fill(root, p, false);
  return true;
}
