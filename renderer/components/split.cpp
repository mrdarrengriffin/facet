// Split: a LAYOUT component (pages are made only from components; Darren 2026-10-23), on the contract
// (renderer/CONTRACT.md, "contract": 2). Two columns over its parent: the left one card-w wide (the art / control
// column: a lock ring, a window, a gauge), space 9, the right one the rest (the words). Each column has a top slot and
// a foot slot (pinned to the column's bottom), so a spec puts nodes in with "in" + "slot": 0 left top, 1 left foot,
// 2 right top, 3 right foot (core/spec.h).
//   art_w    card (w:card-w) or radar (w:radar: the Lightning map's 384 / 256 square); art_mid: the art column's top
//            slot centred down the column (a grow above it as well as under it)
//   art      left (default) or right: the art column on the right (flex row reversed, so the slot numbers stay)
//   size     fills its parent (C5: the Page's Body, the whole content box)
//   columns  a column each, full height: the top slot, a grow spacer, the foot slot; slots stack their nodes with no
//            gap (a section header that isn't first gets layout:group above it: the spec builder's spacing rule),
//            and a full-width node (list row, section header) fills its column
// Nothing relayouts on updates (the nodes inside update themselves). States: none (a layout). Bleed: none.
// Workbench: built alone (not in a spec) or with prop outline, the slots are outlined and named, so the layout reads
// on its own. Contrast: section "layout" (the outline sample only).
#include "fw.h"
#include "fx.h"
#include "spec.h"

namespace {
bool art_right(fw::Props p) { return !strcmp(p["art"] | "left", "right"); }
bool radar_w(fw::Props p) { return !strcmp(p["art_w"] | "card", "radar"); }
std::string sig(fw::Props p) { return std::string((p["outline"] | false) ? "outline" : "") + (art_right(p) ? "|right" : "") + (radar_w(p) ? "|radar" : "") + ((p["art_mid"] | false) ? "|mid" : ""); }
void column(lv_obj_t *c, bool outline, const char *const names[2], bool mid = false) {
  if (mid) fx::use(fx::box(c, "free"), "grow");  // art middle: the top slot centred down the column
  for (int i = 0; i < 2; i++) {
    if (i) fx::use(fx::box(c, "free"), "grow");
    lv_obj_t *s = fx::box(c, "col");
    lv_obj_add_flag(s, fw::spec::SLOT);
    fx::slot(s, names[i]);
    if (outline) {  // the workbench sample: each slot a labelled outline
      for (const char *t : {"h:list-row-h", "border:line-hi", "pad:4"}) fx::use(s, t);
      fx::text(s, names[i], "type:label", "fg:text-dim");
    }
  }
}
}  // namespace

FW_COMPONENT(split, "Split", "components", R"({
  "about": "A layout: two columns over the screen, a card-wide one for the picture or control and the rest for the words, each with a top and a foot slot.",
  "contract": 2,
  "parts": [],
  "frame": [480, 400],
  "slots": 4,
  "props": [
    {"name": "art", "about": "Which side the card-wide column for the picture or control is on: left, or right with the words first (the Lightning map). The slots keep their numbers: 0 and 1 are always the picture column.", "type": "select", "options": ["left", "right"], "default": "left"},
    {"name": "art_w", "about": "How wide the picture column is: card (a Home card's width) or radar (the Lightning radar's square, wider).", "type": "select", "options": ["card", "radar"], "default": "card"},
    {"name": "art_mid", "about": "Centres the picture column's top slot down the screen instead of at the top (the Lightning radar).", "type": "bool", "default": false},
    {"name": "outline", "about": "For previews only: outlines the two columns and their top and foot areas so you can see where things go.", "type": "bool", "default": false}
  ],
  "variants": [
    {"label": "slots", "props": {"outline": true}},
    {"label": "art right", "props": {"outline": true, "art": "right"}}
  ]
})") {
  lv_obj_t *root = fx::box(parent, "free");
  for (const char *t : {"fill", art_right(p) ? "row-top-rev" : "row-top", "gap:9"}) fx::use(root, t);
  fx::name(root, "split");
  fw::shape(root, sig(p));
  const bool o = (p["outline"] | false) || !fw::spec::building_header();  // alone (workbench): show the slots
  static const char *const LN[2] = {"left", "left foot"}, *const RN[2] = {"right", "right foot"};
  lv_obj_t *l = fx::box(root, "col");
  for (const char *t : {"fill", radar_w(p) ? "w:radar" : "w:card-w"}) fx::use(l, t);
  column(l, o, LN, p["art_mid"] | false);
  lv_obj_t *r = fx::box(root, "col");
  for (const char *t : {"fill", "grow-w"}) fx::use(r, t);
  column(r, o, RN);
}

FW_UPDATE(split) { return fw::shape(root, sig(p)); }  // a spec rebinding its nodes keeps them inside: only the outline sample builds it again
