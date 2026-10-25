// Legend and axis: the Facet chart labels (docs: Components > Charts > Legend and axis, @facet/ui charts.ts legend() +
// axis()): legend swatches with their words, and time labels at fractions of a width (the first left-aligned, the last
// right-aligned). Built with the Charts foundation's legend + axis (core/chart.h): swatch ch::legend_sw rad::chip, words
// and labels type c-meta-sm in the axis ink (text-dim), sp::s5 between entries, sp::s2 swatch to word.
//   items  "word,word,..." with the docs sample colours in order: blue, band-normal, accent (price bands); an item
//          starting "~" is a compare entry (outline-only swatch, line-hi)
//   ticks  "00,06,12,18,24": spread evenly over the width (docs at = i / (n - 1))
// Static. Update path: refills (snap). Contrast pairs: colour.json section "chart" (axis ink, series) + "rate_strip".
#include "fw.h"
#include "chart.h"

namespace {
void fill(lv_obj_t *root, fw::Props p) {
  const fw::Theme &t = *fw::T;
  lv_obj_remove_style_all(root);
  lv_obj_remove_flag(root, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  const int w = fw::px((float) (p["width"] | 320));
  lv_obj_set_size(root, w, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(root, fw::sp::s5, 0);
  const std::vector<std::string> items = fw::chart::words(p, "items", "cheap 13.5p,normal 26.2p,peak 39.4p");
  const lv_color_t pal[3] = {t.blue, t.band_normal, fw::ch::ink::highlight(t)};
  std::vector<lv_color_t> cols;
  std::vector<std::string> names;
  bool outl[8] = {};
  for (size_t i = 0; i < items.size() && i < 8; i++) {
    const bool cmp = !items[i].empty() && items[i][0] == '~';
    outl[i] = cmp;
    cols.push_back(cmp ? fw::ch::series(t, 3) : pal[i % 3]);
    names.push_back(cmp ? items[i].substr(1) : items[i]);
  }
  std::vector<const char *> np;
  for (auto &n : names) np.push_back(n.c_str());
  if (!np.empty()) fw::chart::legend_colours(root, t, cols.data(), outl, np.data(), (int) np.size());
  const std::vector<std::string> ticks = fw::chart::words(p, "ticks", "00,06,12,18,24");
  std::vector<const char *> tp;
  std::vector<float> at;
  for (size_t i = 0; i < ticks.size(); i++) tp.push_back(ticks[i].c_str()), at.push_back(ticks.size() > 1 ? i / (float) (ticks.size() - 1) : 0);
  if (!tp.empty()) fw::chart::axis(root, t, w, tp.data(), at.data(), (int) tp.size());
}
}  // namespace

FW_COMPONENT(legend, "Legend and axis", "components", R"({
  "about": "Legend swatches and axis labels, on the grid.",
  "frame": [448, 96],
  "props": [
    {"name": "items", "about": "The colour keys, as words separated by commas, each one shown with its swatch, such as cheap 13.5p.", "type": "text", "default": "cheap 13.5p,normal 26.2p,peak 39.4p"},
    {"name": "ticks", "about": "The labels along the bottom axis, separated by commas, spread evenly from left to right.", "type": "text", "default": "00,06,12,18,24"},
    {"name": "width", "about": "How wide the legend is, in pixels.", "type": "range", "min": 160, "max": 440, "step": 8, "default": 320}
  ],
  "variants": [
    {"label": "price bands + a day", "props": {}},
    {"label": "today vs yesterday", "props": {"items": "today,~yesterday", "ticks": "00:00,12:00,now"}}
  ]
})") {
  lv_obj_t *root = lv_obj_create(parent);
  fill(root, p);
}

FW_UPDATE(legend) {
  fw::refill(root);
  fill(root, p);
  return true;
}
