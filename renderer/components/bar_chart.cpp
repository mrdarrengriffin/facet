// Bar chart: the Facet bar chart (docs: Components > Charts > Bar chart, @facet/ui charts.ts barChart(), CSS .fch-bars),
// on the contract (renderer/CONTRACT.md, "contract": 2): bars on a baseline, coloured by token, with a dotted now
// marker; press and drag to read a bar; bars grow from the baseline when shown. Built with the Charts foundation's
// token bars (core/chart.h fw::chart::token_bars: the Charts page's bars drawn through the Pen with colour token
// names, so a theme switch repaints them): rounded data end (ch::bar_radius) with a square foot, ch::bar_gap between bars, the bar width
// snapped to the grid and the chart hugging its bars, ch::bar_min for the smallest non-zero bar, baseline line-hi,
// now marker = the dotted cursor (text-dim).
//   colour   bands (default, the docs explorer: each bar in its price band's colour, BAND_COLOUR cheap blue / normal
//            band-normal / peak accent) or series (series 1, the now bar in the highlight, later bars outline-only)
//   scrub    press and drag: the bar under the finger takes the select colour, a readout (value + unit + hour) over
//            it, a tick per bar crossed (the slider rule), "scrub" on release (detail "index value"); docs onTap(i)
//   width    dp, 0 = its parent's content width (a screen's full column: the bars keep their grid width and the gaps
//            share the rest, so the chart spans the column; > 0 = the bars snap to the grid, the chart hugs them)
//   compare  (2026-10-29, the Energy screen's Week view; docs barChart group 2) the earlier period, one number per
//            value: the bars come in pairs (the earlier one an outline in line-hi, then the value solid in text-dim), ch
//            bar-gap inside a pair, chart:group-gap between pairs; the colour mode is series, no now marker
//   labels   one word per value (or pair) on the axis under its centre ("Mon,...,Today"), in place of the hour ticks;
//            the readout names it ("16.2 kWh  Tue · week before")
//   legend   the series' words over the plot, each after a legend-sw swatch: "this week,week before" (the second
//            = the compare outline)
//   ticks    hours between axis labels (0 = none): "00" "06" ... in the Charts axis ink + c-meta-sm under their bar's
//            centre, sp::s1 below the baseline, clamped inside the chart width
//   motion   chart motion "bars": they grow up one after another (stagger), new values move every height together
//            (the update path), reduced = a fade at their heights; workbench "opening": false = a rebuild, no growth
// No data = NAN / "-" in values (an empty slot). Update path: same number of bars + same shape = the heights move
// (chart motion bars, silent); anything else (count, colour mode, bands, now, size, pairs, labels, legend) refills (snap).
#include <cmath>
#include "fw.h"
#include "fx.h"
#include "chart.h"

namespace {
const char *DEF_VALUES = "0.3,0.2,0.2,0.2,0.3,0.4,0.9,1.2,0.8,0.5,0.4,0.6,0.7,0.5,0.6,1.1,1.6,1.9,1.4,1.0,0.8,0.6,0.4,0.3";
const char *DEF_BANDS = "cheap,cheap,cheap,cheap,cheap,cheap,normal,normal,normal,normal,normal,normal,normal,normal,normal,normal,peak,peak,peak,normal,normal,normal,cheap,cheap";
const char *band_ink(const std::string &b) {  // docs BAND_COLOUR: peak = the chart highlight (accent)
  return b == "peak" ? "fg:accent" : b == "cheap" ? "fg:blue" : "fg:band-normal";
}
int now_of(fw::Props p) {  // a number, or a bound Home Assistant state (text: "10")
  return p["now"].is<const char *>() ? atoi(p["now"].as<const char *>()) : (int) lround(p["now"] | 14.0);  // binds give doubles
}
std::string sig_of(fw::Props p, size_t n) {
  char b[96];
  snprintf(b, sizeof b, "%zu|%s|%d|%d|%d|%d|", n, p["colour"] | "bands", now_of(p), (int) (p["width"] | 320),
           (int) (p["height"] | 80), (int) (p["scrub"] | true) + 2 * (int) (p["ticks"] | 0));
  return std::string(b) + (p["bands"] | DEF_BANDS) + "|" + (*(p["compare"] | "") ? "pairs" : "") + "|" + (p["labels"] | "") + "|" + (p["legend"] | "");
}
// the bars as drawn: with compare, pairs (earlier, value) per slot
std::vector<float> bars_of(fw::Props p) {
  const std::vector<float> v = fw::chart::values(p, "values", DEF_VALUES);
  if (!*(p["compare"] | "")) return v;
  const std::vector<float> c = fw::chart::values(p, "compare", "");
  std::vector<float> o;
  for (size_t i = 0; i < v.size(); i++) o.push_back(i < c.size() ? c[i] : NAN), o.push_back(v[i]);
  return o;
}
void fill(lv_obj_t *root, fw::Props p, bool opening) {
  fw::chart::TokenBars o;
  o.w_dp = p["width"] | 320;  // 0 = its parent's content width (the bars still snap to the grid)
  for (const char *t : {"box", "col"}) fx::use(root, t);
  if (o.w_dp > 0) fx::use(root, "hug");  // else the full width it is given (the bars span it)
  o.h_dp = p["height"] | 80;
  o.v = bars_of(p);
  const bool pairs = *(p["compare"] | "");
  const int n = (int) o.v.size(), now = pairs ? -1 : now_of(p);
  o.now = now < n ? now : -1;
  o.scrub = p["scrub"] | true;
  o.ticks = p["ticks"] | 0;
  if (*(p["labels"] | "")) o.labels = fw::chart::words(p, "labels", "");
  if (*(p["legend"] | "")) {
    o.legend = fw::chart::words(p, "legend", "");
    for (size_t k = 0; k < o.legend.size(); k++) o.legend_kind.push_back(pairs && k == 1 ? 2 : 0);
  }
  float mx = 0;
  for (float x : o.v) if (std::isfinite(x)) mx = std::max(mx, x);
  o.dec = mx >= 10 ? 1 : 2;  // a day's kWh "16.2", a half-hour's "0.42"
  if (pairs) {
    o.group = 2;
    for (int i = 0; i < n; i++) o.kind.push_back(i % 2 ? 0 : 2);  // the earlier period an outline, the value solid
  } else if (!strcmp(p["colour"] | "bands", "series")) {
    o.kind.assign(n, 0);
    for (int i = 0; i < n; i++) o.kind[i] = now < 0 ? 0 : i == now ? 1 : i > now ? 2 : 0;
  } else {
    const std::vector<std::string> bands = fw::chart::words(p, "bands", DEF_BANDS);
    for (int i = 0; i < n; i++) o.ink.push_back(band_ink(i < (int) bands.size() ? bands[i] : "normal"));
  }
  fw::chart::token_bars(root, o, opening);
  fw::shape(root, sig_of(p, o.v.size()));  // (bars_of: the pairs counted)
}
}  // namespace

FW_COMPONENT(bar_chart, "Bar chart", "components", R"({
  "about": "Bars on a baseline, coloured by token, with a dotted now marker. Press and drag to read a bar; they grow from the baseline when shown.",
  "contract": 2,
  "parts": [],
  "frame": [448, 160],
  "props": [
    {"name": "values", "about": "The height of each bar, as numbers separated by commas, one bar per number from left to right.", "type": "text", "default": "0.3,0.2,0.2,0.2,0.3,0.4,0.9,1.2,0.8,0.5,0.4,0.6,0.7,0.5,0.6,1.1,1.6,1.9,1.4,1.0,0.8,0.6,0.4,0.3"},
    {"name": "colour", "about": "How the bars are coloured: bands colours each bar by its price band (cheap, normal, peak); series draws every bar in the one chart colour.", "type": "select", "options": ["bands", "series"], "default": "bands"},
    {"name": "now", "about": "Which bar is now, counting from 0 on the left; it gets the dotted now marker. -1 shows no marker.", "type": "range", "min": -1, "max": 23, "step": 1, "default": 14},
    {"name": "width", "about": "How wide the whole chart is, in pixels; 0 fills the space it is in.", "type": "range", "min": 0, "max": 440, "step": 8, "default": 320},
    {"name": "height", "about": "How tall the tallest bar can be, in pixels.", "type": "range", "min": 48, "max": 136, "step": 8, "default": 80},
    {"name": "scrub", "about": "Lets you press and drag along the chart to read the value of each bar.", "type": "bool", "default": true},
    {"name": "ticks", "about": "Hour labels on the axis under the bars, every this many hours (6 = 00, 06, 12, 18); 0 shows none.", "type": "range", "min": 0, "max": 12, "step": 1, "default": 0},
    {"name": "compare", "about": "An earlier period to set the values against, one number per value: the bars come in pairs, the earlier one as an outline. Empty shows single bars.", "type": "text", "default": ""},
    {"name": "labels", "about": "One word per value (or pair) on the axis under the bars, such as the days of a week; they replace the hour labels.", "type": "text", "default": ""},
    {"name": "legend", "about": "Words naming the series over the bars, each after its swatch: the values first, then the compare period.", "type": "text", "default": ""}
  ],
  "opening": "Showing the chart grows the bars from the baseline (chart motion bars).",
  "variants": [
    {"label": "price bands · now 14:00", "props": {}},
    {"label": "series · now · later as outlines", "props": {"colour": "series"}},
    {"label": "a week", "props": {"values": "8.2,7.4,9.1,6.8,7.7,10.4,9.6", "now": 6, "colour": "series", "width": 200}},
    {"label": "a week against the week before", "props": {"values": "16.2,17.8,15.1,18.4,16.9,19.2,9.4", "compare": "17.4,18.2,16.8,19.0,17.7,18.9,17.1", "labels": "Fri,Sat,Sun,Mon,Tue,Wed,Today", "legend": "this week,week before", "width": 0, "height": 96}},
    {"label": "no data in places", "props": {"values": "0.4,0.6,-,-,0.9,1.2,0.8,-,0.5,0.7", "now": -1, "colour": "series", "width": 240}}
  ]
})") {
  lv_obj_t *root = lv_obj_create(parent);
  lv_obj_remove_style_all(root);
  lv_obj_remove_flag(root, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  fx::name(root, "bar_chart");
  fill(root, p, p["opening"] | true);
}

// Update path: same number of bars + same shape = the heights move (chart motion bars, silent); anything else refills (snap).
FW_UPDATE(bar_chart) {
  lv_obj_t *s = lv_obj_get_child(root, 0);
  if (!s || !fw::shape(root, sig_of(p, bars_of(p).size())) ||
      !fw::chart::token_bars_move(s, bars_of(p))) {
    fw::refill(root);
    fill(root, p, false);
  }
  return true;
}
