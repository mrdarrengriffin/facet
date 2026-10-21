// Focus ring: the Facet focus ring atom (docs: @facet/ui atoms.ts focusRing(offset, r), CSS .f-ring): a 1 px accent
// ring `offset` px outside its owner, radius = the owner's radius + the offset. The panels have no keyboard focus, so
// it marks the active primary (states.json focus). Drawn by the core atom fw::focus_ring (core/atoms.h, LVGL outline +
// fw::bleed so no parent clips it), which Button primary and the play Icon button call; this page shows it alone.
//   width    stroke focus (1); colour st::focus_ring (accent)
//   offset   touch focus-offset (8 / 4) by default; the box shows none / space 2 / space 4 as well (docs range 0-16)
//   hidden   while disabled / unavailable (st::OFF), even when active
// Static: no events, no sound. Pairs: colour.json contrast section "focus_ring" (+ "button", "icon_button").
#include "fw.h"

FW_COMPONENT(focus_ring, "Focus ring", "components", R"({
  "about": "A thin accent outline around a control that is active or selected.",
  "frame": [240, 112],
  "icons": ["play@feature", "pause@feature"],
  "props": [
    {"name": "on", "type": "select", "options": ["box", "button", "play"], "default": "box"},
    {"name": "offset", "type": "select", "options": ["focus-offset", "none", "space-2", "space-4"], "default": "focus-offset"},
    {"name": "disabled", "type": "bool", "default": false}
  ],
  "variants": [
    {"label": "box · focus-offset", "props": {}},
    {"label": "box · none", "props": {"offset": "none"}},
    {"label": "box · space 2", "props": {"offset": "space-2"}},
    {"label": "box · space 4", "props": {"offset": "space-4"}},
    {"label": "box · disabled = hidden", "props": {"disabled": true}},
    {"label": "Button primary · active", "props": {"on": "button"}},
    {"label": "Icon button play · active", "props": {"on": "play"}},
    {"label": "Button primary · active + disabled = hidden", "props": {"on": "button", "disabled": true}}
  ]
})") {
  const char *on = p["on"] | "box";
  const bool off = p["disabled"] | false;
  if (strcmp(on, "box")) {  // the real components: they call fw::focus_ring themselves (shown while active)
    const bool play = !strcmp(on, "play");
    const fw::Component *c = fw::find(play ? "icon_button" : "button");
    if (!c) return;
    ArduinoJson::JsonDocument bp;
    if (play) bp["style"] = "play";
    bp["state"] = off ? "disabled" : "active";
    c->build(parent, bp.as<fw::Props>());
    // Button's disabled state is plain disabled; show "active + disabled" = the ring hides even when active
    if (off) if (lv_obj_t *b = lv_obj_get_child(parent, -1)) lv_obj_add_state(b, LV_STATE_CHECKED);
    return;
  }
  // a plain surface box with the button radius, so the ring's radius (owner radius + offset) shows
  const char *o = p["offset"] | "focus-offset";
  const int offset = !strcmp(o, "none") ? 0 : !strcmp(o, "space-2") ? fw::sp::s2 : !strcmp(o, "space-4") ? fw::sp::s4 : fw::st::FOCUS_OFFSET;
  lv_obj_t *box = lv_obj_create(parent);
  lv_obj_remove_style_all(box);
  lv_obj_remove_flag(box, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_size(box, fw::lay::tile_w, fw::touch::target);
  lv_obj_set_style_radius(box, fw::rad::button, 0);
  lv_obj_set_style_bg_color(box, fw::T->surface, 0);
  lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(box, fw::stroke::hair, 0);
  lv_obj_set_style_border_color(box, fw::T->line, 0);
  fw::focus_ring(box, 0, offset);
  if (off) lv_obj_add_state(box, fw::st::OFF);
}
