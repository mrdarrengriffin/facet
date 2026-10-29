// Rate strip: the Facet rate strip (docs: Components > Charts > Rate strip, @facet/ui charts.ts rateStrip()), on the
// contract (renderer/CONTRACT.md, "contract": 2): price bands across the day as a strip of runs (equal slots, one
// rounded run per stretch of the same band, a small gap between runs), with a notch at now and an optional hour axis.
// Bands always come with a legend or a label somewhere (docs: never colour alone). Built with the Charts foundation's
// token strip (core/chart.h fw::chart::token_strip: drawn through the Pen with colour token names, so a theme switch
// repaints it and a tuned token re-sizes it).
//   size    width prop (dp; 0 = its parent's content width: a screen's full column, under a full-width Bar chart),
//           chart:strip-h tall (8 at both densities: docs 8; 4 at 480 was too thin to read) + the axis when shown
//   runs    BAND_COLOUR (docs): cheap blue, normal band-normal, peak accent (the chart highlight); chart:bar-radius
//           ends, stroke:edge (2) between runs (docs 2)
//   now     a stroke:edge wide notch in the canvas colour (bg) at the middle of the now slot, the strip's height
//           (docs: a text-coloured tick, 1.6-2.8:1 on the light bands in dark; the notch meets each band at 3:1+)
//   ticks   hours between axis labels (0 = none): "00" "06" ... "24" in c-meta-sm text-dim, space:1 under the strip,
//           placed by time (fraction of the day), the first left-aligned and the last right-aligned (docs axis())
// Static: no touch, no sound. Update path: new bands / now / ticks / width redraw in place (one invalidate, silent).
// Contrast pairs: section "rate_strip".
#include "fw.h"
#include "fx.h"
#include "chart.h"

namespace {
const char *DEF = "cheap,cheap,cheap,cheap,cheap,cheap,normal,normal,normal,normal,normal,normal,normal,normal,normal,normal,peak,peak,peak,normal,normal,normal,cheap,cheap";
int now_of(fw::Props p) {  // a number, or a bound state (text: "10")
  return p["now"].is<const char *>() ? atoi(p["now"].as<const char *>()) : (int) lround(p["now"] | 14.0);
}
}  // namespace

FW_COMPONENT(rate_strip, "Rate strip", "components", R"({
  "about": "Price bands as a strip of runs, with a notch at now and an optional hour axis under it.",
  "contract": 2,
  "parts": [],
  "frame": [448, 64],
  "props": [
    {"name": "bands", "about": "The price band of each slot from midnight, separated by commas: cheap, normal or peak. Any number of slots shares the day equally (24 = hours, 48 = half-hours).", "type": "text", "default": "cheap,cheap,cheap,cheap,cheap,cheap,normal,normal,normal,normal,normal,normal,normal,normal,normal,normal,peak,peak,peak,normal,normal,normal,cheap,cheap"},
    {"name": "now", "about": "Which slot is now, counting from 0; it gets the notch. -1 shows no notch.", "type": "range", "min": -1, "max": 47, "step": 1, "default": 14},
    {"name": "width", "about": "How wide the strip is, in pixels; 0 fills the space it is in.", "type": "range", "min": 0, "max": 440, "step": 8, "default": 320},
    {"name": "ticks", "about": "Hour labels under the strip, every this many hours (6 = 00, 06, 12, 18, 24); 0 shows none.", "type": "range", "min": 0, "max": 12, "step": 1, "default": 0}
  ],
  "variants": [
    {"label": "a day · now 14:00", "props": {}},
    {"label": "with the hour axis", "props": {"ticks": 6}},
    {"label": "no now", "props": {"now": -1}},
    {"label": "narrow", "props": {"width": 200, "now": 7}}
  ]
})") {
  lv_obj_t *root = lv_obj_create(parent);
  lv_obj_remove_style_all(root);
  lv_obj_remove_flag(root, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  fx::name(root, "rate_strip");
  const int w = p["width"] | 320;
  for (const char *t : {"box", "col"}) fx::use(root, t);
  if (w > 0) fx::use(root, "hug");
  fw::chart::token_strip(root, fw::chart::words(p, "bands", DEF), now_of(p), w, p["ticks"] | 0);
}

FW_UPDATE(rate_strip) {
  lv_obj_t *s = lv_obj_get_child(root, 0);
  const int w = p["width"] | 320;
  for (const char *t : {"box", "col"}) fx::use(root, t);
  if (w > 0) fx::use(root, "hug");
  return fw::chart::token_strip_set(s, fw::chart::words(p, "bands", DEF), now_of(p), w, p["ticks"] | 0);
}
