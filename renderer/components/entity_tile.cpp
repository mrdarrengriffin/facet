// Entity tile: a Home card at size s (screen model §4: the tile, three across), on the contract (renderer/CONTRACT.md,
// "contract": 2). One thing in the room at a glance: its icon, its name and its state, and a tap that does the one
// thing (toggle a light, open its screen). The Card's look (the same tokens and the theme's card part) and the Card's
// head: the name top-left, the icon in the top-right corner, the state under them.
//   box      fills its parent (a Home grid cell; meta min layout:tile-w x tile-h, 208 / 136 square), bg:surface,
//            border:line, radius:tile, card-pad (children count from the outer edge); a column; part:card
//   head     a row on the padding line (card-row, gap 2): the label in type label (caps, text-dim, the row's free
//            width, "...") + the icon at the row's end, the tile's top-right corner (icon size 24 / 16: text-dim,
//            on-mark (accent) while on)
//   rest     tile shape, a name the head's row does not hold: its first words stay in the head, the rest goes on one
//            line of its own under it (the tile's full content width, under the icon too; same type, "...")
// Steps (the name, chosen from the content box after layout, both ways, C9): 1. type label, in one line or two;
// 2. type c-label (the same size at 720, tighter caps; 12 at 480) when two lines of label do not hold it; past that
// both lines end in "...". A half keeps one line of label.
//   (grow)
//   status   type title (the Card's value at its pictured step), one line, "...", on the bottom padding line: the
//            state ("Off", "64%", "Open"); text, on-ink (accent-text) while on
//   half     shape half (half a Home cell): the same head, the status under it in type c-tile (a step under body),
//            no padding above or below and the two spread evenly down the half (half-even: the same room above,
//            between and below)
// Props: icon (a UI or screen icon name), label, status, on (bool), act toggle | open | none (what a tap does: the
// sound and the instant look; none = read-only, a Home sensor tile: not clickable, no press look), state default |
// pressed | unavailable, press (the Card's press styles).
// States (tokens per state, C17): on = LV_STATE_USER_2 (not CHECKED: the theme's card part has no "on" look of its
// own), fading with Motion toggle (fade-switch); pressed = the Card press (fade-press / fade-release); unavailable
// (fw::st::OFF): icon + label unavailable-ink, status "Unavailable" in unavailable-note, no press look; a tap = blocked
// + error; missing (Home Assistant has no such entity): the same look, status "Not found" (fw::st::gone / gone_word).
// n/a: loading, unknown, empty, error (the binding maps those to unavailable).
// Tap (states.json): act toggle = the tile shows the other side at once (like a tap on a Toggle) + sound toggle-on /
// toggle-off + event "tap" (the Home's spec sends the toggle; HA's answer settles it); act open = sound open + event
// "tap" (the Home opens the screen). Update path: label / status texts in place, on / off fades (silent); icon or
// unavailable <-> default = the contents again, faded in. Bleed: none. Contrast pairs: tokens/colour.json "entity_tile".
#include "fw.h"
#include "fx.h"

namespace {
constexpr lv_state_t ST_OFF = fw::st::OFF, ST_ON = LV_STATE_USER_2;
constexpr lv_obj_flag_t TOGGLES = LV_OBJ_FLAG_USER_4;  // a tap toggles (else opens)
// name: the label as given (the head draws it in caps); rest: the name's second line, under the icon (tile shape);
// step 0 = the name in type label, 1 = type c-label (the two lines do not hold it in label)
struct Tile { lv_obj_t *label = nullptr, *rest = nullptr, *status = nullptr, *head = nullptr; std::string name; int step = 0, w = -1; bool queued = false; };
void split_cb(void *v);
void tile_free(lv_event_t *e) {
  lv_async_call_cancel(split_cb, lv_event_get_current_target(e));
  delete (Tile *) lv_event_get_user_data(e);
}
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
  return std::string(p["icon"] | "bulb") + "|" + (fw::st::gone(p["state"] | "default") ? p["state"] | "" : "") + "|" + (p["press"] | "") + "|" + (p["shape"] | "tile");
}
const char *NAME_TYPE[] = {"type:label", "type:c-label"};
// the name's texts at k->step: the first line in the head before the icon (top-right), the rest on a line of its own
// under it, the tile's full width (under the icon's column too)
void name_texts(lv_obj_t *c, Tile *k) {
  const bool off = lv_obj_has_state(c, ST_OFF);
  if (k->label) lv_obj_delete(k->label);
  if (k->rest) lv_obj_delete(k->rest);
  const char *ty = NAME_TYPE[k->step];
  k->label = fx::text(k->head, k->name.c_str(), ty, off ? "fg:unavailable-ink" : "fg:text-dim", "dot");
  fx::use(k->label, "cell");  // the row's free width, then "..."
  lv_obj_move_to_index(k->label, 0);  // the name first, the icon in the corner
  k->rest = fx::text(c, "", ty, off ? "fg:unavailable-ink" : "fg:text-dim", "dot");
  fx::use(k->rest, "fill-w");
  fx::lines(k->rest, ty, 1);
  lv_obj_move_to_index(k->rest, 1);  // under the head
  lv_obj_add_flag(k->rest, LV_OBJ_FLAG_HIDDEN);
  if (off) { lv_obj_add_state(k->label, ST_OFF); lv_obj_add_state(k->rest, ST_OFF); }
}
// the name in the head's free width, else its first words there and the rest on the line under the icon (the tile's
// content width): true = it fits; else both lines end in "..."
bool split(lv_obj_t *c, Tile *k) {
  fx::set_text(k->label, k->name.c_str(), NAME_TYPE[k->step]);
  const std::string all = lv_label_get_text(k->label);  // the type's case
  auto width = [&](const std::string &t) { return fx::text_width(k->label, t.c_str()); };
  const int w1 = lv_obj_get_content_width(k->label), w2 = lv_obj_get_content_width(c);
  std::string one = all, two;
  bool fits = width(all) <= w1;
  if (!fits) {
    size_t cut = std::string::npos;  // the last space that leaves the first words inside the head
    for (size_t i = all.find(' '); i != std::string::npos; i = all.find(' ', i + 1)) {
      if (width(all.substr(0, i)) > w1) break;
      cut = i;
    }
    if (cut != std::string::npos) { one = all.substr(0, cut); two = all.substr(cut + 1); fits = width(two) <= w2; }
  }
  lv_label_set_text(k->label, one.c_str());
  lv_label_set_text(k->rest, two.c_str());
  lv_obj_set_flag(k->rest, LV_OBJ_FLAG_HIDDEN, two.empty());
  return fits;
}
// after a layout (a new width or new words): the name at the first step that holds it in two lines (C9 both ways)
void split_cb(void *v) {
  lv_obj_t *c = (lv_obj_t *) v;
  Tile *k = tile_of(c);
  if (!k || !k->head) return;
  k->queued = false;
  lv_obj_update_layout(c);
  if (k->step) { k->step = 0; name_texts(c, k); lv_obj_update_layout(c); }
  if (!split(c, k)) { k->step = 1; name_texts(c, k); lv_obj_update_layout(c); split(c, k); }
}
void queue_split(lv_obj_t *c, Tile *k) {
  if (!k->head || k->queued) return;
  k->queued = true;
  lv_async_call(split_cb, c);
}
void fill(lv_obj_t *c, fw::Props p) {
  Tile *k = tile_of(c);
  const char *state = p["state"] | "default";
  const bool off = fw::st::gone(state);
  fw::shape(c, sig(p));
  const bool half = !strcmp(p["shape"] | "tile", "half");  // half a Home cell: the head and state spread evenly
  fx::use(c, "col");
  for (const char *t : {"fill", "radius:tile", "card-pad", "bg:surface", "border:line", "fade-release"}) fx::use(c, t);
  const char *press = p["press"] | fw::st::PRESS_STYLE;
  const bool p_fill = strcmp(press, "border") != 0, p_border = strcmp(press, "fill") != 0;
  fx::use(c, p_fill ? "bg:pressed-tint" : "bg:surface", LV_STATE_PRESSED);
  fx::use(c, p_border ? "border:pressed-tint-line" : "border:pressed-line", LV_STATE_PRESSED);
  fx::use(c, "fade-press", LV_STATE_PRESSED);
  fx::use(c, "bg:surface", ST_OFF);  // no press look when unavailable
  fx::use(c, "border:line", ST_OFF);
  fx::use(c, "part:card");  // the theme's card (C46), after the tile's own state tokens

  // the Card's head (icon + caps name in type label, one row on the padding line), the state under it: a half spreads
  // the two evenly (state in type c-tile), a tile puts the state on the bottom padding line (type title)
  if (half) fx::use(c, "half-even");  // after card-pad: the head and the state spread evenly down the half
  lv_obj_t *head = fx::box(c, "row");
  for (const char *t : {"fill-w", "gap:2", "card-row"}) fx::use(head, t);
  lv_obj_t *ic = fx::icon(head, p["icon"] | "bulb", "icon", off ? "fg:unavailable-ink" : "fg:text-dim");
  fx::icon_use(ic, "ink:on-mark", ST_ON);
  fx::icon_use(ic, "ink:unavailable-ink", ST_OFF);
  for (lv_state_t st : {(lv_state_t) 0, ST_ON}) fx::icon_use(ic, "fade-switch", st);
  k->name = p["label"] | "";
  k->label = k->rest = nullptr;
  k->head = half ? nullptr : head;
  if (half) {  // one line, "..." at the row's width
    k->label = fx::text(head, p["label"] | "", "type:label", off ? "fg:unavailable-ink" : "fg:text-dim", "dot");
    fx::use(k->label, "cell");
    lv_obj_move_to_index(k->label, 0);  // the name first, the icon in the corner
  } else {
    k->step = 0;
    if (off) lv_obj_add_state(c, ST_OFF);  // name_texts reads it
    name_texts(c, k);
    fx::use(fx::box(c, "free"), "grow");
  }
  const char *st_type = half ? "type:c-tile" : "type:title";
  const char *st_txt = off ? fw::st::gone_word(state) : (p["status"] | "");
  k->status = fx::text(c, st_txt, st_type, off ? "fg:unavailable-note" : "fg:text", "none");
  fx::lines(k->status, st_type, 1);  // one line box, "..." at the width
  lv_label_set_long_mode(k->status, LV_LABEL_LONG_MODE_DOTS);
  if (!off) fx::use(k->status, "fg:on-ink", ST_ON);
  fades(k->status);

  const bool pressed = !strcmp(state, "pressed");
  if (pressed != lv_obj_has_state(c, LV_STATE_PRESSED)) lv_obj_set_state(c, LV_STATE_PRESSED, pressed);
  if (off) fx::state(c, ST_OFF, true);
  fw::st::set_state(c, ST_ON, !off && on_of(p), false);
  k->w = -1;
  if (!half) { split(c, k); queue_split(c, k); }  // the name's lines once the tile has its width
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
    {"name": "state", "debug": true, "about": "How the tile looks: default ready to tap, pressed while held, unavailable when the device has no state now, or missing when Home Assistant has no such device.", "type": "select", "options": ["default", "pressed", "unavailable", "missing"], "default": "default"},
    {"name": "shape", "about": "The tile's shape: tile is a square (the icon and name in a small header, the state at the bottom), half is half a Home cell: the same header, the state under it.", "type": "select", "options": ["tile", "half"], "default": "tile"},
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
    {"label": "unavailable", "props": {"label": "Chair", "state": "unavailable"}},
    {"label": "missing", "props": {"label": "Chair", "state": "missing"}},
    {"label": "half", "props": {"shape": "half"}},
    {"label": "half · sensor", "props": {"shape": "half", "icon": "thermometer", "label": "Kitchen", "status": "21.4\u00b0", "on": false, "act": "none"}}
  ]
})") {
  lv_obj_t *c = lv_obj_create(parent);  // styles: fill() (an update refills the same root)
  lv_obj_remove_style_all(c);
  lv_obj_remove_flag(c, LV_OBJ_FLAG_SCROLLABLE);
  fx::name(c, "entity_tile");
  lv_obj_add_event_cb(c, tile_free, LV_EVENT_DELETE, new Tile);
  lv_obj_add_event_cb(c, [](lv_event_t *e) {  // a new width: the name's lines again
    auto *o = (lv_obj_t *) lv_event_get_current_target(e);
    Tile *k = tile_of(o);
    if (k && lv_obj_get_content_width(o) != k->w) { k->w = lv_obj_get_content_width(o); queue_split(o, k); }
  }, LV_EVENT_SIZE_CHANGED, nullptr);
  fill(c, p);
  fw::st::on_tap(c, [](lv_event_t *e) {
    if (!fw::st::tap_now(e)) return;
    auto *o = (lv_obj_t *) lv_event_get_current_target(e);
    const char *d = tile_of(o)->name.c_str();
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
  const bool was_off = lv_obj_has_state(root, ST_OFF), off = fw::st::gone(state);
  if (fw::shape(root, sig(p))) {
    const bool half = !strcmp(p["shape"] | "tile", "half");  // the status: half in type c-tile, tile in type title
    if (k->name != (p["label"] | "")) {  // the head shows it in caps: compare the name as given
      k->name = p["label"] | "";
      if (half) fx::set_text(k->label, k->name.c_str(), "type:label");
      else { split(root, k); queue_split(root, k); }
    }
    if (!off) put(k->status, half ? "type:c-tile" : "type:title", p["status"] | "");
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
