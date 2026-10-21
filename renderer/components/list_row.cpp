// List row: the Facet hairline row (docs: Settings rows .fs-set, picker rows .f-prow / pickerRow(), sheet rows). Rows
// are not boxed: they sit on the screen background and a 1 px line hairline on top separates them (docs "rows
// separated by hairlines rather than boxes"); a stack of rows = a list with a divider between each.
//   height   lay::list_row_h (docs layout sheet-row-h: 96 at 720, 64 at 480), one line or title + sub line
//   width    the screen content width = 2 x card-w + gutter (656 / 440), i.e. inset to inset
//   padding  space 4 left and right (16 / 8, docs row-pad 16) so the press fill does not run into the text
// Layout (flex row, centred vertically, gap space 4; nothing hand-placed):
//   icon     optional, ic::icon (24 / 16: the Facet icon manifest ships row icons at this size, "picker rows 24";
//            only icon@size pairs that exist are drawn, never scaled), text-dim
//   text     grows: title (type title, 40 / 28 box, text, one line, ellipsis) over an optional sub (type body, text-dim)
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
// other rows emit "tap" + sound "tap". Contrast pairs: tokens/colour.json section "list_row" (+ toggle, card).
#include "fw.h"

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
};

lv_obj_t *row(lv_obj_t *parent, const Spec &s) {
  const fw::Theme &t = *fw::T;
  const bool unavail = !strcmp(s.state, "unavailable"), off = unavail || !strcmp(s.state, "disabled");
  Kind kind = unavail ? K_NONE : kind_of(s.trailing);
  if (!unavail && kind == K_NONE && s.on) kind = K_TOGGLE;            // playground: on implies a toggle
  else if (!unavail && kind == K_NONE && s.value[0]) kind = K_VALUE;  // a typed value implies a value
  const lv_color_t ink = off ? fw::st::disabled_ink(t) : t.text, dim = off ? fw::st::disabled_ink(t) : t.text_dim;

  lv_obj_t *r = lv_obj_create(parent);
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
  const int col_h = fw::style(fw::ty::title).box + (!unavail && s.sub[0] ? fw::style(fw::ty::body).box : 0);
  lv_obj_set_size(col, 0, on_grid(col_h));  // width from flex grow; text ellipsises instead of pushing the trailing
  lv_obj_set_flex_grow(col, 1);
  lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
  lv_obj_t *ti = label(col, fw::ty::title, s.title, ink);
  lv_label_set_long_mode(ti, LV_LABEL_LONG_MODE_DOTS);
  lv_obj_set_width(ti, LV_PCT(100));
  lv_obj_set_height(ti, fw::style(fw::ty::title).box);  // set_long_mode resets the text-box height
  if (!unavail && s.sub[0]) {
    lv_obj_t *su = label(col, fw::ty::body, s.sub, off ? fw::st::disabled_ink(t) : t.text_dim);
    lv_label_set_long_mode(su, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_width(su, LV_PCT(100));
    lv_obj_set_height(su, fw::style(fw::ty::body).box);
  }

  auto *d = new Row;
  d->kind = kind;
  d->title = s.title;
  const int body_h = fw::style(fw::ty::body).box;
  if (unavail) label(slot(r, body_h), fw::ty::body, fw::st::UNAVAILABLE_TEXT, fw::st::unavailable_note(t));
  if (kind == K_VALUE || kind == K_VALUE_CHEVRON) label(slot(r, body_h), fw::ty::body, s.value[0] ? s.value : fw::st::PLACEHOLDER, dim);  // empty = em dash
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

  lv_obj_add_event_cb(r, [](lv_event_t *e) { delete (Row *) lv_event_get_user_data(e); }, LV_EVENT_DELETE, d);
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
}  // namespace

FW_COMPONENT(list_row, "List row", "components", R"({
  "about": "A full-width row in a list or settings page, with an optional icon, a title and sub line, and a value, chevron or toggle.",
  "frame": [448, 0],
  "icons": ["bulb@icon", "speaker@icon", "tv@icon", "volume@icon", "power@icon", "house@icon", "chevron@icon"],
  "props": [
    {"name": "title", "type": "text", "default": "Lights"},
    {"name": "icon", "type": "select", "options": ["none", "bulb", "speaker", "tv", "volume", "power", "house"], "default": "bulb"},
    {"name": "sub", "type": "text", "default": ""},
    {"name": "trailing", "type": "select", "options": ["none", "value", "chevron", "value-chevron", "toggle"], "default": "none"},
    {"name": "value", "type": "text", "default": ""},
    {"name": "on", "type": "bool", "default": false},
    {"name": "state", "type": "select", "options": ["default", "pressed", "disabled", "unavailable"], "default": "default"},
    {"name": "divider", "type": "bool", "default": true},
    {"name": "list", "type": "select", "options": ["single", "stack"], "default": "single"}
  ],
  "variants": [
    {"label": "icon + title", "props": {}},
    {"label": "title + sub", "props": {"title": "Living room", "icon": "none", "sub": "3 lights on"}},
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
    {"label": "stack · dividers", "props": {"list": "stack"}}
  ]
})") {
  if (!strcmp(p["list"] | "single", "stack")) {  // a short list: rows touch, the hairline on top of each = the divider
    lv_obj_t *l = box(parent);
    lv_obj_set_size(l, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(l, LV_FLEX_FLOW_COLUMN);
    const Spec rows[] = {
        {"Lights", "bulb", "", "value-chevron", "3 on", "default", false, true},
        {"Chime", "speaker", "Plays on every tap", "toggle", "", "default", true, true},
        {"Volume", "volume", "", "value", "60%", "default", false, true},
        {"Night dimming", "power", "", "toggle", "", "disabled", false, true},
        {"Porch light", "house", "", "chevron", "", "unavailable", false, true},
    };
    for (const Spec &s : rows) row(l, s);
    return;
  }
  row(parent, {p["title"] | "Lights", p["icon"] | "bulb", p["sub"] | "", p["trailing"] | "none", p["value"] | "",
               p["state"] | "default", p["on"] | false, p["divider"] | true});
}
