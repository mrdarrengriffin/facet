// Entity tile: a Home card at size s (screen model §4: the tile, three across), on the contract (renderer/CONTRACT.md,
// "contract": 2). One thing in the room at a glance: its icon, its name and its state, and a tap that does the one
// thing (toggle a light, open its screen). The Card's look (the same tokens and the theme's card part), with an entity
// icon on top and the name + state at the bottom.
//   box      fills its parent (a Home grid cell; meta min layout:tile-w x tile-h, 208 / 136 square), bg:surface,
//            border:line, radius:tile, card-pad (children count from the outer edge); a column; part:card
//   icon     icon size (24 / 16): text-dim, on-mark (accent) while on; top-left on the padding line
//   (grow)
//   label    type body, text, up to two lines in a fixed box of two body lines (tile-text), "..."
//   status   type label (caps), one line, space 5 above (tile-status): the state ("Off", "64%", "Open"); on-ink
//            (accent-text) while on, else text-dim
// Props: icon (a UI or screen icon name), label, status, on (bool), act toggle | open | none (what a tap does: the
// sound and the instant look; none = read-only, a Home sensor tile: not clickable, no press look), state default |
// pressed | unavailable, press (the Card's press styles).
// States (tokens per state, C17): on = LV_STATE_USER_2 (not CHECKED: the theme's card part has no "on" look of its
// own), fading with Motion toggle (fade-switch); pressed = the Card press (fade-press / fade-release); unavailable
// (fw::st::OFF): icon + label unavailable-ink, status "Unavailable" in unavailable-note, no press look; a tap = blocked
// + error. n/a: loading, unknown, empty, error (the binding maps those to unavailable).
// Tap (states.json): act toggle = the tile shows the other side at once (like a tap on a Toggle) + sound toggle-on /
// toggle-off + event "tap" (the Home's spec sends the toggle; HA's answer settles it); act open = sound open + event
// "tap" (the Home opens the screen). Update path: label / status texts in place, on / off fades (silent); icon or
// unavailable <-> default = the contents again, faded in. Bleed: none. Contrast pairs: tokens/colour.json "entity_tile".
#include "fw.h"
#include "fx.h"

namespace {
constexpr lv_state_t ST_OFF = fw::st::OFF, ST_ON = LV_STATE_USER_2;
constexpr lv_obj_flag_t TOGGLES = LV_OBJ_FLAG_USER_4;  // a tap toggles (else opens)
struct Tile { lv_obj_t *label = nullptr, *status = nullptr; };
void tile_free(lv_event_t *e) { delete (Tile *) lv_event_get_user_data(e); }
Tile *tile_of(lv_obj_t *r) {  // the Tile rides on the root's DELETE event
  for (uint32_t i = 0, n = lv_obj_get_event_count(r); i < n; i++) {
    lv_event_dsc_t *d = lv_obj_get_event_dsc(r, i);
    if (lv_event_dsc_get_cb(d) == tile_free) return (Tile *) lv_event_dsc_get_user_data(d);
  }
  return nullptr;
}
void fades(lv_obj_t *o) {  // the Motion toggle fade on the states the part changes in
  for (lv_state_t st : {(lv_state_t) 0, ST_ON}) fx::use(o, "fade-switch", st);
}
// on: a bool, or a state word "on" (a bind's map: a cover's open)
bool on_of(fw::Props p) { return p["on"].is<bool>() ? p["on"].as<bool>() : !strcmp(p["on"] | "", "on"); }
std::string sig(fw::Props p) {  // what needs a rebuild: the icon, unavailable, the press style (texts and on move in place)
  return std::string(p["icon"] | "bulb") + "|" + (strcmp(p["state"] | "default", "unavailable") ? "" : "off") + "|" + (p["press"] | "");
}
void fill(lv_obj_t *c, fw::Props p) {
  Tile *k = tile_of(c);
  const char *state = p["state"] | "default";
  const bool off = !strcmp(state, "unavailable");
  fw::shape(c, sig(p));
  for (const char *t : {"fill", "col", "radius:tile", "card-pad", "bg:surface", "border:line", "fade-release"}) fx::use(c, t);
  const char *press = p["press"] | fw::st::PRESS_STYLE;
  const bool p_fill = strcmp(press, "border") != 0, p_border = strcmp(press, "fill") != 0;
  fx::use(c, p_fill ? "bg:pressed-tint" : "bg:surface", LV_STATE_PRESSED);
  fx::use(c, p_border ? "border:pressed-tint-line" : "border:pressed-line", LV_STATE_PRESSED);
  fx::use(c, "fade-press", LV_STATE_PRESSED);
  fx::use(c, "bg:surface", ST_OFF);  // no press look when unavailable
  fx::use(c, "border:line", ST_OFF);
  fx::use(c, "part:card");  // the theme's card (C46), after the tile's own state tokens

  lv_obj_t *ic = fx::icon(c, p["icon"] | "bulb", "icon", "fg:text-dim");
  fx::icon_use(ic, "ink:on-mark", ST_ON);
  fx::icon_use(ic, "ink:unavailable-ink", ST_OFF);
  for (lv_state_t st : {(lv_state_t) 0, ST_ON}) fx::icon_use(ic, "fade-switch", st);
  fx::use(fx::box(c, "free"), "grow");
  k->label = fx::text(c, p["label"] | "", "type:body", off ? "fg:unavailable-ink" : "fg:text");
  fx::use(k->label, "tile-text");
  lv_label_set_long_mode(k->label, LV_LABEL_LONG_MODE_DOTS);  // two lines, then "..."
  k->status = fx::text(c, off ? fw::st::UNAVAILABLE_TEXT : (p["status"] | ""), "type:label", off ? "fg:unavailable-note" : "fg:text-dim");
  fx::use(k->status, "tile-status");
  if (!off) fx::use(k->status, "fg:on-ink", ST_ON);
  fades(k->status);

  const bool pressed = !strcmp(state, "pressed");
  if (pressed != lv_obj_has_state(c, LV_STATE_PRESSED)) lv_obj_set_state(c, LV_STATE_PRESSED, pressed);
  if (off) fx::state(c, ST_OFF, true);
  fw::st::set_state(c, ST_ON, !off && on_of(p), false);
  lv_obj_set_flag(c, TOGGLES, !strcmp(p["act"] | "toggle", "toggle"));
  lv_obj_set_flag(c, LV_OBJ_FLAG_CLICKABLE, strcmp(p["act"] | "toggle", "none") != 0);  // none: read-only
}
// a text only when it changes (an update with the same props redraws nothing)
void put(lv_obj_t *l, const char *type, const char *txt) {
  if (l && strcmp(lv_label_get_text(l), txt)) fx::set_text(l, txt, type);
}
}  // namespace

FW_COMPONENT(entity_tile, "Entity tile", "components", R"({
  "about": "A Home tile for one thing in the room: its icon, name and state; a tap toggles it or opens its screen.",
  "contract": 2,
  "parts": ["card"],
  "min": ["layout:tile-w", "layout:tile-h"],
  "frame": [240, 240],
  "icons": ["bulb@icon", "power@icon", "screen-covers@icon", "screen-timer@icon", "screen-media@icon", "screen-weather@icon", "thermometer@icon", "door@icon", "window@icon", "person@icon", "alert@icon", "bolt@icon", "house@icon"],
  "props": [
    {"name": "icon", "about": "The icon on the tile, picked for the kind of thing it is, such as a bulb for a light or a door for a door sensor.", "type": "select", "options": ["bulb", "power", "screen-covers", "screen-timer", "screen-media", "screen-weather", "thermometer", "door", "window", "person", "alert", "bolt", "house"], "default": "bulb"},
    {"name": "label", "about": "The thing's name on the tile.", "type": "text", "default": "Sofa table"},
    {"name": "status", "about": "The short state under the name, such as 64% or Off.", "type": "text", "default": "64%"},
    {"name": "on", "about": "Whether the thing is on. On tiles are lit in the accent colour.", "type": "bool", "default": true},
    {"name": "act", "about": "What a tap does: toggle switches it on or off, open opens its own screen, none does nothing.", "type": "select", "options": ["toggle", "open", "none"], "default": "toggle"},
    {"name": "state", "debug": true, "about": "How the tile looks: default ready to tap, pressed while held, or unavailable when the device cannot be reached.", "type": "select", "options": ["default", "pressed", "unavailable"], "default": "default"},
    {"name": "press", "about": "How the tile shows it is being pressed: fill darkens the whole tile, border lights its edge, border-fill does both.", "type": "select", "options": ["fill", "border", "border-fill"], "default": "fill"}
  ],
  "variants": [
    {"label": "light · on", "props": {}},
    {"label": "light · off", "props": {"on": false, "status": "Off"}},
    {"label": "long name", "props": {"label": "Living room ceiling lights", "on": false, "status": "Off"}},
    {"label": "switch · on", "props": {"icon": "power", "label": "Dishwasher", "status": "On"}},
    {"label": "opens a screen", "props": {"icon": "screen-timer", "label": "Timer", "status": "", "on": false, "act": "open"}},
    {"label": "sensor · read only", "props": {"icon": "thermometer", "label": "Kitchen", "status": "21.4\u00b0", "on": false, "act": "none"}},
    {"label": "pressed", "props": {"state": "pressed", "on": false, "status": "Off"}},
    {"label": "unavailable", "props": {"label": "Chair", "state": "unavailable"}}
  ]
})") {
  lv_obj_t *c = lv_obj_create(parent);  // styles: fill() (an update refills the same root)
  lv_obj_remove_style_all(c);
  lv_obj_remove_flag(c, LV_OBJ_FLAG_SCROLLABLE);
  fx::name(c, "entity_tile");
  lv_obj_add_event_cb(c, tile_free, LV_EVENT_DELETE, new Tile);
  fill(c, p);
  fw::st::on_tap(c, [](lv_event_t *e) {
    if (!fw::st::tap_now(e)) return;
    auto *o = (lv_obj_t *) lv_event_get_current_target(e);
    const char *d = lv_label_get_text(tile_of(o)->label);
    if (lv_obj_has_state(o, ST_OFF)) { fw::st::blocked(o, d); return; }
    if (lv_obj_has_flag(o, TOGGLES)) {  // toggle: the other side at once (HA settles it)
      const bool on = !lv_obj_has_state(o, ST_ON);
      fw::st::set_state(o, ST_ON, on, true);
      fw::sound(o, on ? fw::st::ON_SOUND : fw::st::OFF_SOUND);
    } else fw::sound(o, "open");
    fw::emit(o, "tap", d);
  }, nullptr);
}

// Update path: same icon + availability = texts in place and on / off as a fade (silent, like a tap); otherwise the
// contents again (fw::refill; default <-> unavailable fades them in).
FW_UPDATE(entity_tile) {
  Tile *k = tile_of(root);
  if (!k) return false;
  const char *state = p["state"] | "default";
  const bool was_off = lv_obj_has_state(root, ST_OFF), off = !strcmp(state, "unavailable");
  if (fw::shape(root, sig(p))) {
    put(k->label, "type:body", p["label"] | "");
    if (!off) put(k->status, "type:label", p["status"] | "");
    const bool on = !off && on_of(p);
    if (on != lv_obj_has_state(root, ST_ON)) fw::st::set_state(root, ST_ON, on, true);
    lv_obj_set_flag(root, TOGGLES, !strcmp(p["act"] | "toggle", "toggle"));
    lv_obj_set_flag(root, LV_OBJ_FLAG_CLICKABLE, strcmp(p["act"] | "toggle", "none") != 0);
    const bool pr = !strcmp(state, "pressed");
    if (pr != lv_obj_has_state(root, LV_STATE_PRESSED)) lv_obj_set_state(root, LV_STATE_PRESSED, pr);
    return true;
  }
  fw::refill(root);
  fill(root, p);
  if (was_off != off) fw::st::fade_in_children(root);
  return true;
}
