// Placeholder: a part that isn't built yet, shown as a plain labelled outline (Facet rule: never a look-alike of a
// part to come). Fills its parent's width (contract v2, CONTRACT.md), a hairline line-hi outline (rad::card), the name centred (type label,
// text-dim, caps) over one quiet line (type body, text-dim). First use: the robot vacuum's map area, waiting for the
// Floor plan foundation (Darren 2026-10-23: floor plans get their own drawing rules first).
//   height   card (h:card-h, the default) | tile (h:tile-h) | row (h:list-row-h)
// Static, not a control. Update path: the texts in place. Contrast: section "placeholder".
#include "fw.h"
#include "fx.h"

namespace {
const char *height_of(const char *h) { return !strcmp(h, "tile") ? "h:tile-h" : !strcmp(h, "row") ? "h:list-row-h" : "h:card-h"; }
}  // namespace

FW_COMPONENT(placeholder, "Placeholder", "components", R"({
  "about": "A part that isn't built yet: a plain outline with its name and one line, never a look-alike.",
  "contract": 2,
  "frame": [480, 0],
  "props": [
    {"name": "label", "about": "The name of the part that is not built yet.", "type": "text", "default": "map"},
    {"name": "note", "about": "One line saying what is coming.", "type": "text", "default": "Floor plans are coming"},
    {"name": "height", "about": "How tall the outline is, matching the part it stands in for: a card, a tile or a row.", "type": "select", "options": ["card", "tile", "row"], "default": "card"}
  ],
  "variants": [
    {"label": "card", "props": {}},
    {"label": "row", "props": {"label": "sheet", "note": "Component coming", "height": "row"}}
  ]
})")  {
  lv_obj_t *b = fx::box(parent, "centre");
  fx::name(b, "Placeholder");
  fx::use(b, height_of(p["height"] | "card"));
  fx::use(b, "radius:card");
  fx::use(b, "border:line-hi");
  fx::use(b, "gap:2");
  fx::text(b, p["label"] | "map", "type:label", "fg:text-dim");
  fx::text(b, p["note"] | "", "type:body", "fg:text-dim");
  fw::shape(b, p["height"] | "card");
}

FW_UPDATE(placeholder) {
  if (!fw::shape(root, p["height"] | "card") || lv_obj_get_child_count(root) != 2) return false;
  fx::set_text(lv_obj_get_child(root, 0), p["label"] | "map", "type:label");
  fx::set_text(lv_obj_get_child(root, 1), p["note"] | "", "type:body");
  return true;
}
