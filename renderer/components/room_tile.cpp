// Room tile: one room you can pick for a job (the robot vacuum's rooms: the panel lab's Vacuum screen tiles, 208 x
// 120). tile-w x room-tile-h (three across a Group), no fill at rest (the lab's hairline box: 1 px line edge),
// rad::tile; picked = surface + the accent edge, the room it's in now = surface + a line-hi edge; inside, lay::tile_pad in:
// the room (type c-tile = the tile title role, text, one line; title type cut "Living room" at 480) over its status (type label, caps: "tap to pick" text-dim, "picked"
// accent-text, "cleaning now" accent-text).
//   room     "Kitchen:3" = the name and the room's id (the robot's segment number); the id is what a pick sends
//   picked   the screen's picked rooms (a comma list of ids, spec "$rooms"): this one picked = an accent edge
//            (stroke::edge, inside the box)
//   here     the room it's cleaning now (its name or id): "cleaning now" (only while disabled: a job runs)
//   state    default | disabled (while a job runs, rooms are for the next one: a tap = blocked + error)
// Tap: event "pick" (detail = "Name:id") + sound tap; it shows picked at once (the spec's screen.toggle confirms it).
// Pressed: press-tint fill (states.json pressed tint, Motion press / release).
// Update path: the look follows picked / here / state (fades, silent); texts in place. Contrast: section "room_tile".
#include "fw.h"

namespace {
// a text only when it changes (an update with the same props redraws nothing)
void put(lv_obj_t *l, fw::ty::Id ty, const char *t) {
  if (fw::cased(ty, t) != lv_label_get_text(l)) fw::set_text(l, ty, t);
}
struct RT { lv_obj_t *name, *status; std::string id, title, key = "?"; bool picked = false; };  // key "?": the first take paints
RT *of(lv_obj_t *o) { return (RT *) lv_obj_get_user_data(o); }
bool in_list(const std::string &l, const std::string &x) {  // items "3" or "Kitchen:3" (the id after the colon)
  for (size_t i = 0; i <= l.size();) {
    size_t j = l.find(',', i);
    if (j == std::string::npos) j = l.size();
    std::string v = l.substr(i, j - i);
    if (v.rfind(':') != std::string::npos) v = v.substr(v.rfind(':') + 1);
    while (!v.empty() && v.front() == ' ') v.erase(0, 1);
    if (!v.empty() && v == x) return true;
    i = j + 1;
  }
  return false;
}
const lv_style_prop_t PRESS_PROPS[] = {LV_STYLE_BG_COLOR, LV_STYLE_PROP_INV};
const lv_style_transition_dsc_t *press_tr(bool pressed) {
  static lv_style_transition_dsc_t d[2];
  static bool ready = false;
  if (!ready) { fw::st::press_transitions(d, PRESS_PROPS); ready = true; }
  return &d[pressed];
}
void look(lv_obj_t *r, RT *c, bool here, bool off) {
  const fw::Theme &t = *fw::T;
  const bool mark = c->picked || here;
  lv_obj_set_style_border_color(r, c->picked ? t.accent : here ? t.line_hi : t.line, 0);
  lv_obj_set_style_border_width(r, c->picked ? fw::stroke::edge : fw::stroke::hair, 0);
  lv_obj_set_style_bg_opa(r, mark ? LV_OPA_COVER : LV_OPA_TRANSP, 0);  // the lab: a plain hairline box at rest
  lv_obj_set_style_text_color(c->name, off ? fw::st::disabled_ink(t) : t.text, 0);
  lv_obj_set_style_text_color(c->status, mark ? t.accent_text : t.text_dim, 0);
  put(c->status, fw::ty::label, here ? "cleaning now" : c->picked ? "picked" : off ? "" : "tap to pick");
  if (c->picked) lv_obj_add_state(r, LV_STATE_CHECKED);
  else lv_obj_remove_state(r, LV_STATE_CHECKED);
}
void take(lv_obj_t *r, RT *c, fw::Props p, bool animate) {
  const std::string room = p["room"] | "Kitchen:3";
  const size_t colon = room.rfind(':');
  c->title = colon == std::string::npos ? room : room.substr(0, colon);
  c->id = colon == std::string::npos ? room : room.substr(colon + 1);
  c->picked = in_list(p["picked"] | "", c->id);
  const std::string here = p["here"] | "";
  const bool off = !strcmp(p["state"] | "default", "disabled");
  const bool is_here = off && !here.empty() && (here == c->id || here == c->title);  // only while a job runs (the lab)
  put(c->name, fw::ty::c_tile, c->title.c_str());
  const std::string key = std::string(c->picked ? "p" : "") + (is_here ? "h" : "") + (off ? "o" : "");
  if (key == c->key) return;  // the same look: nothing to redraw
  c->key = key;
  fw::st::set_off(r, off, animate);
  fw::st::restyle(r, [&] { look(r, c, is_here, off); }, animate);
}
}  // namespace

FW_COMPONENT(room_tile, "Room tile", "components", R"({
  "about": "A room you can pick for a job, like the robot vacuum's next clean: its name and whether it's picked or being cleaned now.",
  "frame": [240, 0],
  "props": [
    {"name": "room", "about": "The room's name and its number in the vacuum, written as name:number, such as Kitchen:3.", "type": "text", "default": "Kitchen:3"},
    {"name": "picked", "about": "The rooms picked for the next clean, as name:number pairs separated by commas. If this room is among them it is shown picked.", "type": "text", "default": ""},
    {"name": "here", "about": "The room being cleaned now; if it is this one, the tile shows it.", "type": "text", "default": ""},
    {"name": "state", "about": "default can be tapped to pick the room; disabled cannot be picked.", "type": "select", "options": ["default", "disabled"], "default": "default"}
  ],
  "variants": [
    {"label": "tap to pick", "props": {}},
    {"label": "picked", "props": {"picked": "3"}},
    {"label": "cleaning now", "props": {"here": "Kitchen", "state": "disabled"}},
    {"label": "disabled", "props": {"state": "disabled"}}
  ]
})") {
  const fw::Theme &t = *fw::T;
  auto *c = new RT;
  lv_obj_t *r = lv_obj_create(parent);
  lv_obj_remove_style_all(r);
  lv_obj_remove_flag(r, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(r, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_STATE_TRICKLE));
  lv_obj_remove_flag(r, LV_OBJ_FLAG_PRESS_LOCK);  // slid off before letting go = no pick (press lost, no click)
  lv_obj_set_user_data(r, c);
  lv_obj_add_event_cb(r, [](lv_event_t *e) { delete (RT *) lv_event_get_user_data(e); }, LV_EVENT_DELETE, c);
  lv_obj_set_size(r, fw::lay::tile_w, fw::lay::room_tile_h);
  lv_obj_set_style_radius(r, fw::rad::tile, 0);
  lv_obj_set_style_bg_color(r, t.surface, 0);
  lv_obj_set_style_bg_opa(r, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(r, fw::st::pressed_tint(t), LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(r, LV_OPA_COVER, LV_STATE_PRESSED);
  lv_obj_set_style_bg_color(r, t.surface, fw::st::OFF);
  lv_obj_set_style_transition(r, press_tr(false), 0);
  lv_obj_set_style_transition(r, press_tr(true), LV_STATE_PRESSED);
  lv_obj_set_style_pad_all(r, fw::lay::tile_pad - fw::stroke::hair, 0);  // children count from the outer edge
  lv_obj_set_flex_flow(r, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(r, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  const int w = fw::lay::tile_w - 2 * fw::lay::tile_pad;
  c->name = fw::text(r, fw::ty::c_tile, "", t.text, w);
  c->status = fw::text(r, fw::ty::label, "", t.text_dim, w);
  for (auto [l, ty] : {std::pair{c->name, fw::ty::c_tile}, std::pair{c->status, fw::ty::label}}) {
    lv_obj_set_size(l, w, fw::style(ty).box);
    lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
  }
  fw::st::on_tap(r, [](lv_event_t *e) {
    if (!fw::st::tap_now(e)) return;
    auto *r = (lv_obj_t *) lv_event_get_current_target(e);
    RT *c = of(r);
    if (lv_obj_has_state(r, fw::st::OFF)) { fw::st::blocked(r, c->title.c_str()); return; }
    c->picked = !c->picked;
    c->key = c->picked ? "p" : "";
    const std::string item = c->title + ":" + c->id;  // "Kitchen:3": the name rides along (the Clean button says it)
    fw::emit(r, "pick", item.c_str());
    fw::sound(r, "tap");
    look(r, c, false, false);
  }, c);
  take(r, c, p, false);
}

FW_UPDATE(room_tile) {
  RT *c = of(root);
  if (!c) return false;
  take(root, c, p, true);
  return true;
}
