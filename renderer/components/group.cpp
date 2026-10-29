// Group: a LAYOUT component (pages are made only from components; Darren 2026-10-23), on the contract
// (renderer/CONTRACT.md, "contract": 2). One slot its parent's width wide that wraps its nodes in rows, the gutter
// apart both ways: tiles three across, cards or camera tiles two across, or a row of buttons. A spec puts nodes in
// with "in" (core/spec.h).
//   flow     wrap (the default: rows, gap:gutter) | stack (one column, no gap: a view's blocks, which set their own
//            space, the Vacuum's Rooms / Map views shown by the spec's "show")
//   align    start (left, the default) | end (right, centred across: wrap-end; a page's action buttons)
//   space    none | group: above:group (a group of buttons after a block that isn't a section header) | block:
//            above:9 (the panel lab's Vacuum: blocks inside one view, 32 / 20 apart) | near: above:7 (24 / 16) |
//            line: above:3 (12 / 8: a line that belongs to the block above, the Weather rain line) | strip: above:8
//            (28 / 20: a strip under a hero and its lines, the Weather hours)
// Its nodes update themselves. States: none (a layout). Bleed: none.
// Workbench: built alone (not in a spec) three tile outlines show the wrap (prop sample sets how many).
// Contrast: section "layout" (the sample outlines only).
#include "fw.h"
#include "fx.h"
#include "spec.h"

namespace {
std::string sig(fw::Props p) {
  return std::to_string(p["sample"] | 0) + (p["align"] | "start") + (p["flow"] | "wrap") + (p["space"] | "none");
}
}  // namespace

FW_COMPONENT(group, "Group", "components", R"({
  "about": "A layout: a full-width row that wraps its parts, a gutter apart, for tiles, cards or a row of buttons.",
  "contract": 2,
  "parts": [],
  "frame": [480, 0],
  "slots": 1,
  "props": [
    {"name": "align", "about": "Where the parts line up: start is the left, centre is the middle (a stack centres each part), end is the right.", "type": "select", "options": ["start", "centre", "end"], "default": "start"},
    {"name": "flow", "about": "wrap lays the parts side by side and starts a new line when the row is full; stack puts each part under the last.", "type": "select", "options": ["wrap", "stack"], "default": "wrap"},
    {"name": "space", "about": "The room above the group: none, group for related parts, block for separate blocks, near for parts that belong tightly together, line for a line that belongs to the block above, strip for a strip under a hero and its lines.", "type": "select", "options": ["none", "group", "block", "near", "line", "strip"], "default": "none"},
    {"name": "sample", "about": "For previews only: how many made-up tiles fill the row. 0 shows the empty layout.", "type": "range", "min": 0, "max": 6, "step": 1, "default": 0}
  ],
  "variants": [
    {"label": "four tiles", "props": {"sample": 4}},
    {"label": "end · two", "props": {"sample": 2, "align": "end"}}
  ]
})") {
  const bool stack = !strcmp(p["flow"] | "wrap", "stack"), end = !strcmp(p["align"] | "start", "end"),
             mid = !strcmp(p["align"] | "start", "centre");
  lv_obj_t *g = fx::box(parent, stack ? (mid ? "col-centre" : "col") : "free");
  if (!stack) for (const char *t : {end ? "wrap-end" : mid ? "wrap-centre" : "wrap", "gap:gutter"}) fx::use(g, t);
  fx::name(g, "group");
  fw::shape(g, sig(p));
  lv_obj_add_flag(g, fw::spec::SLOT);
  fx::slot(g, "group");
  const char *sp = p["space"] | "none";
  if (!strcmp(sp, "group")) fx::use(g, "above:10");  // layout:group (48 / 32) is a structure size with no above: token (it aborted); space 10 = 40 / 28 until it gets one
  else if (!strcmp(sp, "block")) fx::use(g, "above:9");
  else if (!strcmp(sp, "near")) fx::use(g, "above:7");
  else if (!strcmp(sp, "line")) fx::use(g, "above:3");
  else if (!strcmp(sp, "strip")) fx::use(g, "above:8");
  const int n = (p["sample"] | 0) ? (p["sample"] | 0) : fw::spec::building_header() ? 0 : 3;  // alone: three outlines
  for (int i = 0; i < n; i++) {  // the workbench sample: tile-sized outlines
    lv_obj_t *t = fx::box(g, "free");
    for (const char *k : {"w:tile-w", "h:room-tile-h", "border:line-hi"}) fx::use(t, k);
  }
}

FW_UPDATE(group) { return fw::shape(root, sig(p)); }  // a spec rebinding its nodes keeps them inside: only the sample builds it again
