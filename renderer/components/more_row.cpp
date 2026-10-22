// More row: the Facet "more" row (docs: @facet/ui layout.ts moreRow(label, onTap) + set(text), CSS .fs-more). Ends a
// list that shows only its top items and opens the rest: "all pull requests · 3 more".
//   box      the content width (2 x card-w + gutter, 656 / 440, = List row) x lay::more_row_h (layout.json rows
//            more-row-h: 64 / 44), on the canvas, 1 px line hairline on top (like every row)
//   padding  top space 7 - hair (24 / 16 from the row's top edge: hair + 23 = the label at 24 like the docs rows),
//            left / right space 4 (16 / 8) = List row, so the label lines up with the rows above it and the press
//            fill does not run into the text (the docs .fs-more has no side padding: its .fs-row neighbours have none)
//   label    screen type label (caps), text-dim, one line, grows and ellipsises
//   chevron  icon chevron at ic::icon (24 / 16 = the size the manifest ships it, "rows and more rows"), a gutter after
//            the label, top-aligned with it (24 / 16 = the label box at both densities); text-dim (docs text-mute:
//            disabled only here, the contrast rule, = List row's chevron)
// Pressed (Motion press / release): List row's press, the states.json pressed tint (mix press-tint fill). Disabled
// (Button rule, USER_1): label + chevron text-mute, no press look, tap = "blocked" + error on finger down. Tap = "tap"
// (detail = the label) + sound "open" (docs data-snd open: it opens the full list). Contrast pairs: tokens/colour.json
// section "more_row" (+ list_row).
#include "fw.h"

namespace {
constexpr lv_state_t ST_OFF = fw::st::OFF;

const lv_style_prop_t TR_PROPS[] = {LV_STYLE_BG_OPA, LV_STYLE_PROP_INV};
const lv_style_transition_dsc_t *tr(bool pressed) {
  static lv_style_transition_dsc_t d[2];
  static bool ready = false;
  if (!ready) { fw::st::press_transitions(d, TR_PROPS); ready = true; }  // states.json pressed motion
  return &d[pressed];
}
}  // namespace

namespace {
void fill(lv_obj_t *r, fw::Props p) {
  const fw::Theme &t = *fw::T;
  const char *state = p["state"] | "default";
  const bool off = !strcmp(state, "disabled");
  const lv_color_t ink = off ? fw::st::disabled_ink(t) : t.text_dim;

  lv_obj_remove_style_all(r);
  lv_obj_add_flag(r, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(r, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(r, 2 * fw::lay::card_w + fw::lay::gutter, fw::lay::more_row_h);
  lv_obj_set_style_border_side(r, LV_BORDER_SIDE_TOP, 0);
  lv_obj_set_style_border_width(r, fw::stroke::hair, 0);
  lv_obj_set_style_border_color(r, t.line, 0);
  lv_obj_set_style_pad_top(r, fw::sp::s7 - fw::stroke::hair, 0);  // the border counts as content offset in LVGL
  lv_obj_set_style_pad_hor(r, fw::sp::s4, 0);
  lv_obj_set_style_bg_color(r, fw::st::pressed_tint(t), 0);  // states.json pressed tint
  lv_obj_set_style_bg_opa(r, LV_OPA_TRANSP, 0);
  lv_obj_set_style_transition(r, tr(false), 0);
  lv_obj_set_style_bg_opa(r, LV_OPA_COVER, LV_STATE_PRESSED);
  lv_obj_set_style_transition(r, tr(true), LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(r, LV_OPA_TRANSP, ST_OFF);  // no press look when disabled
  lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_set_style_pad_column(r, fw::lay::gutter, 0);

  lv_obj_t *l = fw::text(r, fw::ty::label, p["text"] | "", ink);
  lv_obj_remove_flag(l, LV_OBJ_FLAG_CLICKABLE);
  lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_DOTS);
  lv_obj_set_width(l, 0);  // width from flex grow
  lv_obj_set_flex_grow(l, 1);
  lv_obj_set_height(l, fw::style(fw::ty::label).box);
  fw::icon(r, "chevron", fw::ic::icon, ink);

  if (!strcmp(state, "pressed")) lv_obj_add_state(r, LV_STATE_PRESSED);
  if (off) lv_obj_add_state(r, ST_OFF);


}
}  // namespace

FW_COMPONENT(more_row, "More row", "components", R"({
  "about": "The last row of a short list: a caps label and a chevron that open the rest.",
  "frame": [480, 64],
  "icons": ["chevron@icon"],
  "props": [
    {"name": "text", "type": "text", "default": "all pull requests · 3 more"},
    {"name": "state", "type": "select", "options": ["default", "pressed", "disabled"], "default": "default"}
  ],
  "variants": [
    {"label": "default", "props": {}},
    {"label": "pressed", "props": {"state": "pressed"}},
    {"label": "disabled", "props": {"text": "all notifications · 0 more", "state": "disabled"}},
    {"label": "long label", "props": {"text": "every automation that ran in the last 24 hours · 128 more"}}
  ]
})") {
  lv_obj_t *r = lv_obj_create(parent);
  fill(r, p);
  fw::st::built(r, p);

  fw::st::on_tap(r, [](lv_event_t *e) {  // blocked on finger down, a real tap on CLICKED (states.json)
    if (!fw::st::tap_now(e)) return;
    auto *o = (lv_obj_t *) lv_event_get_current_target(e);
    const char *d = lv_label_get_text(lv_obj_get_child(o, 0));  // the label, looked up (updates make it again)
    if (lv_obj_has_state(o, ST_OFF)) fw::st::blocked(o, d);
    else { fw::emit(o, "tap", d); fw::sound(o, "open"); }
  }, nullptr);
}

// Update path: default <-> pressed = the press fade in place; any other prop builds the contents again in the same
// root (fw::refill, snap), disabled in / out fades them in (fw::st::update). Silent.
FW_UPDATE(more_row) { return fw::st::update(root, p, fill); }
