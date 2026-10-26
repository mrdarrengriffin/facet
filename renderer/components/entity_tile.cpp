// Entity tile: a Home card at size s (screen model §4: the tile, three across). One thing in the room at a glance:
// its icon, its name and its state, and a tap that does the one thing (toggle a light, open its screen). Built like the
// agent Tile (the same box, padding, press look and two-line title), with an entity icon in place of the mascot.
//   box      lay::tile_w x tile_h (208 x 208 / 136 x 136), surface fill, 1 px line border, radius rad::tile,
//            padding lay::tile_pad - hair (children count from the outer edge, as the Tile); flex column
//   icon     ic::icon (24 / 16): text-dim, or st::on_mark (accent) while on; top-left on the padding line
//   (grow)
//   label    screen type body, text, up to two lines in a box of 2 x the body box (Tile title), ellipsis
//   gap      space 5 (16 / 12, Tile)
//   status   type label (caps), one line, a fixed box: the state ("Off", "64%", "Open"); st::on_ink (accent-text) while
//            on, else text-dim
// Props: icon (a UI or screen icon name), label, status, on (bool), act toggle | open | none (what a tap does: the
// sound and the instant look; none = read-only, a Home sensor tile: not clickable, no press look), state default | pressed | unavailable.
// Tap (states.json): act toggle = the tile shows the other side at once (Motion toggle fade, like a tap on a Toggle)
// + sound toggle-on / toggle-off + event "tap" (the Home's spec sends the toggle; HA's answer settles it); act open =
// sound open + event "tap" (the Home opens the screen). Pressed = the Card press (states.json pressed.style).
// Unavailable: icon + label text-mute, status "Unavailable" text-dim, no press look; a tap = blocked + error.
// Update path: label / status texts in place (fixed boxes), on / off fades the icon and status colours (silent);
// unavailable <-> default builds the contents again with a fade. Contrast pairs: tokens/colour.json "entity_tile".
#include "fw.h"

namespace {
constexpr lv_state_t ST_OFF = fw::st::OFF;

const lv_style_prop_t TR_PROPS[] = {LV_STYLE_BG_COLOR, LV_STYLE_BORDER_COLOR, LV_STYLE_PROP_INV};
const lv_style_transition_dsc_t *tr(bool pressed) {
  static lv_style_transition_dsc_t d[2];
  static bool ready = false;
  if (!ready) { fw::st::press_transitions(d, TR_PROPS); ready = true; }
  return &d[pressed];
}
lv_obj_t *box(lv_obj_t *parent) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  return o;
}
// parts (fill order): 0 icon, 1 grow, 2 label, 3 gap, 4 status
lv_obj_t *part(lv_obj_t *c, int i) { return lv_obj_get_child(c, i); }
// the look the props ask for: user flag USER_3 = on
bool is_on(lv_obj_t *c) { return lv_obj_has_flag(c, LV_OBJ_FLAG_USER_3); }
void paint(lv_obj_t *c, bool on, bool quiet) {
  const fw::Theme &t = *fw::theme_of(c);  // a tap / update paints after the build: the tile's own theme, not fw::T
  lv_obj_set_flag(c, LV_OBJ_FLAG_USER_3, on);
  if (lv_obj_has_state(c, ST_OFF)) return;
  const lv_color_t ic = on ? fw::st::on_mark(t) : t.text_dim, tx = on ? fw::st::on_ink(t) : t.text_dim;
  if (quiet) {
    fw::icon_colour(part(c, 0), ic);
    lv_obj_set_style_text_color(part(c, 4), tx, 0);
    return;
  }
  fw::st::restyle(c, [&] {
    fw::icon_colour(part(c, 0), ic);
    lv_obj_set_style_text_color(part(c, 4), tx, 0);
  });
}
// on: a bool, or a state word "on" (a bind's map: a cover's open)
bool on_of(fw::Props p) { return p["on"].is<bool>() ? p["on"].as<bool>() : !strcmp(p["on"] | "", "on"); }
std::string sig(fw::Props p) {  // what needs a rebuild: the icon, unavailable (texts and on move in place)
  return std::string(p["icon"] | "bulb") + "|" + (strcmp(p["state"] | "default", "unavailable") ? "" : "off");
}
void fill(lv_obj_t *c, fw::Props p) {
  const fw::Theme &t = *fw::T;
  const char *state = p["state"] | "default";
  const bool off = !strcmp(state, "unavailable");
  lv_obj_remove_style_all(c);
  fw::shape(c, sig(p));
  lv_obj_add_flag(c, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(c, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(c, fw::lay::tile_w, fw::lay::tile_h);
  lv_obj_set_style_radius(c, fw::rad::tile, 0);
  lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(c, fw::stroke::hair, 0);
  lv_obj_set_style_pad_all(c, fw::lay::tile_pad - fw::stroke::hair, 0);
  lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
  const lv_style_selector_t SEL[3] = {0, LV_STATE_PRESSED, ST_OFF};
  const char *press = p["press"] | fw::st::PRESS_STYLE;
  const bool p_fill = strcmp(press, "border") != 0, p_border = strcmp(press, "fill") != 0;
  const lv_color_t BG[3] = {t.surface, p_fill ? fw::st::pressed_tint(t) : t.surface, t.surface};
  const lv_color_t BD[3] = {t.line, p_border ? fw::st::pressed_tint_line(t) : fw::st::pressed_line(t), t.line};
  for (int i = 0; i < 3; i++) {
    lv_obj_set_style_bg_color(c, BG[i], SEL[i]);
    lv_obj_set_style_border_color(c, BD[i], SEL[i]);
    lv_obj_set_style_transition(c, tr(i == 1), SEL[i]);
  }
  const lv_color_t mute = fw::st::unavailable_ink(t);
  fw::icon(c, p["icon"] | "bulb", fw::ic::icon, off ? mute : t.text_dim);
  lv_obj_t *grow = box(c);
  lv_obj_set_width(grow, 1);
  lv_obj_set_flex_grow(grow, 1);
  const int inner = fw::lay::tile_w - 2 * fw::lay::tile_pad;
  lv_obj_t *ti = fw::text(c, fw::ty::body, p["label"] | "", off ? mute : t.text, inner);
  lv_obj_remove_flag(ti, LV_OBJ_FLAG_CLICKABLE);
  lv_label_set_long_mode(ti, LV_LABEL_LONG_MODE_DOTS);
  lv_obj_set_size(ti, inner, 2 * fw::style(fw::ty::body).box);
  lv_obj_t *gap = box(c);
  lv_obj_set_size(gap, 1, fw::sp::s5);
  lv_obj_t *st = fw::text(c, fw::ty::label, off ? fw::st::UNAVAILABLE_TEXT : (p["status"] | ""), off ? fw::st::unavailable_note(t) : t.text_dim);
  lv_obj_remove_flag(st, LV_OBJ_FLAG_CLICKABLE);
  lv_label_set_long_mode(st, LV_LABEL_LONG_MODE_DOTS);
  lv_obj_set_size(st, inner, fw::style(fw::ty::label).box);
  const bool pressed = !strcmp(state, "pressed");
  if (pressed != lv_obj_has_state(c, LV_STATE_PRESSED)) lv_obj_set_state(c, LV_STATE_PRESSED, pressed);
  if (off) lv_obj_add_state(c, ST_OFF);
  paint(c, !off && on_of(p), true);
  lv_obj_set_flag(c, LV_OBJ_FLAG_USER_4, !strcmp(p["act"] | "toggle", "toggle"));  // a tap toggles (else opens)
  lv_obj_set_flag(c, LV_OBJ_FLAG_CLICKABLE, strcmp(p["act"] | "toggle", "none") != 0);  // none: read-only
}
// a text only when it changes (an update with the same props redraws nothing)
void put(lv_obj_t *l, fw::ty::Id ty, const char *txt) {
  if (fw::cased(ty, txt) != lv_label_get_text(l)) fw::set_text(l, ty, txt);
}
}  // namespace

FW_COMPONENT(entity_tile, "Entity tile", "components", R"({
  "about": "A Home tile for one thing in the room: its icon, name and state; a tap toggles it or opens its screen.",
  "frame": [240, 240],
  "icons": ["bulb@icon", "power@icon", "screen-covers@icon", "screen-timer@icon", "screen-media@icon", "screen-weather@icon", "thermometer@icon", "door@icon", "window@icon", "person@icon", "alert@icon", "bolt@icon", "house@icon"],
  "props": [
    {"name": "icon", "about": "The icon on the tile, picked for the kind of thing it is, such as a bulb for a light or a door for a door sensor.", "type": "select", "options": ["bulb", "power", "screen-covers", "screen-timer", "screen-media", "screen-weather", "thermometer", "door", "window", "person", "alert", "bolt", "house"], "default": "bulb"},
    {"name": "label", "about": "The thing's name on the tile.", "type": "text", "default": "Sofa table"},
    {"name": "status", "about": "The short state under the name, such as 64% or Off.", "type": "text", "default": "64%"},
    {"name": "on", "about": "Whether the thing is on. On tiles are lit in the accent colour.", "type": "bool", "default": true},
    {"name": "act", "about": "What a tap does: toggle switches it on or off, open opens its own screen, none does nothing.", "type": "select", "options": ["toggle", "open", "none"], "default": "toggle"},
    {"name": "state", "about": "How the tile looks: default ready to tap, pressed while held, or unavailable when the device cannot be reached.", "type": "select", "options": ["default", "pressed", "unavailable"], "default": "default"},
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
  lv_obj_t *c = lv_obj_create(parent);
  fill(c, p);
  fw::st::on_tap(c, [](lv_event_t *e) {
    if (!fw::st::tap_now(e)) return;
    auto *o = (lv_obj_t *) lv_event_get_current_target(e);
    const char *d = lv_label_get_text(part(o, 2));
    if (lv_obj_has_state(o, ST_OFF)) { fw::st::blocked(o, d); return; }
    if (lv_obj_has_flag(o, LV_OBJ_FLAG_USER_4)) {  // toggle: the other side at once (HA settles it)
      const bool on = !is_on(o);
      paint(o, on, false);
      fw::sound(o, on ? fw::st::ON_SOUND : fw::st::OFF_SOUND);
    } else fw::sound(o, "open");
    fw::emit(o, "tap", d);
  }, nullptr);
}

// Update path: same icon + availability = texts in place and on / off as a fade (silent, like a tap); otherwise the
// contents again (default <-> unavailable fades them in).
FW_UPDATE(entity_tile) {
  const char *state = p["state"] | "default";
  const bool was_off = lv_obj_has_state(root, ST_OFF), off = !strcmp(state, "unavailable");
  if (fw::shape(root, sig(p)) && was_off == off) {
    put(part(root, 2), fw::ty::body, p["label"] | "");
    if (!off) put(part(root, 4), fw::ty::label, p["status"] | "");
    const bool on = !off && on_of(p);
    if (on != is_on(root)) paint(root, on, false);
    lv_obj_set_flag(root, LV_OBJ_FLAG_USER_4, !strcmp(p["act"] | "toggle", "toggle"));
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
