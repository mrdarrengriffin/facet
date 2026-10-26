// Grid: a LAYOUT component of equal cells (board facet-171, notes icon-grid-proposal §3, pieces 2 + 3). Positions are
// computed once at build (again only when an items list changes its count), never live relayout.
//   columns  cols 1-8, or min (a size token): as many min-wide cells + gaps as fit the width (720 and 480 get their
//            own count, at most 12). Width = the content width (in a layout's slot: the slot's). Cell width =
//            (width - gaps) / cols snapped down to the 4 px grid; half the leftover (snapped down to the grid) goes
//            on the left, so the cells sit centred within 4 px
//   gap      none | near (sp::s3) | group (lay::gutter) | block (lay::group)
//   aspect   square (cell height = width) | wide (16:9, snapped down to the grid) | row (the content's height: the
//            cells flow in rows, spans are columns only)
//   rows     0 = no cap; else at most rows x cols cells: an items list longer than that shows rows x cols - 1 items
//            and a last "+N" cell (N = the rest), instead of a partly shown grid
// Static mode (no items): slot n = cell n in reading order (spec nodes go in with "in" + "slot"); a node may add
//   "span": [cols, rows] (core/spec.h hands the grid its nodes' spans as _spans and the cell count as _cells); cells
//   are placed in reading order into the first free place that fits. Built alone (the workbench), cells are outlined
//   and named.
// Items mode: items = a comma list or a JSON list (usually bound to an entity attribute: {"e", "a"}); one cell per
//   item from the spec node's "cell" template (core/spec.h hands it over as _cell; none = an Icon named by the item,
//   fit: cell). "$item" in the template = the item (an object item: its id, else its name), "$item.<key>" = its field.
//   At most ITEMS_MAX (grid_items) cells. Every update re-expands: a changed item rebinds its cell (the cell's own
//   update path: an Icon cross-fades), added cells are made, removed ones go. An empty list = the empty text (not
//   blank). A tap on an item cell emits "pick" with the item + sound tap (actions: screen.param, nav.open); cells
//   smaller than touch::min (a row of hearts) are shown only, never tapped.
// Contrast: section "grid" (outline sample, +N, empty text).
#include "fw.h"
#include "spec.h"

namespace {
using namespace fw;
constexpr int ITEMS_MAX = 64;  // grid_items: cells an items grid makes at most
constexpr int AUTO_COLS = 12;  // columns a min grid makes at most
struct Rect { int x, y, w, h; };
struct G {
  std::string cell, sig;           // the items template (JSON, "" = an Icon by the item); the structural props
  std::vector<std::string> items;  // the items shown now (an object item kept as its JSON)
  bool items_mode = false, row = false;
  int cols = 1, cw = 0, ch = 0, left = 0, gap = 0, cap = ITEMS_MAX, W = 0;
};
G *g_of(lv_obj_t *r) { return (G *) lv_obj_get_user_data(r); }
lv_obj_t *box(lv_obj_t *parent) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  return o;
}
int gap_of(const char *s) {
  if (!strcmp(s, "none")) return 0;
  if (!strcmp(s, "near")) return sp::s3;
  if (!strcmp(s, "block")) return lay::group;
  return lay::gutter;
}
int step_of(const char *s) {
  for (int i = 0; i < ic::N_SIZES; i++)
    if (!strcmp(ic::SIZE_NAMES[i], s)) return i;
  return -1;
}
std::string sig(Props p) {
  return std::to_string(p["cols"] | 3) + "|" + (p["min"] | "none") + "|" + (p["gap"] | "group") + "|" + (p["aspect"] | "square") +
         "|" + std::to_string(p["rows"] | 0) + "|" + ((p["outline"] | false) ? "o" : "");
}
// ---- items: a comma list or a JSON list; "$item" / "$item.key" filled from one item
std::vector<std::string> parse(const char *v) {
  std::vector<std::string> out;
  if (*v == '[') {
    ArduinoJson::JsonDocument d;
    if (!deserializeJson(d, v))
      for (ArduinoJson::JsonVariantConst x : d.as<ArduinoJson::JsonArrayConst>()) {
        std::string s;
        if (x.is<const char *>()) s = x.as<std::string>();
        else serializeJson(x, s);
        out.push_back(s);
      }
    return out;
  }
  for (const char *p = v; *p;) {
    const char *q = strchr(p, ',');
    std::string it(p, q ? (size_t) (q - p) : strlen(p));
    while (!it.empty() && it.front() == ' ') it.erase(0, 1);
    while (!it.empty() && it.back() == ' ') it.pop_back();
    if (!it.empty()) out.push_back(it);
    if (!q) break;
    p = q + 1;
  }
  return out;
}
// one field of an item ("" key = the item itself: an object's id, else its name); *num = the field is a JSON number
std::string field(const std::string &item, const std::string &key, bool *num = nullptr) {
  if (num) *num = false;
  if (item.empty() || item[0] != '{') return key.empty() ? item : "";
  ArduinoJson::JsonDocument d;
  if (deserializeJson(d, item)) return "";
  ArduinoJson::JsonVariantConst v = key.empty() ? (d["id"].isNull() ? d["name"] : d["id"]) : d[key.c_str()];
  if (v.isNull()) return "";
  if (v.is<const char *>()) return v.as<std::string>();
  if (num) *num = v.is<double>() || v.is<bool>();
  std::string s;
  serializeJson(v, s);
  return s;
}
void with_item(ArduinoJson::JsonVariant v, const std::string &item) {
  if (v.is<ArduinoJson::JsonObject>()) { for (ArduinoJson::JsonPair kv : v.as<ArduinoJson::JsonObject>()) with_item(kv.value(), item); return; }
  if (v.is<ArduinoJson::JsonArray>()) { for (ArduinoJson::JsonVariant x : v.as<ArduinoJson::JsonArray>()) with_item(x, item); return; }
  if (!v.is<const char *>()) return;
  const std::string s = v.as<std::string>();
  if (s.find("$item") == std::string::npos) return;
  std::string out;
  bool whole = false, num = false;
  for (size_t i = 0;;) {
    const size_t j = s.find("$item", i);
    if (j == std::string::npos) { out += s.substr(i); break; }
    out += s.substr(i, j - i);
    size_t k = j + 5;
    std::string key;
    if (k < s.size() && s[k] == '.') {
      size_t m = k + 1;
      while (m < s.size() && (isalnum((unsigned char) s[m]) || s[m] == '_')) m++;
      key = s.substr(k + 1, m - k - 1);
      k = m;
    }
    whole = j == 0 && k == s.size();
    out += field(item, key, &num);
    i = k;
  }
  if (whole && num) {  // "$item.count" alone, a number in the item: the prop gets the number
    ArduinoJson::JsonDocument d;
    deserializeJson(d, out);
    v.set(d.as<ArduinoJson::JsonVariantConst>());
  } else v.set(out);
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
// ---- layout
void measure(lv_obj_t *r, G &g, Props p) {
  lv_obj_t *par = lv_obj_get_parent(r);  // in a layout's slot: the slot's width; else the content width
  const int32_t pw = lv_obj_has_flag(par, spec::SLOT) ? lv_obj_get_style_width(par, LV_PART_MAIN) : 0;
  g.W = pw > 0 && !LV_COORD_IS_SPEC(pw) ? (int) pw : lay::content_w;
  g.gap = gap_of(p["gap"] | "group");
  const int m = step_of(p["min"] | "none");
  g.cols = m >= 0 ? LV_CLAMP(1, (g.W + g.gap) / (ic::PX[m] + g.gap), AUTO_COLS) : LV_CLAMP(1, p["cols"] | 3, 8);
  g.cw = (g.W - g.gap * (g.cols - 1)) / g.cols / sp::GRID * sp::GRID;
  g.left = (g.W - g.cw * g.cols - g.gap * (g.cols - 1)) / 2 / sp::GRID * sp::GRID;  // half the leftover, on the grid
  const char *a = p["aspect"] | "square";
  g.row = !strcmp(a, "row");
  g.ch = !strcmp(a, "wide") ? g.cw * 9 / 16 / sp::GRID * sp::GRID : g.cw;
  const int rows = p["rows"] | 0;
  g.cap = rows > 0 ? LV_MIN(rows * g.cols, ITEMS_MAX) : ITEMS_MAX;
  lv_obj_set_width(r, g.W);
  if (g.row) {  // cells flow in rows at the content's height
    lv_obj_set_height(r, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_left(r, g.left, 0);
    lv_obj_set_style_pad_column(r, g.gap, 0);
    lv_obj_set_style_pad_row(r, g.gap, 0);
  }
}
// reading-order placement of n cells with spans (cols x rows): each into the first free place that fits
std::vector<Rect> place(const G &g, const std::vector<std::pair<int, int>> &spans) {
  std::vector<Rect> out;
  std::vector<std::vector<bool>> occ;
  for (const auto &sp : spans) {
    const int w = LV_CLAMP(1, sp.first, g.cols), h = g.row ? 1 : LV_CLAMP(1, sp.second, 8);
    for (int r = 0, done = 0; !done; r++) {
      while ((int) occ.size() < r + h) occ.emplace_back(g.cols, false);
      for (int c = 0; c + w <= g.cols && !done; c++) {
        bool free = true;
        for (int y = r; y < r + h && free; y++)
          for (int x = c; x < c + w && free; x++) free = !occ[y][x];
        if (!free) continue;
        for (int y = r; y < r + h; y++)
          for (int x = c; x < c + w; x++) occ[y][x] = true;
        out.push_back({g.left + c * (g.cw + g.gap), r * (g.ch + g.gap), w * g.cw + (w - 1) * g.gap, h * g.ch + (h - 1) * g.gap});
        done = 1;
      }
    }
  }
  return out;
}
lv_obj_t *cell_at(lv_obj_t *r, const G &g, const Rect &rc, bool centre) {
  lv_obj_t *c = box(r);
  lv_obj_add_flag(c, spec::SLOT);  // a slot (static) / a cell an Icon fits (fit: cell)
  lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
  if (centre) lv_obj_set_flex_align(c, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  if (g.row) lv_obj_set_size(c, rc.w, LV_SIZE_CONTENT);
  else {
    lv_obj_set_pos(c, rc.x, rc.y);
    lv_obj_set_size(c, rc.w, rc.h);
  }
  return c;
}
void fit_height(lv_obj_t *r, const G &g, int rows) {
  if (!g.row) lv_obj_set_height(r, rows > 0 ? rows * g.ch + (rows - 1) * g.gap : 0);
}
// ---- static cells
void build_static(lv_obj_t *r, G &g, Props p) {
  const int rows = p["rows"] | 0;
  const int n = LV_MIN(ITEMS_MAX, LV_MAX((int) (p["_cells"] | 0), rows > 0 ? rows * g.cols : g.cols));
  std::vector<std::pair<int, int>> spans(n, {1, 1});
  for (ArduinoJson::JsonPairConst kv : p["_spans"].as<ArduinoJson::JsonObjectConst>()) {
    const int i = atoi(kv.key().c_str());
    if (i >= 0 && i < n) spans[i] = {kv.value()[0] | 1, kv.value()[1] | 1};
  }
  const std::vector<Rect> rc = place(g, spans);
  const bool outline = (p["outline"] | false) || !spec::building_header();
  const char *a = p["aspect"] | "square";
  int bottom = 0;
  for (int i = 0; i < n; i++) {
    lv_obj_t *c = cell_at(r, g, rc[i], strcmp(a, "row") != 0);
    bottom = LV_MAX(bottom, rc[i].y + rc[i].h);
    if (outline) {  // the workbench sample: each cell a named outline
      if (g.row) lv_obj_set_height(c, lay::list_row_h);
      lv_obj_set_style_border_width(c, stroke::hair, 0);
      lv_obj_set_style_border_color(c, T->line_hi, 0);
      text(c, ty::label, ("cell " + std::to_string(i)).c_str(), T->text_dim);
    }
  }
  if (!g.row) lv_obj_set_height(r, bottom);
}
// ---- items
lv_obj_t *item_cell(lv_obj_t *r, const G &g, int i, const std::string &item, bool more, int rest) {
  lv_obj_t *c = cell_at(r, g, {g.left + (i % g.cols) * (g.cw + g.gap), (i / g.cols) * (g.ch + g.gap), g.cw, g.ch}, true);
  if (more) {  // "+N": the items past the cap
    text(c, ty::body, ("+" + std::to_string(rest)).c_str(), T->text_dim);
    return c;
  }
  lv_obj_set_user_data(c, (void *) (intptr_t) i);
  if (g.cw < touch::min || (!g.row && g.ch < touch::min)) goto content;  // under a touch target: shown, not tapped
  lv_obj_add_flag(c, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(c, [](lv_event_t *e) {
    auto *c = (lv_obj_t *) lv_event_get_current_target(e);
    lv_obj_t *r = lv_obj_get_parent(c);
    G *g = g_of(r);
    const int i = (int) (intptr_t) lv_obj_get_user_data(c);
    if (g && i < (int) g->items.size()) { emit(r, "pick", field(g->items[i], "").c_str()); sound(c, "tap"); }
  }, LV_EVENT_CLICKED, nullptr);
content:
  std::string comp;
  ArduinoJson::JsonDocument cp;
  cell_props(g, item, comp, cp);
  ha::bound(c, comp.c_str(), cp.as<Props>());
  return c;
}
// what the cells show for a list: the items that fit, and how many go in a "+N" cell (0 = none)
int shown(const G &g, const std::vector<std::string> &all) {
  return (int) all.size() > g.cap ? g.cap - 1 : (int) all.size();
}
void empty_text(lv_obj_t *r, const G &g, Props p) {
  if (!g.row) lv_obj_set_height(r, lay::list_row_h);
  lv_obj_t *l = text(r, ty::body, p["empty"] | "Nothing here", T->text_dim);
  lv_obj_set_pos(l, g.left, (lay::list_row_h - style(ty::body).box) / 2 / sp::GRID * sp::GRID);
}
void build_items(lv_obj_t *r, G &g, Props p) {
  std::vector<std::string> all = parse(p["items"] | "");
  if (all.empty()) { g.items.clear(); empty_text(r, g, p); return; }
  const int n = shown(g, all);
  g.items.assign(all.begin(), all.begin() + n);
  for (int i = 0; i < n; i++) item_cell(r, g, i, all[i], false, 0);
  const int cells = n + ((int) all.size() > n ? 1 : 0);
  if (cells > n) item_cell(r, g, n, "", true, (int) all.size() - n);
  fit_height(r, g, (cells + g.cols - 1) / g.cols);
}
void fill(lv_obj_t *r, G &g, Props p) {
  measure(r, g, p);
  if (g.items_mode) build_items(r, g, p);
  else build_static(r, g, p);
}
}  // namespace

FW_COMPONENT(grid, "Grid", "components", R"({
  "about": "A layout of equal cells in columns: a fixed number of columns, or as many as fit a minimum cell size. Put other components in its cells (in + slot, a node may span several cells), or give it a list of items to repeat one cell per item, such as an icon per item in an entity attribute.",
  "frame": [480, 360],
  "slots": 64,
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
  lv_obj_t *r = box(parent);
  auto *g = new G;
  lv_obj_set_user_data(r, g);
  lv_obj_add_event_cb(r, [](lv_event_t *e) { delete (G *) lv_event_get_user_data(e); }, LV_EVENT_DELETE, g);
  if (p["_cell"].is<ArduinoJson::JsonObjectConst>()) serializeJson(p["_cell"], g->cell);
  g->items_mode = (p["_items"] | false) || *(p["items"] | "");
  g->sig = sig(p);
  fill(r, *g, p);
}

// Update path: a static grid keeps its cells (the nodes inside update themselves); an items grid re-expands: a
// changed item rebinds its cell in place (an Icon cross-fades), new cells are made, gone ones deleted, the "+N" cell
// and the height follow the count. Another shape (cols, min, gap, aspect, rows) = rebuilt (false). Silent.
FW_UPDATE(grid) {
  G *g = g_of(root);
  if (!g || sig(p) != g->sig) return false;
  if (!g->items_mode) return true;
  const std::vector<std::string> all = parse(p["items"] | "");
  const int n = shown(*g, all), more = (int) all.size() - n;
  const int old_n = (int) g->items.size();
  const bool was_empty = old_n == 0;
  if (was_empty || all.empty() || n != old_n || more > 0 || (int) lv_obj_get_child_count(root) != old_n) {
    // the count changed (or the empty text / "+N" cell): cells kept where the item is the same, the rest made again
    std::vector<std::string> keep = g->items;
    if (was_empty || all.empty()) lv_obj_clean(root);
    else
      for (int i = (int) lv_obj_get_child_count(root) - 1; i >= 0; i--)
        if (i >= n || i >= old_n || keep[i] != all[i]) lv_obj_delete(lv_obj_get_child(root, i));
    g->items.assign(all.begin(), all.begin() + n);
    if (all.empty()) { empty_text(root, *g, p); return true; }
    for (int i = 0; i < n; i++) {
      if (!was_empty && i < old_n && keep[i] == all[i] && (uint32_t) i < lv_obj_get_child_count(root)) continue;
      lv_obj_t *c = item_cell(root, *g, i, all[i], false, 0);
      lv_obj_move_to_index(c, i);
    }
    if (more > 0) item_cell(root, *g, n, "", true, more);
    fit_height(root, *g, (n + (more > 0) + g->cols - 1) / g->cols);
    return true;
  }
  for (int i = 0; i < n; i++) {  // same count: changed items rebind their cell (its own update path)
    if (g->items[i] == all[i]) continue;
    g->items[i] = all[i];
    lv_obj_t *c = lv_obj_get_child(root, i);
    std::string comp;
    ArduinoJson::JsonDocument cp;
    cell_props(*g, all[i], comp, cp);
    lv_obj_t *in = c ? lv_obj_get_child(c, 0) : nullptr;
    if (!in || !ha::rebind(in, cp.as<Props>())) {
      if (c) lv_obj_clean(c);
      if (c) ha::bound(c, comp.c_str(), cp.as<Props>());
    }
  }
  return true;
}
