// Foundations > Type: every type style (tokens/type.json via the generated type_gen.h + this density's fonts), drawn by
// the panel renderer itself at true size. Sections: the screen and component scales (each style in its own line box,
// hairlines at the box edges, with its spec), mono caps beside sans, number styles (units, tabular digits), the glyph
// coverage of each glyph set, and the fonts of this build with their flash cost. The device switch picks the 720 or
// 480 build, so the sizes are that panel's. Status (WIP/Done) = this file + tokens/type.json in git.
#include <cstdio>
#include "fw.h"

namespace {
using namespace fw;

constexpr float W = 480;  // doc page width in dp; padding and gaps = tokens/space.json
const int PAD = sp::s7;

lv_obj_t *box(lv_obj_t *parent) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
  return o;
}
lv_obj_t *column(lv_obj_t *parent, int gap, int w = LV_PCT(100)) {
  lv_obj_t *c = box(parent);
  lv_obj_set_size(c, w, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(c, gap, 0);
  return c;
}
lv_obj_t *row(lv_obj_t *parent, int gap) {
  lv_obj_t *r = box(parent);
  lv_obj_set_size(r, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
  lv_obj_set_style_pad_column(r, gap, 0);
  return r;
}
// one entry of a list: hairline under it, padding around
lv_obj_t *entry(lv_obj_t *list, int gap = sp::s3) {
  lv_obj_t *e = column(list, gap);
  lv_obj_set_style_pad_ver(e, sp::s5, 0);
  lv_obj_set_style_border_side(e, LV_BORDER_SIDE_BOTTOM, 0);
  lv_obj_set_style_border_width(e, stroke::hair, 0);
  lv_obj_set_style_border_color(e, T->line, 0);
  return e;
}
lv_obj_t *page(lv_obj_t *parent) {
  lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_t *l = column(parent, 0, px(W));
  lv_obj_set_style_pad_hor(l, PAD, 0);
  return l;
}
// the style's line box drawn as hairlines at its top and bottom, the text inside (caps centred by fw::text)
lv_obj_t *boxed(lv_obj_t *parent, ty::Id s, const char *txt, lv_color_t c) {
  lv_obj_t *b = box(parent);
  lv_obj_set_size(b, LV_SIZE_CONTENT, style(s).box);
  lv_obj_add_flag(b, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
  lv_obj_set_style_border_side(b, (lv_border_side_t) (LV_BORDER_SIDE_TOP | LV_BORDER_SIDE_BOTTOM), 0);
  lv_obj_set_style_border_width(b, stroke::hair, 0);
  lv_obj_set_style_border_color(b, T->line, 0);
  lv_obj_set_style_border_post(b, true, 0);
  lv_obj_t *l = text(b, s, txt, c);
  lv_label_set_long_mode(l, LV_LABEL_LONG_CLIP);
  return l;
}
void spec_line(lv_obj_t *parent, ty::Id s) {
  const ty::Style &st = style(s);
  char b[160];
  snprintf(b, sizeof b, "%d/%d · %s · box %d%s%s%s", st.size, st.weight, st.mono ? "JetBrains Mono" : "Inter", st.box,
           st.letter_space ? " · ls " : "", st.letter_space ? std::to_string(st.letter_space).c_str() : "",
           st.upper ? " · caps" : (st.tabular ? " · tabular" : ""));
  text(parent, ty::c_meta, b, T->text_dim);
}
void head(lv_obj_t *parent, const char *name, ty::Id spec) {
  lv_obj_t *r = row(parent, sp::s5);
  lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  text(r, ty::c_caps_sm, name, T->text);
  spec_line(r, spec);
}

void scale(lv_obj_t *l, const char *sc, const char *sample, bool accent) {
  for (int i = 0; i < ty::N; i++) {
    ty::Id s = (ty::Id) i;
    if (strcmp(style(s).scale, sc)) continue;
    lv_obj_t *e = entry(l);
    head(e, style(s).name, s);
    boxed(e, s, *sample ? sample : style(s).sample, accent ? T->accent_text : T->text);
    text(e, ty::c_meta, style(s).use, T->text_mute, px(W) - 2 * PAD);
  }
}

// mono caps beside sans: the same words both ways, then the two together as a card reads
void pair(lv_obj_t *l, bool accent) {
  lv_obj_t *e = entry(l, sp::s5);
  text(e, ty::c_caps_sm, "Same words, both families", T->text_dim);
  lv_obj_t *r = row(e, sp::s9);
  boxed(r, ty::label, "Living room", T->text_dim);
  boxed(r, ty::title, "Living room", T->text);
  e = entry(l, sp::s5);
  text(e, ty::c_caps_sm, "Together: label, value, line", T->text_dim);
  lv_obj_t *card = column(e, 0, px(220));
  lv_obj_set_style_bg_color(card, T->surface, 0);
  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(card, rad::card, 0);
  lv_obj_set_style_pad_all(card, lay::card_pad, 0);
  text(card, ty::label, "Weather", T->text_dim);
  text(card, ty::value, "18°", accent ? T->accent_text : T->text);
  text(card, ty::body, "Dry for the next hour", T->text_mute);
}

void numbers(lv_obj_t *l, bool accent) {
  for (int i = 0; i < ty::N_NUMBERS; i++) {
    const ty::Number &n = ty::NUMBERS[i];
    lv_obj_t *e = entry(l);
    head(e, n.label, n.style);
    lv_obj_t *v = column(e, 0, LV_SIZE_CONTENT);
    boxed(v, n.style, n.sample, accent ? T->accent_text : T->text);
    if (n.unit_style >= 0) boxed(v, (ty::Id) n.unit_style, n.unit, T->text_mute);
    text(e, ty::c_meta, n.about, T->text_mute, px(W) - 2 * PAD);
  }
  // tabular digits: the same width whatever the digits
  lv_obj_t *e = entry(l);
  text(e, ty::c_caps_sm, "Tabular digits", T->text);
  for (const char *s : {"11:11", "88:88", "10:47"}) {
    lv_obj_t *r = row(e, sp::s7);
    lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER);
    lv_obj_t *t = boxed(r, ty::value, s, T->text);
    lv_obj_update_layout(t);
    char b[32];
    snprintf(b, sizeof b, "%d px wide", (int) lv_obj_get_width(t));
    text(r, ty::c_meta, b, T->text_dim);
  }
}

void utf8(std::string &o, uint32_t c) {
  if (c < 0x80) o += (char) c;
  else if (c < 0x800) o += (char) (0xC0 | c >> 6), o += (char) (0x80 | (c & 0x3F));
  else o += (char) (0xE0 | c >> 12), o += (char) (0x80 | (c >> 6 & 0x3F)), o += (char) (0x80 | (c & 0x3F));
}
// every glyph set rendered in the font of the first style that uses it, with the fonts that carry it
void glyphs(lv_obj_t *l) {
  for (int g = 0; g < ty::N_GLYPHSETS; g++) {
    int first = -1;
    std::string users;
    for (int i = 0; i < ty::N; i++)
      if (!strcmp(ty::STYLES[i].glyphs, ty::GLYPHSETS[g].id)) {
        if (first < 0) first = i;
        users += users.empty() ? "" : ", ";
        users += ty::STYLES[i].name;
      }
    if (first < 0) continue;
    const int fi = style_font_index((ty::Id) first);
    const FontRef f = font_ref(fi);
    lv_obj_t *e = entry(l);
    char b[200];
    snprintf(b, sizeof b, "%s · %d glyphs", ty::GLYPHSETS[g].id, f.glyphs);
    text(e, ty::c_caps_sm, b, T->text);
    text(e, ty::c_meta, ty::GLYPHSETS[g].about, T->text_dim, px(W) - 2 * PAD);
    // separate the glyphs with spaces only if the font has one (digits has none: a space would draw a missing-glyph box)
    lv_font_glyph_dsc_t sp_dsc;
    const bool has_space = lv_font_get_glyph_dsc(f.font, &sp_dsc, ' ', 0) && !sp_dsc.is_placeholder;
    std::string all;
    for (int k = 0; k < f.glyphs; k++) {
      uint32_t c = font_glyph(fi, k);
      if (c > ' ') { utf8(all, c); if (has_space) all += ' '; }
    }
    lv_obj_t *t = lv_label_create(e);
    if (!has_space) lv_obj_set_style_text_letter_space(t, sp::s4, 0);
    lv_label_set_long_mode(t, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(t, px(W) - 2 * PAD);
    lv_obj_set_style_text_font(t, f.font, 0);
    lv_obj_set_style_text_color(t, T->text, 0);
    lv_label_set_text(t, all.c_str());
    snprintf(b, sizeof b, "shown in %s; styles: %s", f.id, users.c_str());
    text(e, ty::c_meta, b, T->text_mute, px(W) - 2 * PAD);
  }
}

void fonts(lv_obj_t *l) {
  char b[200];
  lv_obj_t *e = entry(l);
  snprintf(b, sizeof b, "%s build: %d fonts, %d KB flash", D > 1 ? "720 px (1.5x)" : "480 px (1x)", n_fonts(),
           font_bytes() / 1024);
  text(e, ty::c_caps_sm, b, T->text);
  for (int i = 0; i < n_fonts(); i++) {
    const FontRef f = font_ref(i);
    std::string users;
    for (int s = 0; s < ty::N; s++)
      if (style_font_index((ty::Id) s) == i) users += (users.empty() ? "" : ", "), users += ty::STYLES[s].name;
    lv_obj_t *r = entry(l, sp::s1);
    snprintf(b, sizeof b, "%s · %s %d %d px%s · %d glyphs · %.1f KB", f.id, f.family, f.weight, f.size,
             f.tabular ? " tabular" : "", f.glyphs, f.bytes / 1024.0);
    text(r, ty::c_meta, b, T->text);
    text(r, ty::c_meta, users.c_str(), T->text_mute, px(W) - 2 * PAD);
  }
}
}  // namespace

FW_COMPONENT(type, "Type", "foundations", R"({
  "about": "The type scale: every text style at true size, the two font families and the glyphs each panel size ships.",
  "layout": "doc",
  "frame": [480, 0],
  "props": [{"name": "section", "about": "Which part of the type page to show, such as the screen styles or the glyphs each panel carries.", "type": "select", "options": )" FW_TYPE_SECTION_IDS R"(, "default": "screen"},
            {"name": "sample", "about": "Your own words to show in every style. Leave empty for the usual sample text.", "type": "text", "default": ""},
            {"name": "accent", "about": "Draws the samples in the accent colour.", "type": "bool", "default": false}],
  "variants": )" FW_TYPE_SECTIONS R"(
})") {
  const char *s = p["section"] | "screen";
  const char *sample = p["sample"] | "";
  const bool accent = p["accent"] | false;
  lv_obj_t *l = page(parent);
  if (!strcmp(s, "pair")) pair(l, accent);
  else if (!strcmp(s, "numbers")) numbers(l, accent);
  else if (!strcmp(s, "glyphs")) glyphs(l);
  else if (!strcmp(s, "fonts")) fonts(l);
  else scale(l, s, sample, accent);
}
