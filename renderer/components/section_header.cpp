// Section header: the Facet section header (docs: @facet/ui lists.ts sectionHeader({title, count, tone, right}), CSS
// .f-section): a caps title, an optional count badge, and an optional right side, above a group of rows.
//   box      height lay::section_h (48 / 32, = touch min), width = the screen content width (2 x card-w + gutter,
//            656 / 440, = List row), flex row centred, gap space 5 (16 / 12 = docs gutter at both densities)
//   title    the Label component, type caps-sm, text-dim (= docs .f-section-title)
//   count    the Count badge component (prop "badge"; count / tone / zero passed on: 0 = muted or hidden)
//   right    none | meta (the Label, type meta, text-dim: docs .f-section-meta is text-mute, which is disabled-only
//            here) | action (a text button: caps-sm accent-text, not in the docs atom; the board asked for "count or
//            action")
// Action: a hit box section_h tall (= touch min) with space 2 padding, nudged right by that padding (translate) so its
// text ends on the content edge like the meta does; pressed = Card / List row press: mix card-press fill (Motion press /
// release), radius rad::chip; disabled (Button rule, USER_1) = text-mute, no press look, tap = "blocked" + error.
// Tap = a real CLICKED: "tap" (detail = the action text) + sound "tap" (an in-place action such as Clear; a header
// action that opens a screen could play "open" later). The title, badge and meta are static (no events).
// Contrast pairs: tokens/colour.json section "section_header" (+ label, count_badge).
#include "fw.h"

namespace {
constexpr lv_state_t ST_OFF = LV_STATE_USER_1;  // disabled (LVGL sends no events to LV_STATE_DISABLED; see Button)

const lv_style_prop_t TR_PROPS[] = {LV_STYLE_BG_OPA, LV_STYLE_PROP_INV};
const lv_style_transition_dsc_t *tr(bool pressed) {
  static lv_style_transition_dsc_t d[2];
  static bool ready = false;
  if (!ready) { fw::mo::transition(&d[0], fw::mo::release, TR_PROPS); fw::mo::transition(&d[1], fw::mo::press, TR_PROPS); ready = true; }
  return &d[pressed];
}

lv_obj_t *build(const char *id, lv_obj_t *parent, ArduinoJson::JsonDocument &props) {
  const fw::Component *c = fw::find(id);
  if (!c) return nullptr;
  const uint32_t before = lv_obj_get_child_count(parent);
  c->build(parent, props.as<fw::Props>());
  return lv_obj_get_child_count(parent) > before ? lv_obj_get_child(parent, -1) : nullptr;
}
}  // namespace

FW_COMPONENT(section_header, "Section header", "components", R"({
  "frame": [480, 96],
  "props": [
    {"name": "title", "type": "text", "default": "Needs you"},
    {"name": "badge", "type": "bool", "default": true},
    {"name": "count", "type": "range", "min": 0, "max": 150, "step": 1, "default": 3},
    {"name": "tone", "type": "select", "options": ["ok", "warn", "error", "working", "info", "idle"], "default": "warn"},
    {"name": "zero", "type": "select", "options": ["muted", "hide"], "default": "muted"},
    {"name": "right", "type": "select", "options": ["none", "meta", "action"], "default": "meta"},
    {"name": "meta", "type": "text", "default": "updated 2m"},
    {"name": "action", "type": "text", "default": "See all"},
    {"name": "state", "type": "select", "options": ["default", "pressed", "disabled"], "default": "default"}
  ],
  "variants": [
    {"label": "count + meta", "props": {}},
    {"label": "title only", "props": {"title": "Today", "badge": false, "right": "none"}},
    {"label": "count 0 · muted", "props": {"title": "Reviews", "count": 0, "right": "none"}},
    {"label": "count 0 · hidden", "props": {"title": "Reviews", "count": 0, "zero": "hide", "right": "none"}},
    {"label": "99+ · working", "props": {"title": "Running", "count": 128, "tone": "working", "right": "none"}},
    {"label": "action", "props": {"title": "Your pull requests", "count": 2, "tone": "info", "right": "action"}},
    {"label": "action · pressed", "props": {"title": "Notifications", "count": 5, "tone": "error", "right": "action", "action": "Clear", "state": "pressed"}},
    {"label": "action · disabled", "props": {"title": "Notifications", "badge": false, "right": "action", "action": "Clear", "state": "disabled"}}
  ]
})") {
  const fw::Theme &t = *fw::T;
  lv_obj_t *h = lv_obj_create(parent);
  lv_obj_remove_style_all(h);
  lv_obj_remove_flag(h, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_size(h, 2 * fw::lay::card_w + fw::lay::gutter, fw::lay::section_h);
  lv_obj_set_flex_flow(h, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(h, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(h, fw::sp::s5, 0);

  ArduinoJson::JsonDocument tp;
  tp["text"] = p["title"] | "";
  tp["type"] = "caps-sm";
  tp["colour"] = "text-dim";
  build("label", h, tp);

  if (p["badge"] | true) {
    ArduinoJson::JsonDocument bp;
    bp["count"] = p["count"] | 0;
    bp["tone"] = p["tone"] | "warn";
    bp["zero"] = p["zero"] | "muted";
    build("count_badge", h, bp);
  }

  lv_obj_t *grow = lv_obj_create(h);
  lv_obj_remove_style_all(grow);
  lv_obj_remove_flag(grow, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_size(grow, 0, 0);
  lv_obj_set_flex_grow(grow, 1);

  const char *right = p["right"] | "meta";
  if (!strcmp(right, "meta")) {
    ArduinoJson::JsonDocument mp;
    mp["text"] = p["meta"] | "";
    mp["type"] = "meta";
    mp["colour"] = "text-dim";
    build("label", h, mp);
  } else if (!strcmp(right, "action")) {
    const char *state = p["state"] | "default";
    const bool off = !strcmp(state, "disabled");
    lv_obj_t *a = lv_obj_create(h);
    lv_obj_remove_style_all(a);
    lv_obj_add_flag(a, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(a, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(a, LV_SIZE_CONTENT, fw::lay::section_h);
    lv_obj_set_style_pad_hor(a, fw::sp::s2, 0);
    lv_obj_set_style_translate_x(a, fw::sp::s2, 0);  // text ends on the content edge; the press fill overhangs it
    lv_obj_set_style_radius(a, fw::rad::chip, 0);
    lv_obj_set_flex_flow(a, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(a, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_color_t press = t.surface;
    for (int i = 0; i < fw::tokens::N_MIXES; i++)
      if (!strcmp(fw::tokens::MIXES[i].name, "card-press")) press = fw::tokens::mix(fw::tokens::MIXES[i], t);
    lv_obj_set_style_bg_color(a, press, 0);
    lv_obj_set_style_bg_opa(a, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_opa(a, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(a, LV_OPA_TRANSP, ST_OFF | LV_STATE_PRESSED);
    lv_obj_set_style_transition(a, tr(false), 0);
    lv_obj_set_style_transition(a, tr(true), LV_STATE_PRESSED);

    ArduinoJson::JsonDocument ap;
    ap["text"] = p["action"] | "";
    ap["type"] = "caps-sm";
    ap["colour"] = off ? "disabled" : "accent-text";
    lv_obj_t *l = build("label", a, ap);

    if (!strcmp(state, "pressed")) lv_obj_add_state(a, LV_STATE_PRESSED);
    if (off) lv_obj_add_state(a, ST_OFF);
    lv_obj_add_event_cb(a, [](lv_event_t *e) {
      auto *o = (lv_obj_t *) lv_event_get_current_target(e);
      auto *l = (lv_obj_t *) lv_event_get_user_data(e);
      const char *d = l ? lv_label_get_text(l) : "";
      if (lv_obj_has_state(o, ST_OFF)) { fw::emit(o, "blocked", d); fw::sound(o, "error"); }
      else { fw::emit(o, "tap", d); fw::sound(o, "tap"); }
    }, LV_EVENT_CLICKED, l);
  }
}
