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

namespace {
// the root = a content-sized wrapper (no look of its own) holding the box or the real Button, so a change of `on`
// keeps one root for the update path; user data = what it holds (on + corners), to know what an update can move
intptr_t kind_of(fw::Props p) {
  return (strcmp(p["on"] | "box", "button") ? 0 : 1) | (strcmp(p["corners"] | "all", "tr-bl") ? 0 : 2);
}
lv_obj_t *wrapper(lv_obj_t *parent) {
  lv_obj_t *w = lv_obj_create(parent);
  return w;
}
void fill(lv_obj_t *w, fw::Props p) {
  lv_obj_remove_style_all(w);
  lv_obj_remove_flag(w, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_size(w, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_user_data(w, (void *) kind_of(p));
  if (!strcmp(p["on"] | "box", "button")) {  // the real Button: it calls fw::corner_ticks itself (all, while active)
    const fw::Component *b = fw::find("button");
    if (!b) return;
    ArduinoJson::JsonDocument bp;
    bp["kind"] = "secondary";
    bp["label"] = "Cancel";
    bp["state"] = p["state"] | "active";
    b->build(w, bp.as<fw::Props>());
    return;
  }
  // a plain surface box (card look: surface fill, 1 px line border, square corners)
  lv_obj_t *box = lv_obj_create(w);
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
}  // namespace

FW_COMPONENT(corner_ticks, "Corner ticks", "components", R"({
  "about": "Small accent marks on the corners of a box that show it is active.",
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
  fill(wrapper(parent), p);
}

// Update path: the state moves in place (box: the ticks fade in / out with fw::st::set_state; Button: its own update),
// a different `on` / corners builds the wrapper's contents again (fw::refill, snap). Silent.
FW_UPDATE(corner_ticks) {
  lv_obj_t *in = lv_obj_get_child(root, 0);
  if (in && (intptr_t) lv_obj_get_user_data(root) == kind_of(p)) {
    if (!strcmp(p["on"] | "box", "button")) {
      ArduinoJson::JsonDocument bp;
      bp["kind"] = "secondary";
      bp["label"] = "Cancel";
      bp["state"] = p["state"] | "active";
      if (fw::update_child(fw::find("button"), in, bp.as<fw::Props>())) return true;
    } else {
      fw::st::set_state(in, LV_STATE_CHECKED, strcmp(p["state"] | "active", "default") != 0);
      return true;
    }
  }
  fw::refill(root);
  fill(root, p);
  return true;
}
