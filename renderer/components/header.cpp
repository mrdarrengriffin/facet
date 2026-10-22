// Header: the Facet screen header (docs: @facet/ui layout.ts header({label, onBack, right, clock}), CSS .fs-header).
// One row at y = inset across the content width: the Back button, the screen's label, optional items on the right and
// the time at the right edge. The label only names the screen; the hero under it says what the screen is about.
//   box      lay::content_w x lay::row (656 x 48 / 440 x 32) = layout.json header row, checked at compile time: the
//            Back button fills it exactly (touch::back == row). The screen places it at (inset, inset); this
//            component is the row itself, so a screen (and the Layout page) just put it there.
//   back     the Back button component (fw::find("back_button")), lay::back_gap (16 / 12) to the label (layout.json
//            header back-gap; the docs use the gutter for every gap, = back-gap at 720)
//   label    screen type label (caps, mono), text-dim, one line; grows into the free space and ellipsises before the
//            right side (docs: fit8 + a grow spacer; LVGL flex never shrinks, so the label is the grow here)
//   right    optional: the Status chip component (docs explorer "Right chip": working, "2 working"), a gutter apart
//   time     screen type label, text-dim, lay::header_time_w (72 / 48 = docs clock-w) wide, right-aligned on the
//            content edge; a gutter from what comes before it
// Static apart from the Back button, which keeps its own look, press, tap ("back" + sound back) and disabled rules.
// Contrast pairs: tokens/colour.json section "header" (+ icon_button, status_chip).
#include "fw.h"

static_assert(fw::touch::back == fw::lay::row, "the Back button must fill the header row");

namespace {
lv_obj_t *box(lv_obj_t *parent) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  return o;
}
lv_obj_t *build(const char *id, lv_obj_t *parent, ArduinoJson::JsonDocument &props) {
  const fw::Component *c = fw::find(id);
  if (!c) return nullptr;
  const uint32_t before = lv_obj_get_child_count(parent);
  c->build(parent, props.as<fw::Props>());
  return lv_obj_get_child_count(parent) > before ? lv_obj_get_child(parent, -1) : nullptr;
}
lv_obj_t *caps(lv_obj_t *parent, const char *txt) {
  lv_obj_t *l = fw::text(parent, fw::ty::label, txt, fw::T->text_dim);
  lv_obj_remove_flag(l, LV_OBJ_FLAG_CLICKABLE);
  lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_DOTS);
  return l;
}
}  // namespace

FW_COMPONENT(header, "Header", "components", R"({
  "about": "The row at the top of every screen: a back button, the screen's name, optional items and the time.",
  "frame": [480, 80],
  "icons": ["back@glyph"],
  "props": [
    {"name": "label", "type": "text", "default": "Weather"},
    {"name": "back", "type": "bool", "default": true},
    {"name": "clock", "type": "bool", "default": true},
    {"name": "time", "type": "text", "default": "21:42"},
    {"name": "chip", "type": "bool", "default": false}
  ],
  "variants": [
    {"label": "back + label + time", "props": {}},
    {"label": "Home · no back", "props": {"label": "Home", "back": false}},
    {"label": "right chip", "props": {"label": "Agents", "chip": true}},
    {"label": "no time", "props": {"label": "Settings", "clock": false}},
    {"label": "long label", "props": {"label": "Living room ceiling and the sofa lamps", "chip": true}}
  ]
})") {
  lv_obj_t *h = box(parent);
  lv_obj_set_size(h, fw::lay::content_w, fw::lay::row);
  lv_obj_set_flex_flow(h, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(h, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(h, fw::lay::gutter, 0);

  if (p["back"] | true) {
    ArduinoJson::JsonDocument bp;
    if (lv_obj_t *b = build("back_button", h, bp))  // back-gap to the label (the row gap is the gutter)
      lv_obj_set_style_margin_right(b, fw::lay::back_gap - fw::lay::gutter, 0);
  }

  lv_obj_t *l = caps(h, p["label"] | "");
  lv_obj_set_width(l, 0);  // width from flex grow: the label takes the free space and ellipsises
  lv_obj_set_flex_grow(l, 1);
  lv_obj_set_height(l, fw::style(fw::ty::label).box);  // set_long_mode resets the text-box height

  if (p["chip"] | false) {
    ArduinoJson::JsonDocument cp;
    cp["tone"] = "working";
    cp["text"] = "2 working";
    build("status_chip", h, cp);
  }

  if (p["clock"] | true) {
    lv_obj_t *t = caps(h, p["time"] | "");
    lv_obj_set_size(t, fw::lay::header_time_w, fw::style(fw::ty::label).box);
    lv_obj_set_style_text_align(t, LV_TEXT_ALIGN_RIGHT, 0);
  }
}
