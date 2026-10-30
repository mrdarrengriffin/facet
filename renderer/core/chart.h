// Charts library (Foundations > Charts, tokens/chart.json -> chart_gen.h fw::ch::): the line chart (series line +
// area as A8 masks, end dot, ping, draw-in, shift, morph, scrub), bars (rounded data end, square foot, highlight /
// compare, grow, scrub), the gauge, legend, axis and big number. Implemented in foundations/chart.cpp (the Charts page
// shows them); every chart component (Bar chart, Sparkline, Big number, Donut, Legend and axis, Moon by day) builds
// with these, so the page and the components are one system (parity batch B, 2026-10-22). Header-only declarations:
// the firmware bundle picks up core/*.h by itself, and foundations/chart.cpp is bundled like every foundation.
#pragma once
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
#include "fw.h"
#include "chart_gen.h"

namespace fw::chart {

// ---- line chart
struct Line {
  const Theme *t = nullptr;
  lv_obj_t *root = nullptr, *track = nullptr, *area = nullptr, *stroke = nullptr, *dot = nullptr, *ping = nullptr;
  lv_obj_t *cur = nullptr, *sdot = nullptr, *read = nullptr;
  lv_draw_buf_t *lb = nullptr, *ab = nullptr;
  std::vector<float> v, from, to;
  float lo = 0, hi = 1;
  int w = 0, h = 0, tw = 0, weight = 2, series = 0, reveal = -1, scrub = -1, first_hour = 0;
  float y0 = 0, y1 = 0;  // shift: the end dot's old / new y
  bool area_on = false, dot_on = true, dashed = false, unavailable = false, quiet_ticks = false, shifting = false;
  const char *unit = "°";
  lv_color_t colour_override;   // used when has_colour (a sparkline in any token colour; the series colours otherwise)
  bool has_colour = false;
};
struct LineOpts {
  int w, h;
  std::vector<float> v;
  float lo = 12, hi = 22;
  int series = 0, weight = ch::line, first_hour = 0;
  bool area = false, dot = true, grid = true, scrub = false, dashed = false, unavailable = false;
  int now = -1;  // index of a dotted now marker
  bool baseline_dots = false;  // a dotted baseline instead of the grid (the docs sparkline)
  bool has_colour = false;     // colour instead of the series colour
  lv_color_t colour;
  // a colour token name ("steel") instead: the line, end dot and ping take it as shared styles (ink: / bg: / border:)
  // and the dotted baseline bg:line, so a theme / accent switch repaints them (the Sparkline on the contract)
  const char *ink = nullptr;
};
Line *line_chart(lv_obj_t *parent, const Theme &t, const LineOpts &o);
Line *line_chart(lv_obj_t *parent, const LineOpts &o);  // in the live palette (a contract component: inks by o.ink)
void draw_in(Line *L);                             // the line draws itself left to right (chart motion draw)
void push(Line *L, float nv);                      // a new reading slides in (shift + ping)
void morph(Line *L, const std::vector<float> &nv); // every point moves (same length)
void ping(Line *L);
int plot_top(const Line *L);
int plot_bottom(const Line *L);

// ---- bars
struct Bars {
  const Theme *t = nullptr;
  lv_obj_t *root = nullptr, *plot = nullptr, *read = nullptr;
  std::vector<lv_obj_t *> b;
  std::vector<float> v, from;
  std::vector<uint8_t> kind;      // 0 series, 1 highlight, 2 compare (outline only)
  std::vector<lv_color_t> col;    // per-bar colour (empty = by kind); a compare bar keeps its outline
  float max = 1;
  int n = 0, bw = 0, h = 0, top = 0, bot = 0, series = 2, sel = -1, gap = 0, group = 1, group_gap = 0;
  int spread = 0;                 // fill: px left over after the grid-snapped bars, shared over the gaps (no groups)
  // readout while scrubbing: "<value> <unit>  <hh>:00" (hours) or "<value> <unit>  <label>" (labels)
  const char *unit = "kWh";
  int dec = 2, first_hour = 0;
  std::vector<std::string> labels;
};
// bars across `w` (bar width snapped to the grid, the chart hugs them), h tall; kind per bar; scrub = press and drag
Bars *bar_chart(lv_obj_t *parent, const Theme &t, int w, int h, const std::vector<float> &v, const std::vector<uint8_t> &kind,
                float max, bool scrub = true, int now = -1);
struct BarOpts {
  int w = 0, h = 0;
  std::vector<float> v;             // NAN = no data (an empty slot)
  std::vector<uint8_t> kind;        // per bar (empty = all series)
  std::vector<lv_color_t> col;      // per bar (empty = by kind)
  float max = 0;                    // 0 = the largest value
  bool scrub = true, baseline = true, readout = true;
  bool fill = false;                // span exactly w: the left-over px widen the gaps (bars keep their grid width)
  int now = -1, gap = -1, group = 1, group_gap = 0;  // gap -1 = ch::bar_gap
  const char *unit = "";
  int dec = 1, first_hour = -1;     // first_hour >= 0: readout "hh:00"; else labels[i] (or the index)
  std::vector<std::string> labels;
};
Bars *bars(lv_obj_t *parent, const Theme &t, const BarOpts &o);
void bars_play(Bars *B, const std::vector<float> *nv = nullptr);  // grow from the baseline, or move to new heights
int bar_x(const Bars *B, int i);  // a bar's left edge in the chart

// ---- token bars (the Bar chart on the contract): the bars above drawn through the Pen in one strip, inks = colour token
// names ("fg:blue"), so a theme switch repaints them; the readout and the hour axis are Pen text. w_dp 0 = the parent's
// content width (the gaps share what the grid-snapped bars leave). Returns the strip (made on parent).
struct TokenBars {
  std::vector<float> v;             // NAN = no data
  std::vector<std::string> ink;     // per bar (empty = by kind)
  std::vector<uint8_t> kind;        // 0 series, 1 highlight, 2 compare (outline only)
  int w_dp = 0, h_dp = 80, now = -1, ticks = 0, dec = 2, first_hour = 0;
  bool scrub = true;
  std::string unit = "kWh";
  // pairs (2026-10-29, the Energy screen's Week view: docs barChart group 2 / groupGap 16): bars in groups of `group`,
  // ch bar-gap inside a group, chart:group-gap between groups; labels = one axis word per group under its centre (in
  // place of the hour ticks; the readout names the group too); legend = the series' words over the plot, each with a
  // swatch in its kind's ink (legend_kind: 0 solid, 2 compare outline)
  int group = 1;
  std::vector<std::string> labels, legend;
  std::vector<uint8_t> legend_kind;
  // marks (2026-10-30, the Moon screen's next days: docs moonRangeChart marks): per bar 0 none, 1 a new moon, 2 a full
  // moon, a chart:mark disc over the bar's centre in a row above the plot (space 5 under it)
  std::vector<uint8_t> marks;
  std::vector<std::string> names;  // per bar: the readout's word ("24 Oct"; the axis labels may name only some bars)
};
lv_obj_t *token_bars(lv_obj_t *parent, const TokenBars &o, bool opening);
bool token_bars_move(lv_obj_t *strip, const std::vector<float> &nv);  // same count: the heights move (false = rebuild)
// token strip (the Rate strip on the contract): bands ("cheap" / "normal" / "peak") as runs, a notch at now (-1 = none),
// an hour axis under it every `ticks` hours (0 = none); w_dp 0 = the parent's content width. Returns the strip.
lv_obj_t *token_strip(lv_obj_t *parent, const std::vector<std::string> &bands, int now, int w_dp, int ticks);
bool token_strip_set(lv_obj_t *strip, const std::vector<std::string> &bands, int now, int w_dp, int ticks);  // false = not one

// ---- gauge
struct Gauge { lv_obj_t *root = nullptr, *arc = nullptr, *num = nullptr; float lo = 0, hi = 100, v = 0, shown = 0, start = 0; };
Gauge *gauge(lv_obj_t *parent, const Theme &t, float v, const char *unit);
void gauge_to(Gauge *G, float v);

// ---- labels
void legend(lv_obj_t *parent, const Theme &t, const int *series, const char *const *names, int n);
// the same with any token colours (the docs legend takes colours: price bands); outline[i] = a compare swatch
lv_obj_t *legend_colours(lv_obj_t *parent, const Theme &t, const lv_color_t *cols, const bool *outline, const char *const *names, int n);
void axis(lv_obj_t *parent, const Theme &t, int w, const char *const *labels, const float *at, int n);
// returns the value label (a count-up sets its text)
lv_obj_t *big_number(lv_obj_t *parent, const Theme &t, const char *label, const char *value, const char *unit);
void dotted_v(lv_obj_t *parent, const Theme &t, int x, int y0, int y1);  // the dotted now marker

// ---- data from props: a JSON array, or a comma-separated string (the workbench text control); "-" or empty = no
// data (NAN). words(): the same for text items ("cheap,normal,peak").
inline std::vector<float> values(Props p, const char *key, const char *def) {
  std::vector<float> v;
  if (p[key].is<ArduinoJson::JsonArrayConst>()) {
    for (ArduinoJson::JsonVariantConst x : p[key].as<ArduinoJson::JsonArrayConst>()) v.push_back(x.is<float>() ? x.as<float>() : NAN);
    return v;
  }
  const char *s = p[key] | def;
  while (*s) {
    while (*s == ' ') s++;
    char *e = nullptr;
    const float f = strtof(s, &e);
    v.push_back(e && e != s ? f : NAN);
    s = e && e != s ? e : s;
    while (*s && *s != ',') s++;
    if (*s == ',') s++;
  }
  return v;
}
inline std::vector<std::string> words(Props p, const char *key, const char *def) {
  std::vector<std::string> w;
  if (p[key].is<ArduinoJson::JsonArrayConst>()) {
    for (ArduinoJson::JsonVariantConst x : p[key].as<ArduinoJson::JsonArrayConst>()) w.push_back(x | "");
    return w;
  }
  std::string cur;
  for (const char *s = p[key] | def;; s++) {
    if (!*s || *s == ',') {
      const size_t a = cur.find_first_not_of(' '), b = cur.find_last_not_of(' ');
      w.push_back(a == std::string::npos ? "" : cur.substr(a, b - a + 1));
      cur.clear();
      if (!*s) break;
    } else cur += *s;
  }
  return w;
}

// ---- a change against a reference, as Status chip props (docs @facet/ui charts.ts deltaChip(); was the Delta chip
// component until 2026-10-24): "▲ 12% vs yday" / "▼ 12% vs yday", "= 0% vs yday" when flat (under 0.5 % either way),
// "-- vs yday" with no number (NAN). Tone ok when the change goes the good way, warn the bad way, idle flat / none.
// The arrows are in the text glyph set (type.json, c-caps-sm at both densities).
struct Delta { const char *tone; std::string text; };
inline Delta delta(double pct, bool up_is_good, const char *ref = "vs yday") {
  char b[64];
  if (std::isnan(pct)) { snprintf(b, sizeof b, "-- %s", ref); return {"idle", b}; }
  const bool up = pct >= 0.5, down = pct <= -0.5;
  snprintf(b, sizeof b, "%s %d%% %s", up ? "\xE2\x96\xB2" : down ? "\xE2\x96\xBC" : "=", (int) std::lround(std::fabs(pct)), ref);
  return {!up && !down ? "idle" : up == up_is_good ? "ok" : "warn", b};
}

}  // namespace fw::chart

// ---- decor art (core/bins_art.cpp): a collection's wheelie bins, colours ("green", "black", ...) first in front
namespace fw::art {
// size = the bin height token (layout:bin-hero, layout:bin-card)
lv_obj_t *bins(lv_obj_t *parent, const std::vector<std::string> &cols, const char *size);
bool bins_set(lv_obj_t *s, const std::vector<std::string> &cols, const char *size);  // false = not one
}  // namespace fw::art
