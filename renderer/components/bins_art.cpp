// Bins art: a collection's wheelie bins drawn in their colours (the panel lab's Bins screen, board facet-72, the
// "Faithful" bin: apps/hub panel/bins/bin-art.ts), on the contract (renderer/CONTRACT.md, "contract": 2). Decor: the
// words next to it name the bins (never colour alone).
//   size    hero (the Bins screen: layout:bin-hero) or card (a Home card's preview, top right: layout:bin-card)
//   bin     that tall, 4 / 5 of that wide on the 4 px grid: a handle loop, a slab lid over the body, a band
//           under the lid, three slots. Two flat tones: the bin colour (fg:bin-<colour>) and its shade
//           (fg:bin-<colour>-shade: bin-ink over it at comp bin.shade). Black and grey bins get a stroke:edge line-hi
//           edge on handle, body and lid so they read on the canvas.
//   group   each next bin 55 % of a bin to the right and behind (the first is in front); the box hugs them
// Drawn by core/bins_art.cpp (fw::art::bins) through the Pen in one box (fx::strip), inks by token name: a theme switch repaints it, a tuned bin-hero /
// stroke:edge re-sizes it. Static: no touch, no sound. Update path: new colours redraw in place (silent; a different
// count re-sizes the box). Contrast: decor (tokens/colour.json section "bins").
#include "fw.h"
#include "fx.h"
#include "chart.h"

namespace {
const char *const COLOURS[] = {"black", "green", "brown", "blue", "grey", "purple", "red", "yellow"};
std::vector<std::string> colours(fw::Props p) {  // known colours only (an unknown one = grey)
  std::vector<std::string> v;
  const std::string s = p["colours"] | "green,black";
  for (size_t i = 0; i <= s.size() && v.size() < 4;) {
    size_t j = s.find(',', i);
    if (j == std::string::npos) j = s.size();
    std::string c = s.substr(i, j - i);
    while (!c.empty() && c.front() == ' ') c.erase(0, 1);
    while (!c.empty() && c.back() == ' ') c.pop_back();
    if (!c.empty()) v.push_back(std::any_of(std::begin(COLOURS), std::end(COLOURS), [&](const char *k) { return c == k; }) ? c : "grey");
    i = j + 1;
  }
  return v;
}
const char *size_of(fw::Props p) { return !strcmp(p["size"] | "hero", "card") ? "layout:bin-card" : "layout:bin-hero"; }
}  // namespace

FW_COMPONENT(bins_art, "Bins art", "components", R"({
  "about": "A collection's wheelie bins drawn in their colours, overlapping, the first in front.",
  "contract": 2,
  "parts": [],
  "frame": [320, 200],
  "props": [
    {"name": "colours", "about": "The bins' colours, separated by commas, the first in front: black, green, brown, blue, grey, purple, red or yellow.", "type": "text", "default": "green,black"},
    {"name": "size", "about": "hero is the Bins screen's size; card is the small picture at the top right of a Home card.", "type": "select", "options": ["hero", "card"], "default": "hero"}
  ],
  "variants": [
    {"label": "green + black", "props": {}},
    {"label": "one black bin", "props": {"colours": "black"}},
    {"label": "three", "props": {"colours": "blue,brown,grey"}},
    {"label": "purple + yellow", "props": {"colours": "purple,yellow"}},
    {"label": "card size", "props": {"size": "card"}}
  ]
})") {
  lv_obj_t *root = lv_obj_create(parent);
  lv_obj_remove_style_all(root);
  lv_obj_remove_flag(root, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  fx::name(root, "bins_art");
  for (const char *t : {"box", "col", "hug"}) fx::use(root, t);
  fw::art::bins(root, colours(p), size_of(p));
}

FW_UPDATE(bins_art) {
  for (const char *t : {"box", "col", "hug"}) fx::use(root, t);
  return fw::art::bins_set(lv_obj_get_child(root, 0), colours(p), size_of(p));
}
