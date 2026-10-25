// Sparkline: the Facet sparkline (docs: Components > Charts > Sparkline, @facet/ui charts.ts sparkline(), CSS
// .fch-spark): a thin line over the values (oldest first) with a dot on the latest, over a faint dotted baseline.
// Built with the Charts foundation's line chart (core/chart.h fw::chart::line_chart): the line is the smooth A8 stroke
// at ch::line_thin, the end dot ch::dot, no grid, the dotted baseline (docs: a 1 / 3 dash at the bottom), the scale
// from the values themselves (docs min / max) with the plot's own top / bottom room (chart.json plot-top / -bottom).
//   colour  steel / blue (series 1) / accent (highlight) / text-dim (docs choices); any of them passes 3:1 on bg and
//           surface (colour.json section chart)
//   motion  chart motion "draw" on show (the line draws in, the dot rides the tip, one ping); update path: one new
//           reading at the end with the rest shifted = chart motion "shift" (slides left, the dot moves, a ping), a
//           different set of the same length = "morph", anything else refills (snap)
// A read-out (docs: no touch): no events, no sound. Contrast pairs: colour.json section "chart".
#include "fw.h"
#include "chart.h"

namespace {
const char *DEF = "320,300,420,380,900,1200,640,410,380,450,300,299";
lv_color_t colour_of(const fw::Theme &t, const char *c) {
  return !strcmp(c, "blue") ? fw::ch::series(t, 0) : !strcmp(c, "accent") ? fw::ch::ink::highlight(t)
       : !strcmp(c, "text-dim") ? fw::ch::series(t, 2) : fw::ch::series(t, 1);  // steel = series 2
}
void range(const std::vector<float> &v, float *lo, float *hi) {
  *lo = INFINITY, *hi = -INFINITY;
  for (float x : v) if (std::isfinite(x)) *lo = std::min(*lo, x), *hi = std::max(*hi, x);
  if (!std::isfinite(*lo)) *lo = 0, *hi = 1;
  if (*hi - *lo < 1e-6f) *hi = *lo + 1;  // docs: span || 1
}
std::string sig_of(fw::Props p) {
  char b[64];
  snprintf(b, sizeof b, "%s|%d|%d", p["colour"] | "steel", (int) (p["width"] | 148), (int) (p["dot"] | true));
  return b;
}
fw::chart::Line *of(lv_obj_t *root) {
  lv_obj_t *c = lv_obj_get_child(root, 0);
  return c ? (fw::chart::Line *) lv_obj_get_user_data(c) : nullptr;
}
void fill(lv_obj_t *root, fw::Props p, bool opening) {
  const fw::Theme &t = *fw::T;
  lv_obj_remove_style_all(root);
  lv_obj_remove_flag(root, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_size(root, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  fw::chart::LineOpts o{fw::px((float) (p["width"] | 148)), fw::ch::spark_h, fw::chart::values(p, "values", DEF)};
  range(o.v, &o.lo, &o.hi);
  o.weight = fw::ch::line_thin, o.grid = false, o.baseline_dots = true, o.dot = p["dot"] | true;
  o.has_colour = true, o.colour = colour_of(t, p["colour"] | "steel");
  fw::chart::Line *L = fw::chart::line_chart(root, t, o);
  if (opening) fw::chart::draw_in(L);
  fw::shape(root, sig_of(p));
}
}  // namespace

FW_COMPONENT(sparkline, "Sparkline", "components", R"({
  "about": "A thin line over the recent values with a dot on the latest one.",
  "frame": [240, 64],
  "props": [
    {"name": "values", "about": "The recent values, as numbers separated by commas, oldest first.", "type": "text", "default": "320,300,420,380,900,1200,640,410,380,450,300,299"},
    {"name": "colour", "about": "The line colour: steel, blue, accent, or text-dim for a quiet grey.", "type": "select", "options": ["steel", "blue", "accent", "text-dim"], "default": "steel"},
    {"name": "width", "about": "How wide the line is drawn, in pixels.", "type": "range", "min": 96, "max": 224, "step": 8, "default": 148},
    {"name": "dot", "about": "Puts a dot on the latest value at the right end.", "type": "bool", "default": true}
  ],
  "opening": "Showing it draws the line in from the left (chart motion draw).",
  "variants": [
    {"label": "steel", "props": {}},
    {"label": "blue", "props": {"colour": "blue"}},
    {"label": "accent", "props": {"colour": "accent"}},
    {"label": "text-dim · no dot", "props": {"colour": "text-dim", "dot": false}},
    {"label": "flat", "props": {"values": "5,5,5,5,5,5"}}
  ]
})") {
  lv_obj_t *root = lv_obj_create(parent);
  fill(root, p, p["opening"] | true);
}

FW_UPDATE(sparkline) {
  fw::chart::Line *L = of(root);
  const std::vector<float> nv = fw::chart::values(p, "values", DEF);
  if (!L || !fw::shape(root, sig_of(p)) || nv.size() != L->v.size() || nv.size() < 2) {
    fw::refill(root);
    fill(root, p, false);
    return true;
  }
  float lo, hi;
  range(nv, &lo, &hi);
  auto eq = [](float a, float b) { return (std::isnan(a) && std::isnan(b)) || a == b; };
  bool same = true, shifted = lo == L->lo && hi == L->hi;  // a shift keeps the scale (else the old points jump)
  for (size_t i = 0; i < nv.size(); i++) same = same && eq(nv[i], L->v[i]);
  for (size_t i = 0; i + 1 < nv.size() && shifted; i++) shifted = eq(nv[i], L->v[i + 1]);
  if (same) return true;
  if (shifted) fw::chart::push(L, nv.back());
  else if (lo == L->lo && hi == L->hi) fw::chart::morph(L, nv);
  else { fw::refill(root); fill(root, p, false); }
  return true;
}
