// Bins next: the Bins screen's hero, the next collection (the panel lab's Bins screen, board facet-72: the old
// bins_view hero), on the contract (renderer/CONTRACT.md, "contract": 2). Specific to the Bins screen.
//   row      a real Bins art (fw::find("bins_art"), C8b) and a column beside it, the gutter apart, centred on the row
//   column   the caption (type:label caps, fg:accent-text when it matters now: put out tonight / collection today,
//            else fg:text-dim), gap:2, then when (type:value, fg:text: "Tomorrow", "Today", the weekday), which bins
//            (type:title, fg:text: "Green bin + Black bin") and the date + how often (type:body, fg:text-dim:
//            "Fri 24 Oct · every 2 weeks"); the column takes the row's room (grow) and its lines fill it
// Feed: bind every prop to panel.bins (core/bins_source.cpp: colours, caption, accent ("on" / "off"), when, bins, line).
// Static: no touch, no sound. Update path: the texts and the bins in place (silent); a new accent fades the caption
// (fw::st::refill_fade). Contrast: text, label (accent-text / text-dim on the canvas) + bins (decor).
#include "fw.h"
#include "fx.h"

namespace {
bool accent(fw::Props p) { return p["accent"].is<const char *>() ? !strcmp(p["accent"].as<const char *>(), "on") : (p["accent"] | false); }  // a bind gives "on" / "off"
const char *cap_ink(fw::Props p) { return accent(p) ? "fg:accent-text" : "fg:text-dim"; }
void art_props(fw::Props p, ArduinoJson::JsonDocument &d) { d["colours"] = p["colours"] | "green,black"; }
void fill(lv_obj_t *root, fw::Props p) {
  for (const char *t : {"box", "row", "fill-w"}) fx::use(root, t);
  fw::shape(root, accent(p) ? "a" : "-", "next");
  ArduinoJson::JsonDocument d;
  art_props(p, d);
  fw::find("bins_art")->build(root, d.as<fw::Props>());
  lv_obj_t *col = fx::box(root, "col");
  for (const char *t : {"grow", "gap:2"}) fx::use(col, t);
  fx::use(fx::text(col, p["caption"] | "", "type:label", cap_ink(p)), "fill-w");
  lv_obj_t *lines = fx::box(col, "col");
  fx::use(lines, "fill-w");
  fx::use(fx::text(lines, p["when"] | "", "type:value", "fg:text"), "fill-w");
  fx::use(fx::text(lines, p["bins"] | "", "type:title", "fg:text"), "fill-w");
  fx::use(fx::text(lines, p["line"] | "", "type:body", "fg:text-dim"), "fill-w");
}
}  // namespace

FW_COMPONENT(bins_next, "Bins next", "components", R"({
  "about": "The next bin collection: its bins drawn in their colours, when to put them out, which bins and the date.",
  "contract": 2,
  "parts": [],
  "frame": [448, 200],
  "props": [
    {"name": "colours", "about": "The bins' colours, separated by commas, the first in front.", "type": "text", "default": "black"},
    {"name": "caption", "about": "The small caption over the day, such as put out tonight.", "type": "text", "default": "put out tonight"},
    {"name": "accent", "about": "Draws the caption in the accent colour, for when the bins go out tonight or today.", "type": "bool", "default": true},
    {"name": "when", "about": "The day of the collection, such as Tomorrow, Today or a weekday.", "type": "text", "default": "Tomorrow"},
    {"name": "bins", "about": "Which bins go out, such as Green bin + Black bin.", "type": "text", "default": "Black bin"},
    {"name": "line", "about": "The date and how often, such as Fri 24 Oct · every 2 weeks.", "type": "text", "default": "Fri 24 Oct · every 2 weeks"}
  ],
  "variants": [
    {"label": "put out tonight", "props": {}},
    {"label": "two bins · next week", "props": {"colours": "green,brown", "caption": "next collection", "accent": false, "when": "Thursday", "bins": "Green bin + Brown bin", "line": "Thu 29 Oct · every 2 weeks"}},
    {"label": "collected today", "props": {"colours": "green", "caption": "collected today · bring it in", "accent": false, "when": "Today", "bins": "Green bin", "line": "Fri 23 Oct · every 2 weeks"}}
  ]
})") {
  lv_obj_t *root = lv_obj_create(parent);
  lv_obj_remove_style_all(root);
  lv_obj_remove_flag(root, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  fx::name(root, "bins_next");
  fill(root, p);
}

FW_UPDATE(bins_next) {
  lv_obj_t *art = lv_obj_get_child(root, 0), *col = lv_obj_get_child(root, 1);
  if (!art || !col || !fw::shape(root, accent(p) ? "a" : "-", "next")) {
    fw::st::refill_fade(root, [&] { fw::refill(root); fill(root, p); }, true, true);
    return true;
  }
  ArduinoJson::JsonDocument d;
  art_props(p, d);
  fw::update_child(fw::find("bins_art"), art, d.as<fw::Props>());
  fx::set_text(lv_obj_get_child(col, 0), p["caption"] | "", "type:label");
  lv_obj_t *lines = lv_obj_get_child(col, 1);
  fx::set_text(lv_obj_get_child(lines, 0), p["when"] | "", "type:value");
  fx::set_text(lv_obj_get_child(lines, 1), p["bins"] | "", "type:title");
  fx::set_text(lv_obj_get_child(lines, 2), p["line"] | "", "type:body");
  return true;
}
