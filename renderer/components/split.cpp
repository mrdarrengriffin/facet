// Split: a LAYOUT component (pages are made only from components; Darren 2026-10-23). Two columns over the content
// box: the left one card-w wide (lay::card_w, the art / control column: a lock ring, a window, a gauge), sp::s9, the
// right one the rest (the words). Each column has a top slot and a foot slot (pinned to the column's bottom), so a
// spec puts nodes in with "in" + "slot": 0 left top, 1 left foot, 2 right top, 3 right foot (core/spec.h).
//   size     lay::content_w x the content height (screen - top - inset): the whole content box, laid out once
//   columns  a flex column each: the top slot, a grow spacer, the foot slot; slots stack their nodes with no gap
//            (a section header that isn't first gets lay::group above it: the spec builder's spacing rule), and a
//            full-width node (list row, section header) is made as wide as its column
// Positions come from tokens at build; nothing relayouts on updates (the nodes inside update themselves).
// Workbench: built alone (not in a spec) or with prop outline, the slots are outlined and named, so the layout reads
// on its own.
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
lv_obj_t *column(lv_obj_t *root, int x, int w, int h, bool outline, const char *const names[2]) {
  lv_obj_t *c = box(root);
  lv_obj_set_pos(c, x, 0);
  lv_obj_set_size(c, w, h);
  lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
  for (int i = 0; i < 2; i++) {
    if (i) {
      lv_obj_t *g = box(c);
      lv_obj_set_size(g, w, 0);
      lv_obj_set_flex_grow(g, 1);
    }
    lv_obj_t *s = box(c);
    lv_obj_add_flag(s, fw::spec::SLOT);
    lv_obj_set_size(s, w, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(s, LV_FLEX_FLOW_COLUMN);
    if (outline) {  // the workbench sample: each slot a labelled outline
      lv_obj_set_height(s, fw::lay::list_row_h);
      lv_obj_set_style_border_width(s, fw::stroke::hair, 0);
      lv_obj_set_style_border_color(s, fw::T->line_hi, 0);
      lv_obj_set_style_pad_all(s, fw::sp::s4, 0);
      fw::text(s, fw::ty::label, names[i], fw::T->text_dim);
    }
  }
  return c;
}
}  // namespace

FW_COMPONENT(split, "Split", "components", R"({
  "about": "A layout: two columns over the screen, a card-wide one for the picture or control and the rest for the words, each with a top and a foot slot.",
  "frame": [480, 400],
  "slots": 4,
  "props": [
    {"name": "outline", "about": "For previews only: outlines the two columns and their top and foot areas so you can see where things go.", "type": "bool", "default": false}
  ],
  "variants": [
    {"label": "slots", "props": {"outline": true}}
  ]
})") {
  const int H = fw::lay::screen_h - fw::lay::top - fw::lay::inset, L = fw::lay::card_w, G = fw::sp::s9;
  lv_obj_t *root = box(parent);
  lv_obj_set_size(root, fw::lay::content_w, H);
  const bool raw = p["outline"] | false, o = raw || !fw::spec::building_header();  // alone (workbench): show the slots
  lv_obj_set_user_data(root, (void *) (intptr_t) (raw ? 2 : 1));
  static const char *const LN[2] = {"left", "left foot"}, *const RN[2] = {"right", "right foot"};
  column(root, 0, L, H, o, LN);
  column(root, L + G, fw::lay::content_w - L - G, H, o, RN);
}

FW_UPDATE(split) {  // a spec rebinding its nodes must keep the nodes inside: only the outline sample builds it again
  return (intptr_t) lv_obj_get_user_data(root) == ((p["outline"] | false) ? 2 : 1);
}
