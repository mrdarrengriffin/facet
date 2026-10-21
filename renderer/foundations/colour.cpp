// Foundations > Colour: every colour token (tokens/colour.json via the generated colour_gen.h), drawn by the panel
// renderer itself (RGB565, as the panel shows it). One section per group: name + role in the page theme, then the
// colour in its own dark and light context side by side with its hex. Plus the accent sets and contrast pairs.
// Status (WIP/Done) = this file + tokens/colour.json in git.
#include <cstdio>
#include "fw.h"

namespace {
using namespace fw::tokens;

constexpr float W = 480, CELL = 132, ROW = 52;

int role_of(const char *name) {
  for (int r = 0; r < N_ROLES; r++)
    if (!strcmp(ROLES[r], name)) return r;
  return -1;
}
int index_of(const char *name) {
  for (int i = 0; i < N_COLOURS; i++)
    if (!strcmp(COLOURS[i].name, name)) return i;
  return -1;
}
// exact token value (accent roles from the selected set)
uint32_t value(int i, bool light, int set) {
  int r = role_of(COLOURS[i].name);
  if (r >= 0) return light ? ACCENTS[set].light[r] : ACCENTS[set].dark[r];
  return light ? COLOURS[i].light : COLOURS[i].dark;
}
float lum(uint32_t c) {
  auto ch = [](uint32_t v) { float s = v / 255.0f; return s <= 0.03928f ? s / 12.92f : std::pow((s + 0.055f) / 1.055f, 2.4f); };
  return 0.2126f * ch(c >> 16 & 255) + 0.7152f * ch(c >> 8 & 255) + 0.0722f * ch(c & 255);
}
float contrast(uint32_t a, uint32_t b) {
  float x = lum(a), y = lum(b);
  return (std::fmax(x, y) + 0.05f) / (std::fmin(x, y) + 0.05f);
}

lv_obj_t *box(lv_obj_t *parent) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
  return o;
}
lv_obj_t *label(lv_obj_t *parent, const char *text, float dp, lv_color_t c, bool mono = false) {
  lv_obj_t *l = lv_label_create(parent);
  lv_label_set_text(l, text);
  lv_obj_set_style_text_font(l, fw::font(dp, mono), 0);
  lv_obj_set_style_text_color(l, c, 0);
  return l;
}
lv_obj_t *row(lv_obj_t *list) {
  lv_obj_t *r = box(list);
  lv_obj_set_size(r, LV_PCT(100), fw::px(ROW));
  lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_border_side(r, LV_BORDER_SIDE_BOTTOM, 0);
  lv_obj_set_style_border_width(r, 1, 0);
  lv_obj_set_style_border_color(r, fw::T->line, 0);
  return r;
}
// left column: two lines (title + sub) in the page theme
void info(lv_obj_t *r, const char *title, const char *sub, bool mono_title) {
  lv_obj_t *c = box(r);
  lv_obj_set_height(c, LV_SIZE_CONTENT);
  lv_obj_set_flex_grow(c, 1);
  lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_right(c, fw::px(8), 0);
  lv_obj_set_style_pad_row(c, fw::px(2), 0);
  lv_obj_t *t = label(c, title, mono_title ? 13 : 16, fw::T->text, mono_title);
  lv_obj_set_width(t, LV_PCT(100));
  lv_label_set_long_mode(t, LV_LABEL_LONG_DOT);
  if (sub && *sub) {
    lv_obj_t *s = label(c, sub, 12, fw::T->text_dim);
    lv_obj_set_width(s, LV_PCT(100));
    lv_label_set_long_mode(s, LV_LABEL_LONG_DOT);
  }
}
// a theme cell: that theme's canvas, content laid out in a row
lv_obj_t *cell(lv_obj_t *r, const fw::Theme *th) {
  lv_obj_t *c = box(r);
  lv_obj_set_size(c, fw::px(CELL), LV_PCT(100));
  lv_obj_set_style_bg_color(c, th->bg, 0);
  lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
  lv_obj_set_flex_flow(c, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(c, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_hor(c, fw::px(10), 0);
  lv_obj_set_style_pad_column(c, fw::px(8), 0);
  return c;
}
lv_obj_t *chip(lv_obj_t *parent, uint32_t v, const fw::Theme *th, float w = 32, float h = 32) {
  lv_obj_t *s = box(parent);
  lv_obj_set_size(s, fw::px(w), fw::px(h));
  lv_obj_set_style_radius(s, fw::px(8), 0);
  lv_obj_set_style_bg_color(s, lv_color_hex(v), 0);
  lv_obj_set_style_bg_opa(s, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(s, 1, 0);
  lv_obj_set_style_border_color(s, th->line, 0);
  return s;
}
void hex(lv_obj_t *parent, uint32_t v, const fw::Theme *th) {
  char b[8];
  snprintf(b, sizeof b, "%06X", (unsigned) v);
  label(parent, b, 12, th->text_dim, true);
}
lv_obj_t *list(lv_obj_t *parent) {
  lv_obj_t *l = box(parent);
  lv_obj_set_size(l, fw::px(W), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(l, LV_FLEX_FLOW_COLUMN);
  // column header: Dark | Light over the theme cells
  lv_obj_t *r = row(l);
  lv_obj_set_height(r, fw::px(28));
  info(r, "", nullptr, false);
  for (int light = 0; light < 2; light++) {
    lv_obj_t *c = cell(r, fw::theme(light, fw::T->accent_set));
    label(c, light ? "Light" : "Dark", 12, fw::theme(light, fw::T->accent_set)->text_dim);
  }
  return l;
}

void group(lv_obj_t *l, const char *g) {
  const int set = fw::T->accent_set;
  for (int i = 0; i < N_COLOURS; i++) {
    if (strcmp(COLOURS[i].group, g)) continue;
    lv_obj_t *r = row(l);
    info(r, COLOURS[i].name, COLOURS[i].role, true);
    for (int light = 0; light < 2; light++) {
      const fw::Theme *th = fw::theme(light, set);
      lv_obj_t *c = cell(r, th);
      chip(c, value(i, light, set), th);
      hex(c, value(i, light, set), th);
    }
  }
}

void accents(lv_obj_t *l) {
  for (int a = 0; a < N_ACCENTS; a++) {
    lv_obj_t *r = row(l);
    info(r, ACCENTS[a].label, a == fw::T->accent_set ? "selected" : (a == ACCENT_DEFAULT ? "default" : ""), false);
    for (int light = 0; light < 2; light++) {
      const fw::Theme *th = fw::theme(light, a);
      lv_obj_t *c = cell(r, th);
      // the set in use: a fill with its label, accent as text, the dim tone
      lv_obj_t *pill = box(c);
      lv_obj_set_size(pill, fw::px(44), fw::px(28));
      lv_obj_set_style_radius(pill, fw::px(14), 0);
      lv_obj_set_style_bg_color(pill, th->accent, 0);
      lv_obj_set_style_bg_opa(pill, LV_OPA_COVER, 0);
      lv_obj_center(label(pill, "On", 13, th->on_accent));
      label(c, "Aa", 16, th->accent_text);
      chip(c, light ? ACCENTS[a].light[2] : ACCENTS[a].dark[2], th, 20, 20);
    }
    if (a == fw::T->accent_set) lv_obj_set_style_bg_color(r, fw::T->surface, 0), lv_obj_set_style_bg_opa(r, LV_OPA_COVER, 0);
  }
}

void pairs(lv_obj_t *l) {
  const int set = fw::T->accent_set;
  for (int p = 0; p < N_CONTRAST; p++) {
    const Pair &cp = CONTRAST[p];
    int fi = index_of(cp.fg), bi = index_of(cp.bg);
    if (fi < 0 || bi < 0) continue;
    char sub[96];
    snprintf(sub, sizeof sub, "%s on %s, min %.1f", cp.fg, cp.bg, (double) cp.min);
    lv_obj_t *r = row(l);
    info(r, cp.label, sub, false);
    for (int light = 0; light < 2; light++) {
      const fw::Theme *th = fw::theme(light, set);
      uint32_t fg = value(fi, light, set), bg = value(bi, light, set);
      lv_obj_t *c = cell(r, th);
      lv_obj_t *s = chip(c, bg, th, 44, 32);
      lv_obj_center(label(s, "Aa", 16, lv_color_hex(fg)));
      float k = contrast(fg, bg);
      char b[16];
      snprintf(b, sizeof b, k >= 10 ? "%.0f:1" : "%.1f:1", (double) k);
      label(c, b, 13, k >= cp.min ? th->text : th->error, true);
    }
  }
}
}  // namespace

FW_COMPONENT(colour, "Colour", "foundations", R"({
  "layout": "doc",
  "frame": [480, 0],
  "props": [{"name": "group", "type": "select", "options": )" FW_COLOUR_GROUP_IDS R"(, "default": "surfaces"}],
  "variants": )" FW_COLOUR_SECTIONS R"(
})") {
  const char *g = p["group"] | "surfaces";
  lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_t *l = list(parent);
  if (!strcmp(g, "accents")) accents(l);
  else if (!strcmp(g, "contrast")) pairs(l);
  else group(l, g);
}
