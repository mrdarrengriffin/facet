// Foundations > Layout & devices: tokens/layout.json (via the generated layout_gen.h) drawn by the panel renderer at
// true size. Sections (labels + abouts from tools/gen_layout.mjs; the rule sections' text = layout.json rules.sections,
// plain words, no token names on the page): the screen layer by layer (grid, margin, header, content, gutters; chips
// toggle each layer), then one visual per rule (grid, two sizes, spacing steps, groups, screen frame, touch, nested
// corners, when the grid may break) built from real components with the grid on top, then sheets and toasts, devices
// and screen shapes. Values are this build's (device switch = 720 or 480 build), the other screen's in brackets.
// Status (WIP/Done) = this file + tokens/layout.json in git.
#include <cmath>
#include <cstdio>
#include "fw.h"

namespace {
using namespace fw;

constexpr float W = 480;  // doc page width in dp (= the screen width)

lv_obj_t *box(lv_obj_t *parent) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
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
void fill(lv_obj_t *o, lv_color_t c) {
  lv_obj_set_style_bg_color(o, c, 0);
  lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
}
void outline(lv_obj_t *o, lv_color_t c, int w) {
  lv_obj_set_style_border_width(o, w, 0);
  lv_obj_set_style_border_color(o, c, 0);
}
lv_obj_t *rect(lv_obj_t *parent, int x, int y, int w, int h) {
  lv_obj_t *o = box(parent);
  lv_obj_set_pos(o, x, y);
  lv_obj_set_size(o, w, h);
  return o;
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
const char *other_den() { return is1x() ? "720" : "480"; }
const lyt::SizeClass &square() {
  for (int i = 0; i < lyt::N_CLASSES; i++) if (!strcmp(lyt::CLASSES[i].id, "square")) return lyt::CLASSES[i];
  return lyt::CLASSES[0];
}
// the screen frame every sample starts from: S x S on bg, clipped
lv_obj_t *screen_box(lv_obj_t *parent) {
  lv_obj_t *s = box(parent);
  lv_obj_set_size(s, lay::screen_w, lay::screen_h);
  fill(s, T->bg);
  lv_obj_set_style_clip_corner(s, false, 0);
  return s;
}
void caption(lv_obj_t *e, const std::string &s);
// a sample header: back, the screen label, the time slot at the right edge (one row at y = inset)
void legend(lv_obj_t *l, lv_color_t c, bool filled, const char *name, int v, int v1x, const char *use);
// a plain key line: swatch, name, this build's value of a layout.json token (+ the other screen's), one sentence
void key(lv_obj_t *l, lv_color_t c, bool filled, const char *name, const char *token, const char *what);
void legend(lv_obj_t *l, lv_color_t c, bool filled, const char *name, int v, int v1x, const char *use) {
  lv_obj_t *e = entry(l, sp::s1);
  lv_obj_t *r = row(e, sp::s4);
  lv_obj_t *sw = box(r);
  lv_obj_set_size(sw, sp::s7, sp::s7);
  if (filled) fill(sw, c);
  else outline(sw, c, stroke::edge);
  char b[96];
  snprintf(b, sizeof b, "%s", name);
  text(r, ty::c_caps_sm, b, T->text);
  if (v >= 0) {
    if (v == v1x) snprintf(b, sizeof b, "%d px", v);
    else snprintf(b, sizeof b, "%d px · %d at %s", is1x() ? v1x : v, is1x() ? v : v1x, other_den());
    text(r, ty::c_meta, b, T->text_dim);
  }
  if (use && *use) text(e, ty::c_meta, use, T->text_dim, inner_w());
}
const lyt::Token *tok(const char *name) {
  for (int i = 0; i < lyt::N_TOKENS; i++) if (!strcmp(lyt::TOKENS[i].name, name)) return &lyt::TOKENS[i];
  return nullptr;
}
void key(lv_obj_t *l, lv_color_t c, bool filled, const char *name, const char *token, const char *what) {
  const lyt::Token *k = tok(token);
  legend(l, c, filled, name, k ? k->v : -1, k ? k->v1x : 0, what);
}
// ---- Real components (Darren, round 2): every example is built by the approved components' own build functions,
// called through the registry (fw::find + Component::build, as List row builds Toggle and Dial builds Button); a part
// that does not exist yet is a plain labelled outline box, never a look-alike.
using Doc = ArduinoJson::JsonDocument;
// a labelled outline box for a part that is not a component yet ("sheet: component coming")
lv_obj_t *coming(lv_obj_t *parent, const char *what, int w, int h) {
  lv_obj_t *o = box(parent);
  lv_obj_set_size(o, w, h);
  outline(o, T->line_hi, stroke::hair);
  char b[64];
  snprintf(b, sizeof b, "%s: component coming", what);
  lv_obj_t *t = text(o, ty::c_meta_sm, b, T->text_dim);
  lv_obj_align(t, LV_ALIGN_LEFT_MID, sp::s2, 0);
  return o;
}
// build component `id` into parent with props d; returns its root (the last child), or a labelled box if missing
lv_obj_t *comp(lv_obj_t *parent, const char *id, Doc &d) {
  const Component *c = find(id);
  if (!c) return coming(parent, id, px(96), px(32));
  c->build(parent, d.as<Props>());
  return lv_obj_get_child(parent, -1);
}
// the header: the real Header component (back button, the screen's label, the time slot), placed at (inset, inset);
// Home has no back button (docs). Before the Header existed: the Back button, else a ghost Button.
lv_obj_t *header(lv_obj_t *s, const char *label, bool back = true) {
  Doc d;
  d["label"] = label;
  d["back"] = back;
  d["time"] = "21:42";  // a fixed time on the page (the screens show the live clock)
  if (find("header")) {
    lv_obj_t *h = comp(s, "header", d);
    lv_obj_set_pos(h, lay::inset, lay::inset);
    return h;
  }
  lv_obj_t *h = rect(s, lay::inset, lay::inset, lay::content_w, lay::row);
  lv_obj_add_flag(h, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
  lv_obj_set_flex_flow(h, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(h, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(h, lay::gutter, 0);
  if (find("back_button")) comp(h, "back_button", d);
  else {
    d["kind"] = "ghost";
    d["icon"] = "back";
    comp(h, "button", d);
  }
  lv_obj_t *g = box(h);
  lv_obj_set_flex_grow(g, 1);
  lv_obj_t *t = text(h, ty::label, "21:42", T->text_dim);
  lv_obj_set_width(t, lay::header_time_w);
  lv_obj_set_style_text_align(t, LV_TEXT_ALIGN_RIGHT, 0);
  return h;
}
// Home: four real Cards, card-w x card-h, a gutter apart, from y = top (flex wrap; overflow shows past the content).
// Each Card fills its parent (fx "fill", as in a Home cell), so each gets a fixed card-w x card-h cell here: a Card
// straight in the content-height wrap row would be 100% of a parent sized by its children, and LVGL's layout pass
// never settles (the 2026-10-27 ASan hang; tests/functional/layout_settles.test.mjs)
lv_obj_t *home_cards(lv_obj_t *s) {
  lv_obj_t *g = rect(s, lay::inset, lay::top, lay::content_w, LV_SIZE_CONTENT);
  lv_obj_add_flag(g, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
  lv_obj_set_flex_flow(g, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_style_pad_column(g, lay::gutter, 0);
  lv_obj_set_style_pad_row(g, lay::gutter, 0);
  struct { const char *label, *icon, *preview, *mode, *value, *text, *sub; } C[] = {
      {"Weather", "weather", "partly-day", "value", "13°", "", "Dry for the next hour"},
      {"Media", "media", "none", "text", "", "Paper Lanterns on a Quiet Evening", "Living room"},
      {"Timer", "timer", "none", "value", "4:12", "", "Pasta"},
      {"Energy", "energy", "none", "value", "1.2 kW", "", "Today 8.4 kWh"},
  };
  for (auto &c : C) {
    Doc d;
    d["label"] = c.label, d["icon"] = c.icon, d["preview"] = c.preview, d["mode"] = c.mode;
    d["value"] = c.value, d["text"] = c.text, d["sub"] = c.sub;
    lv_obj_t *cell = box(g);
    lv_obj_set_size(cell, lay::card_w, lay::card_h);
    comp(cell, "card", d);
  }
  return g;
}
// a real List row (list_row) into a column; rows touch, the hairline on top of each = the divider
lv_obj_t *list_row(lv_obj_t *col, const char *title, const char *icon, const char *trailing, const char *value, bool on = false) {
  Doc d;
  d["title"] = title, d["icon"] = icon, d["trailing"] = trailing, d["value"] = value, d["on"] = on;
  return comp(col, "list_row", d);
}
// the real Section header (title only: no badge, nothing on the right), else the labelled outline
lv_obj_t *section(lv_obj_t *col, const char *title) {
  if (!find("section_header")) return coming(col, "section header", lay::content_w, style(ty::label).box);
  Doc d;
  d["title"] = title, d["badge"] = false, d["right"] = "none";
  return comp(col, "section_header", d);
}
// token overlays: translucent marks over the real components at an object's screen box (or a given box)
void mark_box(lv_obj_t *layer, int x, int y, int w, int h, lv_color_t c) {
  lv_obj_t *o = rect(layer, x, y, w, h);
  lv_obj_set_style_bg_color(o, c, 0);
  lv_obj_set_style_bg_opa(o, LV_OPA_50, 0);
}
void mark(lv_obj_t *layer, lv_obj_t *s, lv_obj_t *o, lv_color_t c) {
  lv_area_t a, b;
  lv_obj_get_coords(o, &a);
  lv_obj_get_coords(s, &b);
  mark_box(layer, a.x1 - b.x1, a.y1 - b.y1, lv_area_get_width(&a), lv_area_get_height(&a), c);
}
lv_obj_t *spacer(lv_obj_t *col, int h) {
  lv_obj_t *o = box(col);
  lv_obj_set_size(o, LV_PCT(100), h);
  return o;
}
void inset_bands(lv_obj_t *L) {
  const int S = lay::screen_w, in = lay::inset;
  const int bands[4][4] = {{0, 0, S, in}, {0, S - in, S, in}, {0, in, in, S - 2 * in}, {S - in, in, in, S - 2 * in}};
  for (auto &b : bands) mark_box(L, b[0], b[1], b[2], b[3], T->accent);
}

// the grid: text-coloured hairlines every grid step (sp::GRID), every lay::group a stronger one, at component tokens
// grid.minor / grid.major (colour.json): drawn on top of the real components and see-through on them
lv_opa_t grid_opa(bool major) { return (lv_opa_t) lroundf((major ? fw::tokens::comp::grid_major : fw::tokens::comp::grid_minor) * 255); }
void grid_lines(lv_obj_t *L, int w, int h) {
  for (int x = sp::GRID; x < w; x += sp::GRID) {
    lv_obj_t *v = rect(L, x, 0, stroke::hair, h);
    fill(v, T->text);
    lv_obj_set_style_bg_opa(v, grid_opa(x % lay::group == 0), 0);
  }
  for (int y = sp::GRID; y < h; y += sp::GRID) {
    lv_obj_t *o = rect(L, 0, y, w, stroke::hair);
    fill(o, T->text);
    lv_obj_set_style_bg_opa(o, grid_opa(y % lay::group == 0), 0);
  }
}

// ---- Screen overlay: the real Home (Header + Cards) with toggleable token layers on top -----------------------------
enum Layer { GRID, INSET, HEADER, CONTENT, GUTTER, N_LAYERS };
const char *LAYER_NAMES[N_LAYERS] = {"grid", "inset", "header", "content", "gutter"};
struct Overlay {
  lv_obj_t *layer[N_LAYERS];
  lv_obj_t *chip[N_LAYERS];
  lv_obj_t *chip_label[N_LAYERS];
  bool on[N_LAYERS];
};
void chip_style(Overlay *o, int i) {
  lv_obj_t *c = o->chip[i];
  fill(c, o->on[i] ? T->surface_press : T->bg);
  outline(c, o->on[i] ? T->line_hi : T->line, stroke::hair);
  lv_obj_set_style_text_color(o->chip_label[i], o->on[i] ? T->text : T->text_dim, 0);
  if (o->on[i]) lv_obj_remove_flag(o->layer[i], LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(o->layer[i], LV_OBJ_FLAG_HIDDEN);
}
void chip_tap(lv_event_t *e) {
  Overlay *o = (Overlay *) lv_event_get_user_data(e);
  lv_obj_t *c = (lv_obj_t *) lv_event_get_current_target(e);
  for (int i = 0; i < N_LAYERS; i++)
    if (o->chip[i] == c) {
      o->on[i] = !o->on[i];
      chip_style(o, i);
      char b[32];
      snprintf(b, sizeof b, "%s %s", LAYER_NAMES[i], o->on[i] ? "on" : "off");
      emit(c, "layer", b);
      sound(c, o->on[i] ? "toggle-on" : "toggle-off");
    }
}
lv_obj_t *layer(lv_obj_t *s) { return rect(s, 0, 0, lay::screen_w, lay::screen_h); }
void screen(lv_obj_t *parent) {
  lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_t *col = column(parent, 0, lay::screen_w);
  lv_obj_t *s = screen_box(col);
  const int S = lay::screen_w, in = lay::inset;
  Overlay *o = (Overlay *) lv_malloc(sizeof(Overlay));
  lv_obj_add_event_cb(col, [](lv_event_t *e) { lv_free(lv_event_get_user_data(e)); }, LV_EVENT_DELETE, o);
  // the real screen first
  header(s, "Home", false);
  lv_obj_t *cards = home_cards(s);
  lv_obj_update_layout(s);
  // token layers on top
  lv_obj_t *L = o->layer[GRID] = layer(s);
  grid_lines(L, S, S);
  inset_bands(o->layer[INSET] = layer(s));
  L = o->layer[GUTTER] = layer(s);  // the gutters between the cards, measured from the real cards
  lv_obj_t *c0 = lv_obj_get_child(cards, 0), *c3 = lv_obj_get_child(cards, 3);
  if (c0 && c3) {
    lv_area_t a, z, sb;
    lv_obj_get_coords(c0, &a), lv_obj_get_coords(c3, &z), lv_obj_get_coords(s, &sb);
    mark_box(L, a.x2 + 1 - sb.x1, a.y1 - sb.y1, lay::gutter, z.y2 - a.y1 + 1, T->steel);
    mark_box(L, a.x1 - sb.x1, a.y2 + 1 - sb.y1, z.x2 - a.x1 + 1, lay::gutter, T->steel);
  }
  L = o->layer[CONTENT] = layer(s);
  outline(rect(L, in, lay::top, lay::content_w, lay::content_h), T->blue, stroke::edge);
  L = o->layer[HEADER] = layer(s);
  outline(rect(L, in, in, lay::content_w, lay::row), T->accent, stroke::edge);
  mark_box(L, in, in + lay::row, lay::content_w, lay::header_gap, T->blue);
  // chips under the screen = the legend; each toggles its layer
  lv_obj_t *cr = row(col, sp::s3);
  lv_obj_set_flex_flow(cr, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_style_pad_all(cr, in, 0);
  lv_obj_set_style_pad_row(cr, sp::s3, 0);
  const lv_color_t sw[N_LAYERS] = {T->text, T->accent, T->accent, T->blue, T->steel};
  char b[64];
  for (int i = 0; i < N_LAYERS; i++) {
    lv_obj_t *c = o->chip[i] = box(cr);
    lv_obj_set_size(c, LV_SIZE_CONTENT, touch::min);
    lv_obj_set_flex_flow(c, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(c, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_hor(c, sp::s4, 0);
    lv_obj_set_style_pad_column(c, sp::s3, 0);
    lv_obj_set_style_radius(c, rad::chip, 0);
    lv_obj_t *d = box(c);
    lv_obj_set_size(d, sp::s5, sp::s5);
    if (i == CONTENT || i == HEADER) outline(d, sw[i], stroke::edge);
    else fill(d, sw[i]);
    if (i == GRID) lv_obj_set_style_bg_opa(d, grid_opa(true), 0);  // the swatch = a major grid line
    switch (i) {
      case GRID: snprintf(b, sizeof b, "Grid %d", sp::GRID); break;
      case INSET: snprintf(b, sizeof b, "Inset %d", in); break;
      case HEADER: snprintf(b, sizeof b, "Header %d + gap %d", lay::row, lay::header_gap); break;
      case CONTENT: snprintf(b, sizeof b, "Content from %d", lay::top); break;
      default: snprintf(b, sizeof b, "Gutter %d", lay::gutter); break;
    }
    o->chip_label[i] = text(c, ty::c_meta, b, T->text);
    o->on[i] = true;
    lv_obj_add_flag(c, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(c, chip_tap, LV_EVENT_CLICKED, o);
    chip_style(o, i);
  }
}

// ---- Header zone ----------------------------------------------------------------------------------------------------
void header_zone(lv_obj_t *parent) {
  lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_t *col = column(parent, 0, lay::screen_w);
  const int S = lay::screen_w, in = lay::inset, H = lay::top + lay::group;
  lv_obj_t *s = box(col);
  lv_obj_set_size(s, S, H);
  fill(s, T->bg);
  lv_obj_t *h = header(s, "Lights");
  lv_obj_update_layout(s);
  // overlays on top of the real Header: inset (top + sides), the row, the Back button box, the time slot, gap, top
  lv_obj_t *L = layer(s);
  mark_box(L, 0, 0, S, in, T->accent);
  mark_box(L, 0, in, in, H - in, T->accent);
  mark_box(L, S - in, in, in, H - in, T->accent);
  outline(rect(L, in, in, lay::content_w, lay::row), T->accent, stroke::edge);
  if (lv_obj_t *btn = lv_obj_get_child(h, 0)) {
    lv_area_t a, sb;
    lv_obj_get_coords(btn, &a), lv_obj_get_coords(s, &sb);
    outline(rect(L, a.x1 - sb.x1, a.y1 - sb.y1, lv_area_get_width(&a), lv_area_get_height(&a)), T->steel, stroke::hair);
  }
  outline(rect(L, S - in - lay::header_time_w, in, lay::header_time_w, lay::row), T->blue, stroke::hair);
  mark_box(L, in, in + lay::row, lay::content_w, lay::header_gap, T->blue);
  fill(rect(L, in, lay::top, lay::content_w, stroke::edge), T->line_hi);
  lv_obj_t *l = column(col, 0, S);
  lv_obj_set_style_pad_hor(l, sp::s7, 0);
  key(l, T->accent, true, "Margin", "inset", "The same on every side of every screen; the header row starts at its inner corner.");
  key(l, T->accent, false, "Header row", "row", "Back button, the screen's name, then the settings button and the time, in one row.");
  key(l, T->steel, false, "Back button", "row", "A square as tall as the header row, so it fills the row exactly.");
  key(l, T->blue, false, "Time", "header-time-w", "Always at the right end of the row.");
  key(l, T->text_dim, false, "Back button to name", "back-gap", "");
  {
    const lyt::Token *tp = tok("top"), *in = tok("inset"), *rw = tok("row");
    legend(l, T->blue, true, "Gap under the header", tp->v - in->v - rw->v, tp->v1x - in->v1x - rw->v1x, "");
  }
  key(l, T->line_hi, true, "Content starts", "top", "Measured from the top of the screen: the same on every screen.");
}

// ---- The clock, always top right: the real Header (which ends with the real Screen chrome) on two screen tops, Home
// (no back, the settings button before the time: Home only for now, Darren 2026-10-22) and Lights (the time alone),
// the corner's parts outlined; values from layout.json at this build's size
void chrome_strip(lv_obj_t *col, const char *label, bool home) {
  const int S = lay::screen_w, in = lay::inset, H = lay::top;
  lv_obj_t *s = box(col);
  lv_obj_set_size(s, S, H);
  fill(s, T->bg);
  Doc d;
  d["label"] = label;
  d["time"] = "21:42";
  d["back"] = !home;
  d["settings"] = home;
  lv_obj_t *h = comp(s, "header", d);
  lv_obj_set_pos(h, in, in);
  lv_obj_update_layout(s);
  lv_obj_t *L = layer(s);
  const int w = home ? lay::chrome_w : lay::header_time_w, x0 = S - in - w;
  outline(rect(L, x0, in, w, lay::row), T->accent, stroke::edge);
  if (home) {
    outline(rect(L, x0, in, lay::chrome_btn, lay::row), T->steel, stroke::hair);
    mark_box(L, x0 + lay::chrome_btn, in, lay::gutter, lay::row, T->steel);
  }
  outline(rect(L, S - in - lay::header_time_w, in, lay::header_time_w, lay::row), T->blue, stroke::hair);
  mark_box(L, S - in, in, in, lay::row, T->accent);
}
void chrome_sec(lv_obj_t *parent) {
  lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_t *col = column(parent, sp::s2, lay::screen_w);
  chrome_strip(col, "Home", true);
  chrome_strip(col, "Lights", false);
  lv_obj_t *l = column(col, 0, lay::screen_w);
  lv_obj_set_style_pad_hor(l, sp::s7, 0);
  key(l, T->blue, false, "Time", "header-time-w", "On every screen, right-aligned on the margin.");
  key(l, T->steel, false, "Settings button", "chrome-btn", "On Home, a square as tall as the header row, like the back button; it opens Settings. It may move to other screens later.");
  key(l, T->steel, true, "Button to time", "gutter", "");
  {
    const lyt::Token *b = tok("chrome-btn"), *g = tok("gutter"), *t = tok("header-time-w");
    legend(l, T->accent, false, "Corner with the button", b->v + g->v + t->v, b->v1x + g->v1x + t->v1x, "");
  }
  lv_obj_t *e = entry(l);
  caption(e, std::string("Home, with the settings button before the time, then any other screen with the time alone. Switch the device in the sidebar to see it at ") + other_den() + ".");
}

// ---- Groups and rows: a settings screen from real List rows -------------------------------------------------------
void rhythm(lv_obj_t *parent) {
  lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_t *col = column(parent, 0, lay::screen_w);
  lv_obj_t *s = screen_box(col);
  header(s, "Settings");
  lv_obj_t *c = rect(s, lay::inset, lay::top, lay::content_w, lay::content_h);
  lv_obj_add_flag(c, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
  lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
  // the real Section header (section-h tall = touch min, its caps-sm title centred in it): the gaps around it are
  // cut by its centring slack, so what you SEE stays the tokens: title to rows = section-gap, rows to the next title =
  // group (best guess, Darren to confirm)
  const int sh = find("section_header") ? lay::section_h : style(ty::label).box;
  const int slack = find("section_header") ? (lay::section_h - style(ty::c_caps_sm).box) / 2 : 0;
  lv_obj_t *h1 = section(c, "Display");
  lv_obj_set_style_margin_top(h1, -slack, 0);  // the first title's box starts at the content top (its slack bleeds up)
  spacer(c, std::max(0, lay::section_gap - slack));
  list_row(c, "Brightness", "bulb", "value-chevron", "80%");
  lv_obj_t *r2 = list_row(c, "Chime", "speaker", "toggle", "", true);
  spacer(c, std::max(0, lay::group - slack));
  lv_obj_t *h2 = section(c, "Sound");
  spacer(c, std::max(0, lay::section_gap - slack));
  list_row(c, "Volume", "volume", "value", "40%");
  list_row(c, "Television", "tv", "chevron", "");
  {
    Doc d;
    d["text"] = "all settings · 12 more";
    comp(c, "more_row", d);
  }
  lv_obj_update_layout(s);
  lv_obj_t *L = layer(s);
  // the visible gaps: title bottom to the first row (section-gap), the last row to the next title (group)
  lv_area_t sa, a;
  lv_obj_get_coords(s, &sa);
  for (lv_obj_t *h : {h1, h2}) {
    lv_obj_get_coords(h, &a);
    mark_box(L, lay::inset, a.y2 + 1 - sa.y1 - slack, lay::content_w, lay::section_gap, T->steel);
  }
  lv_obj_get_coords(r2, &a);
  mark_box(L, lay::inset, a.y2 + 1 - sa.y1, lay::content_w, lay::group, T->accent);
  outline(rect(L, lay::inset, lay::top, lay::content_w, lay::content_h), T->blue, stroke::hair);
  lv_obj_t *l = column(col, 0, lay::screen_w);
  lv_obj_set_style_pad_hor(l, sp::s7, 0);
  key(l, T->accent, true, "Between groups", "group", "The larger gap, after the last row of a group.");
  key(l, T->steel, true, "Title to its rows", "section-gap", "A small gap: the title belongs to the rows under it.");
  key(l, T->line, false, "Rows", "more-row-h", "Real rows touching each other; the thin line on top of each is the divider. A list that shows only the first few ends with a \"more\" row (this height).");
  (void) sh;
}

// ---- Overlays: sheet + toast positions over the real Home ----------------------------------------------------------
void overlay(lv_obj_t *parent) {
  lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_t *col = column(parent, sp::s7, lay::screen_w);
  const int S = lay::screen_w, in = lay::inset;
  // sheet: the real Bottom sheet (overlay only, shown open, no animation) over the real Home; panel from sheet-top
  // to the bottom edge, content inset
  lv_obj_t *s = screen_box(col);
  header(s, "Home", false);
  home_cards(s);
  if (find("bottom_sheet")) {
    Doc d;
    d["overlay"] = true, d["still"] = true, d["list"] = "speakers", d["selected"] = 1;
    find("bottom_sheet")->build(s, d.as<Props>());
    lv_obj_t *L = layer(s);  // the sheet-top line, over the sheet
    mark_box(L, 0, lay::sheet_top, S, stroke::edge, T->accent);
  } else {
    lv_obj_t *sh = coming(s, "sheet", lay::content_w, S - in - lay::sheet_top);
    lv_obj_set_pos(sh, in, lay::sheet_top);
    fill(sh, T->bg);
    outline(sh, T->accent, stroke::edge);
  }
  // toast: the real Toast layer (overlay only, still): one layer per position, so the top toast sits at toast-top and
  // the bottom one ends at the bottom inset; marks over them: toast-top (accent line), toast-pad (blue content box)
  lv_obj_t *t2 = screen_box(col);
  header(t2, "Home", false);
  home_cards(t2);
  if (find("toast_layer")) {
    for (int k = 0; k < 2; k++) {
      Doc d;
      d["overlay"] = true, d["still"] = true, d["timer"] = false, d["action"] = false;
      d["position"] = k ? "bottom" : "top", d["push"] = k ? "info" : "ok";
      find("toast_layer")->build(t2, d.as<Props>());
    }
    lv_obj_t *L = layer(t2);
    mark_box(L, 0, lay::toast_top, S, stroke::edge, T->accent);
    outline(rect(L, in + lay::toast_pad, lay::toast_top + lay::toast_pad, lay::content_w - 2 * lay::toast_pad, lay::toast_h - 2 * lay::toast_pad), T->blue, stroke::hair);
  } else
    for (int k = 0; k < 2; k++) {
      lv_obj_t *t = coming(t2, k ? "toast (bottom)" : "toast", lay::content_w, lay::toast_h);
      lv_obj_set_pos(t, in, k ? S - in - lay::toast_h : lay::toast_top);
      fill(t, T->bg);
      outline(t, T->accent, stroke::edge);
      outline(rect(t, lay::toast_pad - stroke::edge, lay::toast_pad - stroke::edge, lay::content_w - 2 * lay::toast_pad, lay::toast_h - 2 * lay::toast_pad), T->blue, stroke::hair);
    }
  lv_obj_t *l = column(col, 0, S);
  lv_obj_set_style_pad_hor(l, sp::s7, 0);
  key(l, T->accent, false, "Sheet starts", "sheet-top", "A sheet covers the screen from here down, edge to edge, with the usual margin inside.");
  key(l, T->accent, false, "First toast", "toast-top", "Just under the header; a toast at the bottom ends at the bottom margin.");
  key(l, T->accent, false, "Toast height", "toast-h", "As wide as the content.");
  key(l, T->blue, false, "Inside a toast", "toast-pad", "The space around its icon and text.");
}

// ---- Rules (layout.json rules.sections: each a heading + one or two plain sentences = the section label + about; the
// visual for each is here). Plain words on the page; token names and tables live in the token files.
// both densities' value of a token (space.json or layout.json), for "48 px (32 at 480)" captions
const spt::Token *stok(const char *name) {
  for (int i = 0; i < spt::N_TOKENS; i++) if (!strcmp(spt::TOKENS[i].name, name)) return &spt::TOKENS[i];
  return nullptr;
}
std::string both(int v, int v1x) {
  char b[48];
  if (v == v1x) snprintf(b, sizeof b, "%d px", v);
  else snprintf(b, sizeof b, "%d px (%d at %s)", is1x() ? v1x : v, is1x() ? v : v1x, other_den());
  return b;
}
void caption(lv_obj_t *e, const std::string &s) { text(e, ty::c_meta, s.c_str(), T->text_dim, inner_w()); }
// a real component on a panel with the grid over it and its box outlined; returns the entry for captions
lv_obj_t *on_grid(lv_obj_t *l, const char *id, Doc &d, lv_obj_t **out = nullptr) {
  lv_obj_t *e = entry(l, sp::s3);
  lv_obj_t *pn = box(e);
  fill(pn, T->bg);
  lv_obj_set_style_pad_all(pn, lay::gutter, 0);
  lv_obj_set_size(pn, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_t *c = comp(pn, id, d);
  lv_obj_update_layout(pn);
  const int cw = lv_obj_get_width(c), ch = lv_obj_get_height(c), G = sp::GRID, P = lay::gutter;
  const int pw = std::min(inner_w(), (cw + 2 * P + G - 1) / G * G), ph = (ch + 2 * P + G - 1) / G * G;
  lv_obj_set_size(pn, pw, ph);
  lv_obj_t *L = rect(pn, -P, -P, pw, ph);  // the panel's own box (its padding moves children in)
  lv_obj_add_flag(L, LV_OBJ_FLAG_IGNORE_LAYOUT);
  grid_lines(L, pw, ph);
  outline(rect(L, P, P, cw, ch), T->accent, stroke::hair);
  if (out) *out = c;
  return e;
}
void grid_sec(lv_obj_t *l) {
  {
    Doc d;
    d["label"] = "Start";
    lv_obj_t *e = on_grid(l, "button", d);
    const spt::Token *k = stok("button-h");
    caption(e, std::string("A button: ") + (k ? both(k->v, k->v1x) : "") + " tall, as wide as its label rounded up to the grid.");
  }
  {
    Doc d;
    d["on"] = true;
    lv_obj_t *e = on_grid(l, "toggle", d);
    const spt::Token *k = stok("min");
    caption(e, std::string("A switch: ") + (k ? both(k->v, k->v1x) : "") + " tall; the knob keeps the same gap all round.");
  }
  {
    Doc d;
    d["icon"] = "bulb", d["trailing"] = "chevron";
    lv_obj_t *e = on_grid(l, "list_row", d);
    const spt::Token *k = stok("list-row-h");
    caption(e, std::string("A row: ") + (k ? both(k->v, k->v1x) : "") + " tall and as wide as the content area.");
  }
  lv_obj_t *e = entry(l);
  caption(e, "The orange outline is each component's box. The faint lines are the grid, every 8 px here at 720 (every 4 px at 480), with a stronger line every group.");
}
// today's two screens side by side at 1/4 of their pixels, drawn from the layout tokens of each size: every edge is
// scaled from its real position (not each size on its own), so equal gaps stay equal; the frame line sits outside
void sizes_sec(lv_obj_t *l) {
  lv_obj_t *e = entry(l, sp::s3);
  lv_obj_t *r = row(e, sp::s7);
  lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_START);
  const spt::Token *cw = stok("card-w"), *chh = stok("card-h");
  const lyt::Token *in = tok("inset"), *gu = tok("gutter"), *top = tok("top"), *rw = tok("row");
  lv_obj_t *thumbs[2];
  for (int k = 0; k < 2; k++) {
    const bool big = k == 0;
    auto v = [&](const auto *t) { return big ? t->v : t->v1x; };
    auto X = [](int px_) { return (int) std::lround(px_ / 4.0); };
    const int S = big ? 720 : 480, i = v(in), g = v(gu), tp = v(top), rh = v(rw), w = v(cw), h = v(chh);
    lv_obj_t *c = column(r, sp::s3, X(S));
    lv_obj_t *sc = thumbs[k] = box(c);
    lv_obj_set_size(sc, X(S), X(S));
    fill(sc, T->bg);
    lv_obj_set_style_outline_width(sc, stroke::hair, 0);
    lv_obj_set_style_outline_color(sc, (big != is1x()) ? T->accent : T->line_hi, 0);
    auto part = [&](int x0, int y0, int x1, int y1, lv_color_t col) { fill(rect(sc, X(x0), X(y0), X(x1) - X(x0), X(y1) - X(y0)), col); };
    part(i, i, S - i, i + rh, T->line);
    for (int q = 0; q < 4; q++) {
      const int x0 = i + (q % 2) * (w + g), y0 = tp + (q / 2) * (h + g);
      part(x0, y0, x0 + w, y0 + h, T->surface_press);
    }
    text(c, ty::c_caps_sm, big ? "720 x 720" : "480 x 480", T->text);
    text(c, ty::c_meta, big ? "Waveshare, Shelly" : "Guition", T->text_dim, X(S));
  }
  lv_obj_t *n = entry(l);
  caption(n, "Today's two panels at a quarter of their pixels, drawn from their own layout values: the same margin, header and two columns of cards. The outlined one is the screen this page is built for; switch the device in the sidebar to see the other.");
  lv_obj_update_layout(l);
  for (lv_obj_t *s : thumbs) {
    lv_area_t a;
    lv_obj_get_coords(s, &a);
    char b[48];
    snprintf(b, sizeof b, "%d,%d,%d,%d", (int) a.x1, (int) a.y1, (int) lv_area_get_width(&a), (int) lv_area_get_height(&a));
    emit(s, "thumb", b);  // for tests measuring the gaps (fw_events)
  }
}
// the spacing steps at true size (each distinct step once)
void spacing_sec(lv_obj_t *l) {
  lv_obj_t *e = entry(l, sp::s3);
  lv_obj_t *r = row(e, sp::s5);
  lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
  int last = -1;
  for (int i = 0; i < spt::N_TOKENS; i++) {
    const spt::Token &k = spt::TOKENS[i];
    if (strcmp(k.group, "space")) continue;
    const int v = is1x() ? k.v1x : k.v;
    if (v == last) continue;
    last = v;
    lv_obj_t *c = column(r, sp::s2, LV_SIZE_CONTENT);
    lv_obj_set_flex_align(c, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t *sq = box(c);
    lv_obj_set_size(sq, v, v);
    fill(sq, T->accent);
    char b[8];
    snprintf(b, sizeof b, "%d", v);
    text(c, ty::c_meta, b, T->text_dim);
  }
  lv_obj_t *n = entry(l);
  caption(n, "Every step at true size, in pixels. Inside a status chip the dot and the text are one small step apart; inside a card the content keeps a large step from the edge.");
  Doc d;
  on_grid(l, "status_chip", d);
}
// touch: real controls with their touch area tinted
void touch_sec(lv_obj_t *l) {
  const spt::Token *mn = stok("min"), *rd = stok("radio");
  {
    lv_obj_t *e = entry(l, sp::s3);
    lv_obj_t *pn = box(e);
    lv_obj_set_size(pn, touch::min * 3, touch::min);
    Doc d;
    lv_obj_t *c = comp(pn, "radio", d);
    lv_obj_update_layout(pn);
    lv_obj_align(c, LV_ALIGN_LEFT_MID, (touch::min - lv_obj_get_width(c)) / 2, 0);
    mark_box(pn, 0, 0, touch::min, touch::min, T->accent);
    caption(e, std::string("A radio is drawn ") + (rd ? both(rd->v, rd->v1x) : "") + " wide, but you can tap anywhere in the tinted square: " + (mn ? both(mn->v, mn->v1x) : "") + ".");
  }
  {
    Doc d;
    d["label"] = "Start";
    lv_obj_t *e = on_grid(l, "button", d);
    const spt::Token *k = stok("button-h");
    caption(e, std::string("Buttons and rows are bigger still: ") + (k ? both(k->v, k->v1x) : "") + " tall.");
  }
}
// nested corners: right (inner = outer - gap), square in square, and the mistake (same corner inside)
void corners_sec(lv_obj_t *l) {
  lv_obj_t *e = entry(l, sp::s3);
  lv_obj_t *r = row(e, sp::s7);
  lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  const int W = (inner_w() - 2 * sp::s7) / 3 / sp::GRID * sp::GRID, gap = sp::s5, R = rad::ring + gap;
  struct { int outer, inner; const char *what; lv_color_t edge; } S[] = {
      {R, R - gap, "Inner corner = outer corner minus the gap", T->line_hi},
      {0, 0, "Square outside, square inside", T->line_hi},
      {R, R, "Not this: the same corner inside, the gap bulges", T->warn},
  };
  for (auto &s : S) {
    lv_obj_t *c = column(r, sp::s3, W);
    lv_obj_t *o = box(c);
    lv_obj_set_size(o, W, W * 3 / 4 / sp::GRID * sp::GRID);
    fill(o, T->surface);
    outline(o, s.edge, stroke::hair);
    lv_obj_set_style_radius(o, s.outer, 0);
    lv_obj_set_style_pad_all(o, gap, 0);
    lv_obj_t *in = box(o);
    lv_obj_set_size(in, LV_PCT(100), LV_PCT(100));
    fill(in, T->surface_press);
    lv_obj_set_style_radius(in, s.inner, 0);
    text(c, ty::c_meta, s.what, T->text_dim, W);
  }
  lv_obj_t *n = entry(l);
  caption(n, "The selected option in a switcher follows the same rule: its corner is the box's corner minus the border.");
  Doc d;
  on_grid(l, "segmented", d);
}
// breaks: a hairline row, a centred label, a text line box
void breaks_sec(lv_obj_t *l) {
  {
    Doc d;
    d["icon"] = "speaker", d["trailing"] = "value", d["value"] = "40%";
    lv_obj_t *e = on_grid(l, "list_row", d);
    caption(e, "The divider on top of a row is a 1 px line inside the row's box; the box is on the grid.");
  }
  {
    Doc d;
    d["label"] = "Start";
    lv_obj_t *e = on_grid(l, "button", d);
    caption(e, "The label is centred in the button. The button is on the grid; at 480 its label lands between two lines, and that is fine.");
  }
  {
    lv_obj_t *e = entry(l, sp::s3);
    lv_obj_t *pn = box(e);
    fill(pn, T->bg);
    const int G = sp::GRID, P = lay::gutter;
    lv_obj_t *t = text(pn, ty::title, "Living room", T->text);
    lv_obj_set_pos(t, P, P);
    lv_obj_update_layout(pn);
    const int tw = lv_obj_get_width(t), th = lv_obj_get_height(t);
    const int pw = (tw + 2 * P + G - 1) / G * G, ph = (th + 2 * P + G - 1) / G * G;
    lv_obj_set_size(pn, pw, ph);
    lv_obj_t *L = rect(pn, 0, 0, pw, ph);
    grid_lines(L, pw, ph);
    outline(rect(L, P, P, tw, th), T->accent, stroke::hair);
    caption(e, "Text sits in a line box on the grid (outlined); the letters inside it follow the line, not the grid.");
  }
}

// ---- Devices and density ------------------------------------------------------------------------------------------
bool in_build(const lyt::Device &d) { return !strcmp(d.status, "target") && !strcmp(d.density, is1x() ? "1x" : "1.5x"); }
void devices(lv_obj_t *l) {
  const int thumb = px(160);  // the widest device (1280 dp) at 1/8
  for (int i = 0; i < lyt::N_DEVICES; i++) {
    const lyt::Device &d = lyt::DEVICES[i];
    lv_obj_t *e = entry(l, sp::s3);
    lv_obj_t *r = row(e, sp::s5);
    lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_t *tb = box(r);
    lv_obj_set_size(tb, thumb, LV_SIZE_CONTENT);
    const int dw = (int) std::lround(d.w / d.d), dh = (int) std::lround(d.h / d.d);
    lv_obj_t *sc = box(tb);
    lv_obj_set_size(sc, px(dw / 8.0f), px(dh / 8.0f));
    if (in_build(d)) fill(sc, T->accent_dim);
    outline(sc, in_build(d) ? T->accent : T->line_hi, stroke::edge);
    lv_obj_t *tx = column(r, sp::s1, inner_w() - thumb - sp::s5);
    text(tx, ty::c_body, d.name, T->text);
    char b[160];
    snprintf(b, sizeof b, "%d x %d px · %.1f inch · %s", d.w, d.h, d.inches, d.cls);
    text(tx, ty::c_meta, b, T->text_dim, inner_w() - thumb - sp::s5);
    if (*d.runs) {
      snprintf(b, sizeof b, "%s%s%s", d.runs, *d.mount ? " · mount: " : "", d.mount);
      text(tx, ty::c_meta, b, T->text_dim, inner_w() - thumb - sp::s5);
    }
    std::string st = std::string(in_build(d) ? "THIS PAGE · " : "") + (strcmp(d.status, "target") ? "planned" : "built today");
    text(tx, ty::c_caps_sm, st.c_str(), in_build(d) ? T->accent_text : T->text_dim);
  }
  lv_obj_t *e = entry(l, sp::s1);
  text(e, ty::c_meta, "720 panels have about 250 pixels per inch, 480 panels about 170: the same design gets 1.5 pixels on the sharper screen for every pixel on the other. At 480, thin text is drawn one weight heavier and small text one size up, so it stays crisp.", T->text_dim, inner_w());

}

// ---- Size classes ------------------------------------------------------------------------------------------------
void classes(lv_obj_t *l) {
  for (int i = 0; i < lyt::N_CLASSES; i++) {
    const lyt::SizeClass &c = lyt::CLASSES[i];
    lv_obj_t *e = entry(l, sp::s3);
    char b[128];
    const bool here = !strcmp(c.id, "square");  // every build today is a square panel
    snprintf(b, sizeof b, "%s · Home in %d columns%s", c.id, c.home_cols, here ? " · TODAY'S PANELS" : "");
    text(e, ty::c_caps_sm, b, here ? T->accent_text : T->text);
    text(e, ty::c_meta, c.use, T->text_dim, inner_w());
    // sketch at 1/4 (dp): inset, header row, the Home columns
    const int w = px(c.min_w / 4.0f), h = px(c.min_h / 4.0f), in = sp::s2, gap = sp::s1, top = sp::s7;
    lv_obj_t *sk = box(e);
    lv_obj_set_size(sk, w, h);
    outline(sk, here ? T->accent : T->line_hi, stroke::edge);
    fill(rect(sk, in, in, w - 2 * in, sp::s3), T->line);
    const int cw = (w - 2 * in - (c.home_cols - 1) * gap) / c.home_cols;
    for (int k = 0; k < c.home_cols; k++) fill(rect(sk, in + k * (cw + gap), top, cw, h - top - in), T->surface_press);
  }
}

}  // namespace

FW_COMPONENT(layout, "Layout & devices", "foundations", R"({
  "about": "Screen geometry, devices and size classes: how a screen is laid out on 720 and 480 panels.",
  "layout": "doc",
  "frame": [480, 0],
  "props": [{"name": "section", "about": "Which part of the layout page to show, such as the screen areas, the devices or the size classes.", "type": "select", "options": )" FW_LAYOUT_SECTION_IDS R"(, "default": "screen"}],
  "icons": ["back@icon", "screen-settings@icon"],
  "variants": )" FW_LAYOUT_SECTIONS R"(
})") {
  const char *s = p["section"] | "screen";
  if (!strcmp(s, "screen")) return screen(parent);
  if (!strcmp(s, "frame")) return header_zone(parent);
  if (!strcmp(s, "chrome")) return chrome_sec(parent);
  if (!strcmp(s, "rhythm")) return rhythm(parent);
  if (!strcmp(s, "overlay")) return overlay(parent);
  lv_obj_t *l = page(parent);
  if (!strcmp(s, "grid")) grid_sec(l);
  else if (!strcmp(s, "sizes")) sizes_sec(l);
  else if (!strcmp(s, "spacing")) spacing_sec(l);
  else if (!strcmp(s, "touch")) touch_sec(l);
  else if (!strcmp(s, "corners")) corners_sec(l);
  else if (!strcmp(s, "breaks")) breaks_sec(l);
  else if (!strcmp(s, "devices")) devices(l);
  else classes(l);
}
