// Foundations > Colour: every colour token (tokens/colour.json via the generated colour_gen.h), drawn by the panel
// renderer itself (RGB565, as the panel shows it). One section per group: name + role in the page theme, then the
// colour in its own dark and light context side by side with its hex. Plus the accent sets and the contrast matrix
// (every declared pair x theme x accent set, tools/contrast.mjs is the same test on the command line).
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
// a colour token or a mix token (tokens MIXES: tint over a base at an opacity, = lv_color_mix) by name
uint32_t value_named(const char *name, bool light, int set) {
  const int i = index_of(name);
  if (i >= 0) return value(i, light, set);
  for (int m = 0; m < N_MIXES; m++)
    if (!strcmp(MIXES[m].name, name)) {
      auto mix = [](uint32_t a, uint32_t b, uint32_t o) {  // = lv_color_mix(a, b, o)
        uint32_t r = 0;
        for (int sh = 16; sh >= 0; sh -= 8) r |= ((((a >> sh & 255) * o + (b >> sh & 255) * (255 - o)) * 0x8081u) >> 23) << sh;
        return r;
      };
      uint32_t tint = value_named(MIXES[m].tint, light, set);
      if (MIXES[m].toward[0]) tint = mix(value_named(MIXES[m].toward, light, set), tint, MIXES[m].desat);
      return mix(tint, value_named(MIXES[m].over, light, set), MIXES[m].opa);
    }
  return 0;
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
  // grows with a wrapped title (2+ lines); the theme cells are LV_PCT(100) high, which LVGL leaves out of the
  // content height, so they stretch to whatever the title column needs
  lv_obj_set_size(r, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_style_min_height(r, fw::px(ROW), 0);
  lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_border_side(r, LV_BORDER_SIDE_BOTTOM, 0);
  lv_obj_set_style_border_width(r, fw::stroke::hair, 0);
  lv_obj_set_style_border_color(r, fw::T->line, 0);
  return r;
}
// left column: two lines (title + sub) in the page theme
void info(lv_obj_t *r, const char *title, const char *sub, bool mono_title) {
  lv_obj_t *c = box(r);
  lv_obj_set_height(c, LV_SIZE_CONTENT);
  lv_obj_set_flex_grow(c, 1);
  lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_left(c, fw::sp::s5, 0);  // same inset as the theme cells' content
  lv_obj_set_style_pad_ver(c, fw::sp::s2, 0);  // wrapped titles keep clear of the row lines
  lv_obj_set_style_pad_right(c, fw::sp::s3, 0);
  lv_obj_set_style_pad_row(c, fw::sp::s1, 0);
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
  lv_obj_set_style_pad_hor(c, fw::sp::s5, 0);
  lv_obj_set_style_pad_column(c, fw::sp::s3, 0);
  return c;
}
lv_obj_t *chip(lv_obj_t *parent, uint32_t v, const fw::Theme *th, float w = 32, float h = 32) {
  lv_obj_t *s = box(parent);
  lv_obj_set_size(s, fw::px(w), fw::px(h));
  lv_obj_set_style_radius(s, fw::rad::button, 0);
  lv_obj_set_style_bg_color(s, lv_color_hex(v), 0);
  lv_obj_set_style_bg_opa(s, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(s, fw::stroke::hair, 0);
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
      lv_obj_set_style_radius(pill, LV_RADIUS_CIRCLE, 0);  // rad::round
      lv_obj_set_style_bg_color(pill, th->accent, 0);
      lv_obj_set_style_bg_opa(pill, LV_OPA_COVER, 0);
      lv_obj_center(label(pill, "On", 13, th->on_accent));
      label(c, "Aa", 16, th->accent_text);
      chip(c, light ? ACCENTS[a].light[2] : ACCENTS[a].dark[2], th, 20, 20);
    }
    if (a == fw::T->accent_set) lv_obj_set_style_bg_color(r, fw::T->surface, 0), lv_obj_set_style_bg_opa(r, LV_OPA_COVER, 0);
  }
}

// ---- contrast (tokens/colour.json contrast.pairs, same maths as tools/contrast.mjs): the worse of the exact
// token ratio and the RGB565 ratio the panel shows decides
uint32_t q565(uint32_t c) {
  uint32_t r = c >> 19 & 31, g = c >> 10 & 63, b = c >> 3 & 31;
  return (r << 3 | r >> 2) << 16 | (g << 2 | g >> 4) << 8 | (b << 3 | b >> 2);
}
float worst(uint32_t fg, uint32_t bg) { return std::fmin(contrast(fg, bg), contrast(q565(fg), q565(bg))); }
struct Res { uint32_t fg, bg; float k; bool pass; };
Res test(const Pair &cp, bool light, int set) {
  Res r{value_named(cp.fg, light, set), value_named(cp.bg, light, set), 0, false};
  r.k = worst(r.fg, r.bg);
  r.pass = r.k >= cp.min - 0.0005f;
  return r;
}
void ratio_text(char *b, size_t n, float k) { snprintf(b, n, k >= 10 ? "%.0f:1" : "%.2f:1", (double) k); }

// one theme cell: the pair drawn in that theme (text kinds = "Aa", graphics = a 2-stroke ring) + ratio / required
void sample(lv_obj_t *r, const Pair &cp, bool light, int set) {
  const fw::Theme *th = fw::theme(light, set);
  const Res res = test(cp, light, set);
  lv_obj_t *c = cell(r, th);
  lv_obj_t *s = chip(c, res.bg, th, 44, 32);
  if (strcmp(KINDS[cp.kind].id, "ui") && strcmp(KINDS[cp.kind].id, "decor")) {
    lv_obj_center(label(s, "Aa", 16, lv_color_hex(res.fg)));
  } else {
    lv_obj_t *o = box(s);
    lv_obj_set_size(o, fw::px(18), fw::px(18));
    lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(o, fw::stroke::edge, 0);
    lv_obj_set_style_border_color(o, lv_color_hex(res.fg), 0);
    lv_obj_center(o);
  }
  lv_obj_t *col = box(c);
  lv_obj_set_size(col, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
  char b[24], m[24];
  ratio_text(b, sizeof b, res.k);
  label(col, b, 13, res.pass ? th->text : th->error, true);
  snprintf(m, sizeof m, res.pass ? "min %.1f" : "FAIL %.1f", (double) cp.min);
  label(col, m, 11, res.pass ? th->text_dim : th->error, true);
}

void pair_rows(lv_obj_t *l, const char *section) {
  const int set = fw::T->accent_set;
  for (int p = 0; p < N_CONTRAST; p++) {
    const Pair &cp = CONTRAST[p];
    if (strcmp(cp.section, section)) continue;
    char sub[96];
    snprintf(sub, sizeof sub, "%s on %s · %s", cp.fg, cp.bg, KINDS[cp.kind].label);
    lv_obj_t *r = row(l);
    info(r, cp.use, sub, false);
    const bool bad = !test(cp, false, set).pass || !test(cp, true, set).pass;
    if (bad) lv_obj_set_style_bg_color(r, fw::T->surface, 0), lv_obj_set_style_bg_opa(r, LV_OPA_COVER, 0);
    for (int light = 0; light < 2; light++) sample(r, cp, light, set);
  }
}

// every theme x accent set: passes / tested
void summary(lv_obj_t *l) {
  for (int a = 0; a < N_ACCENTS; a++) {
    lv_obj_t *r = row(l);
    char sub[48];
    snprintf(sub, sizeof sub, "%d pairs per theme", N_CONTRAST);
    info(r, ACCENTS[a].label, a == fw::T->accent_set ? "selected" : sub, false);
    for (int light = 0; light < 2; light++) {
      const fw::Theme *th = fw::theme(light, a);
      int ok = 0;
      for (int p = 0; p < N_CONTRAST; p++) ok += test(CONTRAST[p], light, a).pass;
      lv_obj_t *c = cell(r, th);
      char b[24];
      snprintf(b, sizeof b, "%d/%d", ok, N_CONTRAST);
      label(c, b, 13, ok == N_CONTRAST ? th->text : th->error, true);
      label(c, ok == N_CONTRAST ? "pass" : "FAIL", 11, ok == N_CONTRAST ? th->ok : th->error, true);
    }
    if (a == fw::T->accent_set) lv_obj_set_style_bg_color(r, fw::T->surface, 0), lv_obj_set_style_bg_opa(r, LV_OPA_COVER, 0);
  }
}

// every failing combination, all accents and themes (the sample cell sits in its own theme column)
void failures(lv_obj_t *l) {
  int n = 0;
  for (int a = 0; a < N_ACCENTS; a++)
    for (int light = 0; light < 2; light++)
      for (int p = 0; p < N_CONTRAST; p++) {
        const Pair &cp = CONTRAST[p];
        if (test(cp, light, a).pass) continue;
        n++;
        char sub[112];
        snprintf(sub, sizeof sub, "%s %s · %s on %s · %s", ACCENTS[a].label, light ? "light" : "dark", cp.fg, cp.bg, KINDS[cp.kind].label);
        lv_obj_t *r = row(l);
        info(r, cp.use, sub, false);
        for (int t = 0; t < 2; t++) {
          if (t == light) sample(r, cp, light, a);
          else cell(r, fw::theme(t, a));
        }
      }
  if (!n) {
    lv_obj_t *r = row(l);
    char b[64];
    snprintf(b, sizeof b, "%d combinations, all pass", N_CONTRAST * 2 * N_ACCENTS);
    info(r, "No failures", b, false);
  }
}
}  // namespace

FW_COMPONENT(colour, "Colour", "foundations", R"({
  "about": "Every colour token in dark and light, the accent sets, and the contrast checks they all pass.",
  "layout": "doc",
  "frame": [480, 0],
  "props": [{"name": "group", "type": "select", "options": )" FW_COLOUR_GROUP_IDS R"(, "default": "surfaces"}],
  "variants": )" FW_COLOUR_SECTIONS R"(
})") {
  const char *g = p["group"] | "surfaces";
  lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_t *l = list(parent);
  if (!strcmp(g, "accents")) accents(l);
  else if (!strcmp(g, "contrast")) summary(l);
  else if (!strcmp(g, "contrast-fail")) failures(l);
  else if (!strncmp(g, "contrast-", 9)) pair_rows(l, g + 9);
  else group(l, g);
}
