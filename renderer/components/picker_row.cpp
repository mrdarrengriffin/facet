// Picker row: the Facet picker row (docs: Components > Pickers > Picker row, @facet/ui pickers.ts pickerRow(), CSS
// .f-prow), on the contract (renderer/CONTRACT.md, "contract": 2): one option of a "choose one" list (a speaker, a
// theme, a sound pack), in a Bottom sheet or a settings page. Made of the real components (C8b), nothing drawn here:
//   row      the List row (fw::find("list_row")): hairline, list-row-h tall (layout.json sheet-row-h is the same),
//            icon, name = its title, status = its under line (caps label, text-dim), tone = its tone (working = icon +
//            status accent-text), pressed / disabled / unavailable = its looks, tap = its "tap" (detail = name) + sound
//            tap. Icons at icon size (docs draw them at sheet-icon 32 / 20, but the icon manifest ships the picker-row
//            icons at icon size only and icons are never scaled)
//   radio    the Radio (fw::find("radio")) in the List row's end slot, made non-clickable: the whole row is the
//            target. Selected = radio on (docs: "Selected = radio only", no row highlight)
// Choice: picker rows that share a parent are one group. A real tap on an enabled row turns its radio on and the
// radios of its sibling picker rows off (rows are marked with LV_OBJ_FLAG_USER_1; the List row root's user data is its
// caller's = the radio), so a list needs no glue code; the Bottom sheet only adds "close after a pick". Themed through
// the List row's list-row part and the live palette. Contrast pairs: tokens/colour.json section "picker_row" (+
// list_row, radio).
#include "fw.h"
#include "fx.h"
#include "items.h"

namespace {
constexpr lv_state_t ST_ON = fw::st::ON, ST_OFF = fw::st::OFF;
constexpr lv_obj_flag_t PICKER = LV_OBJ_FLAG_USER_1;  // marks a picker row among its siblings

lv_obj_t *build(const char *id, lv_obj_t *parent, ArduinoJson::JsonDocument &props) {
  const fw::Component *c = fw::find(id);
  if (!c) return nullptr;
  const uint32_t before = lv_obj_get_child_count(parent);
  c->build(parent, props.as<fw::Props>());
  return lv_obj_get_child_count(parent) > before ? lv_obj_get_child(parent, -1) : nullptr;
}
// the row's radio (its last child when it has one)
lv_obj_t *radio_of(lv_obj_t *row) {
  if (!lv_obj_has_flag(row, PICKER)) return nullptr;
  return (lv_obj_t *) lv_obj_get_user_data(row);
}

struct Spec {
  const char *icon, *name, *status, *tone, *state;
  bool selected;
};

void pick_cb(lv_event_t *e) {
  // a real tap on an enabled row: this radio on, the sibling picker rows' radios off (after the List row's own
  // tap / blocked handling, which runs first: it was added first)
  auto *o = (lv_obj_t *) lv_event_get_current_target(e);
  if (lv_obj_has_state(o, ST_OFF)) return;
  lv_obj_t *p = lv_obj_get_parent(o);
  for (uint32_t i = 0; i < lv_obj_get_child_count(p); i++) {
    lv_obj_t *rd = radio_of(lv_obj_get_child(p, i));
    if (rd && lv_obj_get_child(p, i) != o) lv_obj_remove_state(rd, ST_ON);
  }
  if (lv_obj_t *rd = radio_of(o)) lv_obj_add_state(rd, ST_ON);
}
void list_props(const Spec &s, ArduinoJson::JsonDocument &d) {  // the List row underneath
  d["title"] = s.name;
  d["icon"] = s.icon;
  d["under"] = !strcmp(s.state, "unavailable") ? "" : s.status;
  d["tone"] = !strcmp(s.tone, "working") && strcmp(s.state, "disabled") && strcmp(s.state, "unavailable") ? "working" : "dim";
  d["trailing"] = "none";
  d["state"] = s.state;
}
void radio_props(const Spec &s, ArduinoJson::JsonDocument &rp) {
  const bool off = !strcmp(s.state, "unavailable") || !strcmp(s.state, "disabled");
  rp["on"] = s.selected;
  rp["state"] = off ? "disabled" : "default";
}

// the picker's parts on a List row r: the radio in its end slot, the PICKER mark + tap handler
void decorate(lv_obj_t *r, const Spec &s) {
  lv_obj_t *radio = nullptr;
  if (strcmp(s.state, "unavailable")) {
    ArduinoJson::JsonDocument rp;
    radio_props(s, rp);
    lv_obj_t *en = lv_obj_get_child(r, -1);  // the List row's end slot (its last part)
    radio = build("radio", en, rp);
    if (radio) lv_obj_remove_flag(radio, LV_OBJ_FLAG_CLICKABLE);  // the row is the target
  }
  lv_obj_add_flag(r, PICKER);
  lv_obj_set_user_data(r, radio);
  lv_obj_remove_event_cb(r, pick_cb);  // one handler (an update may decorate the same root again)
  lv_obj_add_event_cb(r, pick_cb, LV_EVENT_CLICKED, nullptr);
}

lv_obj_t *picker_row(lv_obj_t *parent, const Spec &s) {
  ArduinoJson::JsonDocument d;
  list_props(s, d);
  lv_obj_t *r = build("list_row", parent, d);
  if (!r) return nullptr;
  decorate(r, s);
  return r;
}

Spec spec_of(fw::Props p) {
  return {p["icon"] | "speaker", p["name"] | "Kitchen speaker", p["status"] | "", p["tone"] | "dim", p["state"] | "default",
          p["selected"] | false};
}
// Group (list: group, 2026-10-26 real rows): one picker row per item of items (core/items.h, the Grid's model): a
// row = this node's props with "$item" / "$item.key" filled; a prop that does not name $item comes from an object
// item's field of that name (name also from its id), a plain-text item is the name. rows caps it (a last "+N" row);
// no items = the List row's empty words. Nothing demo: the sample rows live in the variants.
constexpr int GROUP_MAX = 24;
const char *const ROW_FIELDS[] = {"name", "icon", "status", "tone", "selected", "state"};
const char *const ROW_SKIP[] = {"list", "items", "rows", "empty", "_items"};
void group(lv_obj_t *l, fw::Props p) {
  const std::vector<std::string> all = fw::items::parse(p["items"] | "");
  const int rows = p["rows"] | 0, cap = rows > 0 ? std::min(rows, GROUP_MAX) : GROUP_MAX;
  const int n = (int) all.size() > cap ? cap - 1 : (int) all.size();
  for (int i = 0; i < n; i++) {
    ArduinoJson::JsonDocument d;
    fw::items::row_props(p, all[i], ROW_FIELDS, 6, ROW_SKIP, 5, d);
    if (d["selected"].is<const char *>()) d["selected"] = !strcmp(d["selected"] | "", "on") || !strcmp(d["selected"] | "", "true");
    picker_row(l, spec_of(d.as<fw::Props>()));
  }
  if ((int) all.size() > n) {
    const std::string more = "+" + std::to_string(all.size() - n);
    ArduinoJson::JsonDocument d;  // the rest, as a plain List row: not a choice (no radio, no pick)
    list_props({"none", more.c_str(), "", "dim", "default", false}, d);
    build("list_row", l, d);
  }
}
// the List row column a group sits in: items empty = its empty words (no rows of its own)
void group_list(fw::Props p, ArduinoJson::JsonDocument &lp) {
  lp["list"] = "stack";
  lp["items"] = (p["items"] | "")[0] ? "x" : "";  // a non-empty list: the List row's one placeholder row goes, rows come in
  if (!p["empty"].isNull()) lp["empty"] = p["empty"];
}
// the props that need the parts again: all but a pressed state and selected (those move in place)
std::string sig(fw::Props p) {
  ArduinoJson::JsonDocument r;
  r.set(p);
  if (!strcmp(p["state"] | "default", "pressed")) r["state"] = "default";
  r.remove("selected");
  std::string s;
  serializeJson(r, s);
  return s;
}

}  // namespace

FW_COMPONENT(picker_row, "Picker row", "components", R"({
  "about": "One option of a choose-one list: an icon, the name over a caps status, and a radio; tapping it moves the choice.",
  "contract": 2,
  "parts": [],
  "min": ["layout:card-min-w", "layout:list-row-h"],
  "frame": [448, 288],
  "icons": ["speaker@icon", "speakers@icon", "tv@icon", "stream@icon", "auto@icon", "roller@icon", "venetian@icon", "curtain@icon"],
  "props": [
    {"name": "icon", "about": "The icon at the start of the row, such as a speaker or a TV, or none.", "type": "select", "options": ["none", "speaker", "speakers", "tv", "stream", "auto", "roller", "venetian", "curtain"], "default": "speaker"},
    {"name": "name", "about": "The option's name, such as the speaker's name.", "type": "text", "default": "Kitchen speaker"},
    {"name": "status", "about": "The small capitals line under the name, such as what is playing there.", "type": "text", "default": "playing · Northbound · Marlow & Finch"},
    {"name": "tone", "about": "The status colour: dim for normal, working for something going on now, such as playing.", "type": "select", "options": ["dim", "working"], "default": "dim"},
    {"name": "selected", "about": "Whether this is the chosen option; its radio is on.", "type": "bool", "default": false},
    {"name": "state", "debug": true, "about": "How the row looks: default ready to tap, pressed while held, disabled when it cannot be chosen, or unavailable when its device cannot be reached.", "type": "select", "options": ["default", "pressed", "disabled", "unavailable"], "default": "default"},
    {"name": "list", "about": "single is one row on its own; group is a choose-one list, one row per item of items, where a tap moves the choice.", "type": "select", "options": ["single", "group"], "default": "single"},
    {"name": "items", "about": "With list group: the options, a comma list or a JSON list, usually bound to an entity attribute (such as a media player's source list). Each row is this node's props with $item (or $item.field) filled in; a plain item is the row's name, and an object item's name, icon, status, tone, selected and state fields fill the props that do not name $item. No items shows the empty words, never sample rows.", "type": "text", "default": ""},
    {"name": "rows", "about": "With list group: at most this many rows; 0 for up to 24. A longer list ends with a +N row for the rest.", "type": "range", "min": 0, "max": 24, "step": 1, "default": 0},
    {"name": "empty", "about": "With list group: the words shown when items is empty.", "type": "text", "default": "Nothing here"}
  ],
  "variants": [
    {"label": "idle", "props": {"name": "Study", "status": "idle"}},
    {"label": "selected", "props": {"name": "Study", "status": "idle", "selected": true}},
    {"label": "playing", "props": {"tone": "working"}},
    {"label": "playing · selected", "props": {"tone": "working", "selected": true}},
    {"label": "no icon", "props": {"icon": "none", "name": "Dark", "status": "always", "selected": true}},
    {"label": "long text", "props": {"icon": "speakers", "name": "Downstairs speakers and the kitchen soundbar", "status": "group · 4 speakers · living room, kitchen, hall, study"}},
    {"label": "pressed", "props": {"name": "Study", "status": "idle", "state": "pressed"}},
    {"label": "disabled", "props": {"icon": "tv", "name": "Living room TV", "status": "off", "state": "disabled"}},
    {"label": "unavailable", "props": {"icon": "tv", "name": "Office TV", "state": "unavailable"}},
    {"label": "group · tap to choose", "props": {"list": "group", "items": "[{\"icon\":\"auto\",\"name\":\"Auto\",\"status\":\"follow what's playing\",\"selected\":true},{\"icon\":\"speaker\",\"name\":\"Kitchen speaker\",\"status\":\"playing · Northbound\",\"tone\":\"working\"},{\"icon\":\"tv\",\"name\":\"Living room TV\",\"status\":\"idle\"},{\"icon\":\"speakers\",\"name\":\"Downstairs\",\"status\":\"group · 3 speakers\"}]"}},
    {"label": "group · plain items", "props": {"list": "group", "icon": "none", "status": "", "items": "Light,Dark,Auto"}},
    {"label": "group · empty", "props": {"list": "group", "empty": "No speakers found"}}
  ]
})") {
  if (!strcmp(p["list"] | "single", "group")) {  // a short choose-one list: rows touch, the radio follows the tap
    ArduinoJson::JsonDocument lp;
    group_list(p, lp);  // the List row's column root (so a later update can turn it back into one row)
    lv_obj_t *l = build("list_row", parent, lp);
    if (!l) return;
    if ((p["items"] | "")[0]) {
      lv_obj_clean(l);  // the List row's placeholder row out, the picker rows in
      group(l, p);
    }
    fw::shape(l, sig(p), "picker");
    return;
  }
  lv_obj_t *r = picker_row(parent, spec_of(p));
  if (r) fw::shape(r, sig(p), "picker");
}

// Update path: selected = the Radio's own update (its ring + dot transitions), default <-> pressed = the List row's
// press fade; name, icon, status, tone, disabled / unavailable or the group demo = the List row's contents again
// (its update, forced) and the picker's parts on it again (snap; disabled / unavailable in or out fades in). Silent.
FW_UPDATE(picker_row) {
  const bool grp = !strcmp(p["list"] | "single", "group");
  const Spec s = spec_of(p);
  ArduinoJson::JsonDocument lp;
  if (grp) group_list(p, lp);
  else list_props(s, lp);
  if (fw::shape(root, sig(p), "picker")) {
    if (grp) return true;  // the same items (sig): nothing moves
    fw::update_child(fw::find("list_row"), root, lp.as<fw::Props>());  // only the press can differ: in place
    if (lv_obj_t *rd = radio_of(root)) {
      ArduinoJson::JsonDocument rp;
      radio_props(s, rp);
      fw::update_child(fw::find("radio"), rd, rp.as<fw::Props>());
    }
    return true;
  }
  const bool was_off = lv_obj_has_state(root, ST_OFF);
  fw::shape(root, "");  // the List row's stored props: forget them, so its update builds its contents again
  bool ok = true;
  fw::st::refill_fade(root, [&] {  // a new tone fades (like a tap; shared styles too)
  ok = fw::update_child(fw::find("list_row"), root, lp.as<fw::Props>());
  if (!ok) return;
  if (grp) {
    lv_obj_remove_flag(root, PICKER);
    lv_obj_set_user_data(root, nullptr);
    lv_obj_remove_event_cb(root, pick_cb);
    if ((p["items"] | "")[0]) {
      lv_obj_clean(root);
      group(root, p);
    }
  } else {
    decorate(root, s);
  }
  }, true, true);
  if (!ok) return false;
  if (was_off != lv_obj_has_state(root, ST_OFF)) fw::st::fade_in_children(root);
  return true;
}
