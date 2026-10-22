// Picker row: the Facet picker row (docs: Components > Pickers > Picker row, @facet/ui pickers.ts pickerRow(), CSS
// .f-prow): one option of a "choose one" list (a speaker, a theme, a sound pack), in a Bottom sheet or a settings page.
// Built from the approved parts, never a copy of their drawing (Darren 2026-10-21 "Examples use the real components"):
//   row      the List row component itself (fw::find("list_row")): hairline on top, height lay::list_row_h (96 / 64 =
//            layout.json overlay sheet-row-h, static_assert), content width, side padding, icon, name = its title,
//            pressed = its press-tint fill, disabled / unavailable = its looks + "blocked" + error, tap = its "tap"
//            (detail = name) + sound tap (a pick is an in-place choice; docs: the default tap sound)
//   icon     the List row's icon slot, ic::icon (24 / 16). Docs draw it at layout sheet-icon (32 / 20), but the icon
//            manifest ships the picker-row icons (speaker, speakers, tv, stream, auto) at icon size only ("picker rows
//            24"), and icons are never scaled: icons.json wins (open question for Darren)
//   status   under the name, screen type label (caps, mono; docs .fst-label), one line, ellipsis. The text slot grows
//            so it centres on the 4 px grid (List row rule): 720 40 + 24 = 64 in 96, 480 28 + 16 = 44 -> 48 in 64
//   tone     dim = icon + status text-dim (docs text-mute, which is disabled-only here); working (something plays) =
//            icon + status accent-text (docs --tone-text)
//   radio    the Radio component (fw::find("radio")) as the trailing part, made non-clickable: the whole row is the
//            target. Selected = radio on (docs: "Selected = radio only", no row highlight)
// Choice: picker rows that share a parent are one group. A real tap on an enabled row turns its radio on and the
// radios of its sibling picker rows off (rows are marked with LV_OBJ_FLAG_USER_1), so a list needs no glue code; the
// Bottom sheet only adds "close after a pick". Contrast pairs: tokens/colour.json section "picker_row" (+ list_row, radio).
#include "fw.h"

static_assert(fw::lay::sheet_row_h == fw::lay::list_row_h, "a picker row is a List row: sheet-row-h must equal list-row-h");

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
// h grown so (row - h) is a multiple of 2 x grid: centred in the row, its top lands on the 4 px grid (List row rule)
int on_grid(int h) { return h + ((fw::lay::list_row_h - h) % (2 * fw::sp::s1) + 2 * fw::sp::s1) % (2 * fw::sp::s1); }

// the row's radio (its last child when it has one)
lv_obj_t *radio_of(lv_obj_t *row) {
  if (!lv_obj_has_flag(row, PICKER)) return nullptr;
  return (lv_obj_t *) lv_obj_get_user_data(row);
}

struct Spec {
  const char *icon, *name, *status, *tone, *state;
  bool selected;
};

lv_obj_t *picker_row(lv_obj_t *parent, const Spec &s) {
  const fw::Theme &t = *fw::T;
  const bool unavail = !strcmp(s.state, "unavailable"), off = unavail || !strcmp(s.state, "disabled");
  const bool working = !strcmp(s.tone, "working") && !off;
  ArduinoJson::JsonDocument d;
  d["title"] = s.name;
  d["icon"] = s.icon;
  d["trailing"] = "none";
  d["state"] = s.state;
  lv_obj_t *r = build("list_row", parent, d);
  if (!r) return nullptr;

  // the List row's parts: [icon] text column (the one that grows) [Unavailable]
  lv_obj_t *icon = nullptr, *col = nullptr;
  for (uint32_t i = 0; i < lv_obj_get_child_count(r); i++) {
    lv_obj_t *c = lv_obj_get_child(r, i);
    if (lv_obj_get_style_flex_grow(c, LV_PART_MAIN) == 1) { col = c; break; }
    icon = c;
  }
  if (icon && working) fw::icon_colour(icon, t.accent_text);
  if (col && !unavail && s.status[0]) {
    const lv_color_t c = off ? fw::st::disabled_ink(t) : working ? t.accent_text : t.text_dim;
    lv_obj_t *l = fw::text(col, fw::ty::label, s.status, c);
    lv_obj_remove_flag(l, LV_OBJ_FLAG_CLICKABLE);
    lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_width(l, LV_PCT(100));
    lv_obj_set_height(l, fw::style(fw::ty::label).box);  // set_long_mode resets the text-box height
    lv_obj_set_height(col, on_grid(fw::style(fw::ty::title).box + fw::style(fw::ty::label).box));
  }

  lv_obj_t *radio = nullptr;
  if (!unavail) {
    ArduinoJson::JsonDocument rp;
    rp["on"] = s.selected;
    rp["state"] = off ? "disabled" : "default";
    radio = build("radio", r, rp);
    if (radio) lv_obj_remove_flag(radio, LV_OBJ_FLAG_CLICKABLE);  // the row is the target
  }
  lv_obj_add_flag(r, PICKER);
  lv_obj_set_user_data(r, radio);

  // a real tap on an enabled row: this radio on, the sibling picker rows' radios off (after the List row's own
  // tap / blocked handling, which runs first: it was added first)
  lv_obj_add_event_cb(r, [](lv_event_t *e) {
    auto *o = (lv_obj_t *) lv_event_get_current_target(e);
    if (lv_obj_has_state(o, ST_OFF)) return;
    lv_obj_t *p = lv_obj_get_parent(o);
    for (uint32_t i = 0; i < lv_obj_get_child_count(p); i++) {
      lv_obj_t *rd = radio_of(lv_obj_get_child(p, i));
      if (rd && lv_obj_get_child(p, i) != o) lv_obj_remove_state(rd, ST_ON);
    }
    if (lv_obj_t *rd = radio_of(o)) lv_obj_add_state(rd, ST_ON);
  }, LV_EVENT_CLICKED, nullptr);
  return r;
}

lv_obj_t *column(lv_obj_t *parent) {
  lv_obj_t *l = lv_obj_create(parent);
  lv_obj_remove_style_all(l);
  lv_obj_remove_flag(l, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_size(l, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(l, LV_FLEX_FLOW_COLUMN);
  return l;
}
}  // namespace

FW_COMPONENT(picker_row, "Picker row", "components", R"({
  "about": "One option of a choose-one list: an icon, the name over a caps status, and a radio; tapping it moves the choice.",
  "frame": [448, 288],
  "icons": ["speaker@icon", "speakers@icon", "tv@icon", "stream@icon", "auto@icon"],
  "props": [
    {"name": "icon", "type": "select", "options": ["none", "speaker", "speakers", "tv", "stream", "auto"], "default": "speaker"},
    {"name": "name", "type": "text", "default": "Kitchen speaker"},
    {"name": "status", "type": "text", "default": "playing · Northbound · Marlow & Finch"},
    {"name": "tone", "type": "select", "options": ["dim", "working"], "default": "dim"},
    {"name": "selected", "type": "bool", "default": false},
    {"name": "state", "type": "select", "options": ["default", "pressed", "disabled", "unavailable"], "default": "default"},
    {"name": "list", "type": "select", "options": ["single", "group"], "default": "single"}
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
    {"label": "group · tap to choose", "props": {"list": "group"}}
  ]
})") {
  if (!strcmp(p["list"] | "single", "group")) {  // a short choose-one list: rows touch, the radio follows the tap
    lv_obj_t *l = column(parent);
    const Spec rows[] = {
        {"auto", "Auto", "follow what's playing", "dim", "default", true},
        {"speaker", "Kitchen speaker", "playing · Northbound", "working", "default", false},
        {"tv", "Living room TV", "idle", "dim", "default", false},
        {"speakers", "Downstairs", "group · 3 speakers", "dim", "default", false},
    };
    for (const Spec &s : rows) picker_row(l, s);
    return;
  }
  lv_obj_t *r = picker_row(parent, {p["icon"] | "speaker", p["name"] | "Kitchen speaker", p["status"] | "", p["tone"] | "dim",
                                    p["state"] | "default", p["selected"] | false});
  (void) r;
}
