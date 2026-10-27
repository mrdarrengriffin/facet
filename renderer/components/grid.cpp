// Grid: a LAYOUT component of equal cells on the contract (renderer/CONTRACT.md; board facet-171 pieces 2 + 3, moved
// onto the contract in facet-205). The geometry is the core atom fx::grid (core/fx_core.cpp, C8): this file says only
// what goes in which cell.
//   columns  cols 1-8, or min (an icon size step): as many min-wide cells + gaps as fit the grid's OWN width (C6: 720
//            and 480 get their own count, at most 12). Cell width = (width - gaps) / cols snapped down to the 4 px grid;
//            half the leftover (on the grid) on the left, so the cells sit centred within 4 px
//   gap      none | near (space:3) | group (layout:gutter) | block (layout:group), read live (the Layout lab tunes them)
//   aspect   square (cell height = width) | wide (16:9, on the grid) | row (the content's height: rows that flow)
//   rows     0 = no cap; else at most rows x cols cells: an items list longer than that shows rows x cols - 1 items
//            and a last "+N" cell (N = the rest), instead of a partly shown grid
// Slots (meta "slots", C8a: core/spec.h refuses anything else with a path error): "cell" = 64 numbered cells, one
//   component each from the allow list; "item" = the items template's component.
// Static mode (no items): slot n = cell n in reading order (spec nodes go in with "in" + "slot"); a node may add
//   "span": [cols, rows] (core/spec.h hands the grid its nodes' spans as _spans and the cell count as _cells); cells
//   are placed in reading order into the first free place that fits. Built alone (the workbench), cells are outlined
//   and named.
// Items mode: items = a comma list or a JSON list (usually bound to an entity attribute: {"e", "a"}); one cell per
//   item from the spec node's "cell" template (core/spec.h hands it over as _cell; none = an Icon named by the item,
//   fit: cell). "$item" in the template = the item (an object item: its id, else its name), "$item.<key>" = its field.
//   At most ITEMS_MAX cells. An empty list = the empty text (not blank). A tap on an item cell emits "pick" with the
//   item + sound tap (actions: screen.param, nav.open); cells smaller than touch:min (a row of hearts) are shown only,
//   never tapped (fx::grid_cell tap, C32).
// Steps: the column count in min mode (a new width re-places the cells; an items grid with a rows cap re-expands).
//   Declared minimum: one tile-w square (meta "min"). Items cells are made again after a new size (async), so their
//   contents fit; a static grid's nodes are re-placed with their cells. States: n/a (its cells' components have theirs). Bleed: none.
// Update path: a static grid keeps its cells (the nodes inside update themselves); an items grid re-expands: a changed
//   item rebinds its cell in place (an Icon cross-fades), cells after the first change are made again, the "+N" cell
//   follows the count. Another shape (cols, min, gap, aspect, rows, outline) = rebuilt (false). Silent.
// Contrast: section "grid" (outline sample, +N, empty text).
#include <map>
#include "fw.h"
#include "fx.h"
#include "spec.h"
#include "items.h"

namespace {
using namespace fw;
using namespace fw::items;
constexpr int ITEMS_MAX = 64;  // grid_items: cells an items grid makes at most
struct G {
  std::string cell, sig, src, empty;  // the items template (JSON, "" = an Icon by the item); the structural props; the list
  std::vector<std::string> items;     // the items shown now (an object item kept as its JSON)
  std::vector<lv_obj_t *> cells;      // the item cells in order (+N not included)
  lv_obj_t *more = nullptr;           // the "+N" cell, or the empty text
  bool items_mode = false, queued = false;
  int rows = 0;
};
std::map<lv_obj_t *, G> all;
std::string sig(Props p) {
  return std::to_string(p["cols"] | 3) + "|" + (p["min"] | "none") + "|" + (p["gap"] | "group") + "|" + (p["aspect"] | "square") +
         "|" + std::to_string(p["rows"] | 0) + "|" + ((p["outline"] | false) ? "o" : "");
}
// the props of item i's cell from the template (comp out)
void cell_props(const G &g, const std::string &item, std::string &comp, ArduinoJson::JsonDocument &p) {
  ArduinoJson::JsonDocument t;
  if (g.cell.empty() || deserializeJson(t, g.cell)) {
    t["c"] = "icon";
    t["p"]["name"] = item.size() && item[0] == '{' ? "$item.icon" : "$item";
    t["p"]["fit"] = "cell";
  }
  comp = t["c"] | "icon";
  p.set(t["p"]);
  if (p.isNull()) p.to<ArduinoJson::JsonObject>();
  with_item(p.as<ArduinoJson::JsonVariant>(), item);
}
// ---- static cells
void build_static(lv_obj_t *r, Props p) {
  const int rows = p["rows"] | 0, cols = fx::grid_cols(r);
  const int n = LV_MIN(ITEMS_MAX, LV_MAX((int) (p["_cells"] | 0), rows > 0 ? rows * cols : cols));
  std::vector<std::pair<int, int>> spans(n, {1, 1});
  for (ArduinoJson::JsonPairConst kv : p["_spans"].as<ArduinoJson::JsonObjectConst>()) {
    const int i = atoi(kv.key().c_str());
    if (i >= 0 && i < n) spans[i] = {kv.value()[0] | 1, kv.value()[1] | 1};
  }
  const bool outline = (p["outline"] | false) || !spec::building_header();
  const bool row = !strcmp(p["aspect"] | "square", "row");
  for (int i = 0; i < n; i++) {
    lv_obj_t *c = fx::grid_cell(r, spans[i].first, spans[i].second);
    if (!outline) continue;
    // the workbench sample: each cell a named outline
    if (row) fx::use(c, "h:list-row-h");
    fx::use(c, "border:line-hi");
    fx::use(fx::text(c, ("cell " + std::to_string(i)).c_str(), "type:label", "fg:text-dim"), "cell-text");  // ends in "..." in a narrow cell (C40)
  }
}
// ---- items
int index_of(const G &g, lv_obj_t *c) {
  for (size_t i = 0; i < g.cells.size(); i++) if (g.cells[i] == c) return (int) i;
  return -1;
}
void on_pick(lv_event_t *e) {
  auto *c = (lv_obj_t *) lv_event_get_current_target(e);
  lv_obj_t *r = lv_obj_get_parent(c);
  auto it = all.find(r);
  if (it == all.end()) return;
  const int i = index_of(it->second, c);
  if (i >= 0 && i < (int) it->second.items.size()) { emit(r, "pick", field(it->second.items[i], "").c_str()); sound(c, "tap"); }
}
lv_obj_t *item_cell(lv_obj_t *r, const G &g, const std::string &item) {
  lv_obj_t *c = fx::grid_cell(r, 1, 1, true);  // clickable only while it is a touch target (core)
  lv_obj_add_event_cb(c, on_pick, LV_EVENT_CLICKED, nullptr);
  std::string comp;
  ArduinoJson::JsonDocument cp;
  cell_props(g, item, comp, cp);
  ha::bound(c, comp.c_str(), cp.as<Props>());
  return c;
}
int cap_of(lv_obj_t *r, const G &g) { return g.rows > 0 ? LV_MIN(g.rows * fx::grid_cols(r), ITEMS_MAX) : ITEMS_MAX; }
// the items from index `from` on made again (the cells before it are kept), the "+N" cell or the empty text after
void expand(lv_obj_t *r, G &g, size_t from) {
  const std::vector<std::string> list = parse(g.src.c_str());
  const int cap = cap_of(r, g), n = (int) list.size() > cap ? cap - 1 : (int) list.size();
  if (g.more) { lv_obj_delete(g.more); g.more = nullptr; }
  from = LV_MIN(from, (size_t) n);
  while (g.cells.size() > from) { lv_obj_delete(g.cells.back()); g.cells.pop_back(); }
  g.items.assign(list.begin(), list.begin() + n);
  for (int i = (int) from; i < n; i++) g.cells.push_back(item_cell(r, g, list[i]));
  if (list.empty()) {  // the empty state: one row of words, not a blank grid
    g.more = fx::box(r, "row");
    fx::use(g.more, "h:list-row-h");
    fx::text(g.more, g.empty.c_str(), "type:body", "fg:text-dim");
  } else if ((int) list.size() > n) {  // "+N": the items past the cap
    g.more = fx::grid_cell(r);
    fx::text(g.more, ("+" + std::to_string(list.size() - n)).c_str(), "type:body", "fg:text-dim");
  }
  fx::grid_place(r);
}
// new cell sizes or column count (another width, a tuned gap): an items grid re-expands after the layout, so a capped
// list shows another number and every cell's contents fit the new cell (an Icon's fit: cell); a static grid's nodes
// keep their cells (re-placed by fx::grid)
void reexpand(void *v) {
  lv_obj_t *r = (lv_obj_t *) v;
  auto it = all.find(r);
  if (it == all.end()) return;
  it->second.queued = false;
  if (it->second.items_mode) expand(r, it->second, 0);
}
void on_cols(lv_event_t *e) {
  lv_obj_t *r = (lv_obj_t *) lv_event_get_current_target(e);
  auto it = all.find(r);
  if (it != all.end() && it->second.items_mode && !it->second.queued) { it->second.queued = true; lv_async_call(reexpand, r); }
}
}  // namespace

FW_COMPONENT(grid, "Grid", "components", R"({
  "about": "A layout of equal cells in columns: a fixed number of columns, or as many as fit a minimum cell size. Put other components in its cells (in + slot, a node may span several cells), or give it a list of items to repeat one cell per item, such as an icon per item in an entity attribute.",
  "contract": 2,
  "parts": [],
  "frame": [480, 360],
  "min": ["layout:tile-w", "layout:tile-w"],
  "slots": [
    {"name": "cell", "count": 64, "about": "One component per cell, in reading order (in + slot); a node may span several cells.", "accepts": ["icon", "image", "stat", "readout", "big_number", "tile", "entity_tile", "room_tile", "camera_tile", "checkbox", "toggle", "button", "icon_button", "status_chip", "status_dot", "count_badge", "arc", "donut", "level_bar", "sparkline", "weather_icon", "label"], "max": 1},
    {"name": "item", "template": true, "about": "With items: the one component repeated per item (the node's cell), $item filled in.", "accepts": ["icon", "image", "checkbox", "status_dot", "weather_icon", "stat", "readout", "label", "tile", "entity_tile"]}
  ],
  "props": [
    {"name": "cols", "about": "How many columns, 1 to 8. Leave it out when min is set.", "type": "range", "min": 1, "max": 8, "step": 1, "default": 3},
    {"name": "min", "about": "Instead of cols: the smallest cell, as an icon size step; the grid fits as many columns as it can, so a bigger screen gets more columns. none uses cols.", "type": "select", "options": )" FW_ICON_MIN_IDS R"(, "default": "none"},
    {"name": "gap", "about": "The space between cells: none, near, group or block (from tight to roomy).", "type": "select", "options": ["none", "near", "group", "block"], "default": "group"},
    {"name": "aspect", "about": "The cell shape: square, wide (16 by 9), or row (as tall as what is inside, for text like stats).", "type": "select", "options": ["square", "wide", "row"], "default": "square"},
    {"name": "rows", "about": "At most this many rows; 0 for no limit. A list of items that does not fit shows a last cell with +N for the rest.", "type": "range", "min": 0, "max": 8, "step": 1, "default": 0},
    {"name": "items", "about": "The list to repeat one cell per item: a comma list or a JSON list, usually bound to an entity attribute. Each cell is the node's cell template with $item (or $item.field) filled in; without a template, an icon named by the item.", "type": "text", "default": ""},
    {"name": "empty", "about": "The words shown when the items list is empty.", "type": "text", "default": "Nothing here"},
    {"name": "outline", "about": "For previews only: outlines and names the cells so you can see where things go.", "type": "bool", "default": false}
  ],
  "variants": [
    {"label": "3 cells", "props": {"outline": true}},
    {"label": "2 · wide", "props": {"cols": 2, "aspect": "wide", "outline": true}},
    {"label": "4 · spans", "props": {"cols": 4, "outline": true, "_cells": 6, "_spans": {"0": [2, 2]}}},
    {"label": "items", "props": {"cols": 4, "gap": "near", "items": "bulb,power,timer,bell,person,door,window,house"}},
    {"label": "min glyph · 1 row · +N", "props": {"min": "glyph", "gap": "none", "rows": 1, "items": "bolt,bolt,bolt,bolt,bolt,bolt,bolt,bolt,bolt,bolt,bolt,bolt,bolt,bolt,bolt,bolt,bolt,bolt"}},
    {"label": "row · 3", "props": {"aspect": "row", "outline": true}}
  ]
})") {
  lv_obj_t *r = fx::box(parent, "free");
  fx::name(r, "grid");
  G &g = all[r];
  lv_obj_add_event_cb(r, [](lv_event_t *e) {
    lv_obj_t *o = (lv_obj_t *) lv_event_get_current_target(e);
    lv_async_call_cancel(reexpand, o);
    all.erase(o);
  }, LV_EVENT_DELETE, nullptr);
  lv_obj_add_event_cb(r, on_cols, LV_EVENT_REFRESH, nullptr);
  if (p["_cell"].is<ArduinoJson::JsonObjectConst>()) serializeJson(p["_cell"], g.cell);
  g.items_mode = (p["_items"] | false) || *(p["items"] | "");
  g.sig = sig(p); g.src = p["items"] | ""; g.empty = p["empty"] | "Nothing here"; g.rows = p["rows"] | 0;
  fx::grid(r, p["cols"] | 3, p["min"] | "none", p["gap"] | "group", p["aspect"] | "square");
  if (g.items_mode) expand(r, g, 0);
  else build_static(r, p);
}

FW_UPDATE(grid) {
  auto it = all.find(root);
  if (it == all.end() || sig(p) != it->second.sig) return false;
  G &g = it->second;
  if (!g.items_mode) return true;
  g.empty = p["empty"] | "Nothing here";
  const std::string src = p["items"] | "";
  if (src == g.src) return true;
  const std::vector<std::string> list = parse(src.c_str()), old = g.items;
  g.src = src;
  const int cap = cap_of(root, g), n = (int) list.size() > cap ? cap - 1 : (int) list.size();
  const bool same_shape = n == (int) old.size() && (int) list.size() == n && n > 0;
  if (!same_shape) {  // another count, the empty text or "+N": the cells after the first change are made again
    size_t k = 0;
    while (k < old.size() && k < list.size() && old[k] == list[k]) k++;
    expand(root, g, k);
    return true;
  }
  for (int i = 0; i < n; i++) {  // same count: changed items rebind their cell (its own update path)
    if (g.items[i] == list[i]) continue;
    g.items[i] = list[i];
    lv_obj_t *c = g.cells[i];
    std::string comp;
    ArduinoJson::JsonDocument cp;
    cell_props(g, list[i], comp, cp);
    lv_obj_t *in = lv_obj_get_child(c, 0);
    if (!in || !ha::rebind(in, cp.as<Props>())) { lv_obj_clean(c); ha::bound(c, comp.c_str(), cp.as<Props>()); }
  }
  return true;
}
