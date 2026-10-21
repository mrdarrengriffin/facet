// Foundations > Spacing & radius: tokens/space.json (via the generated space_gen.h) drawn by the panel renderer at
// true size. Sections: the spacing scale (true-size step + bar), the screen layout tokens, radius samples on surface
// cards, stroke widths (line + ring) and touch targets (the hit area shown around a smaller drawn control). The whole
// screen (grid, inset, header, content) is on Layout & devices (foundations/layout.cpp). Values are this build's
// (device switch = 720 or 480 build); every row also shows the other density. Status (WIP/Done) = this file + tokens/space.json in git.
#include <cstdio>
#include "fw.h"

namespace {
using namespace fw;

constexpr float W = 480;  // doc page width in dp (= the screen width)

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
  lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(r, gap, 0);
  return r;
}
void fill(lv_obj_t *o, lv_color_t c, lv_opa_t opa = LV_OPA_COVER) {
  lv_obj_set_style_bg_color(o, c, 0);
  lv_obj_set_style_bg_opa(o, opa, 0);
}
void outline(lv_obj_t *o, lv_color_t c, int w) {
  lv_obj_set_style_border_width(o, w, 0);
  lv_obj_set_style_border_color(o, c, 0);
}
lv_obj_t *entry(lv_obj_t *list, int gap = sp::s2) {
  lv_obj_t *e = column(list, gap);
  lv_obj_set_style_pad_ver(e, sp::s5, 0);
  lv_obj_set_style_border_side(e, LV_BORDER_SIDE_BOTTOM, 0);
  outline(e, T->line, stroke::hair);
  return e;
}
int inner_w() { return px(W) - 2 * sp::s7; }
lv_obj_t *page(lv_obj_t *parent) {
  lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_t *l = column(parent, 0, px(W));
  lv_obj_set_style_pad_hor(l, sp::s7, 0);
  return l;
}
bool is1x() { return D < 1.2f; }
// "16 px · 11 at 480" (this build's value first, the other density after)
std::string val(const spt::Token &k) {
  char b[64];
  const int here = is1x() ? k.v1x : k.v, other = is1x() ? k.v : k.v1x;
  if (here >= 999) snprintf(b, sizeof b, "full");
  else snprintf(b, sizeof b, "%d px · %d at %s", here, other, is1x() ? "720" : "480");
  return b;
}
int here(const spt::Token &k) { return is1x() ? k.v1x : k.v; }
// name + value on one line, the use under it
void head(lv_obj_t *e, const char *ns, const spt::Token &k) {
  lv_obj_t *r = row(e, sp::s5);
  char b[64];
  snprintf(b, sizeof b, "%s::%s", ns, k.name);
  for (char *c = b; *c; c++) if (*c == '-') *c = '_';
  text(r, ty::c_caps_sm, b, T->text);
  text(r, ty::c_meta, val(k).c_str(), T->text_dim);
  if (k.use && *k.use) text(e, ty::c_meta, k.use, T->text_mute, inner_w());
}

// spacing: the step at true size (a square of the step) beside a bar 8x as long, so small steps still read
void space(lv_obj_t *l) {
  const int lab = px(64);
  for (int i = 0; i < spt::N_TOKENS; i++) {
    const spt::Token &k = spt::TOKENS[i];
    if (strcmp(k.group, "space")) continue;
    lv_obj_t *e = entry(l, sp::s1);
    lv_obj_t *r = row(e, sp::s5);
    char b[32];
    snprintf(b, sizeof b, "sp::s%s", k.name);
    lv_obj_t *n = text(r, ty::c_caps_sm, b, T->text);
    lv_obj_set_width(n, lab);
    lv_obj_t *sq = box(r);
    lv_obj_set_size(sq, here(k), here(k));
    fill(sq, T->accent);
    lv_obj_t *bar = box(r);
    lv_obj_set_size(bar, std::min(here(k) * 8, inner_w() - 2 * lab), sp::s2);
    fill(bar, T->accent_dim);
    text(r, ty::c_meta, val(k).c_str(), T->text_dim);
  }
  lv_obj_t *e = entry(l);
  text(e, ty::c_meta, "Square = the step at true size; bar = 8 x the step.", T->text_mute, inner_w());
}

// a group's tokens with a true-size bar each (layout)
void layout(lv_obj_t *l) {
  for (int i = 0; i < spt::N_TOKENS; i++) {
    const spt::Token &k = spt::TOKENS[i];
    if (strcmp(k.group, "layout")) continue;
    lv_obj_t *e = entry(l);
    head(e, "lay", k);
    lv_obj_t *bar = box(e);
    lv_obj_set_size(bar, std::min(here(k), inner_w()), sp::s2);
    fill(bar, T->accent_dim);
  }
  lv_obj_t *e = entry(l);
  char b[96];
  snprintf(b, sizeof b, "grid %d px (%d at %s)", sp::GRID, is1x() ? spt::GRID_REF : spt::GRID_1X, is1x() ? "720" : "480");
  text(e, ty::c_caps_sm, b, T->text);
  text(e, ty::c_meta, spt::GRID_ABOUT, T->text_mute, inner_w());
}

// radius: one surface card per token, the value on it
void radius(lv_obj_t *l) {
  lv_obj_t *e = entry(l, sp::s5);
  lv_obj_t *w = row(e, lay::gutter);
  lv_obj_set_flex_flow(w, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_flex_align(w, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_set_style_pad_row(w, sp::s7, 0);
  const int cw = (inner_w() - 3 * lay::gutter) / 4;
  for (int i = 0; i < spt::N_TOKENS; i++) {
    const spt::Token &k = spt::TOKENS[i];
    if (strcmp(k.group, "radius")) continue;
    lv_obj_t *c = column(w, sp::s2, cw);
    lv_obj_t *card = box(c);
    lv_obj_set_size(card, cw, here(k) >= 999 ? cw / 2 : cw);
    fill(card, T->accent_dim);  // accent tint + accent hairline: the corner reads in both themes
    outline(card, T->accent, stroke::hair);
    lv_obj_set_style_radius(card, here(k) >= 999 ? LV_RADIUS_CIRCLE : here(k), 0);
    char b[32];
    snprintf(b, sizeof b, "rad::%s", k.name);
    text(c, ty::c_caps_sm, b, T->text);
    text(c, ty::c_meta, val(k).c_str(), T->text_dim);
    text(c, ty::c_meta_sm, k.use, T->text_mute, cw);
  }
}

// strokes: a line and a ring at each width
void strokes(lv_obj_t *l) {
  for (int i = 0; i < spt::N_TOKENS; i++) {
    const spt::Token &k = spt::TOKENS[i];
    if (strcmp(k.group, "stroke")) continue;
    lv_obj_t *e = entry(l);
    head(e, "stroke", k);
    lv_obj_t *r = row(e, sp::s7);
    lv_obj_t *line = box(r);
    lv_obj_set_size(line, px(160), here(k));
    fill(line, T->text);
    lv_obj_t *ring = box(r);
    lv_obj_set_size(ring, px(32), px(32));
    lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
    outline(ring, T->text, here(k));
    lv_obj_t *edge = box(r);
    lv_obj_set_size(edge, px(64), px(32));
    lv_obj_set_style_radius(edge, rad::button, 0);
    fill(edge, T->surface_press);
    outline(edge, T->text_dim, here(k));
  }
}

// a hit area (tinted, hairline in accent) of w x h with the drawn control centred in it
lv_obj_t *hit(lv_obj_t *parent, int w, int h) {
  lv_obj_t *a = box(parent);
  lv_obj_set_size(a, w, h);
  fill(a, T->accent_dim);  // hit area: accent tint + accent hairline (reads in both themes)
  outline(a, T->accent, stroke::hair);
  lv_obj_set_flex_flow(a, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(a, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  return a;
}
void sample(lv_obj_t *l, const char *name, const char *about) {
  lv_obj_t *e = lv_obj_get_child(l, -1);
  text(e, ty::c_caps_sm, name, T->text);
  text(e, ty::c_meta, about, T->text_mute, inner_w());
}
void touch_page(lv_obj_t *l) {
  for (int i = 0; i < spt::N_TOKENS; i++) {
    const spt::Token &k = spt::TOKENS[i];
    if (strcmp(k.group, "touch")) continue;
    head(entry(l, sp::s1), "touch", k);
  }
  char b[160];
  // back button: drawn back x back, hit area touch::target
  lv_obj_t *e = entry(l, sp::s3);
  lv_obj_t *a = hit(e, touch::target, touch::target);
  lv_obj_t *btn = box(a);
  lv_obj_set_size(btn, touch::back, touch::back);
  fill(btn, T->surface);
  outline(btn, T->text_dim, stroke::hair);
  lv_obj_set_style_radius(btn, rad::square, 0);
  lv_obj_t *ic = box(btn);
  lv_obj_set_size(ic, touch::icon, touch::icon);
  lv_obj_center(ic);
  outline(ic, T->text_dim, stroke::hair);
  snprintf(b, sizeof b, "Drawn %d x %d, icon %d; hit area %d x %d (touch::target).", touch::back, touch::back, touch::icon,
           touch::target, touch::target);
  sample(l, "Back button", b);
  // a small chip: drawn short, hit area grown to touch::min on the short axis
  e = entry(l, sp::s3);
  a = hit(e, LV_SIZE_CONTENT, touch::min);
  lv_obj_set_style_pad_hor(a, sp::s2, 0);
  lv_obj_t *chip = box(a);
  lv_obj_set_size(chip, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_style_pad_hor(chip, sp::s3, 0);
  lv_obj_set_style_pad_ver(chip, sp::s1, 0);
  fill(chip, T->surface);
  outline(chip, T->text_dim, stroke::hair);
  lv_obj_set_style_radius(chip, rad::chip, 0);
  text(chip, ty::c_caps_sm, "Edit", T->text_dim);
  snprintf(b, sizeof b, "Drawn smaller than the minimum: the hit area grows to %d px (touch::min) on the short axis.", touch::min);
  sample(l, "Chip", b);
  // a list row: the whole row is the target
  e = entry(l, sp::s3);
  a = hit(e, inner_w(), touch::target);
  lv_obj_set_flex_align(a, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_hor(a, sp::s5, 0);
  text(a, ty::c_body, "Living room", T->text);
  text(a, ty::c_meta, "On", T->text_dim);
  snprintf(b, sizeof b, "A row is one target: full width, %d px tall, hairline between rows.", touch::target);
  sample(l, "List row", b);
}

}  // namespace

FW_COMPONENT(space, "Spacing & radius", "foundations", R"({
  "layout": "doc",
  "frame": [480, 0],
  "props": [{"name": "section", "type": "select", "options": )" FW_SPACE_SECTION_IDS R"(, "default": "space"}],
  "variants": )" FW_SPACE_SECTIONS R"(
})") {
  const char *s = p["section"] | "space";
  lv_obj_t *l = page(parent);
  if (!strcmp(s, "layout")) layout(l);
  else if (!strcmp(s, "radius")) radius(l);
  else if (!strcmp(s, "stroke")) strokes(l);
  else if (!strcmp(s, "touch")) touch_page(l);
  else space(l);
}
