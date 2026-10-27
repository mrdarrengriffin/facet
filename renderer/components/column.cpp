// Column: a LAYOUT component (pages are made only from components; Darren 2026-10-23), on the contract
// (renderer/CONTRACT.md, "contract": 2): the content box as one column: a top slot and a foot slot pinned to the
// bottom (the panel lab's Vacuum: the view on top, the action bar at the foot). A spec puts nodes in with "in" +
// "slot": 0 top, 1 foot (core/spec.h).
//   size     fills its parent (C5: the Page's Body, the whole content box); sizes nothing of its own
//   slots    a column: the top slot, a grow spacer, the foot slot; slots stack their nodes with no gap (a section
//            header that isn't first gets layout:group above it; Group's space puts room above a block)
// Nothing relayouts on updates (the nodes inside update themselves; a node that shows / hides by its "show" bind takes
// or gives back its room in its slot). States: none (a layout). Bleed: none.
// Workbench: built alone (not in a spec) or with prop outline, the slots are outlined (border:line-hi) and named.
// Contrast: section "layout" (the outline sample only).
#include "fw.h"
#include "fx.h"
#include "spec.h"

namespace {
std::string sig(fw::Props p) { return (p["outline"] | false) ? "outline" : ""; }
}  // namespace

FW_COMPONENT(column, "Column", "components", R"({
  "about": "A layout: the screen as one column, a top slot for the view and a foot slot pinned to the bottom for the action buttons.",
  "contract": 2,
  "parts": [],
  "frame": [480, 400],
  "slots": 2,
  "props": [
    {"name": "outline", "about": "For previews only: outlines the top and foot areas so you can see where the content and the buttons go.", "type": "bool", "default": false}
  ],
  "variants": [
    {"label": "slots", "props": {"outline": true}}
  ]
})") {
  lv_obj_t *root = fx::box(parent, "col");
  fx::use(root, "fill");
  fx::name(root, "column");
  fw::shape(root, sig(p));
  const bool o = (p["outline"] | false) || !fw::spec::building_header();  // alone (workbench): show the slots
  static const char *const NAMES[2] = {"top", "foot"};
  for (int i = 0; i < 2; i++) {
    if (i) fx::use(fx::box(root, "free"), "grow");
    lv_obj_t *s = fx::box(root, "col");
    lv_obj_add_flag(s, fw::spec::SLOT);
    fx::slot(s, NAMES[i]);
    if (o) {  // the workbench sample: each slot a labelled outline
      for (const char *t : {"h:list-row-h", "border:line-hi", "pad:4"}) fx::use(s, t);
      fx::text(s, NAMES[i], "type:label", "fg:text-dim");
    }
  }
}

FW_UPDATE(column) { return fw::shape(root, sig(p)); }  // a spec rebinding its nodes keeps them inside: only the outline sample builds it again
