// Stat: the Facet stat (docs: @facet/ui layout.ts stat(label) + set(value, tone, label), CSS .fs-stat). A caps label
// over one line of body text: "trend / 48 km/h", "lit / 63%". Screens lay stats out in columns; a stat fills the width
// its parent gives it.
//   box      flex column, gap space 2: label box + 8 + body box = 24 + 8 + 32 = 64 at 720 (= docs), 16 + 4 + 20 = 40
//            at 480; width lay::card_w (one Home column, 320 / 212), or the parent's width with the internal prop
//            "fill" (not in the playground; a screen's column passes it), so the value ellipsises in its column
//   label    screen type label (caps), text-dim (docs text-mute: disabled only here, the contrast rule)
//   value    screen type body, one line, ellipsis; tone dim (default, docs) = text-dim, text = text (docs moon facts
//            pass 'text'), warn = warn, ok = ok
// Unavailable (states.json; a stat often shows one device value): label text-mute, value = "Unavailable" in
// st::unavailable_note. Static: no press, no events, no sound (docs). The docs explorer sample is 240 px wide. Contrast pairs: tokens/colour.json section "stat".
#include "fw.h"

namespace {
lv_obj_t *line(lv_obj_t *parent, fw::ty::Id s, const char *txt, lv_color_t c) {
  lv_obj_t *l = fw::text(parent, s, txt, c);
  lv_obj_remove_flag(l, LV_OBJ_FLAG_CLICKABLE);
  lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_DOTS);
  lv_obj_set_width(l, LV_PCT(100));
  lv_obj_set_height(l, fw::style(s).box);  // set_long_mode resets the text-box height
  return l;
}
lv_color_t tone(const char *t) {
  if (!strcmp(t, "text")) return fw::T->text;
  if (!strcmp(t, "warn")) return fw::T->warn;
  if (!strcmp(t, "ok")) return fw::T->ok;
  return fw::T->text_dim;
}
}  // namespace

namespace {
void fill(lv_obj_t *s, fw::Props p) {
  const fw::Theme &t = *fw::T;
  const bool off = !strcmp(p["state"] | "default", "unavailable");
  lv_obj_remove_style_all(s);
  lv_obj_remove_flag(s, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_size(s, (p["fill"] | false) ? LV_PCT(100) : fw::lay::card_w, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(s, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(s, fw::sp::s2, 0);
  line(s, fw::ty::label, p["label"] | "", off ? fw::st::unavailable_ink(t) : t.text_dim);
  line(s, fw::ty::body, off ? fw::st::UNAVAILABLE_TEXT : (p["value"] | ""), off ? fw::st::unavailable_note(t) : tone(p["tone"] | "dim"));
}
}  // namespace

FW_COMPONENT(stat, "Stat", "components", R"({
  "about": "A caps label over one line of text: one fact in a column of facts.",
  "frame": [240, 64],
  "props": [
    {"name": "label", "type": "text", "default": "trend"},
    {"name": "value", "type": "text", "default": "48 km/h"},
    {"name": "tone", "type": "select", "options": ["dim", "text", "warn", "ok"], "default": "dim"},
    {"name": "state", "type": "select", "options": ["default", "unavailable"], "default": "default"}
  ],
  "variants": [
    {"label": "dim · default", "props": {}},
    {"label": "text", "props": {"label": "lit", "value": "63%", "tone": "text"}},
    {"label": "warn", "props": {"label": "gusts", "value": "71 km/h", "tone": "warn"}},
    {"label": "ok", "props": {"label": "backup", "value": "done 04:12", "tone": "ok"}},
    {"label": "long value", "props": {"label": "next", "value": "Bin day tomorrow: recycling and garden waste", "tone": "text"}},
    {"label": "unavailable", "props": {"label": "outside", "value": "12°", "state": "unavailable"}}
  ]
})") {
  fill(lv_obj_create(parent), p);
}

// Update path: label, value, tone and unavailable snap in the same root (fw::refill), silent.
FW_UPDATE(stat) {
  fw::refill(root);
  fill(root, p);
  return true;
}
