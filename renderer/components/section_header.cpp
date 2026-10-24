// Section header: the Facet section header (docs: @facet/ui lists.ts sectionHeader({title, count, tone, right}), CSS
// .f-section): a caps title, an optional count badge, and an optional right side, above a group of rows.
//   box      height lay::section_h (48 / 32, = touch min), width = the screen content width (2 x card-w + gutter,
//            656 / 440, = List row), flex row centred, gap space 5 (16 / 12 = docs gutter at both densities)
//   title    the Label component, type caps-sm, text-dim (= docs .f-section-title); size label = the screen label
//            type instead (the panel lab's Door lock "last 24 h")
//   count    the Count badge component (prop "badge"; count / tone / zero passed on: 0 = muted or hidden)
//   right    none | meta (the Label, type meta, text-dim: docs .f-section-meta is text-mute, which is disabled-only
//            here) | action (a text button: caps-sm accent-text, not in the docs atom; the board asked for "count or
//            action")
// Action: a hit box section_h tall (= touch min) with space 2 padding, nudged right by that padding (translate) so its
// text ends on the content edge like the meta does; pressed = Card / List row press: mix press-tint fill (Motion press /
// release), radius rad::chip; disabled (Button rule, USER_1) = text-mute, no press look, tap = "blocked" + error.
// Tap = a real CLICKED: "tap" (detail = the action text) + sound "tap" (an in-place action such as Clear; a header
// action that opens a screen could play "open" later). The title, badge and meta are static (no events).
// Contrast pairs: tokens/colour.json section "section_header" (+ label, count_badge).
#include "fw.h"

namespace {
constexpr lv_state_t ST_OFF = fw::st::OFF;  // disabled (LVGL sends no events to LV_STATE_DISABLED; see Button)

const lv_style_prop_t TR_PROPS[] = {LV_STYLE_BG_OPA, LV_STYLE_PROP_INV};
const lv_style_transition_dsc_t *tr(bool pressed) {
  static lv_style_transition_dsc_t d[2];
  static bool ready = false;
  if (!ready) { fw::st::press_transitions(d, TR_PROPS); ready = true; }  // states.json pressed motion
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

namespace {
void fill(lv_obj_t *h, fw::Props p) {
  const fw::Theme &t = *fw::T;
  lv_obj_remove_style_all(h);
  lv_obj_remove_flag(h, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_size(h, 2 * fw::lay::card_w + fw::lay::gutter, fw::lay::section_h);
  lv_obj_set_flex_flow(h, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(h, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(h, fw::sp::s5, 0);

  if (!strcmp(p["size"] | "sm", "label")) {  // the screen label type (the panel lab's lock "last 24 h" heading):
    lv_obj_set_height(h, fw::style(fw::ty::label).box + fw::sp::s2);  // its box + sp::s2 to the rows, at the top
    lv_obj_set_flex_align(h, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_t *l = fw::text(h, fw::ty::label, p["title"] | "", t.text_dim);
    lv_obj_set_height(l, fw::style(fw::ty::label).box);
  } else {
    ArduinoJson::JsonDocument tp;
    tp["text"] = p["title"] | "";
    tp["type"] = "caps-sm";
    tp["colour"] = "text-dim";
    build("label", h, tp);
  }

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
    fw::bleed(a, fw::sp::s2);  // the overhang draws outside the header box (else LVGL clips the fill's right end)
    lv_obj_set_style_radius(a, fw::rad::chip, 0);
    lv_obj_set_flex_flow(a, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(a, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_color(a, fw::st::pressed_tint(t), 0);  // states.json pressed tint
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
    fw::st::on_tap(a, [](lv_event_t *e) {  // blocked on finger down, a real tap on CLICKED (states.json)
      if (!fw::st::tap_now(e)) return;
      auto *o = (lv_obj_t *) lv_event_get_current_target(e);
      auto *l = (lv_obj_t *) lv_event_get_user_data(e);
      const char *d = l ? lv_label_get_text(l) : "";
      if (lv_obj_has_state(o, ST_OFF)) fw::st::blocked(o, d);
      else { fw::emit(o, "tap", d); fw::sound(o, "tap"); }
    }, l);
  }
}
}  // namespace

FW_COMPONENT(section_header, "Section header", "components", R"({
  "about": "A caption that starts a group of content, with an optional count and a meta text or action on the right.",
  "audit": {"snaps": {"count": "the Count badge's count changes like text"}},
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
    {"name": "state", "type": "select", "options": ["default", "pressed", "disabled"], "default": "default"},
    {"name": "size", "type": "select", "options": ["sm", "label"], "default": "sm"}
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
  lv_obj_t *h = lv_obj_create(parent);
  fill(h, p);
  fw::st::built(h, p);
}

// Update path: the action's default <-> pressed = its press fade in place; any other prop builds the header again in
// the same root (fw::refill, snap: badge, right side, texts), the action going disabled / back fades in. Silent.
FW_UPDATE(section_header) {
  const bool pr = !strcmp(p["state"] | "default", "pressed");
  const bool was_off = !strcmp(p["right"] | "meta", "action") && lv_obj_has_state(lv_obj_get_child(root, -1), ST_OFF);
  if (fw::shape(root, [&] {  // the props with pressed read as default (fw::st::built stored the same)
        ArduinoJson::JsonDocument r;
        r.set(p);
        if (pr) r["state"] = "default";
        std::string s;
        serializeJson(r, s);
        return s;
      }())) {
    lv_obj_t *a = lv_obj_get_child(root, -1);
    if (a && !strcmp(p["right"] | "meta", "action") && pr != lv_obj_has_state(a, LV_STATE_PRESSED))
      lv_obj_set_state(a, LV_STATE_PRESSED, pr);
    return true;
  }
  fw::st::refill_fade(root, [&] { fw::refill(root); fill(root, p); });  // a new tone fades (like a tap)
  const bool off = !strcmp(p["right"] | "meta", "action") && !strcmp(p["state"] | "default", "disabled");
  if (off != was_off) fw::st::fade_in_children(lv_obj_get_child(root, -1));
  return true;
}
