// Sparkline: the Facet sparkline (docs: Components > Charts > Sparkline, @facet/ui charts.ts sparkline(), CSS
// .fch-spark): a thin line over the values (oldest first) with a dot on the latest, over a faint dotted baseline.
// On the contract (renderer/CONTRACT.md, "contract": 2): built with the Charts foundation's line chart (core/chart.h
// fw::chart::line_chart) in colour token names (LineOpts ink: shared styles, a theme switch repaints it): the line is the smooth A8 stroke
// at chart:line-thin, the end dot chart:dot, no grid, the dotted baseline (docs: a 1 / 3 dash at the bottom), the scale
// from the values themselves (docs min / max) with the plot's own top / bottom room (chart.json plot-top / -bottom).
//   colour  steel / blue (series 1) / accent (highlight) / text-dim (docs choices); any of them passes 3:1 on bg and
//           surface (colour.json section chart)
//   motion  chart motion "draw" on show (the line draws in, the dot rides the tip, one ping); update path: one new
//           reading at the end with the rest shifted = chart motion "shift" (slides left, the dot moves, a ping), a
//           different set of the same length = "morph", anything else refills (snap)
// A read-out (docs: no touch): no events, no sound. Contrast pairs: colour.json section "chart".
#include "fw.h"
#include "fx.h"
#include "chart.h"

namespace {
const char *DEF = "320,300,420,380,900,1200,640,410,380,450,300,299";
const char *ink_of(const char *c) {  // the docs choices as colour tokens: steel (series 2), blue (series 1), accent (highlight), text-dim
  for (const char *k : {"blue", "accent", "text-dim"}) if (!strcmp(c, k)) return k;
  return "steel";
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
// the chart tokens the line is rastered at: a tune / theme that moves one (STYLE_CHANGED) rebuilds it at its values
std::string geo() {
  std::string g;
  for (const char *t : {"chart:spark-h", "chart:line-thin", "chart:dot", "chart:ping", "chart:plot-top", "chart:plot-bottom", "chart:dash", "chart:dash-gap", "chart:grid", "stroke:edge"})
    g += std::to_string(fx::size(t)) + ",";
  return g;
}
struct Spark { int width = 148; bool dot = true; std::string colour = "steel", geo; };
void spark_free(lv_event_t *e) { delete (Spark *) lv_event_get_user_data(e); }
Spark *spark_of(lv_obj_t *root) {
  for (uint32_t i = 0, n = lv_obj_get_event_count(root); i < n; i++) {
    lv_event_dsc_t *d = lv_obj_get_event_dsc(root, i);
    if (lv_event_dsc_get_cb(d) == spark_free) return (Spark *) lv_event_dsc_get_user_data(d);
  }
  return nullptr;
}
fw::chart::Line *of(lv_obj_t *root) {
  lv_obj_t *c = lv_obj_get_child(root, 0);
  return c ? (fw::chart::Line *) lv_obj_get_user_data(c) : nullptr;
}
void draw(lv_obj_t *root, const Spark &k, std::vector<float> v, bool opening) {
  for (const char *t : {"box", "hug"}) fx::use(root, t);
  fw::chart::LineOpts o{fw::px((float) k.width), fx::size("chart:spark-h"), std::move(v)};
  range(o.v, &o.lo, &o.hi);
  o.weight = fx::size("chart:line-thin"), o.grid = false, o.baseline_dots = true, o.dot = k.dot;
  o.ink = ink_of(k.colour.c_str());
  fw::chart::Line *L = fw::chart::line_chart(root, o);
  if (opening) fw::chart::draw_in(L);
}
void retune(lv_event_t *e) {
  lv_obj_t *root = (lv_obj_t *) lv_event_get_current_target(e);
  Spark *k = spark_of(root);
  fw::chart::Line *L = of(root);
  if (!k || !L || lv_event_get_target(e) != root || k->geo == geo()) return;
  k->geo = geo();
  const std::vector<float> v = L->v;
  fw::refill(root);
  draw(root, *k, v, false);
}
void fill(lv_obj_t *root, fw::Props p, bool opening) {
  Spark *k = spark_of(root);
  if (!k) {
    k = new Spark();
    lv_obj_add_event_cb(root, spark_free, LV_EVENT_DELETE, k);
    lv_obj_add_event_cb(root, retune, LV_EVENT_STYLE_CHANGED, nullptr);
  }
  k->width = p["width"] | 148, k->dot = p["dot"] | true, k->colour = p["colour"] | "steel", k->geo = geo();
  draw(root, *k, fw::chart::values(p, "values", DEF), opening);
  fw::shape(root, sig_of(p));
}
}  // namespace

FW_COMPONENT(sparkline, "Sparkline", "components", R"({
  "about": "A thin line over the recent values with a dot on the latest one.",
  "contract": 2,
  "parts": [],
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
  lv_obj_remove_style_all(root);
  lv_obj_remove_flag(root, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  fx::name(root, "sparkline");
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
