// Group: a LAYOUT component (pages are made only from components; Darren 2026-10-23). One slot the content width
// wide that wraps its nodes in rows, lay::gutter apart both ways: tiles three across (tile-w), cards or camera tiles
// two across (card-w), or a row of buttons. A spec puts nodes in with "in" (core/spec.h).
//   align    start (left, the default) | end (right: a page's action buttons, the panel lab's button bar)
//   space    none | group: lay::group above it (a group of buttons after a block that isn't a section header)
// Laid out once from tokens; its nodes update themselves.
// Workbench: built alone (not in a spec) three tile outlines show the wrap (prop sample sets how many).
// Contrast: section "layout" (the sample outlines only).
#include "fw.h"
#include "spec.h"

FW_COMPONENT(group, "Group", "components", R"({
  "about": "A layout: a full-width row that wraps its parts, a gutter apart, for tiles, cards or a row of buttons.",
  "frame": [480, 0],
  "slots": 1,
  "props": [
    {"name": "align", "type": "select", "options": ["start", "end"], "default": "start"},
    {"name": "space", "type": "select", "options": ["none", "group"], "default": "none"},
    {"name": "sample", "type": "range", "min": 0, "max": 6, "step": 1, "default": 0}
  ],
  "variants": [
    {"label": "four tiles", "props": {"sample": 4}},
    {"label": "end · two", "props": {"sample": 2, "align": "end"}}
  ]
})") {
  lv_obj_t *g = lv_obj_create(parent);
  lv_obj_remove_style_all(g);
  lv_obj_remove_flag(g, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_add_flag(g, fw::spec::SLOT);
  lv_obj_set_size(g, fw::lay::content_w, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(g, LV_FLEX_FLOW_ROW_WRAP);
  const bool end = !strcmp(p["align"] | "start", "end");
  lv_obj_set_flex_align(g, end ? LV_FLEX_ALIGN_END : LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_set_style_pad_column(g, fw::lay::gutter, 0);
  lv_obj_set_style_pad_row(g, fw::lay::gutter, 0);
  lv_obj_set_user_data(g, (void *) (intptr_t) (1 + (p["sample"] | 0) + 8 * end));
  if (!strcmp(p["space"] | "none", "group")) lv_obj_set_style_margin_top(g, fw::lay::group, 0);
  const int n = (p["sample"] | 0) ? (p["sample"] | 0) : fw::spec::building_header() ? 0 : 3;  // alone: three outlines
  for (int i = 0; i < n; i++) {  // the workbench sample: tile-sized outlines
    lv_obj_t *t = lv_obj_create(g);
    lv_obj_remove_style_all(t);
    lv_obj_remove_flag(t, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
    lv_obj_set_size(t, fw::lay::tile_w, fw::lay::room_tile_h);
    lv_obj_set_style_border_width(t, fw::stroke::hair, 0);
    lv_obj_set_style_border_color(t, fw::T->line_hi, 0);
  }
}

FW_UPDATE(group) {  // a spec rebinding its nodes must keep the nodes inside: only the workbench sample builds it again
  return (intptr_t) lv_obj_get_user_data(root) == 1 + (p["sample"] | 0) + 8 * !strcmp(p["align"] | "start", "end");
}
