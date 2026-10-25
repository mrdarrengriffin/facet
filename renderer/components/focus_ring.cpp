// Focus ring: the Facet focus ring atom (docs: @facet/ui atoms.ts focusRing(offset, r), CSS .f-ring): a 1 px accent
// ring `offset` px outside its owner, radius = the owner's radius + the offset. The panels have no keyboard focus, so
// it marks the active primary (states.json focus). Drawn by the core atom fw::focus_ring (core/atoms.h, LVGL outline +
// fw::bleed so no parent clips it), which Button primary and the play Icon button call; this page shows it alone.
//   width    stroke focus (1); colour st::focus_ring (accent)
//   offset   touch focus-offset (8 / 4) by default; the box shows none / space 2 / space 4 as well (docs range 0-16)
//   hidden   while disabled / unavailable (st::OFF), even when active
// Static: no events, no sound. Pairs: colour.json contrast section "focus_ring" (+ "button", "icon_button").
#include "fw.h"

namespace {
// the root = a content-sized wrapper (no look of its own) holding the box or the real component, so a change of `on`
// keeps one root for the update path; user data = what it holds (on + offset)
intptr_t kind_of(fw::Props p) {
  const char *on = p["on"] | "box", *o = p["offset"] | "focus-offset";
  return (!strcmp(on, "box") ? 0 : !strcmp(on, "play") ? 1 : 2) * 8 +
         (!strcmp(o, "none") ? 0 : !strcmp(o, "space-2") ? 1 : !strcmp(o, "space-4") ? 2 : 3);
}
// the real component's props (Button / play Icon button)
void child_props(fw::Props p, ArduinoJson::JsonDocument &bp) {
  if (!strcmp(p["on"] | "box", "play")) bp["style"] = "play";
  bp["state"] = (p["disabled"] | false) ? "disabled" : "active";
}
void fill(lv_obj_t *w, fw::Props p) {
  lv_obj_remove_style_all(w);
  lv_obj_remove_flag(w, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_size(w, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_user_data(w, (void *) kind_of(p));
  const char *on = p["on"] | "box";
  const bool off = p["disabled"] | false;
  if (strcmp(on, "box")) {  // the real components: they call fw::focus_ring themselves (shown while active)
    const fw::Component *c = fw::find(!strcmp(on, "play") ? "icon_button" : "button");
    if (!c) return;
    ArduinoJson::JsonDocument bp;
    child_props(p, bp);
    c->build(w, bp.as<fw::Props>());
    // Button's disabled state is plain disabled; show "active + disabled" = the ring hides even when active
    if (off) if (lv_obj_t *b = lv_obj_get_child(w, -1)) lv_obj_add_state(b, LV_STATE_CHECKED);
    return;
  }
  // a plain surface box with the button radius, so the ring's radius (owner radius + offset) shows
  const char *o = p["offset"] | "focus-offset";
  const int offset = !strcmp(o, "none") ? 0 : !strcmp(o, "space-2") ? fw::sp::s2 : !strcmp(o, "space-4") ? fw::sp::s4 : fw::st::FOCUS_OFFSET;
  lv_obj_t *box = lv_obj_create(w);
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
}  // namespace

FW_COMPONENT(focus_ring, "Focus ring", "components", R"({
  "about": "A thin accent outline around a control that is active or selected.",
  "audit": {"snaps": {"on": "the sample owner, box, button or play, not a state"}},
  "frame": [240, 112],
  "icons": ["play@feature", "pause@feature"],
  "props": [
    {"name": "on", "about": "What the outline goes round: box for a card or tile, button for a button, play for the round play button.", "type": "select", "options": ["box", "button", "play"], "default": "box"},
    {"name": "offset", "about": "The gap between the control and its outline: the usual focus gap, none for no gap, or a small or larger step.", "type": "select", "options": ["focus-offset", "none", "space-2", "space-4"], "default": "focus-offset"},
    {"name": "disabled", "about": "Greys the outline, for a chosen control that cannot be used right now.", "type": "bool", "default": false}
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
  fill(lv_obj_create(parent), p);
}

// Update path: disabled moves in place (box: the ring fades out / in with fw::st::set_off; Button / Icon button: their
// own update, the ring kept hidden while disabled), a different `on` / offset builds the wrapper's contents again
// (fw::refill, snap). Silent.
FW_UPDATE(focus_ring) {
  lv_obj_t *in = lv_obj_get_child(root, 0);
  const bool off = p["disabled"] | false;
  if (in && (intptr_t) lv_obj_get_user_data(root) == kind_of(p)) {
    if (strcmp(p["on"] | "box", "box")) {
      ArduinoJson::JsonDocument bp;
      child_props(p, bp);
      const bool play = !strcmp(p["on"] | "box", "play");
      if (fw::update_child(fw::find(play ? "icon_button" : "button"), in, bp.as<fw::Props>())) {
        if (off) lv_obj_add_state(in, LV_STATE_CHECKED);
        return true;
      }
    } else {
      fw::st::set_off(in, off);
      return true;
    }
  }
  fw::refill(root);
  fill(root, p);
  return true;
}
