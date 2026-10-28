// Title: the screen's name in the Header's title slot, on the contract (renderer/CONTRACT.md, "contract": 2). A real
// component (2026-10-28) so the slot only ever holds parts: an editor removes it and adds it back like any other.
//   text   type label (caps, mono), text-dim; one line; a long one ends in an ellipsis. Its ink is the nearest box
//          part's text ink (the theme's header part, C46), as every text inside a part.
//   width  in a row (the Header's title box: a flex row) it takes the row's room from 0 (style header-label), so it
//          gives way before the end slot and ellipsises; anywhere else it is its text's width on the 4 px grid
//          (fx::fit_grid), so what follows it stays on the grid.
// Spec: "header": {"label": ...} is the shorthand (the Header builds a Title from it); the explicit form is a node
// {"c": "title", "p": {"text": ...}, "in": "header", "slot": "title"} (core/spec.h). No label and no node = no title.
// Steps: one (the ellipsis). Bleed: none. States: n/a (static text, no events). Update path: the text in place
// (same object, silent). Contrast pairs: tokens/colour.json section "title" (+ header).
#include "fw.h"
#include "fx.h"

namespace {
bool in_row(lv_obj_t *parent) {
  return parent && lv_obj_get_style_layout(parent, LV_PART_MAIN) == LV_LAYOUT_FLEX &&
         lv_obj_get_style_flex_flow(parent, LV_PART_MAIN) == LV_FLEX_FLOW_ROW;
}
void fit(lv_obj_t *l) {
  if (in_row(lv_obj_get_parent(l))) fx::use(l, "header-label");
  else fx::fit_grid(l);
}
}  // namespace

FW_COMPONENT(title, "Title", "components", R"({
  "about": "The screen's name at the start of the header, in capitals; a long one ends in an ellipsis so the header's other items always fit.",
  "contract": 2,
  "parts": [],
  "frame": [480, 80],
  "props": [
    {"name": "text", "about": "The words to show, like Living room. It can follow an entity's name.", "type": "text", "default": "Weather"}
  ],
  "variants": [
    {"label": "default", "props": {}},
    {"label": "long name", "props": {"text": "Living room ceiling and the sofa lamps"}}
  ]
})") {
  fit(fx::text(parent, p["text"] | "", "type:label", "fg:text-dim"));
}

FW_UPDATE(title) {
  if (!lv_obj_check_type(root, &lv_label_class)) return false;
  fx::set_text(root, p["text"] | "", "type:label");
  if (!in_row(lv_obj_get_parent(root))) fx::fit_grid(root);
  return true;
}
