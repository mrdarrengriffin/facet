// List row: the Facet hairline row (docs: Settings rows .fs-set, picker rows .f-prow / pickerRow(), sheet rows). Rows
// are not boxed: they sit on the screen background and a 1 px line hairline on top separates them (docs "rows
// separated by hairlines rather than boxes"); a stack of rows = a list with a divider between each.
//   height   lay::list_row_h (docs layout sheet-row-h: 96 at 720, 64 at 480), one line or title + sub line
//   width    the screen content width = 2 x card-w + gutter (656 / 440), i.e. inset to inset
//   padding  space 4 left and right (16 / 8, docs row-pad 16) so the press fill does not run into the text
// Layout (flex row, centred vertically, gap space 4; nothing hand-placed):
//   icon     optional, ic::icon (24 / 16: the Facet icon manifest ships row icons at this size, "picker rows 24";
//            only icon@size pairs that exist are drawn, never scaled), text-dim
//   text     grows: an optional over line (prop over: type label, text-dim, caps; the panel lab's row3 label above the
//            title, e.g. Overhead's "B763 · 21,000 FT · 22 KM S"), the title (type title, 40 / 28 box, text, one line,
//            ellipsis) and an optional sub (type body, text-dim)
//            (40 + 32 = 72 in 96, 28 + 20 = 48 in 64: both lines sit on the 4 px grid)
//   trailing none | value (type body, text-dim, = docs .fs-set-val) | chevron (ic::icon 24 / 16) | value + chevron
//            (= a Settings row that opens a picker) | toggle (the Toggle component itself, built through fw::find)
// 4 px grid at both densities: a part centred in the row lands on the grid only when row - part is a multiple of 8, so
// each text part sits in a slot grown to that (480: title box 28 -> 32, body box 20 -> 24) with the text at the slot
// top; every box top then lands on the grid (480: 16 / 20 / 24, 720: the boxes already fit). With the divider the
// content box is row - hair; LVGL's floor + the hair gives the same tops.
// Playground: trailing "none" with a value typed in shows the value; "on" with trailing none shows a toggle, so every
// control changes a fresh row; a value kind with no value shows an em dash.
// Chevron colour: the docs use text-mute; here text-dim, because text-mute is for disabled only (contrast rule).
// Pressed (Motion press / release): the Card press approach and token: fill = mix press-tint (the greyed accent tint
// Darren picked for Card, over the surface). Tried first: the same tint over the canvas (bg) as its own mix; on RGB565
// it either vanishes on the dark canvas (#000 + 4-5 % = the same 5/6-bit step) or, strong enough to show, drops the
// accent toggle track under 3:1 on the light canvas. press-tint shows in both and keeps every pair at 0 fails.
// Disabled (Button rule, USER_1): icon, title, value and chevron text-mute, a trailing toggle shows its own disabled
// look; no press look; a tap emits "blocked" + plays error. Unavailable (Card rule: the entity is gone): icon + title
// text-mute, the trailing part replaced by "Unavailable" (body, text-dim), no sub; tap = blocked + error too.
// Tap = a real CLICKED: chevron rows emit "tap" (detail = title) + sound "open" (they navigate); a toggle row hands
// the tap to its toggle (whole row = hit area: the toggle slides, emits tap on/off, plays toggle-on / toggle-off);
// other rows emit "tap" + sound "tap".
// Stack (list: stack, 2026-10-26 real rows): one row per item of items (core/items.h, the Grid's model: a comma list or
// a JSON list, usually bound to an entity attribute). A row = this node's props with "$item" / "$item.key" filled; a
// prop that does not name $item comes from an object item's field of the same name (title also from its name / id),
// a plain-text item is the title. rows caps the list (a last "+N" row for the rest); no items = the empty words.
// Nothing demo: the sample rows live in the variants. Update: the same row count = each row's own update path (texts
// in place, a toggle slides); another count = the rows again. Contrast pairs: tokens/colour.json section "list_row" (+ toggle, card).
#include "fw.h"
#include "items.h"

namespace {
constexpr lv_state_t ST_OFF = fw::st::OFF;  // disabled / unavailable (LVGL sends no events to LV_STATE_DISABLED)

enum Kind { K_NONE, K_VALUE, K_CHEVRON, K_VALUE_CHEVRON, K_TOGGLE };
Kind kind_of(const char *s) {
  if (!strcmp(s, "value")) return K_VALUE;
  if (!strcmp(s, "chevron")) return K_CHEVRON;
  if (!strcmp(s, "value-chevron")) return K_VALUE_CHEVRON;
  if (!strcmp(s, "toggle")) return K_TOGGLE;
  return K_NONE;
}

struct Row {
  Kind kind = K_NONE;
  lv_obj_t *toggle = nullptr;
  lv_obj_t *ti = nullptr, *su = nullptr, *val = nullptr, *ov = nullptr;  // title, sub, value, over labels (the update path sets their text)
  std::string title;
};

const lv_style_prop_t TR_PROPS[] = {LV_STYLE_BG_OPA, LV_STYLE_PROP_INV};
const lv_style_transition_dsc_t *tr(bool pressed) {
  static lv_style_transition_dsc_t d[2];
  static bool ready = false;
  if (!ready) { fw::st::press_transitions(d, TR_PROPS); ready = true; }  // states.json pressed motion
  return &d[pressed];
}

lv_obj_t *box(lv_obj_t *parent) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  return o;
}

lv_obj_t *label(lv_obj_t *parent, fw::ty::Id s, const char *txt, lv_color_t c) {
  lv_obj_t *l = fw::text(parent, s, txt, c);
  lv_obj_remove_flag(l, LV_OBJ_FLAG_CLICKABLE);
  return l;
}

// h grown so that (row height - h) is a multiple of 2 x grid: centred, its top is on the grid (see the grid note above)
int on_grid(int h) { return h + ((fw::lay::list_row_h - h) % (2 * fw::sp::s1) + 2 * fw::sp::s1) % (2 * fw::sp::s1); }
// a slot of that height; the label keeps its own box at the slot top
lv_obj_t *slot(lv_obj_t *parent, int h) {
  lv_obj_t *b = box(parent);
  lv_obj_set_size(b, LV_SIZE_CONTENT, on_grid(h));
  return b;
}

struct Spec {
  const char *title, *icon, *sub, *trailing, *value, *state;
  bool on, divider;
  const char *over = "";
};
std::string caps(const char *s) {
  std::string o(s);
  for (char &c : o) c = (char) toupper((unsigned char) c);  // ASCII only; UTF-8 bytes pass through
  return o;
}

void row_free(lv_event_t *e) { delete (Row *) lv_event_get_user_data(e); }
Row *row_of(lv_obj_t *r) {  // the Row rides on the root's DELETE event (the root's own user data is the caller's)
  for (uint32_t i = 0, n = lv_obj_get_event_count(r); i < n; i++) {
    lv_event_dsc_t *d = lv_obj_get_event_dsc(r, i);
    if (lv_event_dsc_get_cb(d) == row_free) return (Row *) lv_event_dsc_get_user_data(d);
  }
  return nullptr;
}
Kind effective(const Spec &s) {
  const bool unavail = !strcmp(s.state, "unavailable");
  Kind kind = unavail ? K_NONE : kind_of(s.trailing);
  if (!unavail && kind == K_NONE && s.on) kind = K_TOGGLE;            // playground: on implies a toggle
  else if (!unavail && kind == K_NONE && s.value[0]) kind = K_VALUE;  // a typed value implies a value
  return kind;
}

// the row's look and contents on r (build: a new object; update: the same root after fw::refill)
void fill(lv_obj_t *r, const Spec &s, Row *d) {
  const fw::Theme &t = *fw::T;
  const bool unavail = !strcmp(s.state, "unavailable"), off = unavail || !strcmp(s.state, "disabled");
  const Kind kind = effective(s);
  const lv_color_t ink = off ? fw::st::disabled_ink(t) : t.text, dim = off ? fw::st::disabled_ink(t) : t.text_dim;

  lv_obj_remove_style_all(r);
  lv_obj_add_flag(r, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(r, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(r, 2 * fw::lay::card_w + fw::lay::gutter, fw::lay::list_row_h);
  lv_obj_set_style_pad_hor(r, fw::sp::s4, 0);
  if (s.divider) {  // hairline on top; children start below it, so the content box is centred in row - hair
    lv_obj_set_style_border_side(r, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_border_width(r, fw::stroke::hair, 0);
    lv_obj_set_style_border_color(r, t.line, 0);
  }
  lv_obj_set_style_bg_color(r, fw::st::pressed_tint(t), 0);  // states.json pressed tint
  lv_obj_set_style_bg_opa(r, LV_OPA_TRANSP, 0);
  lv_obj_set_style_transition(r, tr(false), 0);
  lv_obj_set_style_bg_opa(r, LV_OPA_COVER, LV_STATE_PRESSED);
  lv_obj_set_style_transition(r, tr(true), LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(r, LV_OPA_TRANSP, ST_OFF);  // no press look when disabled / unavailable
  lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(r, fw::sp::s4, 0);

  if (strcmp(s.icon, "none")) fw::icon(r, s.icon, fw::ic::icon, dim);

  lv_obj_t *col = box(r);
  const int col_h = fw::style(fw::ty::title).box + (!unavail && s.sub[0] ? fw::style(fw::ty::body).box : 0) +
                    (s.over[0] ? fw::style(fw::ty::label).box : 0);
  lv_obj_set_size(col, 0, on_grid(col_h));  // width from flex grow; text ellipsises instead of pushing the trailing
  lv_obj_set_flex_grow(col, 1);
  lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
  d->ov = nullptr;
  if (s.over[0]) {
    lv_obj_t *ov = d->ov = label(col, fw::ty::label, caps(s.over).c_str(), dim);
    lv_label_set_long_mode(ov, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_width(ov, LV_PCT(100));
    lv_obj_set_height(ov, fw::style(fw::ty::label).box);
  }
  lv_obj_t *ti = d->ti = label(col, fw::ty::title, s.title, ink);
  d->su = d->val = nullptr;
  lv_label_set_long_mode(ti, LV_LABEL_LONG_MODE_DOTS);
  lv_obj_set_width(ti, LV_PCT(100));
  lv_obj_set_height(ti, fw::style(fw::ty::title).box);  // set_long_mode resets the text-box height
  if (!unavail && s.sub[0]) {
    lv_obj_t *su = d->su = label(col, fw::ty::body, s.sub, off ? fw::st::disabled_ink(t) : t.text_dim);
    lv_label_set_long_mode(su, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_width(su, LV_PCT(100));
    lv_obj_set_height(su, fw::style(fw::ty::body).box);
  }

  d->kind = kind;
  d->title = s.title;
  d->toggle = nullptr;
  const int body_h = fw::style(fw::ty::body).box;
  if (unavail) label(slot(r, body_h), fw::ty::body, fw::st::UNAVAILABLE_TEXT, fw::st::unavailable_note(t));
  // empty = the placeholder em dash, in text: a thin 1 px dash in text-dim measures under 4.5:1 at 1x (anti-aliased)
  if (kind == K_VALUE || kind == K_VALUE_CHEVRON) d->val = label(slot(r, body_h), fw::ty::body, s.value[0] ? s.value : fw::st::PLACEHOLDER, s.value[0] || off ? dim : t.text);
  if (kind == K_CHEVRON || kind == K_VALUE_CHEVRON) fw::icon(r, "chevron", fw::ic::icon, dim);
  if (kind == K_TOGGLE) {
    if (const fw::Component *tc = fw::find("toggle")) {  // the Toggle component itself, not a copy
      ArduinoJson::JsonDocument tp;
      tp["on"] = s.on;
      tp["state"] = off ? "disabled" : "default";
      tc->build(r, tp.as<fw::Props>());
      d->toggle = lv_obj_get_child(r, -1);
      lv_obj_remove_flag(d->toggle, LV_OBJ_FLAG_CLICKABLE);  // the row is the hit area and hands the tap on
    }
  }

  if (!strcmp(s.state, "pressed")) lv_obj_add_state(r, LV_STATE_PRESSED);
  if (off) lv_obj_add_state(r, ST_OFF);
}

// the root (made once): the Row + the tap handler; stack = the same root as a column of fixed demo rows
lv_obj_t *root(lv_obj_t *parent) {
  lv_obj_t *r = lv_obj_create(parent);
  auto *d = new Row;
  lv_obj_add_event_cb(r, row_free, LV_EVENT_DELETE, d);
  fw::st::on_tap(r, [](lv_event_t *e) {  // blocked on finger down, a real tap on CLICKED (states.json)
    if (!fw::st::tap_now(e)) return;
    auto *o = (lv_obj_t *) lv_event_get_current_target(e);
    auto *d = (Row *) lv_event_get_user_data(e);
    if (lv_obj_has_state(o, ST_OFF)) { fw::st::blocked(o, d->title.c_str()); return; }
    if (d->toggle) { lv_obj_send_event(d->toggle, LV_EVENT_CLICKED, nullptr); return; }
    fw::emit(o, "tap", d->title.c_str());
    fw::sound(o, d->kind == K_CHEVRON || d->kind == K_VALUE_CHEVRON ? "open" : "tap");
  }, d);
  return r;
}
lv_obj_t *row(lv_obj_t *parent, const Spec &s) {
  lv_obj_t *r = root(parent);
  fill(r, s, row_of(r));
  return r;
}

Spec spec_of(fw::Props p) {
  return {p["title"] | "", p["icon"] | "bulb", p["sub"] | "", p["trailing"] | "none", p["value"] | "",
          p["state"] | "default", p["on"] | false, p["divider"] | true, p["over"] | ""};
}
std::string stack_sig(fw::Props p);  // below: a stack's row count
// the props that need the contents again: all but a pressed state and a toggle's on (those move in place)
std::string sig(fw::Props p) {
  ArduinoJson::JsonDocument r;
  if (!strcmp(p["list"] | "single", "stack")) return "stack|" + stack_sig(p);
  r.set(p);
  if (!strcmp(p["state"] | "default", "pressed")) r["state"] = "default";
  if (effective(spec_of(p)) == K_TOGGLE) { r.remove("on"); r["trailing"] = "toggle"; }
  // texts move in place (a light's "Off" -> "On · 64%" must not rebuild the row: its Toggle would snap); only whether
  // there is a sub / a value is structure
  r["title"] = "";
  r["sub"] = (p["sub"] | "")[0] ? "sub" : "";
  r["over"] = (p["over"] | "")[0] ? "over" : "";
  if (r["value"].is<const char *>()) r["value"] = (p["value"] | "")[0] ? "value" : "";
  std::string s;
  serializeJson(r, s);
  return s;
}
constexpr int LIST_MAX = 24;  // list_items: rows a stack makes at most
const char *const ROW_FIELDS[] = {"title", "icon", "sub", "over", "trailing", "value", "on", "state"};
const char *const ROW_SKIP[] = {"list", "items", "rows", "empty", "_items"};
// the stack's rows: the props of each item that fits, then a "+N" row for the rest
std::vector<ArduinoJson::JsonDocument> stack_rows(fw::Props p) {
  std::vector<ArduinoJson::JsonDocument> out;
  const std::vector<std::string> all = fw::items::parse(p["items"] | "");
  const int rows = p["rows"] | 0, cap = rows > 0 ? std::min(rows, LIST_MAX) : LIST_MAX;
  const int n = (int) all.size() > cap ? cap - 1 : (int) all.size();
  for (int i = 0; i < n; i++) {
    out.emplace_back();
    ArduinoJson::JsonDocument &d = out.back();
    fw::items::row_props(p, all[i], ROW_FIELDS, 8, ROW_SKIP, 5, d);
    if (d["on"].is<const char *>()) d["on"] = !strcmp(d["on"] | "", "on") || !strcmp(d["on"] | "", "true");  // an HA state
  }
  if ((int) all.size() > n) {
    out.emplace_back();
    ArduinoJson::JsonDocument &m = out.back();
    m["title"] = "+" + std::to_string(all.size() - n);
    m["icon"] = "none";
  }
  return out;
}
// the stack's signature part: its row count (texts and toggles of the same rows move in place)
std::string stack_sig(fw::Props p) { return std::to_string(stack_rows(p).size()) + "|" + (p["empty"] | ""); }
void empty_row(lv_obj_t *r, const char *txt) {
  lv_obj_t *b = box(r);
  lv_obj_set_size(b, 2 * fw::lay::card_w + fw::lay::gutter, fw::lay::list_row_h);
  lv_obj_set_style_pad_hor(b, fw::sp::s4, 0);
  lv_obj_set_flex_flow(b, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(b, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  label(slot(b, fw::style(fw::ty::body).box), fw::ty::body, txt, fw::T->text_dim);
}
void fill_props(lv_obj_t *r, fw::Props p) {
  fw::shape(r, sig(p));
  if (!strcmp(p["list"] | "single", "stack")) {  // a list: rows touch, the hairline on top of each = the divider
    lv_obj_remove_style_all(r);
    lv_obj_remove_flag(r, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
    lv_obj_set_size(r, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(r, LV_FLEX_FLOW_COLUMN);
    std::vector<ArduinoJson::JsonDocument> rows = stack_rows(p);
    if (rows.empty()) empty_row(r, p["empty"] | "Nothing here");
    for (ArduinoJson::JsonDocument &d : rows) fill_props(root(r), d.as<fw::Props>());
    return;
  }
  fill(r, spec_of(p), row_of(r));
}
}  // namespace

FW_COMPONENT(list_row, "List row", "components", R"({
  "about": "A full-width row in a list or settings page, with an optional icon, a title and sub line, and a value, chevron or toggle.",
  "audit": {"base": {"trailing": "toggle"}},
  "frame": [448, 0],
  "icons": ["bulb@icon", "speaker@icon", "tv@icon", "volume@icon", "power@icon", "house@icon", "chevron@icon"],
  "props": [
    {"name": "title", "about": "The main words of the row.", "type": "text", "default": "Lights"},
    {"name": "icon", "about": "The icon at the start of the row, or none.", "type": "select", "options": ["none", "bulb", "speaker", "tv", "volume", "power", "house"], "default": "bulb"},
    {"name": "sub", "about": "A quieter second line under the title. Leave empty for none.", "type": "text", "default": ""},
    {"name": "over", "about": "A small line above the title, for extra detail such as a flight's height and distance. Leave empty for none.", "type": "text", "default": ""},
    {"name": "trailing", "about": "What sits at the right end: none, a value, a chevron to show it opens more, a value and a chevron, or an on/off switch.", "type": "select", "options": ["none", "value", "chevron", "value-chevron", "toggle"], "default": "none"},
    {"name": "value", "about": "The value shown at the right end when there is one, such as 42%.", "type": "text", "default": ""},
    {"name": "on", "about": "Whether the switch at the right end is on.", "type": "bool", "default": false},
    {"name": "state", "about": "How the row looks: default ready to tap, pressed while held, disabled when it cannot be used, or unavailable when its device cannot be reached.", "type": "select", "options": ["default", "pressed", "disabled", "unavailable"], "default": "default"},
    {"name": "divider", "about": "Draws a thin line under the row to separate it from the next.", "type": "bool", "default": true},
    {"name": "list", "about": "single is one row on its own; stack is a list of rows touching, with lines between them, one row per item of items.", "type": "select", "options": ["single", "stack"], "default": "single"},
    {"name": "items", "about": "With list stack: the rows, a comma list or a JSON list, usually bound to an entity attribute. Each row is this node's props with $item (or $item.field) filled in; a plain item is the row's title, and an object item's title (or name), icon, sub, over, trailing, value, on and state fields fill the props that do not name $item. No items shows the empty words, never sample rows.", "type": "text", "default": ""},
    {"name": "rows", "about": "With list stack: at most this many rows; 0 for up to 24. A longer list ends with a +N row for the rest.", "type": "range", "min": 0, "max": 24, "step": 1, "default": 0},
    {"name": "empty", "about": "With list stack: the words shown when items is empty.", "type": "text", "default": "Nothing here"}
  ],
  "variants": [
    {"label": "icon + title", "props": {}},
    {"label": "title + sub", "props": {"title": "Living room", "icon": "none", "sub": "3 lights on"}},
    {"label": "over + title", "props": {"title": "Brussels \u2192 Nottingham", "icon": "none", "over": "B763 \u00b7 21,000 ft \u00b7 22 km S"}},
    {"label": "icon + title + sub", "props": {"title": "Kitchen", "icon": "bulb", "sub": "Off since 18:40"}},
    {"label": "trailing value", "props": {"title": "Brightness", "icon": "none", "trailing": "value", "value": "80%"}},
    {"label": "trailing chevron", "props": {"title": "Sounds", "icon": "volume", "trailing": "chevron"}},
    {"label": "value + chevron", "props": {"title": "Theme", "icon": "none", "trailing": "value-chevron", "value": "Dark"}},
    {"label": "toggle · off", "props": {"title": "Chime", "icon": "speaker", "trailing": "toggle"}},
    {"label": "toggle · on", "props": {"title": "Chime", "icon": "speaker", "sub": "Plays on every tap", "trailing": "toggle", "on": true}},
    {"label": "pressed", "props": {"title": "Sounds", "icon": "volume", "trailing": "chevron", "state": "pressed"}},
    {"label": "disabled · toggle", "props": {"title": "Night dimming", "icon": "power", "sub": "Needs a clock source", "trailing": "toggle", "on": true, "state": "disabled"}},
    {"label": "disabled · value", "props": {"title": "Brightness", "icon": "none", "trailing": "value-chevron", "value": "80%", "state": "disabled"}},
    {"label": "unavailable", "props": {"title": "Porch light", "icon": "bulb", "trailing": "toggle", "state": "unavailable"}},
    {"label": "stack · dividers", "props": {"list": "stack", "items": "[{\"title\":\"Lights\",\"icon\":\"bulb\",\"trailing\":\"value-chevron\",\"value\":\"3 on\"},{\"title\":\"Chime\",\"icon\":\"speaker\",\"sub\":\"Plays on every tap\",\"trailing\":\"toggle\",\"on\":true},{\"title\":\"Volume\",\"icon\":\"volume\",\"trailing\":\"value\",\"value\":\"60%\"},{\"title\":\"Night dimming\",\"icon\":\"power\",\"trailing\":\"toggle\",\"state\":\"disabled\"},{\"title\":\"Porch light\",\"icon\":\"house\",\"trailing\":\"chevron\",\"state\":\"unavailable\"}]"}},
    {"label": "stack · plain items · cap", "props": {"list": "stack", "icon": "none", "trailing": "chevron", "items": "Kitchen,Hallway,Study,Bedroom,Garden", "rows": 3}},
    {"label": "stack · empty", "props": {"list": "stack", "empty": "No rooms yet"}}
  ]
})") {
  fill_props(root(parent), p);
}

// Update path: default <-> pressed = the press fade in place, a toggle row's on = the Toggle's own update (the knob
// slides, silent); any other prop (title, icon, trailing, value, disabled / unavailable, stack) = the contents again
// in the same root (fw::refill, snap; disabled / unavailable in or out fades them in). Silent.
FW_UPDATE(list_row) {
  Row *d = row_of(root);
  if (!d) return false;
  const bool pr = !strcmp(p["state"] | "default", "pressed");
  if (fw::shape(root, sig(p))) {
    if (!strcmp(p["list"] | "single", "stack")) {  // the same rows: each one's own update (texts in place, toggles slide)
      std::vector<ArduinoJson::JsonDocument> rows = stack_rows(p);
      for (uint32_t i = 0; i < rows.size() && i < lv_obj_get_child_count(root); i++)
        fw::update_child(fw::find("list_row"), lv_obj_get_child(root, i), rows[i].as<fw::Props>());
      return true;
    }
    if (pr != lv_obj_has_state(root, LV_STATE_PRESSED)) lv_obj_set_state(root, LV_STATE_PRESSED, pr);
    const Spec sp = spec_of(p);
    auto put = [](lv_obj_t *l, fw::ty::Id st, const char *t) { if (l && strcmp(lv_label_get_text(l), t)) fw::set_text(l, st, t); };
    put(d->ti, fw::ty::title, sp.title);
    d->title = sp.title;
    put(d->su, fw::ty::body, sp.sub);
    if (d->ov) put(d->ov, fw::ty::label, caps(sp.over).c_str());
    put(d->val, fw::ty::body, sp.value[0] ? sp.value : fw::st::PLACEHOLDER);
    if (d->val && !lv_obj_has_state(root, ST_OFF)) lv_obj_set_style_text_color(d->val, sp.value[0] ? fw::T->text_dim : fw::T->text, 0);
    if (d->toggle) {
      ArduinoJson::JsonDocument tp;
      tp["on"] = p["on"] | false;
      tp["state"] = "default";
      fw::update_child(fw::find("toggle"), d->toggle, tp.as<fw::Props>());
    }
    return true;
  }
  const bool was_off = lv_obj_has_state(root, ST_OFF);
  fw::refill(root);
  fill_props(root, p);
  if (pr != lv_obj_has_state(root, LV_STATE_PRESSED)) lv_obj_set_state(root, LV_STATE_PRESSED, pr);
  if (was_off != lv_obj_has_state(root, ST_OFF)) fw::st::fade_in_children(root);
  return true;
}
