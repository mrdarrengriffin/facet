// Column: a LAYOUT component (pages are made only from components; Darren 2026-10-23). The content box as one column:
// a top slot and a foot slot pinned to the bottom (the panel lab's Vacuum: the view on top, the action bar at the
// foot). A spec puts nodes in with "in" + "slot": 0 top, 1 foot (core/spec.h).
//   size     lay::content_w x the content height (screen - top - inset): the whole content box, laid out once
//   slots    flex column: the top slot, a grow spacer, the foot slot; slots stack their nodes with no gap (a section
//            header that isn't first gets lay::group above it; Group's space puts room above a block)
// Positions come from tokens at build; nothing relayouts on updates (the nodes inside update themselves; a node that
// shows / hides by its "show" bind takes or gives back its room in its slot).
// Workbench: built alone (not in a spec) or with prop outline, the slots are outlined and named.
// Contrast: section "layout" (the outline sample only).
#include "fw.h"
#include "spec.h"

namespace {
lv_obj_t *box(lv_obj_t *parent) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  return o;
}
}  // namespace

FW_COMPONENT(column, "Column", "components", R"({
  "about": "A layout: the screen as one column, a top slot for the view and a foot slot pinned to the bottom for the action buttons.",
  "frame": [480, 400],
  "slots": 2,
  "props": [
    {"name": "outline", "about": "For previews only: outlines the top and foot areas so you can see where the content and the buttons go.", "type": "bool", "default": false}
  ],
  "variants": [
    {"label": "slots", "props": {"outline": true}}
  ]
})") {
  const int H = fw::lay::screen_h - fw::lay::top - fw::lay::inset, W = fw::lay::content_w;
  lv_obj_t *root = box(parent);
  lv_obj_set_size(root, W, H);
  lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
  const bool raw = p["outline"] | false, o = raw || !fw::spec::building_header();  // alone (workbench): show the slots
  lv_obj_set_user_data(root, (void *) (intptr_t) (raw ? 2 : 1));
  static const char *const NAMES[2] = {"top", "foot"};
  for (int i = 0; i < 2; i++) {
    if (i) {
      lv_obj_t *g = box(root);
      lv_obj_set_size(g, W, 0);
      lv_obj_set_flex_grow(g, 1);
    }
    lv_obj_t *s = box(root);
    lv_obj_add_flag(s, fw::spec::SLOT);
    lv_obj_set_size(s, W, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(s, LV_FLEX_FLOW_COLUMN);
    if (o) {  // the workbench sample: each slot a labelled outline
      lv_obj_set_height(s, fw::lay::list_row_h);
      lv_obj_set_style_border_width(s, fw::stroke::hair, 0);
      lv_obj_set_style_border_color(s, fw::T->line_hi, 0);
      lv_obj_set_style_pad_all(s, fw::sp::s4, 0);
      fw::text(s, fw::ty::label, NAMES[i], fw::T->text_dim);
    }
  }
}

FW_UPDATE(column) {  // a spec rebinding its nodes must keep the nodes inside: only the outline sample builds it again
  return (intptr_t) lv_obj_get_user_data(root) == ((p["outline"] | false) ? 2 : 1);
}
