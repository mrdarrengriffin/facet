// Card grid: a LAYOUT component (pages are made only from components; Darren 2026-10-23): the Home grid of screen
// model §4. Cells of three sizes flow in the given order into rows of one size, each cell a slot a spec node goes into
// ("in" / "slot", core/spec.h):
//   s   a tile, lay::tile_w x tile_h (208 / 136), three per row
//   m   a card, the full content width x card_h for now (Darren 2026-10-26: a card has no width of its own, it fills its
//       cell; l = the same; a Grid component that lays cards out two-up comes later), one per row
//   h   half a card slot (a State readout): two in a row share one m slot, stacked sp::s2 apart with a hairline between
//       them inset by the card padding, each half on the grid (136 / 88: the first at the slot's top, the second ending on
//       its bottom, the hairline centred in the gap), so they count as one m cell
// Packing (§4): a cell joins the current row if it is the row's size and the row has room, else it starts a new row; a
// short row stays left-aligned; no reordering. Rows are lay::gutter apart, cells lay::gutter apart: every row lands on
// the grid tokens (3 tiles = 2 cards = 1 wide = the content width at both densities). Positions are computed once at
// build (no flex), so the layout audit can check every Home.
//   cells   a comma list of sizes, one per slot in order ("s,s,s,m,h,h,l"); slots are numbered in that order
//   sample  workbench only: an outline in every slot (built alone on its page); a spec passes false
// Box: lay::content_w wide, its rows' height. Laid out once; its nodes update themselves; an update with other cells
// builds it again (the spec rebuilds the Home then).
// Contrast: section "layout" (the sample outlines), "readout" (the hairline between two halves).
#include <string>
#include <vector>
#include "fw.h"
#include "spec.h"

namespace {
struct Cell { char size; int x, y, w, h; };
// the cells' rectangles (relative to the grid box) + the hairlines between stacked halves; returns the total height
int pack(const std::string &cells, std::vector<Cell> &out, std::vector<lv_area_t> &rules) {
  const int g = fw::lay::gutter, hh = (fw::lay::card_h - fw::sp::s2) / 2 / fw::sp::GRID * fw::sp::GRID;  // on the grid (136 / 88)
  char row = 0;     // the current row's size (h counts as m)
  int in_row = 0;   // m slots / tiles used in it
  int y = 0, row_h = 0;
  bool half_open = false;  // the last m slot holds one h and has room for a second
  auto per_row = [](char k) { return k == 's' ? 3 : 1; };  // m = full width for now (Darren 2026-10-26: cards have no width; a Grid comes later)
  for (size_t i = 0; i < cells.size(); i++) {
    char k = cells[i];
    if (k == ',' || k == ' ') continue;
    if (k != 's' && k != 'm' && k != 'l' && k != 'h') continue;
    const char rk = k == 'h' ? 'm' : k;
    if (k == 'h' && half_open && row == 'm') {  // the second half of an open slot
      const Cell &a = out.back();
      const int y2 = a.y + fw::lay::card_h - hh, mid = (a.y + hh + y2) / 2;  // the second half ends on the slot's bottom
      out.push_back({'h', a.x, y2, a.w, hh});
      rules.push_back({(int32_t) (a.x + fw::lay::card_pad), (int32_t) mid, (int32_t) (a.x + a.w - fw::lay::card_pad - 1), (int32_t) (mid + fw::stroke::hair - 1)});
      half_open = false;
      continue;
    }
    half_open = false;
    if (row != rk || in_row >= per_row(rk)) {  // a new row
      if (row) y += row_h + g;
      row = rk;
      in_row = 0;
      row_h = rk == 's' ? fw::lay::tile_h : fw::lay::card_h;
    }
    const int w = rk == 's' ? fw::lay::tile_w : fw::lay::content_w;  // a card fills its cell (contract): one per row
    const int x = in_row * (w + g);
    out.push_back({k, x, y, w, k == 'h' ? hh : row_h});
    in_row++;
    half_open = k == 'h';
  }
  return row ? y + row_h : 0;
}
lv_obj_t *box(lv_obj_t *parent) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  return o;
}
void fill(lv_obj_t *g, fw::Props p) {
  const std::string cells = p["cells"] | "s,s,s,m,h,h,l";
  fw::shape(g, cells + (p["sample"] | true ? "|sample" : ""));
  lv_obj_remove_style_all(g);
  lv_obj_remove_flag(g, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  std::vector<Cell> cs;
  std::vector<lv_area_t> rules;
  const int h = pack(cells, cs, rules);
  lv_obj_set_size(g, fw::lay::content_w, h);
  const bool sample = p["sample"] | true;
  for (const Cell &c : cs) {
    lv_obj_t *s = box(g);
    lv_obj_add_flag(s, fw::spec::SLOT);
    lv_obj_set_pos(s, c.x, c.y);
    lv_obj_set_size(s, c.w, c.h);
    if (sample && c.size != 'h') {  // the workbench: the slot outlined (halves: the hairline shows them)
      lv_obj_set_style_border_width(s, fw::stroke::hair, 0);
      lv_obj_set_style_border_color(s, fw::T->line_hi, 0);
    }
  }
  for (const lv_area_t &a : rules) {  // the hairline between two stacked readouts (decor, behind nothing)
    lv_obj_t *r = box(g);
    lv_obj_set_pos(r, a.x1, a.y1);
    lv_obj_set_size(r, lv_area_get_width(&a), lv_area_get_height(&a));
    lv_obj_set_style_bg_color(r, fw::T->line, 0);
    lv_obj_set_style_bg_opa(r, LV_OPA_COVER, 0);
  }
}
}  // namespace

FW_COMPONENT(card_grid, "Card grid", "components", R"({
  "about": "A layout: the Home grid. Tiles, cards and wide cards in the given order, in rows of one size; two readouts share a card slot.",
  "frame": [480, 0],
  "slots": 24,
  "props": [
    {"name": "cells", "about": "The cards in order, separated by commas: s is a small square tile, three to a row; m is half the row; l is the full width; h is a read-only readout, two of which share one slot.", "type": "text", "default": "s,s,s,m,h,h,l"},
    {"name": "sample", "about": "For previews only: fills the cells with made-up cards so the layout can be seen.", "type": "bool", "default": true}
  ],
  "variants": [
    {"label": "tiles, a card, two readouts, a wide card", "props": {}},
    {"label": "a short row stays left", "props": {"cells": "s,s,m,m,m"}},
    {"label": "one readout alone", "props": {"cells": "m,h,s"}}
  ]
})") {
  lv_obj_t *g = lv_obj_create(parent);
  fill(g, p);
}

// a spec rebinding its nodes keeps them inside: the same cells = nothing to do; other cells = a rebuild (the Home
// builds again when its deck page changes)
FW_UPDATE(card_grid) { return fw::shape(root, std::string(p["cells"] | "s,s,s,m,h,h,l") + (p["sample"] | true ? "|sample" : "")); }
