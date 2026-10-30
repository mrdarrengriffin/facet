// Room tile: one room you can pick for a job (the robot vacuum's rooms: the panel lab's Vacuum screen tiles, 208 x
// 120), on the contract (renderer/CONTRACT.md, "contract": 2). w:tile-w x h:room-tile-h (three across a Group), no
// fill at rest (the lab's hairline box: border:line), radius:tile, card-pad in (children count from the outer edge); a
// column, its two texts at the top and the bottom: the room (type:c-tile, fg:text, one line, "...") over its status
// (type:label, caps: "tap to pick" fg:text-dim, "picked" / "cleaning now" fg:accent-text).
//   room     "Kitchen:3" = the name and the room's id (the robot's segment number); the id is what a pick sends
//   picked   the screen's picked rooms (a comma list of ids, spec "$rooms"): this one picked = CHECKED: bg:surface +
//            an accent edge (border:accent, ring-full-2: two hairlines = stroke:edge, inside the box)
//   here     the room it's cleaning now (its name or id): USER_2 = bg:surface + border:line-hi, "cleaning now" (only
//            while disabled: a job runs)
//   state    default | disabled (fw::st::OFF while a job runs, rooms are for the next one: the name fg:disabled-ink,
//            no status; a tap = blocked + error)
// Tap: event "pick" (detail = "Name:id") + sound tap; it shows picked at once (the spec's screen.toggle confirms it).
// Pressed: bg:pressed-tint (Motion press / release: fade-press / fade-release). Picked / here / disabled fade
// (fade-switch, Motion toggle), silent. Update path: the looks follow picked / here / state as state changes (fades),
// texts in place. Bleed: none. Contrast: tokens/colour.json section "room_tile".
#include "fw.h"
#include "fx.h"

namespace {
constexpr lv_state_t ST_HERE = LV_STATE_USER_2;
struct RT { lv_obj_t *name, *status; std::string id, title; bool picked = false; };
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
// the status words follow the states (a fade moves the colours; the words change at once)
void words(lv_obj_t *r, RT *c) {
  const bool here = lv_obj_has_state(r, ST_HERE), off = lv_obj_has_state(r, fw::st::OFF);
  fx::set_text(c->status, here ? "cleaning now" : c->picked ? "picked" : off ? "" : "tap to pick", "type:label");
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
  fx::set_text(c->name, c->title.c_str(), "type:c-tile");
  if (c->picked != lv_obj_has_state(r, LV_STATE_CHECKED)) fw::st::set_state(r, LV_STATE_CHECKED, c->picked, animate);
  if (is_here != lv_obj_has_state(r, ST_HERE)) fw::st::set_state(r, ST_HERE, is_here, animate);
  if (off != lv_obj_has_state(r, fw::st::OFF)) fw::st::set_off(r, off, animate);
  words(r, c);
}
}  // namespace

FW_COMPONENT(room_tile, "Room tile", "components", R"({
  "about": "A room you can pick for a job, like the robot vacuum's next clean: its name and whether it's picked or being cleaned now.",
  "contract": 2,
  "parts": [],
  "min": ["layout:tile-w", "layout:room-tile-h"],
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
  auto *c = new RT;
  lv_obj_t *r = lv_obj_create(parent);
  lv_obj_remove_style_all(r);
  lv_obj_remove_flag(r, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(r, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(r, LV_OBJ_FLAG_PRESS_LOCK);  // slid off before letting go = no pick (press lost, no click)
  fx::name(r, "room_tile");
  lv_obj_set_user_data(r, c);
  lv_obj_add_event_cb(r, [](lv_event_t *e) { delete (RT *) lv_event_get_user_data(e); }, LV_EVENT_DELETE, c);
  for (const char *t : {"w:tile-w", "h:room-tile-h", "max-h:room-tile-h", "radius:tile", "card-pad", "border:line", "fade-release"}) fx::use(r, t);
  fx::use(r, "col");
  for (lv_state_t st : {LV_STATE_CHECKED, ST_HERE}) fx::use(r, "bg:surface", st);
  fx::use(r, "border:line-hi", ST_HERE);
  fx::use(r, "border:accent", LV_STATE_CHECKED);
  fx::use(r, "ring-full-2", LV_STATE_CHECKED);
  fx::use(r, "bg:pressed-tint", LV_STATE_PRESSED);
  fx::use(r, "fade-press", LV_STATE_PRESSED);
  for (lv_state_t st : {(lv_state_t) 0, LV_STATE_CHECKED, ST_HERE, fw::st::OFF}) fx::use(r, "fade-switch", st);
  c->name = fx::text(r, "", "type:c-tile", "fg:text");
  fx::use(c->name, "fg:disabled-ink", fw::st::OFF);
  fx::use(fx::box(r, "free"), "grow");  // the name at the top, the status at the bottom
  c->status = fx::text(r, "", "type:label", "fg:text-dim");
  for (lv_state_t st : {LV_STATE_CHECKED, ST_HERE}) fx::use(c->status, "fg:accent-text", st);
  for (lv_obj_t *l : {c->name, c->status}) {
    fx::use(l, "fill-w");
    lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_DOTS);
    for (lv_state_t st : {(lv_state_t) 0, LV_STATE_CHECKED, ST_HERE, fw::st::OFF}) fx::use(l, "fade-switch", st);
  }
  fw::st::on_tap(r, [](lv_event_t *e) {
    if (!fw::st::tap_now(e)) return;
    auto *r = (lv_obj_t *) lv_event_get_current_target(e);
    RT *c = of(r);
    if (lv_obj_has_state(r, fw::st::OFF)) { fw::st::blocked(r, c->title.c_str()); return; }
    c->picked = !c->picked;
    const std::string item = c->title + ":" + c->id;  // "Kitchen:3": the name rides along (the Clean button says it)
    fw::emit(r, "pick", item.c_str());
    fw::sound(r, "tap");
    fw::st::set_state(r, LV_STATE_CHECKED, c->picked, true);
    words(r, c);
  }, c);
  take(r, c, p, false);
}

FW_UPDATE(room_tile) {
  RT *c = of(root);
  if (!c) return false;
  take(root, c, p, true);
  return true;
}
