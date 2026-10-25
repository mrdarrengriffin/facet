// Bar chart: the Facet bar chart (docs: Components > Charts > Bar chart, @facet/ui charts.ts barChart(), CSS .fch-bars):
// bars on a baseline, coloured by token, with a dotted now marker; press and drag to read a bar; bars grow from the
// baseline when shown. Built with the Charts foundation's bars (core/chart.h fw::chart::bars, the same bars the
// Charts page shows): rounded data end (ch::bar_radius) with a square foot, ch::bar_gap between bars, the bar width
// snapped to the grid and the chart hugging its bars, ch::bar_min for the smallest non-zero bar, baseline line-hi,
// now marker = the dotted cursor (text-dim).
//   colour   bands (default, the docs explorer: each bar in its price band's colour, BAND_COLOUR cheap blue / normal
//            band-normal / peak accent) or series (series 1, the now bar in the highlight, later bars outline-only)
//   scrub    press and drag: the bar under the finger takes the select colour, a readout (value + unit + hour) over
//            it, a tick per bar crossed (the slider rule), "scrub" on release (detail "index value"); docs onTap(i)
//   motion   chart motion "bars": they grow up one after another (stagger), new values move every height together
//            (the update path), reduced = a fade at their heights; workbench "opening": false = a rebuild, no growth
// No data = NAN / "-" in values (an empty slot). Update path: same number of bars + same shape = the heights move
// (chart motion bars, silent); anything else (count, colour mode, bands, now, size) refills (snap).
// Contrast pairs: colour.json section "chart" (series, highlight, select, baseline, cursor) + "rate_strip" (bands).
#include "fw.h"
#include "chart.h"

namespace {
const char *DEF_VALUES = "0.3,0.2,0.2,0.2,0.3,0.4,0.9,1.2,0.8,0.5,0.4,0.6,0.7,0.5,0.6,1.1,1.6,1.9,1.4,1.0,0.8,0.6,0.4,0.3";
const char *DEF_BANDS = "cheap,cheap,cheap,cheap,cheap,cheap,normal,normal,normal,normal,normal,normal,normal,normal,normal,normal,peak,peak,peak,normal,normal,normal,cheap,cheap";
lv_color_t band_colour(const fw::Theme &t, const std::string &b) {  // docs BAND_COLOUR
  return b == "peak" ? fw::ch::ink::highlight(t) : b == "cheap" ? t.blue : t.band_normal;
}
std::string sig_of(fw::Props p, size_t n) {
  char b[96];
  snprintf(b, sizeof b, "%zu|%s|%d|%d|%d|%d|", n, p["colour"] | "bands", (int) (p["now"] | 14), (int) (p["width"] | 320),
           (int) (p["height"] | 80), (int) (p["scrub"] | true));
  return std::string(b) + (p["bands"] | DEF_BANDS);
}
fw::chart::Bars *of(lv_obj_t *root) {
  lv_obj_t *b = lv_obj_get_child(root, 0);
  return b ? (fw::chart::Bars *) lv_obj_get_user_data(b) : nullptr;
}
void fill(lv_obj_t *root, fw::Props p, bool opening) {
  const fw::Theme &t = *fw::T;
  lv_obj_remove_style_all(root);
  lv_obj_remove_flag(root, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_size(root, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  fw::chart::BarOpts o;
  o.w = fw::px((float) (p["width"] | 320));
  o.h = fw::px((float) (p["height"] | 80));
  o.v = fw::chart::values(p, "values", DEF_VALUES);
  const int n = (int) o.v.size(), now = p["now"] | 14;
  o.now = now < n ? now : -1;
  o.scrub = p["scrub"] | true;
  o.unit = "kWh", o.dec = 2, o.first_hour = 0;
  if (!strcmp(p["colour"] | "bands", "series")) {
    o.kind.assign(n, 0);
    for (int i = 0; i < n; i++) o.kind[i] = now < 0 ? 0 : i == now ? 1 : i > now ? 2 : 0;
  } else {
    const std::vector<std::string> bands = fw::chart::words(p, "bands", DEF_BANDS);
    for (int i = 0; i < n; i++) o.col.push_back(band_colour(t, i < (int) bands.size() ? bands[i] : "normal"));
  }
  fw::chart::Bars *B = fw::chart::bars(root, t, o);
  if (opening) fw::chart::bars_play(B);
  fw::shape(root, sig_of(p, o.v.size()));
}
}  // namespace

FW_COMPONENT(bar_chart, "Bar chart", "components", R"({
  "about": "Bars on a baseline, coloured by token, with a dotted now marker. Press and drag to read a bar; they grow from the baseline when shown.",
  "frame": [448, 160],
  "props": [
    {"name": "values", "about": "The height of each bar, as numbers separated by commas, one bar per number from left to right.", "type": "text", "default": "0.3,0.2,0.2,0.2,0.3,0.4,0.9,1.2,0.8,0.5,0.4,0.6,0.7,0.5,0.6,1.1,1.6,1.9,1.4,1.0,0.8,0.6,0.4,0.3"},
    {"name": "colour", "about": "How the bars are coloured: bands colours each bar by its price band (cheap, normal, peak); series draws every bar in the one chart colour.", "type": "select", "options": ["bands", "series"], "default": "bands"},
    {"name": "now", "about": "Which bar is now, counting from 0 on the left; it gets the dotted now marker. -1 shows no marker.", "type": "range", "min": -1, "max": 23, "step": 1, "default": 14},
    {"name": "width", "about": "How wide the whole chart is, in pixels.", "type": "range", "min": 160, "max": 440, "step": 8, "default": 320},
    {"name": "height", "about": "How tall the tallest bar can be, in pixels.", "type": "range", "min": 48, "max": 136, "step": 8, "default": 80},
    {"name": "scrub", "about": "Lets you press and drag along the chart to read the value of each bar.", "type": "bool", "default": true}
  ],
  "opening": "Showing the chart grows the bars from the baseline (chart motion bars).",
  "variants": [
    {"label": "price bands · now 14:00", "props": {}},
    {"label": "series · now · later as outlines", "props": {"colour": "series"}},
    {"label": "a week", "props": {"values": "8.2,7.4,9.1,6.8,7.7,10.4,9.6", "now": 6, "colour": "series", "width": 200}},
    {"label": "no data in places", "props": {"values": "0.4,0.6,-,-,0.9,1.2,0.8,-,0.5,0.7", "now": -1, "colour": "series", "width": 240}}
  ]
})") {
  lv_obj_t *root = lv_obj_create(parent);
  fill(root, p, p["opening"] | true);
}

FW_UPDATE(bar_chart) {
  fw::chart::Bars *B = of(root);
  const std::vector<float> nv = fw::chart::values(p, "values", DEF_VALUES);
  if (!B || !fw::shape(root, sig_of(p, nv.size()))) {
    fw::refill(root);
    fill(root, p, false);
    return true;
  }
  bool same = true;
  for (size_t i = 0; i < nv.size(); i++) same = same && ((std::isnan(nv[i]) && std::isnan(B->v[i])) || nv[i] == B->v[i]);
  if (!same) {
    // a new largest value rescales the chart: keep the old bars' heights where they are (their values scaled into the
    // new max), then every height moves together (chart motion bars)
    float mx = 0;
    for (float x : nv) if (std::isfinite(x)) mx = std::max(mx, x);
    mx = mx > 0 ? mx : 1;
    if (mx != B->max) { for (float &x : B->v) x *= mx / B->max; B->max = mx; }
    fw::chart::bars_play(B, &nv);
  }
  return true;
}
