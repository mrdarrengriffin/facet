// Hero block: the Facet hero block (docs: @facet/ui layout.ts heroBlock({label, role}) + set({n, label, line, accent}),
// CSS .fs-hero). A screen's one hero: a big number with a caps label over one line to its right. Accent = it matters
// now. One per screen (design principles).
//   box      the content width (2 x card-w + gutter, 656 / 440, = List row / Section header) x the number's type box
//            (hero 96 / 64; role display 144 / 96); flex row, top-aligned, gap lay::gutter (16)
//   number   screen type hero (or display), text; accent = accent-text (docs .is-accent); box width = the text width
//            rounded up to the grid (docs fit8), so the column after it starts on the grid
//   column   flex column, grows: label (screen type label, caps, text) over the line (type body, text-dim, one line,
//            ellipsis), gap space 2 (8 / 4). It sits centred on the hero type box: (96 - (24 + 8 + 32)) / 2 = 16 = the
//            docs padding-top 16 at 720, and 12 at 480 (64 - 40) / 2: a derived offset, no literal. With role display
//            the docs keep 16 from the top; here it stays on the hero box offset too (top-aligned with the hero row of
//            other screens), so the label / line read at the same height whichever number style is used.
// Unavailable (states.json; the hero shows a device's state): number = the placeholder em dash in st::unavailable_ink,
// no accent, line = "Unavailable" in st::unavailable_note. Static: no press, no events, no sound (docs).
// Contrast pairs: tokens/colour.json section "hero_block".
#include "fw.h"

namespace {
lv_obj_t *box(lv_obj_t *parent) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  return o;
}
lv_obj_t *line(lv_obj_t *parent, fw::ty::Id s, const char *txt, lv_color_t c) {
  lv_obj_t *l = fw::text(parent, s, txt, c);
  lv_obj_remove_flag(l, LV_OBJ_FLAG_CLICKABLE);
  lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_DOTS);
  lv_obj_set_width(l, LV_PCT(100));
  lv_obj_set_height(l, fw::style(s).box);  // set_long_mode resets the text-box height
  return l;
}
}  // namespace

namespace {
void fill(lv_obj_t *h, fw::Props p) {
  const fw::Theme &t = *fw::T;
  const bool off = !strcmp(p["state"] | "default", "unavailable");
  const bool accent = !off && (p["accent"] | false);
  const fw::ty::Id role = !strcmp(p["role"] | "hero", "display") ? fw::ty::display : fw::ty::hero;

  lv_obj_remove_style_all(h);
  lv_obj_remove_flag(h, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_size(h, 2 * fw::lay::card_w + fw::lay::gutter, fw::style(role).box);
  lv_obj_set_flex_flow(h, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(h, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_set_style_pad_column(h, fw::lay::gutter, 0);

  const char *n = off ? fw::st::PLACEHOLDER : (p["n"] | "");
  lv_obj_t *num = fw::text(h, role, n, off ? fw::st::unavailable_ink(t) : accent ? t.accent_text : t.text);
  lv_obj_remove_flag(num, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_update_layout(num);
  const int g = fw::sp::GRID, w = lv_obj_get_width(num);
  lv_obj_set_width(num, (w + g - 1) / g * g);  // fit8: the box on the grid, the text at its left
  lv_obj_set_height(num, fw::style(role).box);

  const int lab = fw::style(fw::ty::label).box, body = fw::style(fw::ty::body).box;
  lv_obj_t *col = box(h);
  lv_obj_set_size(col, 0, lab + fw::sp::s2 + body);  // width from flex grow: the line ellipsises
  lv_obj_set_flex_grow(col, 1);
  lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(col, fw::sp::s2, 0);
  lv_obj_set_style_margin_top(col, (fw::style(fw::ty::hero).box - (lab + fw::sp::s2 + body)) / 2, 0);
  line(col, fw::ty::label, p["label"] | "", off ? fw::st::unavailable_ink(t) : t.text);
  line(col, fw::ty::body, off ? fw::st::UNAVAILABLE_TEXT : (p["line"] | ""), off ? fw::st::unavailable_note(t) : t.text_dim);
}
}  // namespace

FW_COMPONENT(hero_block, "Hero block", "components", R"({
  "about": "A screen's one big number, with a label and one line beside it; accent when it matters now.",
  "frame": [480, 112],
  "props": [
    {"name": "n", "type": "text", "default": "3"},
    {"name": "label", "type": "text", "default": "need you"},
    {"name": "line", "type": "text", "default": "2 reviews · 2 assigned"},
    {"name": "accent", "type": "bool", "default": true},
    {"name": "role", "type": "select", "options": ["hero", "display"], "default": "hero"},
    {"name": "state", "type": "select", "options": ["default", "unavailable"], "default": "default"}
  ],
  "variants": [
    {"label": "accent · needs you", "props": {}},
    {"label": "quiet", "props": {"n": "12", "label": "open", "line": "3 drafts · 1 waiting on CI", "accent": false}},
    {"label": "money", "props": {"n": "£2.42", "label": "today", "line": "8.4 kWh · cheaper than yesterday", "accent": false}},
    {"label": "display role", "props": {"n": "21", "label": "inside", "line": "Feels like 19 · heating off", "accent": false, "role": "display"}},
    {"label": "long line", "props": {"n": "7", "label": "strikes", "line": "nearest 14 km north-east, moving away at 30 km/h", "accent": true}},
    {"label": "unavailable", "props": {"n": "21", "label": "inside", "line": "", "accent": false, "state": "unavailable"}}
  ]
})") {
  fill(lv_obj_create(parent), p);
}

// Update path: number, label, line, accent, role and unavailable snap in the same root (fw::refill), silent.
FW_UPDATE(hero_block) {
  fw::refill(root);
  fill(root, p);
  return true;
}
