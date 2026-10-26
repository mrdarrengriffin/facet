// Cells: a LAYOUT component on the contract (renderer/CONTRACT.md, release piece 3, 2026-10-26): rows of equal cells,
// one slotted child per cell (core/spec.h "in" / "slot"), any component. It is the Home grid: one Cells per run of
// same-size cards (tiles: min tile-w x tile-h; cards: min home-card-min-w x card-h; a wide card: wide, one per row).
//   columns   one wrapping flex ("wrap", gap:gutter): each cell grows from its min-w:<layout token>, so a row holds as
//             many min-width cells plus the gutter between as fit the layout's width (at least one) and they share the
//             leftover evenly; rows a gutter apart. LVGL does this in its layout pass, synchronously: a size or token
//             change reflows it at once. 3 tiles and 2 cards fill the content width at both densities (tokens/space.json
//             home-card-min-w). "wide": every cell starts its own row (a wide card).
//   short row empty filler cells (no slot) keep the last row's cells the width of the rows above, left-aligned: their
//             count (columns - cells left over) is worked out from the layout's OWN content width after layout (C6: the
//             one size it reads), (width + gutter) / (min-w + gutter), the same sum the flex makes. last:"fill" = no
//             fillers: the cells on a short last row share the whole row (a lone Home card goes full width, Darren
//             2026-10-26); the default "column" keeps them the column width (tiles, readouts).
//   height    every cell is exactly the min-h token tall (h: + max-h:, so a child that fills, the Card, is the cell and
//             follows the token both ways); a child taller than that overflows and the bounds proof reports it (C11).
//   pair      a "p" cell holds two half-height slots (two readouts sharing a card slot, Darren 2026-10-26): each
//             pair-h tall, the first at the cell's top, the second ending on its bottom, a hairline (bg:line) between
//             them inset by the card padding, centred in the space left (decor).
// Steps: the column count (the flex, both ways); the fillers follow it after the layout (the Card's measure-later
// pattern: an async check on a size or token change). Declared minimum: one cell
// (meta "min" = the default min-w x min-h); below it the bounds proof FAILS by design (the one cell is wider than the
// layout). States: n/a (no look, no touch of its own: its children have theirs). Bleed: none. Events: none.
// Update path: the same cells = nothing to do (its children update themselves); cells added at the end or removed from
// the end = in place (removed ones go with their children; the fillers follow); a child that resizes itself stays in its
// cell (the cell is the token's size); other cells, min-w, min-h, wide or sample = a rebuild (the spec builds again).
// Sample (workbench only, a spec passes false): a real Card in every one-slot cell, an outlined "Readout" in each half.
// Contrast: section "layout" (the sample outlines), "readout" (the hairline between two halves).
#include <map>
#include <string>
#include <vector>
#include "fw.h"
#include "fx.h"
#include "spec.h"

namespace {
struct Cells {
  std::vector<lv_obj_t *> cells;  // the real cells in order; the fillers are the children after them
  std::string kinds, minw, minh;  // "ccp": c = one slot, p = a pair
  bool wide = false, fill = false, sample = true, queued = false;  // fill: last "fill" (no fillers)
};
std::map<lv_obj_t *, Cells> all;

std::string kinds_of(fw::Props p) {  // "c,c,p" -> "ccp" (anything else is skipped)
  std::string k;
  for (const char *s = p["cells"] | "c,c,c"; *s; s++) if (*s == 'c' || *s == 'p') k += *s;
  return k;
}
std::string opt(fw::Props p, const char *name, const char *def, std::initializer_list<const char *> ok) {
  const char *v = p[name] | def;
  for (const char *o : ok) if (!strcmp(o, v)) return v;
  return def;
}
lv_obj_t *slot(lv_obj_t *o) { lv_obj_add_flag(o, fw::spec::SLOT); return o; }
lv_obj_t *cell(lv_obj_t *g, const Cells &k) {  // an empty cell (a filler, or a real one before its contents)
  lv_obj_t *c = fx::box(g, "col");
  fx::use(c, "grow");
  fx::use(c, ("min-w:" + k.minw).c_str());
  fx::use(c, ("h:" + k.minh).c_str());
  fx::use(c, ("max-h:" + k.minh).c_str());  // exactly that tall: a child that fills (the Card) is the cell
  if (k.wide) lv_obj_add_flag(c, LV_OBJ_FLAG_FLEX_IN_NEW_TRACK);
  return c;
}
void sample_half(lv_obj_t *h) {  // an unbuilt part: a plain labelled outline (Readout is not on the contract yet)
  lv_obj_t *o = fx::box(h, "centre");
  for (const char *s : {"fill", "border:line-hi"}) fx::use(o, s);
  fx::text(o, "Readout", "type:label", "fg:text-dim");
}
void sample_card(lv_obj_t *s) {
  const fw::Component *c = fw::find("card");
  ArduinoJson::JsonDocument d;
  d["preview"] = "none";
  if (c) c->build(s, d.as<fw::Props>());
}
lv_obj_t *make(lv_obj_t *g, const Cells &k, char kind) {
  lv_obj_t *c = cell(g, k);
  if (kind == 'c') {
    slot(c);
    if (k.sample) sample_card(c);
    return c;
  }
  lv_obj_t *pr = fx::box(c, "col");
  fx::use(pr, "pair");
  for (int i = 0; i < 2; i++) {
    if (i) {  // the space between the halves (what is left of the cell: whole grid steps), the hairline centred in it
      lv_obj_t *gap = fx::box(pr, "centre");
      fx::use(gap, "grow");
      lv_obj_t *r = fx::box(gap, "free");
      fx::use(r, "pair-rule");
      lv_obj_t *ink = fx::box(r, "free");
      fx::use(ink, "fill");
      fx::use(ink, "bg:line");
    }
    lv_obj_t *h = slot(fx::box(pr, "col"));
    fx::use(h, "h:pair-h");
    fx::use(h, "max-h:pair-h");
    if (k.sample) sample_half(h);
  }
  return c;
}

// after a layout: the columns the flex made, (width + gutter) / (min-w + gutter) from the OWN content box; fillers
// after the real cells so the last row has that many
void fillers(lv_obj_t *g) {
  Cells &k = all[g];
  size_t want = 0;
  if (!k.cells.empty() && !k.wide && !k.fill) {
    lv_obj_update_layout(g);  // the own content box after layout (C6, C9)
    const int w = lv_obj_get_content_width(g), m = lv_obj_get_style_min_width(k.cells[0], LV_PART_MAIN);
    const int gap = lv_obj_get_style_pad_column(g, LV_PART_MAIN);
    int n = (w + gap) / (m + gap > 0 ? m + gap : 1);
    if (n < 1) n = 1;
    want = k.cells.size() % n ? n - k.cells.size() % n : 0;
  }
  while (lv_obj_get_child_count(g) > k.cells.size() + want) lv_obj_delete(lv_obj_get_child(g, -1));
  while (lv_obj_get_child_count(g) < k.cells.size() + want) cell(g, k);
}
void check(void *v) {
  lv_obj_t *g = (lv_obj_t *) v;
  auto it = all.find(g);
  if (it == all.end()) return;
  it->second.queued = false;
  fillers(g);
}
void queue(lv_obj_t *g) {
  Cells &k = all[g];
  if (!k.queued) { k.queued = true; lv_async_call(check, g); }
}
void on_change(lv_event_t *e) { queue((lv_obj_t *) lv_event_get_current_target(e)); }  // a new size, a token tuned
void on_delete(lv_event_t *e) {
  lv_obj_t *g = (lv_obj_t *) lv_event_get_current_target(e);
  lv_async_call_cancel(check, g);
  all.erase(g);
}
std::string sig(const Cells &k) { return k.minw + "|" + k.minh + (k.wide ? "|wide" : "") + (k.fill ? "|fill" : "") + (k.sample ? "|sample" : ""); }
Cells read(fw::Props p) {
  Cells k;
  k.kinds = kinds_of(p);
  k.minw = opt(p, "min-w", "tile-w", {"tile-w", "home-card-min-w", "card-min-w"});
  k.minh = opt(p, "min-h", "tile-h", {"tile-h", "card-h", "card-min-h"});
  k.wide = p["wide"] | false;
  k.fill = opt(p, "last", "column", {"column", "fill"}) == "fill";
  k.sample = p["sample"] | true;
  return k;
}
}  // namespace

FW_COMPONENT(cells, "Cells", "components", R"({
  "about": "A layout: rows of equal cells, one component in each. As many columns as cells of the minimum width fit; the cells share the rest. The Home grid.",
  "contract": 2,
  "frame": [480, 0],
  "min": ["layout:tile-w", "layout:tile-h"],
  "slots": 24,
  "props": [
    {"name": "cells", "about": "The cells in order, separated by commas: c holds one component; p holds two half-height readouts, one above the other.", "type": "text", "default": "c,c,c"},
    {"name": "min-w", "about": "The narrowest a cell gets: tile-w for tiles (three to a row), home-card-min-w for Home cards (two to a row).", "type": "select", "options": ["tile-w", "home-card-min-w", "card-min-w"], "default": "tile-w"},
    {"name": "min-h", "about": "The shortest a cell gets: tile-h for tiles, card-h for Home cards.", "type": "select", "options": ["tile-h", "card-h", "card-min-h"], "default": "tile-h"},
    {"name": "last", "about": "A short last row: column keeps its cells the width of the rows above; fill lets them share the whole row (a lone Home card goes full width).", "type": "select", "options": ["column", "fill"], "default": "column"},
    {"name": "wide", "about": "Every cell on its own row, the full width: a wide card.", "type": "bool", "default": false},
    {"name": "sample", "about": "For previews only: fills the cells with cards so the layout can be seen.", "type": "bool", "default": true}
  ],
  "variants": [
    {"label": "tiles, three to a row", "props": {}},
    {"label": "tiles, a short row stays aligned", "props": {"cells": "c,c,c,c,c"}},
    {"label": "Home cards, two to a row", "props": {"cells": "c,c,c", "min-w": "home-card-min-w", "min-h": "card-h"}},
    {"label": "Home cards, a lone last card fills the row", "props": {"cells": "c,c,c", "min-w": "home-card-min-w", "min-h": "card-h", "last": "fill"}},
    {"label": "a card and two readouts sharing a cell", "props": {"cells": "c,p", "min-w": "home-card-min-w", "min-h": "card-h"}},
    {"label": "a wide card", "props": {"cells": "c", "min-w": "home-card-min-w", "min-h": "card-h", "wide": true}}
  ]
})") {
  lv_obj_t *g = fx::box(parent, "wrap");
  fx::name(g, "cells");
  fx::use(g, "gap:gutter");
  Cells &k = all[g] = read(p);
  fw::shape(g, sig(k));
  for (char c : k.kinds) k.cells.push_back(make(g, k, c));
  lv_obj_add_event_cb(g, on_change, LV_EVENT_SIZE_CHANGED, nullptr);
  lv_obj_add_event_cb(g, on_change, LV_EVENT_STYLE_CHANGED, nullptr);
  lv_obj_add_event_cb(g, on_delete, LV_EVENT_DELETE, nullptr);
  queue(g);
}

FW_UPDATE(cells) {
  auto it = all.find(root);
  if (it == all.end()) return false;
  Cells &k = it->second, n = read(p);
  if (!fw::shape(root, sig(n))) return false;  // another minimum, wide or sample: rebuild
  const size_t a = k.kinds.size(), b = n.kinds.size(), common = a < b ? a : b;
  if (k.kinds.compare(0, common, n.kinds, 0, common)) return false;  // cells changed in the middle: rebuild
  if (a == b) return true;
  while (lv_obj_get_child_count(root) > k.cells.size()) lv_obj_delete(lv_obj_get_child(root, -1));  // the fillers
  for (size_t i = a; i < b; i++) k.cells.push_back(make(root, k, n.kinds[i]));  // added at the end
  while (k.cells.size() > b) { lv_obj_delete(k.cells.back()); k.cells.pop_back(); }  // removed from the end
  k.kinds = n.kinds;
  fillers(root);
  return true;
}
