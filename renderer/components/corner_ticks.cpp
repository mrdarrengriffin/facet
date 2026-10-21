// Corner ticks: the Facet corner ticks atom (docs: @facet/ui atoms.ts cornerTicks(corners, len, thick), CSS .f-ticks):
// L-shaped accent marks on the corners of an active owner, the "selected, keeps its box" mark of states.json on.mark.
// Drawn by the core atom fw::corner_ticks (core/atoms.h), which every component calls; this page shows it alone.
//   corners  all = the four corners (Button secondary active); tr-bl = top-right + bottom-left (docs: cards and
//            tiles; no approved WASM component uses it yet). Cards / tiles: decide with the screens (Darren
//            2026-10-21: no ticks on the approved Card for now)
//   size     layout corner-tick (8 / 4) long x stroke corner-tick (2) wide (docs btn-tick-len / btn-tick-thick)
//   colour   st::on_mark (accent); st::on_mark_pressed (accent-text) while the owner is pressed, on the owner's border box (past its padding + border), inside the box: no bleed
//   shown    for a state selector (st::ON = LV_STATE_CHECKED, the default; 0 = always); corners = box only (Button: all)
// Static: no events, no sound. Pairs: colour.json contrast section "corner_ticks" (+ "button").
#include "fw.h"

FW_COMPONENT(corner_ticks, "Corner ticks", "components", R"({
  "frame": [240, 112],
  "props": [
    {"name": "on", "type": "select", "options": ["box", "button"], "default": "box"},
    {"name": "corners", "type": "select", "options": ["all", "tr-bl"], "default": "all"},
    {"name": "state", "type": "select", "options": ["active", "active-pressed", "default"], "default": "active"}
  ],
  "variants": [
    {"label": "box · all", "props": {}},
    {"label": "box · tr-bl = cards / tiles, decide with the screens", "props": {"corners": "tr-bl"}},
    {"label": "Button secondary · active", "props": {"on": "button"}},
    {"label": "Button secondary · active + pressed", "props": {"on": "button", "state": "active-pressed"}},
    {"label": "Button secondary · default = none", "props": {"on": "button", "state": "default"}}
  ]
})") {
  if (!strcmp(p["on"] | "box", "button")) {  // the real Button: it calls fw::corner_ticks itself (all, while active)
    const fw::Component *b = fw::find("button");
    if (!b) return;
    ArduinoJson::JsonDocument bp;
    bp["kind"] = "secondary";
    bp["label"] = "Cancel";
    bp["state"] = p["state"] | "active";
    b->build(parent, bp.as<fw::Props>());
    return;
  }
  // a plain surface box (card look: surface fill, 1 px line border, square corners)
  lv_obj_t *box = lv_obj_create(parent);
  lv_obj_remove_style_all(box);
  lv_obj_remove_flag(box, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_size(box, fw::lay::tile_w, fw::touch::target);
  lv_obj_set_style_radius(box, fw::rad::card, 0);
  lv_obj_set_style_bg_color(box, fw::T->surface, 0);
  lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(box, fw::stroke::hair, 0);
  lv_obj_set_style_border_color(box, fw::T->line, 0);
  // shown while active (CHECKED), like a component would; state default = hidden (the box has no press look)
  fw::corner_ticks(box, strcmp(p["corners"] | "all", "tr-bl") ? fw::Corners::all : fw::Corners::tr_bl);
  lv_obj_add_flag(box, LV_OBJ_FLAG_STATE_TRICKLE);  // the ticks are children: they follow the box state
  if (strcmp(p["state"] | "active", "default")) lv_obj_add_state(box, LV_STATE_CHECKED);
}
