// fx_core.cpp: the contract core (renderer/CONTRACT.md). Token styles are SHARED lv_style_t, one set per display
// (the workbench shows several stages in different themes at once; a panel has one), named like the tokens
// (tools/gen_fx.mjs -> fx_gen.h). A theme / accent / text-size change repaints the display's set and reports one
// style change: no object holds a colour, so none can keep an old one.
#include "fx.h"
#include "fw.h"
#include "fx_gen.h"
#include "spec.h"  // spec::SLOT: a Grid cell is a slot
#include "theme_file.h"  // the live Skin (core only, C44): its palette, parts, shapes
#include "theme_gen.h"   // the part list (tokens/theme.json)
#include "draw/lv_draw_private.h"  // a text part's shadow: its label task drawn twice (core/fx_core.cpp text_shadow)
#include <memory>
#include "core/lv_obj_private.h"  // fx::uses walks o->styles
#include "core/lv_obj_style_private.h"
#include "display/lv_display_private.h"  // a display going away: its objects first, then its look (no leak)
#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace fw {
void text_metrics(ty::Id s, int &line_space, int &pad_top, int &pad_bottom, int &translate_y);
const lv_font_t *theme_font(const char *family, int style, int px, bool *exact = nullptr);  // core/fonts.cpp: a built-in theme family's font
int style_role(int style);                                          // the font role a type style is drawn in
uint32_t role_glyph(int role, int i);                               // the code points the role's styles draw (0 = end)
}
#include "font_store.h"

namespace {
enum Kind { FG, BG, BORDER, LINE, INK, PAD, GAP, PADX, RADIUS, HEIGHT, MAXH, MINW, MAXW, ABOVE, WIDTH, TYPE, SHAPE, DERIVED,
            TINT, OUTLINE, DIGIT, COLON, RAISE, BASE, PART, PTEXT, ICONPX };
namespace G = fx::tgen;
namespace S = fx::skin;
#ifdef FW_1X
constexpr int DN = 1;  // a Skin length's px at this density ([1.5x, 1x])
#else
constexpr int DN = 0;
#endif
struct Def { std::string name; Kind k; int i; };
std::vector<Def> defs;  // sorted by name
const char *const SHAPES[] = {"box", "col", "row", "stack", "centre", "cell", "grow", "free", "page", "page-head", "body",
                              "body:list", "body:cards", "body:center", "body:page", "bleed", "bg-none", "fit",
                              "fill", "fill-w", "row-top", "fade-press", "fade-release", "gap:gutter", "pair", "wrap",
                              "header", "header-back", "header-title", "header-label", "header-end", "header-time",
                              "hug", "over", "over-cell", "transp", "opaque", "fade-press-ink", "fade-release-ink", "no-outline", "cells", "cell-text",
                              "rule-top", "row-mid", "row-end", "fade-press-fill", "fade-release-fill",
                              "square:ibtn-play", "square:ibtn-round", "square:ibtn-square", "tall:chip-h", "tall:list-row-h",
                              // the controls (Toggle, Count badge, Button, Segmented, Slider): their parts' sizes
                              "toggle", "toggle-off-pad", "toggle-knob", "toggle-knob-on", "toggle-slide", "word-slot",
                              "badge", "badge-num", "button", "button-fill", "border-clear", "fade-press-all",
                              "fade-release-all", "btn-underline", "seg", "seg-part", "seg-label", "seg-grow", "seg-sep", "seg-plate",
                              "seg-half-l", "seg-half-r", "ring-full-2", "ring-tb-2", "ring-l", "ring-r", "ring-lr",
                              "slider", "slider-word", "text-mid", "grow-w", "button-sm",
                              // the Checkbox: its box, the in-progress dot, a done label, the Motion toggle fade
                              "checkbox", "checkbox-dot", "strike", "fade-switch",
                              // a Group of buttons that lines up from the end (right), centred across
                              "wrap-end",
                              // the Radio: its ring and the chosen dot
                              "radio", "radio-dot"};
// tint:<c> = the colour at chip.tint over what is behind (Status chip; colour.json mixes chip-<tone>); outline:<c> = the
// focus ring (states.json focus: stroke focus, touch focus-offset outside the box)
const char *const TINTS[] = {"ok", "warn", "error", "accent", "blue", "text-dim"};
const char *const OUTLINES[] = {"focus-ring"};
// baseline:<a>:<b>: text b's figures (its glyph `ch`) on text a's baseline (a margin top), the Time's AM / PM + seconds
struct Base { fw::ty::Id a, b; uint32_t ch; };
const Base BASES[] = {{fw::ty::clock_hero_mono, fw::ty::clock_seconds_mono, '0'}, {fw::ty::clock_hero, fw::ty::clock_seconds, '0'},
  {fw::ty::clock_hero_mono, fw::ty::clock_ampm_mono, 'M'}, {fw::ty::clock_hero, fw::ty::clock_ampm, 'M'},
  {fw::ty::clock_mid_mono, fw::ty::clock_ampm_mono, 'M'}, {fw::ty::clock_mid, fw::ty::clock_ampm, 'M'},
  {fw::ty::clock_compact_mono, fw::ty::clock_ampm_sm, 'M'}, {fw::ty::clock_compact, fw::ty::clock_ampm_sm, 'M'},
  {fw::ty::hero, fw::ty::title, '0'}, {fw::ty::display, fw::ty::title, '0'}};  // the Hero block's unit on its number's baseline
const char *type_name(fw::ty::Id id) {
  for (int i = 0; i < fx::gen::N_TYPES; i++) if (fx::gen::TYPES[i].id == id) return fx::gen::TYPES[i].name;
  abort();
}
// derived sizes: one value worked out from other tokens, so a component never does the sum (paint() says how)
const char *const DERIVED_N[] = {"dial-wing-up", "dial-wing-down", "dial-slot", "card-pad", "card-text", "card-gap", "card-gap-s", "card-row", "pair-rule", "label-row",
                                 "list-text-t", "list-text-ts", "list-text-ot", "list-text-ots", "list-slot", "section-label", "empty-pad",
                                 // 17-23: padding a theme moves (C45: a flush bar), 24: the Segmented's own layers (a theme that styles the segment part draws it)
                                 "page-move", "head-move", "body-move", "header-move", "card-move", "card-head-move", "card-body-move", "seg-layer",
                                 // 25: the More row (its height + the top space that puts its label on the rows' 24 / 16 line)
                                 "more-row",
                                 // 26-27: the Entity tile's name (two body lines, a fixed box) and its status (space 5 above)
                                 "tile-text", "tile-status",
                                 // 28: the State readout's padding: card-pad from the top and sides, none below (a half
                                 // is taller than its content; a bigger theme font may use the space under it)
                                 "readout-pad",
                                 // 29-30: the Hero block's text column beside the number (label or title type over a
                                 // body line, space 2 apart), centred on the hero type box
                                 "hero-col", "hero-col-title",
                                 // 31-32: the Hero block's row: a top-aligned row, the gutter apart, as tall as its number's type box
                                 "hero-row", "hero-row-display",
                                 // 33: the Toast's box: the overlay's toast-h tall, toast-pad in (less the edge hair) and
                                 // between its items, a row centred on its height
                                 "toast"};
// A display's look: its shared styles painted from theme t (mode + accent set) through the live Skin (theme_live: its
// palette for that mode x set = th, the colours every contract style reads; Facet's Skin = Facet's colours, so th == *t)
// and its generation (gen: a theme apply bumps it, the look repaints; never by Skin pointer, a freed one's address can
// come back). Legacy components keep fw::T (Facet's palette): never half-themed (theme-engine design 8).
struct Look { lv_display_t *d; const fw::Theme *t = nullptr; int large = -1; uint32_t gen = 0, fgen = 0; std::shared_ptr<const fx::Skin> skin;
              fw::Theme th; std::vector<lv_style_t> s; };
// parts (C46, tokens/theme.json parts): "part:<p>" = 16 shared styles, one per combination of on / focus / pressed /
// disabled (bit i = state i, the merge order), each the part's MERGED block for that combination (Skin::merged), attached
// last at exactly that selector: LVGL takes each property from the matching style with the largest state, newest first
// at equal state, so the theme's block wins at its own weight and a key it does not set lets the component's token show
// (design 3.4). "ptext:<p>" = the same 16 with only the part's text ink: on every text and icon ink inside the part.
int part_def[G::N_PARTS][16], ptext_def[G::N_PARTS][16];
int skin_ix[256];          // fx colour token -> Skin palette index (by name)
int size_ix[512];          // fx size token -> Skin size index (by name; -1 none)
int type_ti[128];          // type style -> themeable type index (tokens/theme.json type; -1 = not themeable)
int ix_pixel = -1;         // the Skin size index of stroke:pixel (the snapped cell, C29)
const fx::Skin *live_raw = nullptr;  // the Skin the looks were last painted with (fx::size reads its look sizes)
int type_part[64];         // fx type style -> its text part (tokens/theme.json parts of kind text)
bool boxy(int p) { const char *k = G::PARTS[p].kind; return !strcmp(k, "box") || !strcmp(k, "header") || !strcmp(k, "bar") || !strcmp(k, "frame") || !strcmp(k, "caption"); }
bool styled(int p) { const char *k = G::PARTS[p].kind; return boxy(p) || !strcmp(k, "text") || !strcmp(k, "icon") || p == G::P_SWITCH; }
int part_ix(const char *n) { for (int p = 0; p < G::N_PARTS; p++) if (!strcmp(G::PARTS[p].name, n)) return p; abort(); }
lv_style_selector_t sel_of(int m) {
  return (m & S::ON ? LV_STATE_CHECKED : 0) | (m & S::FOCUS ? LV_STATE_FOCUSED : 0) | (m & S::PRESSED ? LV_STATE_PRESSED : 0) | (m & S::DISABLED ? fw::st::OFF : 0);
}
int combo(lv_obj_t *o) {  // the object's states as a part combination
  const lv_state_t s = lv_obj_get_state(o);
  return (s & LV_STATE_CHECKED ? S::ON : 0) | (s & LV_STATE_FOCUSED ? S::FOCUS : 0) | (s & LV_STATE_PRESSED ? S::PRESSED : 0) | (s & fw::st::OFF ? S::DISABLED : 0);
}
std::vector<Look *> looks;
#ifndef ESP_PLATFORM
bool stale = false;  // break-it (fx::break_stale): looks ignore the theme generation, so a live switch never repaints
#define GEN_OLD(L) (!stale && ((L)->gen != fx::theme_generation() || (L)->fgen != fx::fonts::generation()))
#else
#define GEN_OLD(L) ((L)->gen != fx::theme_generation() || (L)->fgen != fx::fonts::generation())
#endif

void init_defs() {
  if (!defs.empty()) return;
  for (int i = 0; i < fx::gen::N_COLOURS + fx::gen::N_ROLES; i++) {  // colours, then the state roles
    const std::string c = i < fx::gen::N_COLOURS ? fx::gen::COLOURS[i].name : fx::gen::ROLES[i - fx::gen::N_COLOURS].name;
    defs.push_back({"fg:" + c, FG, i}); defs.push_back({"bg:" + c, BG, i});
    defs.push_back({"border:" + c, BORDER, i}); defs.push_back({"line:" + c, LINE, i});
    defs.push_back({"ink:" + c, INK, i});  // an icon layer's recolour (fx::icon)
  }
  for (int i = 0; i < fx::gen::N_SIZES; i++) {
    const std::string n = fx::gen::SIZES[i].name;
    const size_t c = n.find(':');
    const std::string g = n.substr(0, c), v = n.substr(c + 1);
    if (g == "space") { defs.push_back({"pad:" + v, PAD, i}); defs.push_back({"gap:" + v, GAP, i}); defs.push_back({"pad-x:" + v, PADX, i}); defs.push_back({"above:" + v, ABOVE, i}); }
    else if (g == "radius") defs.push_back({"radius:" + v, RADIUS, i});
    else if (g == "layout" || g == "touch") {
      defs.push_back({"h:" + v, HEIGHT, i}); defs.push_back({"max-w:" + v, MAXW, i}); defs.push_back({"w:" + v, WIDTH, i});
      defs.push_back({"min-w:" + v, MINW, i});  // a floor: fills its parent but never narrower (a Cells cell)
      defs.push_back({"max-h:" + v, MAXH, i});  // a ceiling: with h: of the same token = exactly that tall (a Cells cell)
      if (g == "layout") defs.push_back({"gap:" + v, GAP, i});  // gap between children = a layout token
      if (g == "layout") defs.push_back({"above:" + v, ABOVE, i});  // space above a block (a Group after a block)
    }
  }
  for (int i = 0; i < fx::gen::N_TYPES; i++) defs.push_back({std::string("type:") + fx::gen::TYPES[i].name, TYPE, i});
  for (int i = 0; i < (int) (sizeof(SHAPES) / sizeof(SHAPES[0])); i++) defs.push_back({SHAPES[i], SHAPE, i});
  for (int i = 0; i < (int) (sizeof(DERIVED_N) / sizeof(DERIVED_N[0])); i++) defs.push_back({DERIVED_N[i], DERIVED, i});
  auto colour_ix = [](const char *c) {
    for (int i = 0; i < fx::gen::N_COLOURS; i++) if (!strcmp(fx::gen::COLOURS[i].name, c)) return i;
    for (int i = 0; i < fx::gen::N_ROLES; i++) if (!strcmp(fx::gen::ROLES[i].name, c)) return fx::gen::N_COLOURS + i;
    abort();
  };
  for (const char *c : TINTS) defs.push_back({std::string("tint:") + c, TINT, colour_ix(c)});
  for (const char *c : OUTLINES) defs.push_back({std::string("outline:") + c, OUTLINE, colour_ix(c)});
  for (int i = 0; i < (int) (sizeof(fw::ty::CELLS) / sizeof(fw::ty::CELLS[0])); i++) {
    const std::string n = type_name(fw::ty::CELLS[i].style);
    defs.push_back({"digit:" + n, DIGIT, i}); defs.push_back({"colon:" + n, COLON, i}); defs.push_back({"colon-raise:" + n, RAISE, i});
  }
  for (int i = 0; i < (int) (sizeof(BASES) / sizeof(BASES[0])); i++)
    defs.push_back({std::string("baseline:") + type_name(BASES[i].a) + ":" + type_name(BASES[i].b), BASE, i});
  defs.push_back({"icon-px", ICONPX, 0});  // an icon layer that has pixel art: hidden while the theme draws pixel icons
  for (int p = 0; p < G::N_PARTS; p++) {  // "part:card", then "part:card~1" .. "~f" (the combinations, attached together)
    if (!styled(p)) continue;  // drawn parts (knob, tick, arc) are the Pen's, read at draw time
    for (int m = 0; m < 16; m++) {
      static const char HEX[] = "0123456789abcdef";
      const std::string sfx = m ? std::string("~") + HEX[m] : "";
      defs.push_back({std::string("part:") + G::PARTS[p].name + sfx, PART, p * 16 + m});
      if (boxy(p)) defs.push_back({std::string("ptext:") + G::PARTS[p].name + sfx, PTEXT, p * 16 + m});
    }
  }
  std::sort(defs.begin(), defs.end(), [](const Def &a, const Def &b) { return a.name < b.name; });
  for (size_t i = 1; i < defs.size(); i++) if (defs[i].name == defs[i - 1].name) abort();  // a name made twice
  for (size_t i = 0; i < defs.size(); i++)
    if (defs[i].k == PART) part_def[defs[i].i / 16][defs[i].i % 16] = (int) i;
    else if (defs[i].k == PTEXT) ptext_def[defs[i].i / 16][defs[i].i % 16] = (int) i;
  static_assert(fx::gen::N_COLOURS <= 256 && fx::gen::N_TYPES <= 64 && fx::gen::N_SIZES <= 512 && fw::ty::N <= 128, "skin_ix / type_part / size_ix / type_ti");
  for (int i = 0; i < fx::gen::N_COLOURS; i++) {
    skin_ix[i] = -1;
    for (int c = 0; c < G::N_COLOURS; c++) if (!strcmp(G::COLOURS[c], fx::gen::COLOURS[i].name)) skin_ix[i] = c;
    if (skin_ix[i] < 0) abort();  // a colour token the theme schema does not have: gen_theme.mjs is out of date
  }
  // a type style's text part: its own name (title, body, label, value, display), else the nearest kind of text
  for (int i = 0; i < fx::gen::N_TYPES; i++) {
    const std::string n = fx::gen::TYPES[i].name;
    const char *q = "label";
    for (const char *t : {"title", "body", "label", "value", "display"}) if (n == t) q = t;
    if (q == std::string("label")) {
      if (n == "hero" || n == "headline" || n.find("clock") != std::string::npos) q = "display";
      else if (n.find("title") != std::string::npos) q = "title";
      else if (n.find("number") != std::string::npos || n.find("value") != std::string::npos) q = "value";
      else if (n.find("body") != std::string::npos) q = "body";
    }
    type_part[i] = part_ix(q);
  }
  for (int i = 0; i < fx::gen::N_SIZES; i++) {  // fx size token -> the Skin's size index (look sizes come from the theme)
    size_ix[i] = -1;
    for (int j = 0; j < G::N_SIZES; j++) if (!strcmp(G::SIZES[j].name, fx::gen::SIZES[i].name)) size_ix[i] = j;
  }
  for (int j = 0; j < G::N_SIZES; j++) if (!strcmp(G::SIZES[j].name, "stroke:pixel")) ix_pixel = j;
  for (int i = 0; i < fw::ty::N; i++) {  // type style -> its themeable index (tokens/theme.json type), -1 = Facet's size
    type_ti[i] = -1;
    for (int j = 0; j < G::N_TYPES; j++) if (!strcmp(G::TYPES[j].name, fw::ty::STYLES[i].name)) type_ti[i] = j;
  }
}
int index_of(const char *n) {
  init_defs();
  auto it = std::lower_bound(defs.begin(), defs.end(), n, [](const Def &d, const char *k) { return d.name < k; });
  return it != defs.end() && it->name == n ? (int) (it - defs.begin()) : -1;
}
int ix(const char *n) { const int i = index_of(n); if (i < 0) { fprintf(stderr, "fx: unknown token %s\n", n); abort(); } return i; }
void flex(lv_style_t *s, lv_flex_flow_t f, lv_flex_align_t main, lv_flex_align_t cross, lv_flex_align_t track) {
  lv_style_set_layout(s, LV_LAYOUT_FLEX); lv_style_set_flex_flow(s, f);
  lv_style_set_flex_main_place(s, main); lv_style_set_flex_cross_place(s, cross); lv_style_set_flex_track_place(s, track);
}
// fx::tune (the proofs, the workbench Layout lab): a size token as if it said px; "squeeze" = the Body's max width
std::map<std::string, int> tuned;
std::set<std::string> *reads = nullptr;  // fx::uses: the size tokens read while on (styles + pen)
// the structure sizes (layout.json, not in space.json) the core reads live: page, header, overlays (toast, sheet).
// Listed in the lab (tunables) and read by fx::size, so what the core places from them follows a tune too
const std::pair<const char *, int> STRUCTURE[] = {{"layout:inset", fw::lay::inset}, {"layout:gutter", fw::lay::gutter}, {"layout:group", fw::lay::group},
  {"space:3", fw::sp::s3}, {"layout:top", fw::lay::top}, {"layout:header-gap", fw::lay::header_gap}, {"layout:row", fw::lay::row},
  {"layout:back-gap", fw::lay::back_gap}, {"layout:header-item-gap", fw::lay::header_item_gap}, {"layout:header-max-w", fw::lay::header_max_w},
  {"space:2", fw::sp::s2}, {"space:7", fw::sp::s7}, {"space:9", fw::sp::s9}, {"touch:button-h", fw::touch::button_h},
  {"layout:toast-h", fw::lay::toast_h}, {"layout:toast-pad", fw::lay::toast_pad}, {"layout:toast-top", fw::lay::toast_top},
  {"layout:sheet-top", fw::lay::sheet_top}, {"layout:sheet-row-h", fw::lay::sheet_row_h}};
int tok(const char *n, int v) { if (reads) reads->insert(n); auto it = tuned.find(n); return it == tuned.end() ? v : it->second; }
void shape(lv_style_t *s, const char *n) {  // the structure styles: density only, set once per look
  using namespace fw;
  // read lazily: a token counts as read (fx::uses, the lab's list) only by the styles that really use it
  const auto INSET = [] { return tok("layout:inset", lay::inset); };
  const auto GUT = [] { return tok("layout:gutter", lay::gutter); };
  const auto TOP = [] { return tok("layout:top", lay::top); };
  const auto HGAP = [] { return tok("layout:header-gap", lay::header_gap); };
  const auto SQ = [] { return tok("squeeze", -1); };
  const auto ROW = [] { return tok("layout:row", lay::row); };  // the header row (layout.json header)
  const auto IGAP = [] { return tok("layout:header-item-gap", lay::header_item_gap); };
  const auto S = LV_FLEX_ALIGN_START, C = LV_FLEX_ALIGN_CENTER;
  const std::string k = n;
  if (k == "box") { lv_style_set_width(s, lv_pct(100)); lv_style_set_height(s, LV_SIZE_CONTENT); }
  else if (k == "col") flex(s, LV_FLEX_FLOW_COLUMN, S, S, S);
  else if (k == "row") { flex(s, LV_FLEX_FLOW_ROW, S, C, C); lv_style_set_pad_column(s, GUT()); }
  else if (k == "stack") { flex(s, LV_FLEX_FLOW_COLUMN, S, S, S); lv_style_set_pad_row(s, tok("space:2", sp::s2)); }
  else if (k == "centre") { flex(s, LV_FLEX_FLOW_COLUMN, C, C, C); }
  else if (k == "cell" || k == "grow") lv_style_set_flex_grow(s, 1);
  else if (k == "checkbox") {  // the box: layout:checkbox square, radius chip, a stroke:edge border all round
    const int b = tok("layout:checkbox", lay::checkbox);
    lv_style_set_width(s, b); lv_style_set_height(s, b); lv_style_set_radius(s, tok("radius:chip", rad::chip));
    lv_style_set_border_width(s, tok("stroke:edge", stroke::edge)); lv_style_set_border_side(s, LV_BORDER_SIDE_FULL);
  } else if (k == "checkbox-dot") {  // in progress: a round dot layout:checkbox-dot across, centred in the box
    const int d = tok("layout:checkbox-dot", lay::checkbox_dot);
    lv_style_set_width(s, d); lv_style_set_height(s, d); lv_style_set_radius(s, LV_RADIUS_CIRCLE); lv_style_set_align(s, LV_ALIGN_CENTER);
  } else if (k == "radio") {  // the ring: layout:radio round, a stroke:edge border all round
    const int b = tok("layout:radio", lay::radio);
    lv_style_set_width(s, b); lv_style_set_height(s, b); lv_style_set_radius(s, LV_RADIUS_CIRCLE);
    lv_style_set_border_width(s, tok("stroke:edge", stroke::edge)); lv_style_set_border_side(s, LV_BORDER_SIDE_FULL);
  } else if (k == "radio-dot") {  // chosen: a round dot layout:radio-dot across, centred in the ring
    const int d = tok("layout:radio-dot", lay::radio_dot);
    lv_style_set_width(s, d); lv_style_set_height(s, d); lv_style_set_radius(s, LV_RADIUS_CIRCLE); lv_style_set_align(s, LV_ALIGN_CENTER);
  } else if (k == "strike") lv_style_set_text_decor(s, LV_TEXT_DECOR_STRIKETHROUGH);  // a done item, struck through
  else if (k == "fade-switch") {  // Motion pattern "toggle" without a slide: colours and opacities fade, nothing moves
    static lv_style_transition_dsc_t d;
    static const lv_style_prop_t P[] = {LV_STYLE_BG_COLOR, LV_STYLE_BG_OPA, LV_STYLE_BORDER_COLOR, LV_STYLE_OPA, LV_STYLE_TEXT_COLOR,
                                        LV_STYLE_IMAGE_RECOLOR, LV_STYLE_PROP_INV};
    static bool ready = false;
    if (!ready) { mo::transition(&d, st::SWITCH, P); ready = true; }
    lv_style_set_transition(s, &d);
  }
  else if (k == "grow-w") { lv_style_set_width(s, 0); lv_style_set_flex_grow(s, 1); }  // the free width in a row: a text in it ends in "...", never pushes what follows
  else if (k == "page") {  // the inset above the header is the page's; the header gap is the head's margin
    lv_style_set_width(s, lv_pct(100)); lv_style_set_height(s, lv_pct(100)); flex(s, LV_FLEX_FLOW_COLUMN, S, S, S);
    lv_style_set_pad_top(s, INSET());
    if (SQ() >= 0) lv_style_set_flex_cross_place(s, C);  // a squeezed Body sits centred
  } else if (k == "page-head") {  // the header row only (top - inset - header gap): its region is the Header itself,
    // so a mark the body bleeds upward lands in the gap, never on the header (the centre-layout audit fix)
    lv_style_set_width(s, lv_pct(100)); lv_style_set_height(s, TOP() - INSET() - HGAP());
    lv_style_set_pad_hor(s, INSET()); lv_style_set_margin_bottom(s, HGAP());
  }
  else if (k == "body") { lv_style_set_width(s, lv_pct(100)); lv_style_set_flex_grow(s, 1); lv_style_set_pad_hor(s, INSET()); lv_style_set_pad_bottom(s, INSET()); if (SQ() >= 0) lv_style_set_max_width(s, SQ()); }
  else if (k == "body:list") flex(s, LV_FLEX_FLOW_COLUMN, S, S, S);
  else if (k == "body:cards") { flex(s, LV_FLEX_FLOW_ROW_WRAP, S, S, S); lv_style_set_pad_row(s, GUT()); lv_style_set_pad_column(s, GUT()); }
  else if (k == "body:center") flex(s, LV_FLEX_FLOW_COLUMN, C, C, C);
  else if (k == "body:page") flex(s, LV_FLEX_FLOW_COLUMN, S, C, C);
  else if (k == "bleed") lv_style_set_transform_width(s, INSET());
  else if (k == "bg-none") lv_style_set_bg_opa(s, LV_OPA_TRANSP);
  else if (k == "fit") lv_style_set_max_height(s, lv_pct(100));
  else if (k == "fill") { lv_style_set_width(s, lv_pct(100)); lv_style_set_height(s, lv_pct(100)); }  // a component that IS its parent's size (Card)
  else if (k == "fill-w") lv_style_set_width(s, lv_pct(100));
  else if (k == "row-top") flex(s, LV_FLEX_FLOW_ROW, S, S, S);  // a row, top-aligned, no gap
  else if (k == "wrap") flex(s, LV_FLEX_FLOW_ROW_WRAP, S, S, S);
  else if (k == "wrap-end") flex(s, LV_FLEX_FLOW_ROW_WRAP, LV_FLEX_ALIGN_END, C, S);  // rows that wrap: a growing child counts its min-w
  else if (k == "gap:gutter") { lv_style_set_pad_row(s, GUT()); lv_style_set_pad_column(s, GUT()); }  // gutter both ways
  else if (k == "header") {  // the Header row (components/header.cpp): the row tall, up to its max width, items centred;
    // the gaps are its parts' margins (back-gap after Back, the item gap before the end slot and before the Time), so
    // a hidden part takes its gap with it and no gap is a difference of two tokens
    lv_style_set_width(s, lv_pct(100)); lv_style_set_max_width(s, tok("layout:header-max-w", lay::header_max_w)); lv_style_set_height(s, ROW());
    flex(s, LV_FLEX_FLOW_ROW, S, C, C);
  } else if (k == "header-back" || k == "header-end") {  // natural size, the item gap between its items
    lv_style_set_width(s, LV_SIZE_CONTENT); lv_style_set_height(s, LV_SIZE_CONTENT);
    flex(s, LV_FLEX_FLOW_ROW, S, C, C); lv_style_set_pad_column(s, IGAP());
    if (k == "header-back") lv_style_set_margin_right(s, tok("layout:back-gap", lay::back_gap));  // back-gap to the title
    else lv_style_set_margin_left(s, IGAP());
  } else if (k == "header-time") {  // the time's place: the row tall (a long press there is a full touch target)
    lv_style_set_width(s, LV_SIZE_CONTENT); lv_style_set_height(s, ROW()); flex(s, LV_FLEX_FLOW_COLUMN, C, C, C);
    lv_style_set_margin_left(s, IGAP());
  } else if (k == "header-title") {  // the rest of the row: grows from 0, never below its content's minimum (min-w
    // content = the flex minimum of its items: a node at its natural size, the label 0), so the label ellipsises
    // before the end slot and a node in the title hugs (an overfull row is the bounds proof's to report, C11)
    lv_style_set_width(s, 0); lv_style_set_min_width(s, LV_SIZE_CONTENT); lv_style_set_flex_grow(s, 1);
    // the row's height, its items centred on it (a content height would follow the label's text-box translate)
    lv_style_set_height(s, lv_pct(100)); flex(s, LV_FLEX_FLOW_ROW, S, C, C);
  } else if (k == "header-label") {  // the title's label: the title's room (grows from 0, so its minimum is 0)
    lv_style_set_width(s, 0); lv_style_set_flex_grow(s, 1);
  }
  else if (k == "hug") { lv_style_set_width(s, LV_SIZE_CONTENT); lv_style_set_height(s, LV_SIZE_CONTENT); }  // its content's size
  else if (k == "over") flex(s, LV_FLEX_FLOW_COLUMN, C, C, C);  // its children centred (one glyph)
  else if (k == "over-cell") lv_style_set_align(s, LV_ALIGN_CENTER);  // a child taken out of the layout, centred: two glyphs on top of each other
  else if (k.rfind("square:", 0) == 0 || k.rfind("tall:", 0) == 0) {  // exactly a layout token: a button's square, a chip's
    // height (a min size alone leaves a flex row's items at the top of the extra room)
    const bool sq = k[0] == 's';
    const std::string t = "layout:" + k.substr(sq ? 7 : 5);
    int v = 0;
    for (int i = 0; i < fx::gen::N_SIZES; i++) if (t == fx::gen::SIZES[i].name) v = tok(t.c_str(), fx::gen::SIZES[i].v);
    lv_style_set_height(s, v);
    if (sq) lv_style_set_width(s, v);
  }
  else if (k == "cell-text") { lv_style_set_width(s, lv_pct(100)); lv_style_set_text_align(s, LV_TEXT_ALIGN_CENTER); }  // a digit in its cell
  else if (k == "cells") flex(s, LV_FLEX_FLOW_ROW, C, S, S);  // a row of fixed boxes centred in it, no gap (the Time)
  else if (k == "no-outline") { lv_style_set_outline_width(s, 0); lv_style_set_outline_opa(s, LV_OPA_TRANSP); }
  else if (k == "transp" || k == "opaque") lv_style_set_opa(s, k == "transp" ? LV_OPA_TRANSP : LV_OPA_COVER);  // a part shown per state
  else if (k == "fade-press-ink" || k == "fade-release-ink") {  // the press fade of an icon's ink (states.json pressed motion)
    static lv_style_transition_dsc_t d[2];
    static const lv_style_prop_t P[] = {LV_STYLE_IMAGE_RECOLOR, LV_STYLE_PROP_INV};
    static bool ready = false;
    if (!ready) { fw::st::press_transitions(d, P); ready = true; }
    lv_style_set_transition(s, &d[k == "fade-press-ink"]);
  }
  else if (k == "pair") {  // two halves sharing a cell (a grow between them): the first at its top, the second on its bottom
    lv_style_set_width(s, lv_pct(100)); lv_style_set_height(s, lv_pct(100)); flex(s, LV_FLEX_FLOW_COLUMN, S, S, S);
  }
  else if (k == "row-mid") flex(s, LV_FLEX_FLOW_ROW, S, C, C);  // a row, items centred on it, no gap of its own (a gap:<n> sets it)
  else if (k == "row-end") {  // a row's end part (the List row's end slot): its content's width, the row's full content
    // height, its items centred on it one by one (as if they sat in the row itself: each lands where the row centres it)
    lv_style_set_width(s, LV_SIZE_CONTENT); lv_style_set_height(s, lv_pct(100)); flex(s, LV_FLEX_FLOW_ROW, S, C, C);
  }
  else if (k == "rule-top") {  // the List row's hairline on TOP (after a line:<c>, which draws it at the bottom); the same
    // inset at the bottom keeps the content box centred on the row (centred parts stay on the 4 px grid)
    lv_style_set_border_side(s, LV_BORDER_SIDE_TOP); lv_style_set_pad_bottom(s, tok("stroke:hair", stroke::hair));
  }
  else if (k == "fade-press-fill" || k == "fade-release-fill") {  // a press fill over nothing (List row, Section header
    // action): the fill's opacity fades in / out (states.json pressed motion)
    static lv_style_transition_dsc_t d[2];
    static const lv_style_prop_t P[] = {LV_STYLE_BG_OPA, LV_STYLE_PROP_INV};
    static bool ready = false;
    if (!ready) { fw::st::press_transitions(d, P); ready = true; }
    lv_style_set_transition(s, &d[k == "fade-press-fill"]);
  }
  else if (k == "fade-press" || k == "fade-release") {  // states.json pressed motion: the bg / border fade of a press
    static lv_style_transition_dsc_t d[2];
    static const lv_style_prop_t P[] = {LV_STYLE_BG_COLOR, LV_STYLE_BORDER_COLOR, LV_STYLE_PROP_INV};
    static bool ready = false;
    if (!ready) { fw::st::press_transitions(d, P); ready = true; }
    lv_style_set_transition(s, &d[k == "fade-press"]);
  }  // a drawn box never outgrows its parent's height  // a state with no fill (an outline button)
  // ---- the controls (renderer/CONTRACT.md: fixed touch sizes from tokens; their colours are the colour styles) ----
  const auto HAIR = [] { return tok("stroke:hair", stroke::hair); };
  const auto TMIN = [] { return tok("touch:min", touch::min); };
  const auto S2 = [] { return tok("space:2", sp::s2); };
  if (k == "toggle") {  // the Toggle track: toggle-w x touch:min, a pill, the knob inset space 2 all round
    lv_style_set_width(s, tok("layout:toggle-w", lay::toggle_w)); lv_style_set_height(s, TMIN());
    lv_style_set_radius(s, LV_RADIUS_CIRCLE); lv_style_set_pad_all(s, S2());
  } else if (k == "toggle-off-pad") lv_style_set_pad_all(s, S2() - HAIR());  // disabled off: the hair border is inside, the knob keeps its place
  else if (k == "toggle-knob") {
    const int kn = tok("layout:toggle-knob", lay::toggle_knob);
    lv_style_set_width(s, kn); lv_style_set_height(s, kn); lv_style_set_radius(s, LV_RADIUS_CIRCLE);
  } else if (k == "toggle-knob-on") {  // on: the knob moved by the travel (track - knob - 2 x inset)
    lv_style_set_translate_x(s, tok("layout:toggle-w", lay::toggle_w) - tok("layout:toggle-knob", lay::toggle_knob) - 2 * S2());
  } else if (k == "toggle-slide") {  // Motion pattern "toggle": the knob slides and the colours fade together
    static lv_style_transition_dsc_t d;
    static const lv_style_prop_t P[] = {LV_STYLE_BG_COLOR, LV_STYLE_BORDER_COLOR, LV_STYLE_TRANSLATE_X, LV_STYLE_PROP_INV};
    static bool ready = false;
    if (!ready) { mo::transition(&d, st::SWITCH, P); ready = true; }
    lv_style_set_transition(s, &d);
  } else if (k == "word-slot") {  // the word Unavailable in place of a control: its width, touch:min tall, centred
    lv_style_set_width(s, LV_SIZE_CONTENT); lv_style_set_height(s, TMIN()); flex(s, LV_FLEX_FLOW_ROW, S, C, C);
  } else if (k == "badge") {  // the Count badge pill: badge-h tall and at least as wide (one digit = a circle)
    const int bh = tok("layout:badge-h", lay::badge_h);
    lv_style_set_width(s, LV_SIZE_CONTENT); lv_style_set_height(s, bh); lv_style_set_min_width(s, bh);
    lv_style_set_pad_hor(s, S2()); lv_style_set_radius(s, LV_RADIUS_CIRCLE); flex(s, LV_FLEX_FLOW_ROW, C, C, C);
  } else if (k == "badge-num") {  // the number: at least the pill's inner square (badge-h - 2 x space 2), centred
    lv_style_set_min_width(s, tok("layout:badge-h", lay::badge_h) - 2 * S2()); lv_style_set_text_align(s, LV_TEXT_ALIGN_CENTER);
  } else if (k == "button-sm") {  // the small Button (after "button"): touch:min tall, the label space 4 in from the outer edge
    lv_style_set_height(s, TMIN()); lv_style_set_pad_hor(s, tok("space:4", sp::s4) - HAIR());
  } else if (k == "button") {  // the Button: touch:button-h tall, its label's width, the label space 7 in from the
    // outer edge (the hair border on every kind is inside), icon to label space 2
    lv_style_set_width(s, LV_SIZE_CONTENT); lv_style_set_height(s, tok("touch:button-h", touch::button_h));
    lv_style_set_pad_hor(s, tok("space:7", sp::s7) - HAIR()); lv_style_set_pad_column(s, S2());
    flex(s, LV_FLEX_FLOW_ROW, C, C, C);
  } else if (k == "button-fill") lv_style_set_width(s, lv_pct(100));
  else if (k == "border-clear") {  // a hair border that does not show (primary / ghost): the boxes stay the same
    lv_style_set_border_width(s, HAIR()); lv_style_set_border_opa(s, LV_OPA_TRANSP); lv_style_set_border_side(s, LV_BORDER_SIDE_FULL);
  } else if (k == "fade-press-all" || k == "fade-release-all") {  // the Button's press fade: fill, edge, label, icon
    static lv_style_transition_dsc_t d[2];
    static const lv_style_prop_t P[] = {LV_STYLE_BG_COLOR, LV_STYLE_BG_OPA, LV_STYLE_BORDER_COLOR, LV_STYLE_BORDER_OPA,
                                        LV_STYLE_TEXT_COLOR, LV_STYLE_IMAGE_RECOLOR, LV_STYLE_PROP_INV};
    static bool ready = false;
    if (!ready) { st::press_transitions(d, P); ready = true; }
    lv_style_set_transition(s, &d[k == "fade-press-all"]);
  } else if (k == "btn-underline") {  // the ghost Button's active mark: space 9 x stroke edge, space 2 above the content bottom
    lv_style_set_width(s, tok("space:9", sp::s9)); lv_style_set_height(s, tok("stroke:edge", stroke::edge));
    lv_style_set_align(s, LV_ALIGN_BOTTOM_MID); lv_style_set_translate_y(s, -S2());
  } else if (k == "seg") {  // the Segmented box: touch:min tall, the segments side by side (no gap)
    lv_style_set_height(s, TMIN()); flex(s, LV_FLEX_FLOW_ROW, S, S, S);
  } else if (k == "seg-part") {  // one segment: the box's height, its label + space 7 each side, the label centred
    // (the room is the label's margin, so the plates and the separator, which fill the segment, cover it all)
    lv_style_set_width(s, LV_SIZE_CONTENT); lv_style_set_height(s, lv_pct(100)); flex(s, LV_FLEX_FLOW_ROW, C, C, C);
  } else if (k == "seg-label") {  // space 7 each side; its text centred in its grid-rounded box (fx::fit_grid)
    lv_style_set_margin_left(s, tok("space:7", sp::s7)); lv_style_set_margin_right(s, tok("space:7", sp::s7));
    lv_style_set_text_align(s, LV_TEXT_ALIGN_CENTER);
  } else if (k == "seg-grow") { lv_style_set_width(s, 0); lv_style_set_flex_grow(s, 1); }  // width fill: equal segments
  else if (k == "seg-sep") { lv_style_set_width(s, HAIR()); lv_style_set_height(s, lv_pct(100)); }  // the separator line (out of the layout)
  else if (k == "seg-plate") { lv_style_set_width(s, lv_pct(100)); lv_style_set_height(s, lv_pct(100)); }
  else if (k == "seg-half-l" || k == "seg-half-r") {  // an end plate's square inner edge (LVGL: one radius per box):
    // a radius:button wide strip on the segment's inner side over the rounded plate's corners (on the grid)
    lv_style_set_width(s, tok("radius:button", rad::button)); lv_style_set_height(s, lv_pct(100));
    lv_style_set_align(s, k == "seg-half-l" ? LV_ALIGN_LEFT_MID : LV_ALIGN_RIGHT_MID);
  } else if (k.rfind("ring-", 0) == 0) {  // the chosen segment's line-hi ring: 2 hairlines on the box's outer sides (the
    // box line covers the outer one: the ring shows just inside it), 1 on the separator sides
    const bool two = k == "ring-full-2" || k == "ring-tb-2";
    lv_style_set_border_width(s, two ? 2 * HAIR() : HAIR());
    lv_style_set_border_side(s, k == "ring-full-2" ? LV_BORDER_SIDE_FULL : k == "ring-tb-2" ? (lv_border_side_t) (LV_BORDER_SIDE_TOP | LV_BORDER_SIDE_BOTTOM)
                                : k == "ring-l" ? LV_BORDER_SIDE_LEFT : k == "ring-r" ? LV_BORDER_SIDE_RIGHT
                                : (lv_border_side_t) (LV_BORDER_SIDE_LEFT | LV_BORDER_SIDE_RIGHT));
  } else if (k == "slider") {  // the Slider: its parent's width, exactly touch:min tall (the Pen draws in it)
    lv_style_set_width(s, lv_pct(100)); lv_style_set_height(s, TMIN());
  } else if (k == "text-mid") lv_style_set_text_align(s, LV_TEXT_ALIGN_CENTER);
  else if (k == "slider-word") {  // the word Unavailable where the track starts (half a knob in)
    lv_style_set_align(s, LV_ALIGN_LEFT_MID); lv_style_set_translate_x(s, tok("layout:slider-knob", lay::slider_knob) / 2);
  }
}
lv_color_t colour(int i, const fw::Theme *t) {  // a colour token or a state role, resolved in theme t
  return i < fx::gen::N_COLOURS ? t->*fx::gen::COLOURS[i].f : fx::gen::ROLES[i - fx::gen::N_COLOURS].f(*t);
}
int box_of(fw::ty::Id id) { return fw::ty::STYLES[id].box; }
lv_color_t hex(uint32_t c) { return lv_color_hex(c); }
// ---- theme fonts (C49): a type style's font in a Skin. Its font role's family at the theme's size for the style
// (themeable styles, within [floor, box]) else Facet's; built in (core/fonts.cpp THEME_FONTS) or downloaded by the panel
// (core/font_store.h: until the file is there, the built-in fallback draws). Facet's family at Facet's size = fw::font
// (so Large text and S1 hold). The line box never changes (C45): only the font inside it.
const std::vector<uint32_t> &role_need(int r) {  // the code points the role's styles draw (a download's coverage check)
  static std::vector<uint32_t> need[S::N_FONT_ROLES];
  if (need[r].empty()) for (int i = 0;; i++) { const uint32_t c = fw::role_glyph(r, i); if (!c) break; need[r].push_back(c); }
  return need[r];
}
const lv_font_t *skin_font(const S::Skin *k, fw::ty::Id id) {
  const lv_font_t *facet = fw::font(id);
  if (!k || id < 0 || id >= fw::ty::N) return facet;
  const int r = fw::style_role(id), ti = type_ti[id];
  const int px = ti >= 0 ? k->type[DN][ti] : fw::ty::STYLES[id].size;
  const lv_font_t *f = k->font[r] == G::FONT_DEFAULT[r] && px == fw::ty::STYLES[id].size ? facet : fw::theme_font(G::FAMILIES[k->font[r]], id, px);
  if (!f) f = facet;  // a size this build lacks (a sent theme): Facet's font in the same box
  if (k->font_src[r])
    if (const lv_font_t *d = fx::fonts::get(k->font_url[r], k->font_family[r], px, role_need(r), k->font_fallback[r], f)) return d;
  return f;
}
int cap_px(const lv_font_t *f) {  // as core/fw_core.cpp: cap height above the baseline ('H', else '0')
  lv_font_glyph_dsc_t g;
  for (uint32_t c : {(uint32_t) 'H', (uint32_t) '0'})
    if (lv_font_get_glyph_dsc(f, &g, c, 0) && g.box_h > 0) return g.box_h + g.ofs_y;
  return (int) lroundf(lv_font_get_line_height(f) * 0.6f);
}
// fw::text_metrics for any font: the caps centred in the style's fixed line box (the text-box rule)
void metrics(fw::ty::Id s, const lv_font_t *f, int &line_space, int &pad_top, int &pad_bottom, int &translate_y) {
  if (f == fw::font(s)) { fw::text_metrics(s, line_space, pad_top, pad_bottom, translate_y); return; }
  const int box = fw::ty::STYLES[s].box, lh = lv_font_get_line_height(f), asc = lh - f->base_line;
  const int off = (int) lroundf((box + cap_px(f)) / 2.0f) - asc;
  line_space = box - lh;
  pad_top = off > 0 ? off : 0;
  pad_bottom = -(std::max(0, pad_top + lh - box) + 2);
  translate_y = off < 0 ? off : 0;
}
int snap_cell(const S::Skin *k) { return k && k->snap && ix_pixel >= 0 ? std::max<int>(1, k->size[DN][ix_pixel]) : 0; }
// how far a part's solid edge layers reach in, over its blocks (the dotted focus rectangle is drawn inside: not counted)
int solid_reach(const S::Skin &k, int p) {
  int r = 0;
  for (const S::Block &b : k.state[p])
    for (int i = 0; (b.has & S::K_EDGES) && i < b.n_edges; i++) { const S::Edge &e = k.edges[b.edge0 + i]; if (!e.dotted) r = std::max(r, e.inset.px[DN] + e.w.px[DN]); }
  return r;
}
// C45 moved padding: a flush bar (parts.header / parts.card-head flush) runs to its frame's edges. The frame keeps only
// what its edges need (e); the rest of its padding moves into the bar (sides) and into the body (top / sides / bottom);
// the bar's own vertical padding (pad, v) is taken from the gap under it. Every sum stays, so the content box of the
// header row, the body and the card's content are exactly Facet's (the theme guard bounds v; P2 / P6 prove the fit).
void moves(Look *L, lv_style_t *s, int i) {
  using namespace fw;
  const S::Skin &k = *L->skin;
  if (i == 24) {  // the Segmented's plates, separators and ring: gone when the theme draws the segment part itself
    const S::Block b = k.merged(G::P_SEGMENT, 0);
    if (b.has & (S::K_FACE | S::K_EDGES)) lv_style_set_opa(s, LV_OPA_TRANSP);
    return;
  }
  const bool card = i >= 21;
  const S::Block &h = k.state[card ? G::P_CARD_HEAD : G::P_HEADER][0];
  if (!h.flush) return;
  const int v = (h.has & S::K_PAD) ? h.pad.px[DN] : 0;
  if (card) {
    const int P = tok("layout:card-pad", lay::card_pad) - tok("stroke:hair", stroke::hair), e = solid_reach(k, G::P_CARD);
    if (i == 21) lv_style_set_pad_all(s, e);                                                             // the card: its edges only
    else if (i == 22) { lv_style_set_pad_hor(s, P - e); lv_style_set_pad_ver(s, v); }                   // the head bar
    else { lv_style_set_pad_hor(s, P - e); lv_style_set_pad_bottom(s, P - e); lv_style_set_pad_top(s, P - e - 2 * v); }  // the body
    return;
  }
  const int inset = tok("layout:inset", lay::inset), hgap = tok("layout:header-gap", lay::header_gap), e = solid_reach(k, G::P_PAGE);
  if (i == 17) lv_style_set_pad_top(s, e);  // the page: its frame edges only
  else if (i == 18) {                       // the head: the bar's region, its gap less the bar's padding
    lv_style_set_pad_hor(s, e); lv_style_set_height(s, tok("layout:top", lay::top) - inset - hgap + 2 * v); lv_style_set_margin_bottom(s, hgap - 2 * v);
  } else if (i == 19) lv_style_set_pad_top(s, inset - e);  // the body: the inset the page gave up
  else {                                                     // the Header bar: the side insets inside it, v above and below
    lv_style_set_pad_hor(s, inset - e); lv_style_set_pad_ver(s, v); lv_style_set_height(s, tok("layout:row", lay::row) + 2 * v);
    lv_style_set_max_width(s, tok("layout:header-max-w", lay::header_max_w) + 2 * (inset - e));
  }
}
// a part's style for one combination: what its merged block sets, nothing else (text = only the text ink)
void part_style(Look *L, lv_style_t *s, int p, int m, bool text_only) {
  lv_style_reset(s);  // a key the new theme does not set must go
  lv_style_init(s);
  const S::Skin &k = *L->skin;
  const S::Block b = k.merged(p, m);
  const int lt = L->th.light, set = L->th.accent_set;
  auto ink = [&](const S::Ink &i) { return hex(k.ink(i, lt, set)); };
  if (p == G::P_SWITCH) {  // shape check: the switch's own track + knob go; the core draws the box + tick (switch_draw)
    if ((b.has & S::K_SHAPE) && b.shape == 1) { lv_style_set_bg_opa(s, LV_OPA_TRANSP); lv_style_set_border_opa(s, LV_OPA_TRANSP); lv_style_set_shadow_opa(s, LV_OPA_TRANSP); }
    return;
  }
  if (text_only || !boxy(p)) {  // a text / icon part, or the text ink of a box part: labels and icon ink layers take it
    if (b.has & S::K_TEXT) {
      lv_style_set_text_color(s, ink(b.text));
      lv_style_set_image_recolor(s, ink(b.text)); lv_style_set_image_recolor_opa(s, LV_OPA_COVER);
    }
    return;
  }
  if (b.has & S::K_FACE) {  // the face replaces the component's fill (a pattern's second ink: drawn by part_draw)
    lv_style_set_bg_color(s, ink(b.face.a)); lv_style_set_bg_opa(s, LV_OPA_COVER);
    if (b.face.fill == S::FILL_GRADIENT) { lv_style_set_bg_grad_color(s, ink(b.face.b)); lv_style_set_bg_grad_dir(s, b.face.down ? LV_GRAD_DIR_VER : LV_GRAD_DIR_HOR); }
    else lv_style_set_bg_grad_dir(s, LV_GRAD_DIR_NONE);
  }
  if (b.has & S::K_RADIUS) lv_style_set_radius(s, b.radius.px[DN]);
  // the theme's edges replace the component's border (drawn by part_draw); its width stays, so nothing moves (C45)
  if (b.has & S::K_EDGES) lv_style_set_border_opa(s, LV_OPA_TRANSP);
}
void palette(Look *L, const fw::Theme *t) {  // th = t's colours through the live Skin (mode x accent set)
  L->skin = fx::theme_live(); L->gen = fx::theme_generation(); L->fgen = fx::fonts::generation();
  live_raw = L->skin.get();
  L->th = *t;
  for (int i = 0; i < fx::gen::N_COLOURS; i++) L->th.*fx::gen::COLOURS[i].f = hex(L->skin->rgb(t->light, t->accent_set, skin_ix[i]));
}
void paint(Look *L, const fw::Theme *t, int only = -1) {  // colours from the theme, type from the text-size setting (only = one def)
  L->t = t; L->large = fw::text_large();
  if (only < 0 || !L->skin) palette(L, t);
  const fw::Theme *th = &L->th;  // every colour below: the skinned palette
  for (size_t i = 0; i < defs.size(); i++) {
    if (only >= 0 && (int) i != only) continue;
    lv_style_t *s = &L->s[i];
    const Def &d = defs[i];
    if (d.k == PART || d.k == PTEXT) { part_style(L, s, d.i / 16, d.i % 16, d.k == PTEXT); continue; }
    if (d.k == ICONPX) { lv_style_reset(s); lv_style_init(s); if (L->skin->icons == 1) lv_style_set_opa(s, LV_OPA_TRANSP); continue; }
    if (d.k == DERIVED) {
      // dial-wing: the Dial's centre column is wing + value + wing, so the value's middle is the dial's middle (the
      // approved geometry). Above: caption + space 7; below: space 2 + unit + space 9 + the button. Both wings take
      // the larger, the caption sits at the top wing's foot and the unit row at the bottom wing's head.
      using namespace fw;
      if (d.i >= 17 && d.i <= 24) { lv_style_reset(s); lv_style_init(s); moves(L, s, d.i); continue; }  // they follow the live theme
      if (d.i == 3) {  // card-pad: children count from the outer edge (layout:card-pad less the border hair, = docs)
        lv_style_set_pad_all(s, tok("layout:card-pad", lay::card_pad) - tok("stroke:hair", stroke::hair)); continue;
      }
      if (d.i == 4) {  // card-text: up to two lines of its type (type:title), then "..."
        lv_style_set_width(s, lv_pct(100)); lv_style_set_height(s, LV_SIZE_CONTENT); lv_style_set_max_height(s, 2 * box_of(ty::title)); continue;
      }
      if (d.i == 8) {  // pair-rule: the hairline between two halves, inset by the card padding (its child = the ink)
        lv_style_set_width(s, lv_pct(100)); lv_style_set_height(s, tok("stroke:hair", stroke::hair));
        lv_style_set_pad_hor(s, tok("layout:card-pad", lay::card_pad)); continue;
      }
      if (d.i >= 10 && d.i <= 14) {  // list-text-* / list-slot (List row): the text column (title, + sub / over line) or a
        // body slot, grown so that (row - it) is a multiple of 2 x space 1: centred in the row, every box top lands on
        // the 4 px grid (480: title 28 -> 32, body 20 -> 24)
        const int row = tok("layout:list-row-h", lay::list_row_h), g = std::max(1, 2 * tok("space:1", sp::s1));  // a lab drag to 0: no grid
        const int h = d.i == 14 ? box_of(ty::body) : box_of(ty::title) + (d.i == 11 || d.i == 13 ? box_of(ty::body) : 0) +
                                                      (d.i == 12 || d.i == 13 ? box_of(ty::label) : 0);
        lv_style_set_height(s, h + ((row - h) % g + g) % g);
        if (d.i == 14) { lv_style_set_width(s, LV_SIZE_CONTENT); continue; }
        lv_style_set_width(s, 0); lv_style_set_flex_grow(s, 1);  // the free width: the text ellipsises, never pushes the end
        flex(s, LV_FLEX_FLOW_COLUMN, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START); continue;
      }
      if (d.i == 15) {  // section-label: a Section header in the screen label type: its box + space 2 to the rows, at the top
        lv_style_set_height(s, box_of(ty::label) + tok("space:2", sp::s2));
        flex(s, LV_FLEX_FLOW_ROW, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START); continue;
      }
      if (d.i == 25) {  // more-row: more-row-h tall; top space 7 less the hairline on top (LVGL counts the border in the content offset)
        lv_style_set_height(s, tok("layout:more-row-h", lay::more_row_h));
        lv_style_set_pad_top(s, tok("space:7", sp::s7) - tok("stroke:hair", stroke::hair)); continue;
      }
      if (d.i == 26) {  // tile-text: two lines of type body in a fixed box (the status below never moves)
        lv_style_set_width(s, lv_pct(100)); lv_style_set_height(s, 2 * box_of(ty::body)); continue;
      }
      if (d.i == 27) {  // tile-status: one line of type label, space 5 above it
        lv_style_set_width(s, lv_pct(100)); lv_style_set_height(s, box_of(ty::label));
        lv_style_set_margin_top(s, tok("space:5", sp::s5)); continue;
      }
      if (d.i == 28) {  // readout-pad: layout:card-pad top and sides, nothing at the bottom
        const int P = tok("layout:card-pad", lay::card_pad);
        lv_style_set_pad_top(s, P); lv_style_set_pad_hor(s, P); lv_style_set_pad_bottom(s, 0); continue;
      }
      if (d.i == 29 || d.i == 30) {  // hero-col(-title): the free width, its lines space 2 apart, centred on the hero box
        const int s2 = tok("space:2", sp::s2), h = box_of(d.i == 30 ? ty::title : ty::label) + s2 + box_of(ty::body);
        lv_style_set_width(s, 0); lv_style_set_flex_grow(s, 1); lv_style_set_height(s, LV_SIZE_CONTENT);  // a theme's taller type grows it
        flex(s, LV_FLEX_FLOW_COLUMN, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        lv_style_set_pad_row(s, s2); lv_style_set_margin_top(s, (box_of(ty::hero) - h) / 2); continue;
      }
      if (d.i == 31 || d.i == 32) {  // hero-row(-display): the row the number's type box tall (hero / display)
        lv_style_set_height(s, box_of(d.i == 32 ? ty::display : ty::hero));
        flex(s, LV_FLEX_FLOW_ROW, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        lv_style_set_pad_column(s, tok("layout:gutter", lay::gutter)); continue;
      }
      if (d.i == 33) {  // toast: layout toast-h tall, the full width it is given; padding toast-pad less the edge hair
        const int P = tok("layout:toast-pad", lay::toast_pad);
        lv_style_set_width(s, lv_pct(100)); lv_style_set_height(s, tok("layout:toast-h", lay::toast_h));
        lv_style_set_pad_all(s, P - tok("stroke:hair", stroke::hair)); lv_style_set_pad_column(s, P);
        flex(s, LV_FLEX_FLOW_ROW, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER); continue;
      }
      if (d.i == 16) {  // empty-pad: the Empty state's padding from the outer edge (less the border hair)
        lv_style_set_pad_all(s, tok("layout:empty-pad", lay::empty_pad) - tok("stroke:hair", stroke::hair)); continue;
      }
      if (d.i == 9) {  // label-row: at least the label type's box tall (a hugging box around one label: its caps translate
        // would otherwise take a pixel off the box, off the grid)
        lv_style_set_min_height(s, box_of(ty::label)); continue;
      }
      if (d.i == 7) {  // card-row: the label row is at least the label's type box tall (its caps translate stays inside)
        lv_style_set_min_height(s, box_of(ty::label)); continue;
      }
      if (d.i == 5 || d.i == 6) {  // card-gap: space 9 between the value and the sub line (value bottom = 88 / 56 from the edge); -s = the compact step's space 2
        lv_style_set_height(s, d.i == 5 ? tok("space:9", sp::s9) : tok("space:2", sp::s2)); continue;
      }
      if (d.i == 2) {  // dial-slot: the value's box (type display), holding a word in its middle (Unavailable)
        lv_style_set_width(s, lv_pct(100)); lv_style_set_height(s, box_of(ty::display));
        flex(s, LV_FLEX_FLOW_COLUMN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        continue;
      }
      const int s2 = tok("space:2", sp::s2), s7 = tok("space:7", sp::s7), s9 = tok("space:9", sp::s9);
      const int up = box_of(ty::label) + s7, down = s2 + box_of(ty::body) + s9 + tok("touch:button-h", touch::button_h);
      const bool top = d.i == 0;
      lv_style_set_width(s, lv_pct(100)); lv_style_set_height(s, up > down ? up : down);
      flex(s, LV_FLEX_FLOW_COLUMN, top ? LV_FLEX_ALIGN_END : LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
      if (top) lv_style_set_pad_bottom(s, s7); else { lv_style_set_pad_top(s, s2); lv_style_set_pad_row(s, s9); }
      continue;
    }
    if (d.k == TINT) { lv_style_set_bg_color(s, colour(d.i, th)); lv_style_set_bg_opa(s, (lv_opa_t) lroundf(fw::tokens::comp::chip_tint * 255)); continue; }
    if (d.k == OUTLINE) {
      lv_style_set_outline_color(s, colour(d.i, th)); lv_style_set_outline_opa(s, LV_OPA_COVER);
      lv_style_set_outline_width(s, tok("stroke:focus", fw::stroke::focus)); lv_style_set_outline_pad(s, tok("touch:focus-offset", fw::touch::focus_offset) - tok("stroke:focus", fw::stroke::focus));  // the ring's outer edge at the offset
      continue;
    }
    if (d.k == DIGIT || d.k == COLON || d.k == RAISE) {  // type.json cells: em of the type's size
      const fw::ty::Cells &k = fw::ty::CELLS[d.i];
      const fw::ty::Style &st = fw::ty::STYLES[k.style];
      if (d.k == RAISE) {
        int ls, pt, pb, ty;
        metrics(k.style, skin_font(L->skin.get(), k.style), ls, pt, pb, ty);
        lv_style_set_translate_y(s, ty + (int) lroundf(k.colon_y * st.size));
      } else {
        lv_style_set_width(s, (int) lroundf((d.k == COLON ? k.colon : k.digit) * st.size)); lv_style_set_height(s, st.box);
        flex(s, LV_FLEX_FLOW_ROW, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
      }
      continue;
    }
    if (d.k == BASE) {  // where b's figures end below its box top, against a's (cap height centred in the box)
      const Base &b = BASES[d.i];
      auto cap_bottom = [L](fw::ty::Id id, uint32_t ch) {
        const lv_font_t *f = skin_font(L->skin.get(), id);
        lv_font_glyph_dsc_t g;
        const int cap = lv_font_get_glyph_dsc(f, &g, ch, 0) ? g.box_h : f->line_height / 2;
        return (fw::ty::STYLES[id].box + cap) / 2;
      };
      lv_style_set_margin_top(s, cap_bottom(b.a, '0') - cap_bottom(b.b, b.ch));
      continue;
    }
    if (d.k <= INK) {
      const lv_color_t c = colour(d.i, th);
      if (d.k == FG) lv_style_set_text_color(s, c);
      else if (d.k == INK) { lv_style_set_image_recolor(s, c); lv_style_set_image_recolor_opa(s, LV_OPA_COVER); }
      else if (d.k == BG) { lv_style_set_bg_color(s, c); lv_style_set_bg_opa(s, LV_OPA_COVER); }
      else {
        lv_style_set_border_color(s, c); lv_style_set_border_width(s, tok("stroke:hair", fw::stroke::hair)); lv_style_set_border_opa(s, LV_OPA_COVER);
        lv_style_set_border_side(s, d.k == LINE ? LV_BORDER_SIDE_BOTTOM : LV_BORDER_SIDE_FULL);
      }
    } else if (d.k == TYPE) {
      const fw::ty::Id id = fx::gen::TYPES[d.i].id;
      int ls, pt, pb, ty;
      const lv_font_t *f = skin_font(L->skin.get(), id);
      metrics(id, f, ls, pt, pb, ty);
      lv_style_set_text_font(s, f);
      lv_style_set_text_letter_space(s, fw::ty::STYLES[id].letter_space);
      lv_style_set_text_line_space(s, ls); lv_style_set_pad_top(s, pt); lv_style_set_pad_bottom(s, pb);
      lv_style_set_translate_y(s, ty); lv_style_set_height(s, fw::ty::STYLES[id].box);
    }
  }
}
void sized(lv_style_t *s, const Def &d) {  // the size + structure styles (density, tuned): at make, again on tune
  const int v = d.k >= PAD && d.k <= WIDTH ? tok(fx::gen::SIZES[d.i].name, fx::gen::SIZES[d.i].v) : 0;
  if (d.k == PAD) lv_style_set_pad_all(s, v);
  else if (d.k == GAP) { lv_style_set_pad_row(s, v); lv_style_set_pad_column(s, v); }
  else if (d.k == PADX) lv_style_set_pad_hor(s, v);
  else if (d.k == RADIUS) lv_style_set_radius(s, v);
  else if (d.k == HEIGHT) lv_style_set_min_height(s, v);
  else if (d.k == MAXH) lv_style_set_max_height(s, v);
  else if (d.k == MINW) lv_style_set_min_width(s, v);
  else if (d.k == MAXW) lv_style_set_max_width(s, v);
  else if (d.k == WIDTH) lv_style_set_width(s, v);
  else if (d.k == ABOVE) lv_style_set_margin_top(s, v);
  else if (d.k == SHAPE) shape(s, d.name.c_str());
}
Look *make(lv_display_t *d, const fw::Theme *t) {
  init_defs();
  fx::fonts::on_change(fx::restyle);  // a downloaded theme font landed (or was refused): every look repaints live
  Look *L = new Look;
  L->d = d;
  L->s.resize(defs.size());  // never resized again: objects hold these addresses
  for (size_t i = 0; i < defs.size(); i++) {
    lv_style_t *s = &L->s[i];
    lv_style_init(s);
    sized(s, defs[i]);
  }
  paint(L, t);
  looks.push_back(L);
  // the display goes: delete its objects now (as lv_display_delete would next), so none still holds a shared style,
  // then free the look (ASan: a look per stage leaked 137 KB over the workbench driver)
  lv_display_add_event_cb(d, [](lv_event_t *e) {
    auto *L = (Look *) lv_event_get_user_data(e);
    lv_display_t *d = L->d;
    for (lv_obj_t **o : {&d->sys_layer, &d->top_layer, &d->bottom_layer}) if (*o) { lv_obj_delete(*o); *o = nullptr; }
    d->act_scr = nullptr;
    while (d->screen_cnt) lv_obj_delete(d->screens[0]);
    for (lv_style_t &st : L->s) lv_style_reset(&st);
    looks.erase(std::find(looks.begin(), looks.end(), L));
    delete L;
  }, LV_EVENT_DELETE, L);
  return L;
}
Look *look_of(lv_obj_t *o) {
  lv_display_t *d = o ? lv_obj_get_display(o) : nullptr;
  if (!d) d = lv_display_get_default();
  for (Look *L : looks) if (L->d == d) {
    if (fw::T && (L->t != fw::T || L->large != (int) fw::text_large() || GEN_OLD(L))) { paint(L, fw::T); lv_obj_report_style_change(nullptr); }
    return L;
  }
  return make(d, fw::T ? fw::T : fw::theme(false));
}
// drawn parts: inks read from the object's look at draw time
lv_color_t ink(lv_obj_t *o, const char *n) {
  const int i = ix(n);
  if (defs[i].k > LINE) abort();
  return colour(defs[i].i, &look_of(o)->th);
}
// ---- parts at draw time: edges, shadows, pattern faces (box parts) and text shadows; no extra objects (C24 core-only)
std::map<lv_obj_t *, int> part_of;  // an object -> the box part on it (its texts and icon inks take the part's text ink)
void edge_bar(lv_layer_t *ly, int x1, int y1, int x2, int y2, lv_color_t c, bool dotted) {
  if (x2 < x1 || y2 < y1) return;
  if (dotted) {  // every other cell of its width (a dashed line along the bar)
    const bool hor = x2 - x1 >= y2 - y1;
    const int w = hor ? y2 - y1 + 1 : x2 - x1 + 1;
    lv_draw_line_dsc_t d;
    lv_draw_line_dsc_init(&d);
    d.color = c; d.width = w; d.dash_width = w; d.dash_gap = w; d.opa = LV_OPA_COVER;
    if (hor) { d.p1.x = x1; d.p2.x = x2 + 1; d.p1.y = d.p2.y = y1 + w / 2; } else { d.p1.y = y1; d.p2.y = y2 + 1; d.p1.x = d.p2.x = x1 + w / 2; }
    lv_draw_line(ly, &d);
    return;
  }
  lv_draw_rect_dsc_t d;
  lv_draw_rect_dsc_init(&d);
  d.bg_color = c; d.bg_opa = LV_OPA_COVER;
  const lv_area_t a = {x1, y1, x2, y2};
  lv_draw_rect(ly, &d, &a);
}
// a block's edge layers inside area A (outermost first, each inset from A's edge; later layers on top)
void draw_edges(lv_layer_t *ly, const lv_area_t &A, const S::Skin &k, const S::Block &b, int lt, int set, int radius) {
  for (int i = 0; i < b.n_edges; i++) {
    const S::Edge &e = k.edges[b.edge0 + i];
    const int w = e.w.px[DN], in = e.inset.px[DN];
    if (w <= 0) continue;
    const lv_area_t a = {A.x1 + in, A.y1 + in, A.x2 - in, A.y2 - in};
    if (a.x2 - a.x1 < 2 * w || a.y2 - a.y1 < 2 * w) continue;
    const lv_color_t c = hex(k.ink(e.ink, lt, set));
    if (e.side == S::SIDE_ALL && !e.dotted && radius > in) {  // a round box: one ring
      lv_draw_rect_dsc_t d;
      lv_draw_rect_dsc_init(&d);
      d.bg_opa = LV_OPA_TRANSP; d.border_color = c; d.border_width = w; d.border_opa = LV_OPA_COVER; d.radius = radius - in;
      lv_draw_rect(ly, &d, &a);
      continue;
    }
    const int sd = e.side == S::SIDE_TL ? (S::SIDE_TOP | S::SIDE_LEFT) : e.side == S::SIDE_BR ? (S::SIDE_BOTTOM | S::SIDE_RIGHT) : e.side;
    if (sd & S::SIDE_TOP) edge_bar(ly, a.x1, a.y1, a.x2, a.y1 + w - 1, c, e.dotted);
    if (sd & S::SIDE_LEFT) edge_bar(ly, a.x1, a.y1, a.x1 + w - 1, a.y2, c, e.dotted);
    if (sd & S::SIDE_BOTTOM) edge_bar(ly, a.x1, a.y2 - w + 1, a.x2, a.y2, c, e.dotted);
    if (sd & S::SIDE_RIGHT) edge_bar(ly, a.x2 - w + 1, a.y1, a.x2, a.y2, c, e.dotted);
  }
}
int snap_cell(const S::Skin *k);
int bold_cell(const S::Skin &k) { return std::max(1, snap_cell(&k)); }
int bold_reach(const S::Skin &k, int p) {  // a bold part draws one cell past its text
  for (int m = 0; m < 16; m++) { const S::Block b = k.merged(p, m); if ((b.has & S::K_WEIGHT) && b.bold) return bold_cell(k); }
  return 0;
}
int shadow_reach(const S::Skin &k, int p) {  // the farthest any combination's shadow reaches outside the box
  int r = 0;
  for (int m = 0; m < 16; m++) {
    const S::Block b = k.merged(p, m);
    if (b.has & S::K_SHADOW) r = std::max(r, std::max(abs(b.shadow.dx.px[DN]), abs(b.shadow.dy.px[DN])));
  }
  return r;
}
Look *look_of(lv_obj_t *o);
std::map<lv_obj_t *, bool> switch_of;  // an object with the switch part -> it draws (the root; false = its knob)
bool is_check(lv_obj_t *o) {  // the live theme's switch part has shape check on this switch root
  if (!switch_of.count(o)) return false;
  const S::Block b = look_of(o)->skin->merged(G::P_SWITCH, combo(o));
  return (b.has & S::K_SHAPE) && b.shape == 1;
}
// switch part shape check (a theme's classic checkbox): a square the switch's height at its right end (the tap area
// stays the switch), the part's face + edges, and when on a 7 x 7 cell tick in the part's ink (cell = the square / 13,
// the classic proportion). Read at draw time, so a live theme switch repaints it (C44)
void switch_draw(lv_event_t *e) {
  lv_obj_t *o = (lv_obj_t *) lv_event_get_current_target(e);
  if (!is_check(o)) return;
  Look *L = look_of(o);
  const S::Skin &k = *L->skin;
  const int m = combo(o), lt = L->th.light, set = L->th.accent_set;
  const S::Block b = k.merged(G::P_SWITCH, m);
  lv_layer_t *ly = lv_event_get_layer(e);
  lv_area_t A;
  lv_obj_get_coords(o, &A);
  const int side = lv_area_get_height(&A);
  const lv_area_t q = {A.x2 - side + 1, A.y1, A.x2, A.y2};
  if (b.has & S::K_FACE) {
    lv_draw_rect_dsc_t d;
    lv_draw_rect_dsc_init(&d);
    d.bg_color = hex(k.ink(b.face.a, lt, set)); d.bg_opa = LV_OPA_COVER;
    lv_draw_rect(ly, &d, &q);
  }
  if (b.has & S::K_EDGES) draw_edges(ly, q, k, b, lt, set, 0);
  if (!(m & S::ON)) return;
#ifndef ESP_PLATFORM
  if (fx::pen_veil == "switch") return;  // tests: the tick veiled (pixel contrast)
#endif
  static const char *const TICK[7] = {"......#", ".....##", "#...###", "##.###.", "#####..", ".###...", "..#...."};
  const int cs = std::max(1, side / 13), x0 = q.x1 + (side - 7 * cs) / 2, y0 = q.y1 + (side - 7 * cs) / 2;
  lv_draw_rect_dsc_t d;
  lv_draw_rect_dsc_init(&d);
  d.bg_color = hex(b.has & S::K_INK ? k.ink(b.ink, lt, set) : k.rgb(lt, set, G::C_TEXT_DIM)); d.bg_opa = LV_OPA_COVER;
  for (int r = 0; r < 7; r++)
    for (int c0 = 0; c0 < 7; c0++) {
      if (TICK[r][c0] != '#') continue;
      int c1 = c0;
      while (c1 + 1 < 7 && TICK[r][c1 + 1] == '#') c1++;
      const lv_area_t a = {x0 + c0 * cs, y0 + r * cs, x0 + (c1 + 1) * cs - 1, y0 + (r + 1) * cs - 1};
      lv_draw_rect(ly, &d, &a);
      c0 = c1;
    }
}
bool seg_box(lv_obj_t *o, Look *L) {  // a Segmented's box ("seg") while the live theme styles the segment part (C46)
  const S::Block sb = L->skin->merged(G::P_SEGMENT, 0);
  if (!(sb.has & (S::K_FACE | S::K_EDGES))) return false;
  const lv_style_t *seg = &L->s[ix("seg")];
  for (uint32_t k = 0; k < o->style_cnt; k++) if (o->styles[k].style == seg) return true;
  return false;
}
void part_draw(lv_event_t *e) {  // a box part: shadow under it (MAIN_BEGIN), pattern face + edges over its fill (MAIN_END)
  lv_obj_t *o = (lv_obj_t *) lv_event_get_current_target(e);
  const int p = (int) (intptr_t) lv_event_get_user_data(e);
  Look *L = look_of(o);
  const S::Skin &k = *L->skin;
  if (lv_event_get_code(e) == LV_EVENT_REFR_EXT_DRAW_SIZE) { lv_event_set_ext_draw_size(e, shadow_reach(k, p)); return; }
  const S::Block b = k.merged(p, combo(o));
  if (!(b.has & (S::K_SHADOW | S::K_EDGES | S::K_FACE))) return;  // Facet: nothing (no draw call, S1)
  if (is_check(o)) return;  // the theme draws this switch as a checkbox (switch_draw): its track part goes
  if (p == G::P_CONTROL && seg_box(o, L)) return;
  if (lv_obj_get_style_opa_recursive(o, LV_PART_MAIN) <= LV_OPA_MIN) return;  // a transparent object draws none of its part (as LVGL its own styles)  // the theme draws each segment (part segment): the Segmented's box edges go
  lv_layer_t *ly = lv_event_get_layer(e);
  const int lt = L->th.light, set = L->th.accent_set, radius = lv_obj_get_style_radius(o, LV_PART_MAIN);
  lv_area_t A;
  lv_obj_get_coords(o, &A);
  if (lv_event_get_code(e) == LV_EVENT_DRAW_MAIN_BEGIN) {
    if (!(b.has & S::K_SHADOW)) return;
    lv_draw_rect_dsc_t d;
    lv_draw_rect_dsc_init(&d);
    d.bg_color = hex(k.ink(b.shadow.ink, lt, set)); d.bg_opa = LV_OPA_COVER; d.radius = radius;
    lv_area_move(&A, b.shadow.dx.px[DN], b.shadow.dy.px[DN]);
    lv_draw_rect(ly, &d, &A);
    return;
  }
  if ((b.has & S::K_FACE) && (b.face.fill == S::FILL_CHECKER || b.face.fill == S::FILL_BLOCKS)) {
    // the second ink in cells over the first (the bg): checker = alternate cells of stroke:pixel (or the run), blocks
    // = run-wide blocks with gap between, rows of run with gap rows between; one dashed line per row of cells
    const bool chk = b.face.fill == S::FILL_CHECKER;
    const int px = std::max(1, (int) fx::size("stroke:pixel"));
    const int run = std::max(1, b.face.run.px[DN] > 0 ? (int) b.face.run.px[DN] : px), gap = chk ? run : std::max(1, (int) b.face.gap.px[DN]);
    const lv_color_t c = hex(k.ink(b.face.b, lt, set));
    for (int y = A.y1, row = 0; y <= A.y2; y += chk ? run : run + gap, row++) {
      lv_draw_line_dsc_t d;
      lv_draw_line_dsc_init(&d);
      d.color = c; d.opa = LV_OPA_COVER; d.width = std::min<int32_t>(run, A.y2 - y + 1); d.dash_width = run; d.dash_gap = gap;
      d.p1.x = A.x1 + (chk && (row & 1) ? run : 0); d.p2.x = A.x2 + 1; d.p1.y = d.p2.y = y + d.width / 2;
      if (d.p1.x <= A.x2) lv_draw_line(ly, &d);
    }
  }
  if (b.has & S::K_EDGES) draw_edges(ly, A, k, b, lt, set, radius);
}
// a text part's shadow: the label's own draw task becomes the shadow (moved, in the shadow ink) and the text is drawn
// again on top, so the text keeps its exact layout (ellipsis, alignment) with no copy of the label code
bool in_shadow = false;
void text_shadow(lv_event_t *e) {
  lv_obj_t *o = (lv_obj_t *) lv_event_get_current_target(e);
  const int p = (int) (intptr_t) lv_event_get_user_data(e);
  Look *L = look_of(o);
  const S::Block b = L->skin->merged(p, combo(o));
  if (lv_event_get_code(e) == LV_EVENT_REFR_EXT_DRAW_SIZE) { lv_event_set_ext_draw_size(e, shadow_reach(*L->skin, p) + bold_reach(*L->skin, p)); return; }
  const bool sh = b.has & S::K_SHADOW, bo = (b.has & S::K_WEIGHT) && b.bold;
  if (in_shadow || (!sh && !bo)) return;
  lv_draw_task_t *t = lv_event_get_draw_task(e);
  if (t->type != LV_DRAW_TASK_TYPE_LABEL) return;
  auto *dsc = (lv_draw_label_dsc_t *) t->draw_dsc;
  lv_draw_label_dsc_t top = *dsc;
  const lv_area_t at = t->area;
  const int cell = bold_cell(*L->skin);
  in_shadow = true;
  if (sh) {  // the task becomes the shadow; the text again on top (and a bold shadow its own second copy)
    dsc->color = hex(L->skin->ink(b.shadow.ink, L->th.light, L->th.accent_set));
    const int dx = b.shadow.dx.px[DN], dy = b.shadow.dy.px[DN];
    lv_area_move(&t->area, dx, dy); lv_area_move(&t->_real_area, dx, dy);
    if (bo) { lv_draw_label_dsc_t sd = *dsc; lv_area_t a = t->area; lv_area_move(&a, cell, 0); lv_draw_label(t->target_layer, &sd, &a); }
    lv_draw_label(t->target_layer, &top, &at);
  }
  // synthetic bold (Appendix A weight: bold): the text again a cell to the right (1 px, or the snapped cell), the
  // way 1998 GDI made bold of a bitmap font; the text box does not move (C45), the extra column is ext draw
  if (bo) { lv_area_t a = at; lv_area_move(&a, cell, 0); lv_draw_label(t->target_layer, &top, &a); }
  in_shadow = false;
}
// a text's parts: its type's text part, then the nearest box part round it (its text ink wins: the part's face is under it)
void text_parts(lv_obj_t *o, Look *L, int tp, bool label) {
  if (label) {  // the shadow callbacks first: adding the styles below then sizes the label's ext draw area (shadow reach)
    lv_obj_add_flag(o, LV_OBJ_FLAG_SEND_DRAW_TASK_EVENTS);
    lv_obj_add_event_cb(o, text_shadow, LV_EVENT_DRAW_TASK_ADDED, (void *) (intptr_t) tp);
    lv_obj_add_event_cb(o, text_shadow, LV_EVENT_REFR_EXT_DRAW_SIZE, (void *) (intptr_t) tp);
  }
  for (int m = 0; m < 16; m++) lv_obj_add_style(o, &L->s[part_def[tp][m]], sel_of(m));
  for (lv_obj_t *a = lv_obj_get_parent(o); a; a = lv_obj_get_parent(a)) {
    auto it = part_of.find(a);
    if (it == part_of.end()) continue;
    for (int m = 0; m < 16; m++) lv_obj_add_style(o, &L->s[ptext_def[it->second][m]], sel_of(m));
    break;
  }
  if (label) lv_obj_refresh_ext_draw_size(o);
}
// a drawn part's block now (the Pen's object's states); ink = the theme's ink for it, unless the component shows a
// state the theme says nothing about (disabled with no disabled ink keeps the component's dimmed ink)
struct PenPart { S::Block b; const S::Skin *k; int lt, set; bool ink; };
PenPart pen_part(lv_obj_t *o, int p) {
  Look *L = look_of(o);
  const int m = combo(o);
  PenPart r{L->skin->merged(p, m), L->skin.get(), L->th.light, L->th.accent_set, false};
  r.ink = (r.b.has & S::K_INK) && (!(m & S::DISABLED) || (L->skin->state[p][1 + G::ST_DISABLED].has & S::K_INK));
  return r;
}
std::map<lv_obj_t *, const char *> names;
void forget(lv_event_t *e) { names.erase((lv_obj_t *) lv_event_get_current_target(e)); }
fw::ty::Id type_id(const char *type) {
  const int i = ix(type);
  if (defs[i].k != TYPE) abort();
  return fx::gen::TYPES[defs[i].i].id;
}
void square(lv_obj_t *o, int &cx, int &cy, int &half) {
  lv_area_t c;
  lv_obj_get_content_coords(o, &c);
  const int w = lv_area_get_width(&c), h = lv_area_get_height(&c), m = w < h ? w : h;
  cx = c.x1 + w / 2; cy = c.y1 + h / 2; half = (m - 1) / 2;
}
// ---- pixel icons (shape.icons pixel): the icon's 8 x 8 art (tokens/icons.json "pixel") in whole cells, centred in its
// box, in its ink layer's colour (state and the theme's text ink included); snapped themes: cells of stroke:pixel on
// the screen grid. The line layers are hidden by the shared style icon-px, so a theme switch needs no rebuild (C44).
int gdown(int v, int cs);
void icon_pixels(lv_event_t *e) {
  lv_obj_t *o = (lv_obj_t *) lv_event_get_current_target(e);
  Look *L = look_of(o);
  if (!L || !L->skin || L->skin->icons != 1) return;
  const auto *pi = (const G::PixelIcon *) lv_event_get_user_data(e);
  lv_obj_t *ink_layer = nullptr;
  for (uint32_t i = 0; i < lv_obj_get_child_count(o) && !ink_layer; i++)
    if ((uintptr_t) lv_obj_get_user_data(lv_obj_get_child(o, (int32_t) i)) == fw::ic::R_ink) ink_layer = lv_obj_get_child(o, (int32_t) i);
  const lv_color_t c = ink_layer ? lv_obj_get_style_image_recolor(ink_layer, LV_PART_MAIN) : lv_obj_get_style_text_color(o, LV_PART_MAIN);
  lv_area_t a;
  lv_obj_get_coords(o, &a);
  const int w = lv_area_get_width(&a), h = lv_area_get_height(&a), pc = snap_cell(L->skin.get());
  int cs = std::max(1, std::min(w, h) / 8), ox = a.x1 + (w - 8 * cs) / 2, oy = a.y1 + (h - 8 * cs) / 2;
  if (pc) {  // whole cells of stroke:pixel; on the screen grid when the art fits inside its box there, else on the box's
    // own grid (an icon box exactly 8 cells tall off the screen grid: clipped part cells would be worse than an offset)
    cs = std::max(1, std::min(w, h) / 8 / pc) * pc;
    ox = a.x1 + (w - 8 * cs) / 2; oy = a.y1 + (h - 8 * cs) / 2;
    const int gx = gdown(ox + pc - 1, pc) <= a.x2 + 1 - 8 * cs ? gdown(ox + pc - 1, pc) : gdown(ox, pc);
    const int gy = gdown(oy + pc - 1, pc) <= a.y2 + 1 - 8 * cs ? gdown(oy + pc - 1, pc) : gdown(oy, pc);
    if (gx >= a.x1 && gx + 8 * cs - 1 <= a.x2) ox = gx;
    if (gy >= a.y1 && gy + 8 * cs - 1 <= a.y2) oy = gy;
  }
  lv_layer_t *ly = lv_event_get_layer(e);
  lv_draw_rect_dsc_t d;
  lv_draw_rect_dsc_init(&d);
  d.bg_color = c; d.bg_opa = LV_OPA_COVER; d.radius = 0;
  for (int r = 0; r < 8; r++)
    for (int x = 0; x < 8;) {
      if (!(pi->rows[r] & (0x80 >> x))) { x++; continue; }
      const int x0 = x;
      while (x < 8 && (pi->rows[r] & (0x80 >> x))) x++;
      const lv_area_t q = {ox + x0 * cs, oy + r * cs, ox + x * cs - 1, oy + r * cs + cs - 1};
      lv_draw_rect(ly, &d, &q);
    }
}
const G::PixelIcon *pixel_icon(const char *name) {
  int lo = 0, hi = G::N_PIXEL_ICONS - 1;
  while (lo <= hi) {
    const int m = (lo + hi) / 2, c = strcmp(G::PIXEL_ICONS[m].name, name);
    if (!c) return &G::PIXEL_ICONS[m];
    if (c < 0) lo = m + 1; else hi = m - 1;
  }
  return nullptr;
}
// ---- snapped drawing (C29, shape.snap): every Pen primitive as whole cells of stroke:pixel on a grid fixed to the
// screen, no anti-aliasing, one rect per run. The Pen's centre is a CELL centre (so a shape centred on it is symmetric
// in cells), a knob and its arc are judged at the same cell centres (they share cells), a one-cell line is Bresenham on
// the cell grid, a thicker line k whole cells across its minor axis (never a blob), an icon in whole cells
// (facet-201 faults; tests/functional/theme_snap.test.mjs).
struct Shp { enum K { DISC, SQUARE, DIAMOND, RING, SEG, SECTOR } k; float x, y, r, r2 = 0, x2 = 0, y2 = 0, a0 = 0, a1 = 0; };
bool inside(const Shp &s, float px, float py) {
  const float dx = px - s.x, dy = py - s.y;
  switch (s.k) {
    case Shp::DISC: return dx * dx + dy * dy <= s.r * s.r;
    case Shp::SQUARE: return fabsf(dx) <= s.r && fabsf(dy) <= s.r;
    case Shp::DIAMOND: return fabsf(dx) + fabsf(dy) <= s.r;
    case Shp::RING: { const float d2 = dx * dx + dy * dy; return d2 <= s.r * s.r && d2 >= s.r2 * s.r2; }
    case Shp::SEG: {
      const float vx = s.x2 - s.x, vy = s.y2 - s.y, l = vx * vx + vy * vy;
      float t = l > 0 ? (dx * vx + dy * vy) / l : 0;
      t = fmaxf(0, fminf(1, t));
      const float qx = s.x + t * vx - px, qy = s.y + t * vy - py;
      return qx * qx + qy * qy <= s.r * s.r;
    }
    case Shp::SECTOR: {
      const float d2 = dx * dx + dy * dy;
      if (d2 > s.r * s.r || d2 < s.r2 * s.r2) return false;
      const float a = fmodf(atan2f(dy, dx) * 57.29578f + 360, 360);
      return fmodf(a - s.a0 + 720, 360) <= s.a1 - s.a0;
    }
  }
  return false;
}
int gdown(int v, int cs) { return (v >= 0 ? v / cs : (v - cs + 1) / cs) * cs; }
struct Grid {  // the Pen's frame on the cell grid: shapes are given relative to the Pen centre, in px
  int cs; float ox, oy;  // the centre cell's middle, screen px
  int gx0, gy0, gx1, gy1;  // the whole cells inside the drawing object: a cell partly outside is not drawn (no part cells)
  Grid(const fx::Pen &p, int c) : cs(c), ox(gdown(p.cx, c) + c / 2.0f), oy(gdown(p.cy, c) + c / 2.0f) {
    lv_area_t b;
    lv_obj_get_coords(p.obj, &b);
    gx0 = gdown(b.x1 + c - 1, c) / c; gy0 = gdown(b.y1 + c - 1, c) / c; gx1 = gdown(b.x2 + 1, c) / c - 1; gy1 = gdown(b.y2 + 1, c) / c - 1;
  }
  int cell(float rel, float o) const { return (int) floorf((o + rel) / cs); }  // the cell index of a relative px
};
void fill_cells(lv_layer_t *ly, const Grid &g, lv_color_t c, int cx1, int cy1, int cx2, int cy2) {  // cells, inclusive
  cx1 = std::max(cx1, g.gx0); cy1 = std::max(cy1, g.gy0); cx2 = std::min(cx2, g.gx1); cy2 = std::min(cy2, g.gy1);
  if (cx1 > cx2 || cy1 > cy2) return;
  lv_draw_rect_dsc_t d;
  lv_draw_rect_dsc_init(&d);
  d.bg_color = c; d.bg_opa = LV_OPA_COVER; d.radius = 0;
  const lv_area_t a = {cx1 * g.cs, cy1 * g.cs, cx2 * g.cs + g.cs - 1, cy2 * g.cs + g.cs - 1};
  lv_draw_rect(ly, &d, &a);
}
void cells(const fx::Pen &p, const Grid &g, const Shp &s, lv_color_t c) {  // the shape's cells (centre inside), as runs
  float x1 = s.x - s.r, x2 = s.x + s.r, y1 = s.y - s.r, y2 = s.y + s.r;
  if (s.k == Shp::SEG) { x1 = fminf(s.x, s.x2) - s.r; x2 = fmaxf(s.x, s.x2) + s.r; y1 = fminf(s.y, s.y2) - s.r; y2 = fmaxf(s.y, s.y2) + s.r; }
  const int cs = g.cs, gx0 = g.cell(x1, g.ox) - 1, gx1 = g.cell(x2, g.ox) + 1, gy0 = g.cell(y1, g.oy) - 1, gy1 = g.cell(y2, g.oy) + 1;
  const lv_area_t &clip = p.layer->_clip_area;
  for (int gy = gy0; gy <= gy1; gy++) {
    if (gy * cs + cs - 1 < clip.y1 || gy * cs > clip.y2) continue;
    int run = INT32_MIN;
    for (int gx = gx0; gx <= gx1 + 1; gx++) {
      const bool in = gx <= gx1 && inside(s, gx * cs + cs / 2.0f - g.ox, gy * cs + cs / 2.0f - g.oy);
      if (in && run == INT32_MIN) run = gx;
      if (!in && run != INT32_MIN) { fill_cells(p.layer, g, c, run, gy, gx - 1, gy); run = INT32_MIN; }
    }
  }
}
// a line k cells thick: Bresenham on the cell grid, repeated across the minor axis (every step the same k cells)
void cell_line(const fx::Pen &p, const Grid &g, float x1, float y1, float x2, float y2, int k, lv_color_t c) {
  int ax = g.cell(x1, g.ox), ay = g.cell(y1, g.oy);
  const int bx = g.cell(x2, g.ox), by = g.cell(y2, g.oy);
  const int dx = abs(bx - ax), dy = -abs(by - ay), sx = ax < bx ? 1 : -1, sy = ay < by ? 1 : -1;
  const bool hor = dx >= -dy;
  const int lo = -(k - 1) / 2, hi = k / 2;  // the k cells across, centred on the line
  int err = dx + dy;
  for (;;) {
    if (hor) fill_cells(p.layer, g, c, ax, ay + lo, ax, ay + hi);
    else fill_cells(p.layer, g, c, ax + lo, ay, ax + hi, ay);
    if (ax == bx && ay == by) break;
    const int e2 = 2 * err;
    if (e2 >= dy) { err += dy; ax += sx; }
    if (e2 <= dx) { err += dx; ay += sy; }
  }
}
int cells_of(int px, int cs) { return std::max(1, (int) lroundf((float) px / cs)); }  // a stroke in whole cells
void on_draw(lv_event_t *e) {
  lv_obj_t *o = (lv_obj_t *) lv_event_get_current_target(e);
  fx::Pen p{lv_event_get_layer(e), o, 0, 0, 0};
  square(o, p.cx, p.cy, p.half);
  p.w = lv_obj_get_content_width(o); p.h = lv_obj_get_content_height(o);
  if (p.half < 1) return;  // a parent with no room (the Layout lab's squeeze): nothing to draw, never a negative radius
  ((void (*)(const fx::Pen &)) lv_event_get_user_data(e))(p);
}
void on_size(lv_event_t *e) {  // a drawn box: content height >= content width
  lv_obj_t *o = (lv_obj_t *) lv_event_get_current_target(e);
  if (lv_event_get_code(e) == LV_EVENT_GET_SELF_SIZE) {
    auto *p = (lv_point_t *) lv_event_get_param(e);
    const int cw = lv_obj_get_content_width(o);
    if (cw > p->y) p->y = cw;
  } else if (lv_area_get_width((lv_area_t *) lv_event_get_param(e)) != lv_obj_get_width(o)) lv_obj_refresh_self_size(o);
}
}  // namespace

namespace fx {
void look(lv_display_t *d, const fw::Theme *t) {
  for (Look *L : looks) if (L->d == d) {
    if (L->t != t || L->large != (int) fw::text_large() || GEN_OLD(L)) { paint(L, t); lv_obj_report_style_change(nullptr); }
    return;
  }
  make(d, t);
}
void restyle() {  // a theme was applied: every look whose generation is old repaints, one style change for all
  bool any = false;
  for (Look *L : looks) if (GEN_OLD(L)) { paint(L, L->t); any = true; }
  if (any) lv_obj_report_style_change(nullptr);
}
#ifndef ESP_PLATFORM
void break_stale(bool on) { stale = on; }
std::string pen_veil;  // tests only (fw_test "veil_part"): the drawn part (knob, track-fill, arc-fill) the Pen skips
#define PEN_VEILED(p) ((p) && !pen_veil.empty() && pen_veil == (p))
#else
#define PEN_VEILED(p) false
#endif
int pen_cell() { return snap_cell(fx::theme_live().get()); }
// the font a type token draws now (the workbench Type page, tests/functional/theme_fonts.test.mjs): its px, line
// height, and whether it is Facet's own, a built-in theme font or a downloaded one
std::string type_font_info(const char *type) {
  const int i = index_of(type);
  if (i < 0 || defs[i].k != TYPE) return "{\"error\":\"not a type token\"}";
  const fw::ty::Id id = fx::gen::TYPES[defs[i].i].id;
  const auto sk = fx::theme_live();
  const lv_font_t *f = skin_font(sk.get(), id);
  const int r = fw::style_role(id);
  const int px = type_ti[id] >= 0 ? sk->type[DN][type_ti[id]] : fw::ty::STYLES[id].size;
  bool exact = false;
  const lv_font_t *bf = fw::theme_font(G::FAMILIES[sk->font[r]], id, px, &exact);
  const bool dl = sk->font_src[r] && f != fw::font(id) && f != bf;
  if (f == fw::font(id)) exact = px == fw::ty::STYLES[id].size || f == bf;  // Facet's own font at its own size
  lv_font_glyph_dsc_t g;
  const int adv = lv_font_get_glyph_dsc(f, &g, 'M', 0) ? g.adv_w : 0;
  char b[200];
  snprintf(b, sizeof b, "{\"px\":%d,\"line_height\":%d,\"adv_M\":%d,\"facet\":%s,\"download\":%s,\"exact\":%s,\"role\":%d}", px,
           (int) lv_font_get_line_height(f), adv, f == fw::font(id) ? "true" : "false", dl ? "true" : "false", exact || dl ? "true" : "false", r);
  return b;
}
lv_color_t bg(lv_display_t *d) {  // the canvas under a contract page: bg in the live theme (fw::T's mode + accent)
  if (fw::T) look(d, fw::T);
  for (Look *L : looks) if (L->d == d) return L->th.bg;
  return make(d, fw::T ? fw::T : fw::theme(false))->th.bg;
}
void use(lv_obj_t *o, const char *t, lv_style_selector_t sel) {
  Look *L = look_of(o);
  const int i = ix(t);
  if (defs[i].k == PTEXT) abort();  // the core's own (text_parts)
  if (defs[i].k != PART) { lv_obj_add_style(o, &L->s[i], sel); return; }
  // a part (C46): its 16 combinations, last (after the component's own state tokens); a box part also draws its edges,
  // shadow and pattern (once per object: a refill re-adds the styles, the callbacks stay) and tints the texts inside
  const int p = defs[i].i / 16;
  if (sel || defs[i].i % 16) abort();  // a part is attached whole, at every state
  if (boxy(p) && !part_of.count(o)) {  // its draw callbacks first (once per object): the styles below then size its ext area
    lv_obj_add_event_cb(o, part_draw, LV_EVENT_DRAW_MAIN_BEGIN, (void *) (intptr_t) p);
    lv_obj_add_event_cb(o, part_draw, LV_EVENT_DRAW_MAIN_END, (void *) (intptr_t) p);
    lv_obj_add_event_cb(o, part_draw, LV_EVENT_REFR_EXT_DRAW_SIZE, (void *) (intptr_t) p);
    lv_obj_add_event_cb(o, [](lv_event_t *e) { part_of.erase((lv_obj_t *) lv_event_get_current_target(e)); }, LV_EVENT_DELETE, nullptr);
  }
  for (int m = 0; m < 16; m++) lv_obj_add_style(o, &L->s[part_def[p][m]], sel_of(m));
  if (p == G::P_SWITCH && !switch_of.count(o)) {  // the switch root draws the checkbox; a part inside it (its knob) only hides
    const bool root = !switch_of.count(lv_obj_get_parent(o));
    switch_of[o] = root;
    if (root) lv_obj_add_event_cb(o, switch_draw, LV_EVENT_DRAW_MAIN_END, nullptr);
    lv_obj_add_event_cb(o, [](lv_event_t *e) { switch_of.erase((lv_obj_t *) lv_event_get_current_target(e)); }, LV_EVENT_DELETE, nullptr);
  }
  if (!boxy(p)) return;
  // texts and icon inks already inside (built before the part was attached) take its text ink now; a part inside owns its own
  std::vector<lv_obj_t *> todo;
  for (uint32_t c = 0; c < lv_obj_get_child_count(o); c++) todo.push_back(lv_obj_get_child(o, (int32_t) c));
  while (!todo.empty()) {
    lv_obj_t *x = todo.back();
    todo.pop_back();
    if (part_of.count(x)) continue;
    bool texty = false;
    for (uint32_t k = 0; k < x->style_cnt && !texty; k++) {
      const lv_style_t *st = x->styles[k].style;
      if (st >= L->s.data() && st < L->s.data() + L->s.size()) { const Def &dd = defs[st - L->s.data()]; texty = dd.k == PART && !boxy(dd.i / 16); }
    }
    if (texty) {  // this part is now its nearest: an outer part's text ink goes (an icon in a header's button)
      std::vector<lv_style_t *> old;
      for (uint32_t k = 0; k < x->style_cnt; k++) {
        const lv_style_t *st = x->styles[k].style;
        if (st >= L->s.data() && st < L->s.data() + L->s.size() && defs[st - L->s.data()].k == PTEXT) old.push_back(&L->s[st - L->s.data()]);
      }
      for (lv_style_t *st : old) lv_obj_remove_style(x, st, (lv_style_selector_t) (LV_PART_ANY | LV_STATE_ANY));
      for (int m = 0; m < 16; m++) lv_obj_add_style(x, &L->s[ptext_def[p][m]], sel_of(m));
    }
    for (uint32_t c = 0; c < lv_obj_get_child_count(x); c++) todo.push_back(lv_obj_get_child(x, (int32_t) c));
  }
  part_of[o] = p;
  lv_obj_refresh_ext_draw_size(o);
}
lv_obj_t *box(lv_obj_t *p, const char *layout) {
  lv_obj_t *o = lv_obj_create(p);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, (lv_obj_flag_t) (LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE));
  lv_obj_add_flag(o, LV_OBJ_FLAG_EVENT_BUBBLE);
  use(o, "box");
  if (strcmp(layout, "free")) use(o, layout);
  return o;
}
lv_obj_t *text(lv_obj_t *p, const char *s, const char *type, const char *fg) {
  lv_obj_t *o = lv_label_create(p);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
  use(o, type); use(o, fg);
  text_parts(o, look_of(o), type_part[defs[ix(type)].i], true);  // the theme's text parts, last
  lv_label_set_text(o, fw::cased(type_id(type), s).c_str());
  return o;
}
void set_text(lv_obj_t *l, const char *s, const char *type) {
  const std::string c = fw::cased(type_id(type), s);
  if (c != lv_label_get_text(l)) lv_label_set_text(l, c.c_str());
}
void state(lv_obj_t *o, lv_state_t s, bool on) {
  if (on) lv_obj_add_state(o, s); else lv_obj_remove_state(o, s);
  for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) state(lv_obj_get_child(o, (int32_t) i), s, on);
}
lv_obj_t *row(lv_obj_t *p) { return box(p, "row"); }
bool in_bar(lv_obj_t *o) {  // the nearest box part round o is a bar (card-head): its face is a title bar, so a theme's text ink colours all of an icon
  for (lv_obj_t *a = lv_obj_get_parent(o); a; a = lv_obj_get_parent(a)) {
    auto it = part_of.find(a);
    if (it != part_of.end()) return !strcmp(G::PARTS[it->second].kind, "bar");
  }
  return false;
}
void hit(lv_obj_t *o) {  // C32: the hit area at least touch:min tall around o as laid out now
  lv_obj_update_layout(o);
  lv_obj_set_ext_click_area(o, std::max(0, (tok("touch:min", fw::touch::min) - (int) lv_obj_get_height(o)) / 2));
}

lv_obj_t *icon(lv_obj_t *p, const char *name, const char *size, const char *ink) {
  const fw::ic::Size sz = !strcmp(size, "card") ? fw::ic::card : !strcmp(size, "glyph") ? fw::ic::glyph
                         : !strcmp(size, "feature") ? fw::ic::feature
                         : !strcmp(size, "checkbox") ? (fw::ic::PX[fw::ic::icon] == fw::lay::checkbox ? fw::ic::icon : fw::ic::glyph)  // the size that fills the box
                         : fw::ic::icon;
  lv_obj_t *b = fw::icon(p, name, sz, lv_color_black());
  if (!b) return nullptr;
  lv_obj_add_flag(b, LV_OBJ_FLAG_EVENT_BUBBLE);
  for (uint32_t i = 0; i < lv_obj_get_child_count(b); i++) {  // every layer's ink = a shared style (its role's colour)
    lv_obj_t *im = lv_obj_get_child(b, (int32_t) i);
    const int role = (int) (uintptr_t) lv_obj_get_user_data(im);
    lv_obj_remove_local_style_prop(im, LV_STYLE_IMAGE_RECOLOR, 0);
    lv_obj_remove_local_style_prop(im, LV_STYLE_IMAGE_RECOLOR_OPA, 0);
    use(im, role == fw::ic::R_accent ? "ink:accent" : role == fw::ic::R_text_dim ? "ink:text-dim" : role == fw::ic::R_blue ? "ink:blue"
            : role == fw::ic::R_steel ? "ink:steel" : (std::string("ink:") + (strchr(ink, ':') + 1)).c_str());
    if (role == fw::ic::R_ink) text_parts(im, look_of(im), part_ix("icon"), false);  // the ink layers: the icon part + the box's text ink
    else if (in_bar(im)) text_parts(im, look_of(im), part_ix("icon"), false);  // a bar (card head) draws every layer in its text ink
  }
  if (const G::PixelIcon *pi = pixel_icon(name)) {  // pixel art: drawn instead of the line layers while the theme says so
    for (uint32_t i = 0; i < lv_obj_get_child_count(b); i++) use(lv_obj_get_child(b, (int32_t) i), "icon-px");
    lv_obj_add_event_cb(b, icon_pixels, LV_EVENT_DRAW_MAIN, (void *) pi);
  }
  return b;
}
void icon_use(lv_obj_t *icon, const char *token, lv_style_selector_t sel) {
  for (uint32_t i = 0; icon && i < lv_obj_get_child_count(icon); i++) {
    lv_obj_t *im = lv_obj_get_child(icon, (int32_t) i);
    if ((uintptr_t) lv_obj_get_user_data(im) == fw::ic::R_ink) use(im, token, sel);
  }
}
bool icon_multi(lv_obj_t *icon) {
  for (uint32_t i = 0; icon && i < lv_obj_get_child_count(icon); i++)
    if ((uintptr_t) lv_obj_get_user_data(lv_obj_get_child(icon, (int32_t) i)) != fw::ic::R_ink) return true;
  return false;
}
void focus_ring(lv_obj_t *o, lv_style_selector_t show) {  // decor (C14): the one ring outside the box, a shared style
  use(o, "outline:focus-ring", show);
  use(o, "no-outline", fw::st::OFF);  // disabled / unavailable hide it, even when active
  fw::bleed(o, fw::touch::focus_offset);  // the audit / bounds proof: this object's ink may reach that far outside it
}
void pulse(lv_obj_t *o) {
  fw::Anim a{[](void *x, int32_t v) { lv_obj_set_style_opa((lv_obj_t *) x, (lv_opa_t) v, 0); }, LV_OPA_COVER,
             (int32_t) lroundf(fw::mo::OPA_PULSE_MIN * 255), fw::mo::OPA};
  a.loop = true;
  fw::animate(o, fw::mo::pulse, a);
}
lv_obj_t *cell(lv_obj_t *r) { lv_obj_t *c = box(r, "col"); use(c, "cell"); return c; }
void bleed(lv_obj_t *o) { use(o, "bleed"); }
void fit_grid(lv_obj_t *l) {  // the label's box = its text's width rounded up to the 4 px grid (docs fit8)
  if (!lv_obj_check_type(l, &lv_label_class)) {  // a row of fixed boxes: their sum, rounded up
    lv_obj_set_width(l, LV_SIZE_CONTENT);
    lv_obj_update_layout(l);
    lv_obj_set_width(l, (lv_obj_get_width(l) + fw::sp::GRID - 1) / fw::sp::GRID * fw::sp::GRID);
    return;
  }
  // a theme can change the label's font live (C49): its width follows on every style or state change, not only at build
  struct R { static void refit(lv_event_t *e) {
    static bool busy = false;
    lv_obj_t *o = (lv_obj_t *) lv_event_get_current_target(e);
    if (busy) return;
    lv_point_t sz;
    lv_text_get_size(&sz, lv_label_get_text(o), lv_obj_get_style_text_font(o, LV_PART_MAIN), lv_obj_get_style_text_letter_space(o, LV_PART_MAIN), 0,
                     LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    const int w = (sz.x + fw::sp::GRID - 1) / fw::sp::GRID * fw::sp::GRID;
    if (lv_obj_get_style_width(o, LV_PART_MAIN) == w) return;
    busy = true; lv_obj_set_width(o, w); busy = false;
  } };
  bool has = false;
  for (uint32_t i = 0; i < lv_obj_get_event_count(l) && !has; i++) has = lv_event_dsc_get_cb(lv_obj_get_event_dsc(l, i)) == R::refit;
  if (!has) {  // a state can switch the font too (a theme's disabled text look): refit then as well
    lv_obj_add_event_cb(l, R::refit, LV_EVENT_STYLE_CHANGED, nullptr);
    lv_obj_add_event_cb(l, R::refit, LV_EVENT_STATE_CHANGED, nullptr);
  }
  lv_point_t sz;
  lv_text_get_size(&sz, lv_label_get_text(l), lv_obj_get_style_text_font(l, LV_PART_MAIN), lv_obj_get_style_text_letter_space(l, LV_PART_MAIN), 0,
                   LV_COORD_MAX, LV_TEXT_FLAG_NONE);
  lv_obj_set_width(l, (sz.x + fw::sp::GRID - 1) / fw::sp::GRID * fw::sp::GRID);
}
void equal(lv_obj_t *row) {  // the children (not out of the layout) share row's content width on the 4 px grid
  auto place = [](lv_event_t *e) {
    lv_obj_t *r = (lv_obj_t *) lv_event_get_current_target(e);
    std::vector<lv_obj_t *> kids;
    for (uint32_t i = 0; i < lv_obj_get_child_count(r); i++) {
      lv_obj_t *c = lv_obj_get_child(r, (int32_t) i);
      if (!lv_obj_has_flag(c, LV_OBJ_FLAG_IGNORE_LAYOUT)) kids.push_back(c);
    }
    if (kids.empty()) return;
    const int G = fw::sp::GRID, cw = lv_obj_get_content_width(r), n = (int) kids.size(), each = std::max(0, cw / n / G * G);
    for (int i = 0; i < n; i++) lv_obj_set_width(kids[i], i < n - 1 ? each : std::max(0, cw - each * (n - 1)));
  };
  lv_obj_add_event_cb(row, place, LV_EVENT_SIZE_CHANGED, nullptr);
  lv_obj_send_event(row, LV_EVENT_SIZE_CHANGED, nullptr);
}
// a "pair" box (Cells, Darren 2026-10-27): its halves (children 0 and 2) split its OWN content height (C6, C9), equal,
// each the largest grid multiple that leaves the rule's box (child 1's child) between them, capped by their max-h; the
// space left (child 1, grow) holds the rule centred. Follows the pair's size inside the layout pass (no wrong frame).
void split(lv_obj_t *pr) {
  lv_obj_t *a = lv_obj_get_child(pr, 0), *gap = lv_obj_get_child(pr, 1), *b = lv_obj_get_child(pr, 2);
  if (!a || !gap || !b || !lv_obj_get_child(gap, 0)) return;
  const int room = lv_obj_get_content_height(pr) - lv_obj_get_style_height(lv_obj_get_child(gap, 0), LV_PART_MAIN);
  int h = room > 0 ? room / 2 / fw::sp::GRID * fw::sp::GRID : 0;
  const int cap = lv_obj_get_style_max_height(a, LV_PART_MAIN);
  if (h > cap) h = cap;
  for (lv_obj_t *o : {a, b}) if (lv_obj_get_style_height(o, LV_PART_MAIN) != h) lv_obj_set_height(o, h);
}
void pair_split(lv_obj_t *pr) {
  lv_obj_add_event_cb(pr, [](lv_event_t *e) { split((lv_obj_t *) lv_event_get_current_target(e)); }, LV_EVENT_SIZE_CHANGED, nullptr);
  split(pr);
}
void name(lv_obj_t *o, const char *n) { if (!names.count(o)) lv_obj_add_event_cb(o, forget, LV_EVENT_DELETE, nullptr); names[o] = n; }
#ifndef ESP_PLATFORM
static std::map<lv_obj_t *, const char *> &slot_marks() { static std::map<lv_obj_t *, const char *> m; return m; }
void slot(lv_obj_t *o, const char *n) {
  if (!slot_marks().count(o)) lv_obj_add_event_cb(o, [](lv_event_t *e) { slot_marks().erase((lv_obj_t *) lv_event_get_current_target(e)); }, LV_EVENT_DELETE, nullptr);
  slot_marks()[o] = n;
}
std::string slots_json(lv_obj_t *scr, const std::function<int(lv_obj_t *)> &owner) {
  ArduinoJson::JsonDocument doc;
  ArduinoJson::JsonArray a = doc.to<ArduinoJson::JsonArray>();
  for (auto &[o, n] : slot_marks()) {
    if (lv_obj_get_screen(o) != scr) continue;
    lv_obj_t *par = lv_obj_get_parent(o);
    if (!par || lv_obj_has_flag(par, LV_OBJ_FLAG_HIDDEN)) continue;
    lv_area_t r;
    lv_obj_get_coords(o, &r);
    ArduinoJson::JsonObject b = a.add<ArduinoJson::JsonObject>();
    if (lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN)) {  // an empty slot takes no space: where it would open = just after the
      lv_area_t pr;                                // last shown sibling before it, the parent's height (w 0, "empty")
      lv_obj_get_coords(par, &pr);
      int32_t x = pr.x1 + lv_obj_get_style_pad_left(par, LV_PART_MAIN);
      for (int32_t i = lv_obj_get_index(o) - 1; i >= 0; i--) {
        lv_obj_t *sib = lv_obj_get_child(par, i);
        if (lv_obj_has_flag(sib, LV_OBJ_FLAG_HIDDEN)) continue;
        lv_area_t sr;
        lv_obj_get_coords(sib, &sr);
        x = sr.x2 + 1;
        break;
      }
      b["n"] = n, b["x"] = x, b["y"] = pr.y1, b["w"] = 0, b["h"] = lv_area_get_height(&pr), b["empty"] = true;
    } else b["n"] = n, b["x"] = r.x1, b["y"] = r.y1, b["w"] = lv_area_get_width(&r), b["h"] = lv_area_get_height(&r);
    if (const int k = owner ? owner(o) : -2; k >= 0) b["in"] = k; else if (k == -1) b["in"] = "header";
  }
  std::string out;
  serializeJson(doc, out);
  return out;
}
#else
void slot(lv_obj_t *, const char *) {}
std::string slots_json(lv_obj_t *, const std::function<int(lv_obj_t *)> &) { return "[]"; }
#endif
int type_box(const char *type) { return box_of(type_id(type)); }
int size(const char *t) {  // a size token now: tuned (the lab) > the live theme's look size (C19) > Facet's
  for (int i = 0; i < gen::N_SIZES; i++) if (!strcmp(gen::SIZES[i].name, t)) {
    const int si = size_ix[i];
    return tok(t, si >= 0 && live_raw && G::SIZES[si].look ? live_raw->size[DN][si] : gen::SIZES[i].v);
  }
  for (auto &e : STRUCTURE) if (!strcmp(e.first, t)) return tok(t, e.second);  // a structure size (layout.json)
  abort();
}
bool Pen::sees(int x1, int y1, int x2, int y2) const {
  const lv_area_t &c = layer->_clip_area;
  return cx + std::max(x1, x2) >= c.x1 && cx + std::min(x1, x2) <= c.x2 && cy + std::max(y1, y2) >= c.y1 && cy + std::min(y1, y2) <= c.y2;
}
// a drawn line's colour from a theme part: a box part's face (track, track-fill), else its ink; none = the component's ink
lv_color_t part_ink(lv_obj_t *o, const char *part, const char *k) {
  if (!part) return ink(o, k);
  const int pi = part_ix(part);
  const PenPart pp = pen_part(o, pi);
  const bool off = combo(o) & S::DISABLED;
  if ((pp.b.has & S::K_FACE) && (!off || (pp.k->state[pi][1 + G::ST_DISABLED].has & S::K_FACE))) return hex(pp.k->ink(pp.b.face.a, pp.lt, pp.set));
  return pp.ink ? hex(pp.k->ink(pp.b.ink, pp.lt, pp.set)) : ink(o, k);
}
void Pen::line(int x1, int y1, int x2, int y2, const char *stroke, const char *k, bool round, const char *part) const {
  if (PEN_VEILED(part)) return;
  const int w = size(stroke);
  if (!sees(std::min(x1, x2) - w, std::min(y1, y2) - w, std::max(x1, x2) + w, std::max(y1, y2) + w)) return;
  const lv_color_t col = part_ink(obj, part, k);
  if (const int cs = snap_cell(look_of(obj)->skin.get())) { cell_line(*this, Grid(*this, cs), x1, y1, x2, y2, cells_of(w, cs), col); return; }
  lv_draw_line_dsc_t d;
  lv_draw_line_dsc_init(&d);
  d.p1.x = cx + x1; d.p1.y = cy + y1; d.p2.x = cx + x2; d.p2.y = cy + y2;
  d.width = w; d.color = col; d.opa = LV_OPA_COVER; d.round_start = d.round_end = round && !look_of(obj)->skin->cap;
  lv_draw_line(layer, &d);
}
void Pen::arc(int r, float a0, float a1, const char *stroke, const char *k, bool round, const char *part) const {
  if (PEN_VEILED(part)) return;
  if (r < 1 || a1 - a0 < 0.5f) return;
  lv_draw_arc_dsc_t d;
  lv_draw_arc_dsc_init(&d);
  const int w = size(stroke);
  if (!sees(-r - w, -r - w, r + w, r + w)) return;
  // a partial redraw wholly inside the band's hole (a dial's value text in the middle of its ring) can't touch it:
  // skip it, as LVGL's arc renderer would work its circle masks over every pixel of that area for nothing (a dial
  // drag repainted its number at ~86 ms a frame on the Waveshare P4, 2026-10-28)
  {
    const lv_area_t &c = layer->_clip_area;
    const int64_t in = std::max(0, r - w / 2 - 2), in2 = in * in;
    auto inside = [&](int32_t x, int32_t y) { const int64_t dx = x - cx, dy = y - cy; return dx * dx + dy * dy < in2; };
    if (inside(c.x1, c.y1) && inside(c.x2, c.y1) && inside(c.x1, c.y2) && inside(c.x2, c.y2)) return;
  }
  const PenPart pp = part ? pen_part(obj, part_ix(part)) : PenPart{S::Block(), look_of(obj)->skin.get(), 0, 0, false};
  const lv_color_t col = pp.ink ? hex(pp.k->ink(pp.b.ink, pp.lt, pp.set)) : ink(obj, k);
  if (const int cs = snap_cell(pp.k)) {  // the band in whole cells, judged at cell centres round the centre cell
    const float hw = cells_of(w, cs) * cs / 2.0f, s0 = fmodf(a0, 360);
    Shp sh{Shp::SECTOR, 0, 0, r + hw, std::max(0.0f, r - hw), 0, 0, s0, s0 + std::min(360.0f, a1 - a0)};
    cells(*this, Grid(*this, cs), sh, col);
    return;
  }
  d.center.x = cx; d.center.y = cy; d.radius = r + w / 2; d.width = w; d.rounded = round && !pp.k->cap;  // shape.cap square: butt ends
  d.start_angle = fmodf(a0, 360); d.end_angle = fmodf(a1, 360);
  d.color = col; d.opa = LV_OPA_COVER;
  lv_draw_arc(layer, &d);
}
void Pen::dot(int x, int y, const char *diameter, const char *k0, const char *part) const {
  if (PEN_VEILED(part)) return;
  const int dm = size(diameter);
  const lv_color_t col = part_ink(obj, part, k0);
  if (!sees(x - dm, y - dm, x + dm, y + dm)) return;
  if (const int cs = snap_cell(look_of(obj)->skin.get())) {
    Shp sh{Shp::DISC, roundf((float) x / cs) * cs, roundf((float) y / cs) * cs, cells_of(dm, cs) * cs / 2.0f};
    cells(*this, Grid(*this, cs), sh, col);
    return;
  }
  lv_draw_rect_dsc_t d;
  lv_draw_rect_dsc_init(&d);
  d.radius = LV_RADIUS_CIRCLE; d.bg_color = col; d.bg_opa = LV_OPA_COVER;
  lv_area_t a = {cx + x - dm / 2, cy + y - dm / 2, cx + x - dm / 2 + dm - 1, cy + y - dm / 2 + dm - 1};
  lv_draw_rect(layer, &d, &a);
}
void Pen::tick(float deg, const char *length, const char *stroke, const char *k, const char *part) const {
  // the tick ring rule (core/atoms.cpp ring_draw): ends rounded from the edge-grid position, the outer end pulled in
  // so a slanted butt end's corners stay on the circle
  const int w = size(stroke), len = size(length);
  const float R = half + 1.0f, rad = deg * 0.01745329f, c = cosf(rad), s = sinf(rad);
  const float Ro = sqrtf(std::max(0.0f, (R - 0.5f) * (R - 0.5f) - w * w / 4.0f));
  const int x1 = (int) lroundf(c * Ro), y1 = (int) lroundf(s * Ro), x2 = (int) lroundf(c * (R - len)), y2 = (int) lroundf(s * (R - len));
  if (!sees(std::min(x1, x2) - w, std::min(y1, y2) - w, std::max(x1, x2) + w, std::max(y1, y2) + w)) return;
  const PenPart pp = pen_part(obj, part_ix(part));
  const lv_color_t col = pp.ink ? hex(pp.k->ink(pp.b.ink, pp.lt, pp.set)) : ink(obj, k);
  const int shp = pp.b.has & S::K_SHAPE ? pp.b.shape : 0;  // tokens/theme.json shape.ticks: lines, dots, blocks, none
  if (shp == 3) return;
  if (const int cs = snap_cell(pp.k)) {  // lines: k whole cells across, Bresenham (no thick blob); dots / blocks: a cell shape
    const Grid g(*this, cs);
    if (shp == 1 || shp == 2) {
      Shp sh{shp == 1 ? Shp::DISC : Shp::SQUARE, roundf((x1 + x2) / 2.0f / cs) * cs, roundf((y1 + y2) / 2.0f / cs) * cs, cells_of(2 * w, cs) * cs / 2.0f};
      cells(*this, g, sh, col);
    } else cell_line(*this, g, (float) x1, (float) y1, (float) x2, (float) y2, cells_of(w, cs), col);
    return;
  }
  if (shp == 1 || shp == 2) {  // a dot / a square block at the mark's middle, as wide as twice its stroke
    const int mx = cx + (x1 + x2) / 2, my = cy + (y1 + y2) / 2, dm = 2 * w;
    lv_draw_rect_dsc_t r;
    lv_draw_rect_dsc_init(&r);
    r.bg_color = col; r.bg_opa = LV_OPA_COVER; r.radius = shp == 1 ? LV_RADIUS_CIRCLE : 0;
    const lv_area_t a = {mx - dm / 2, my - dm / 2, mx - dm / 2 + dm - 1, my - dm / 2 + dm - 1};
    lv_draw_rect(layer, &r, &a);
    return;
  }
  lv_draw_line_dsc_t d;
  lv_draw_line_dsc_init(&d);
  d.p1.x = cx + x1; d.p1.y = cy + y1; d.p2.x = cx + x2; d.p2.y = cy + y2;
  d.width = w; d.color = col; d.opa = LV_OPA_COVER;
  lv_draw_line(layer, &d);
}
void Pen::knob(int x, int y, const char *sz, const char *k, const char *halo, const char *ring) const {
  if (PEN_VEILED("knob")) return;
  const int dm = size(sz), rw = size(ring);
  const PenPart pp = pen_part(obj, G::P_KNOB);
  const int sh = pp.b.has & S::K_SHADOW ? std::max(abs(pp.b.shadow.dx.px[DN]), abs(pp.b.shadow.dy.px[DN])) : 0;
  if (!sees(x - dm - sh, y - dm - sh, x + dm + sh, y + dm + sh)) return;
  const int shp = pp.b.has & S::K_SHAPE ? pp.b.shape : 0;  // shape.knob: circle, square, diamond, ring, pixel (T3: snapped)
  const lv_color_t c = pp.ink ? hex(pp.k->ink(pp.b.ink, pp.lt, pp.set)) : ink(obj, k), h = ink(obj, halo);
  if (const int cs = snap_cell(pp.k)) {  // an odd number of whole cells, centred on a cell centre on the arc's grid
    const Grid g(*this, cs);
    const float X = roundf((float) x / cs) * cs, Y = roundf((float) y / cs) * cs;
    const int n = cells_of(dm, cs) | 1, nh = rw > 0 ? cells_of(rw, cs) : 0;
    const Shp::K K = shp == 0 || shp == 3 ? Shp::DISC : shp == 2 ? Shp::DIAMOND : Shp::SQUARE;
    const float R = n * cs / 2.0f, RD = K == Shp::DIAMOND ? R * 1.0f : R;
    if (sh) {
      const float sx = roundf((float) pp.b.shadow.dx.px[DN] / cs) * cs, sy = roundf((float) pp.b.shadow.dy.px[DN] / cs) * cs;
      cells(*this, g, Shp{K, X + sx, Y + sy, RD + nh * cs}, hex(pp.k->ink(pp.b.shadow.ink, pp.lt, pp.set)));
    }
    if (shp == 3) {  // a ring: the ink round the halo
      cells(*this, g, Shp{Shp::DISC, X, Y, R}, c);
      cells(*this, g, Shp{Shp::DISC, X, Y, R - std::max(1, cells_of(std::max(rw, dm / 4), cs)) * cs}, h);
    } else {
      if (nh) cells(*this, g, Shp{K, X, Y, RD + nh * cs}, h);
      cells(*this, g, Shp{K, X, Y, RD}, c);
    }
    if (pp.b.has & S::K_EDGES) {  // bevels on the handle's cells
      const int gx = g.cell(X, g.ox), gy = g.cell(Y, g.oy);
      const lv_area_t in = {(gx - n / 2) * cs, (gy - n / 2) * cs, (gx + n / 2) * cs + cs - 1, (gy + n / 2) * cs + cs - 1};
      draw_edges(layer, in, *pp.k, pp.b, pp.lt, pp.set, 0);
    }
    return;
  }
  lv_area_t a = {cx + x - dm / 2, cy + y - dm / 2, cx + x - dm / 2 + dm - 1, cy + y - dm / 2 + dm - 1};
  lv_draw_rect_dsc_t d;
  lv_draw_rect_dsc_init(&d);
  if (sh) {  // its shadow first
    d.bg_color = hex(pp.k->ink(pp.b.shadow.ink, pp.lt, pp.set)); d.bg_opa = LV_OPA_COVER; d.radius = shp == 0 || shp == 3 ? LV_RADIUS_CIRCLE : 0;
    lv_area_t s = a;
    lv_area_move(&s, pp.b.shadow.dx.px[DN], pp.b.shadow.dy.px[DN]);
    if (shp != 2) lv_draw_rect(layer, &d, &s);
    lv_draw_rect_dsc_init(&d);
  }
  if (shp == 2) {  // a diamond: two triangles, the halo ring round it as a bigger diamond under
    for (int pass = 0; pass < 2; pass++) {
      const int r = dm / 2 + (pass ? 0 : rw);
      lv_draw_triangle_dsc_t t;
      lv_draw_triangle_dsc_init(&t);
      t.color = pass ? c : h; t.opa = LV_OPA_COVER;
      const int X = cx + x, Y = cy + y;
      t.p[0] = {(lv_value_precise_t) (X - r), (lv_value_precise_t) Y}; t.p[1] = {(lv_value_precise_t) X, (lv_value_precise_t) (Y - r)}; t.p[2] = {(lv_value_precise_t) (X + r), (lv_value_precise_t) Y};
      lv_draw_triangle(layer, &t);
      t.p[1] = {(lv_value_precise_t) X, (lv_value_precise_t) (Y + r)};
      lv_draw_triangle(layer, &t);
    }
  } else {
    d.radius = shp == 1 || shp == 4 ? 0 : LV_RADIUS_CIRCLE;
    d.bg_color = shp == 3 ? h : c; d.bg_opa = LV_OPA_COVER;  // a ring: the ink as a thick border round the halo
    d.border_width = shp == 3 ? std::max(rw, dm / 4) : rw; d.border_color = shp == 3 ? c : h; d.border_opa = LV_OPA_COVER;
    lv_draw_rect(layer, &d, &a);
  }
  if (pp.b.has & S::K_EDGES) {  // bevels on the handle (inside its ring)
    lv_area_t in = a;
    lv_area_increase(&in, -rw, -rw);
    draw_edges(layer, in, *pp.k, pp.b, pp.lt, pp.set, shp == 0 || shp == 3 ? dm / 2 : 0);
  }
}
void Pen::text(int x, int y, const char *s, const char *type, const char *k) const {
  const fw::ty::Id id = type_id(type);
  const lv_font_t *f = skin_font(look_of(obj)->skin.get(), id);  // the theme's font for the style (C49)
  const int ls = fw::ty::STYLES[id].letter_space;
  lv_point_t sz;
  lv_text_get_size(&sz, s, f, ls, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
  lv_font_glyph_dsc_t g;  // the visual middle of a digit below the line top (cap height centred, the text-box rule)
  float mid = f->line_height / 2.0f;
  if (lv_font_get_glyph_dsc(f, &g, '0', 0)) mid = (f->line_height - f->base_line) - (g.ofs_y + g.box_h / 2.0f);
  const int x0 = x - (int) lroundf(sz.x / 2.0f), y0 = y - (int) lroundf(mid);
  const PenPart pp = pen_part(obj, type_part[defs[ix(type)].i]);  // the text part of its type (ink, shadow)
  const int sx = pp.b.has & S::K_SHADOW ? pp.b.shadow.dx.px[DN] : 0, sy = pp.b.has & S::K_SHADOW ? pp.b.shadow.dy.px[DN] : 0;
  // a partial redraw that crosses only its shadow draws it too (else the shadow there is wiped)
  if (!sees(x0 + std::min(0, sx), y0 + std::min(0, sy), x0 + sz.x + std::max(0, sx) + bold_cell(*look_of(obj)->skin), y0 + f->line_height + std::max(0, sy))) return;
  lv_draw_label_dsc_t d;
  lv_draw_label_dsc_init(&d);
  d.font = f; d.color = pp.b.has & S::K_TEXT && (!(combo(obj) & S::DISABLED) || (pp.k->state[type_part[defs[ix(type)].i]][1 + G::ST_DISABLED].has & S::K_TEXT))
                         ? hex(pp.k->ink(pp.b.text, pp.lt, pp.set)) : ink(obj, k);
  d.letter_space = ls; d.text = s; d.text_local = 1;
  lv_area_t a = {cx + x0, cy + y0, cx + x0 + sz.x, cy + y0 + f->line_height};
  if (pp.b.has & S::K_SHADOW) {
    lv_draw_label_dsc_t sd = d;
    sd.color = hex(pp.k->ink(pp.b.shadow.ink, pp.lt, pp.set));
    lv_area_t sa = a;
    lv_area_move(&sa, pp.b.shadow.dx.px[DN], pp.b.shadow.dy.px[DN]);
    lv_draw_label(layer, &sd, &sa);
  }
  lv_draw_label(layer, &d, &a);
  if ((pp.b.has & S::K_WEIGHT) && pp.b.bold) { lv_area_move(&a, bold_cell(*pp.k), 0); lv_draw_label(layer, &d, &a); }  // synthetic bold
}
int Pen::text_w(const char *s, const char *type) const {
  const fw::ty::Id id = type_id(type);
  lv_point_t sz;
  lv_text_get_size(&sz, s, skin_font(look_of(obj)->skin.get(), id), fw::ty::STYLES[id].letter_space, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
  return sz.x;
}
void Pen::rect(int x, int y, int w, int h, const char *k, int radius, int border, int foot) const {
  if (w <= 0 || h <= 0 || !sees(x, y, x + w - 1, y + h - 1)) return;
  lv_draw_rect_dsc_t d;
  lv_draw_rect_dsc_init(&d);
  d.radius = radius;
  if (border > 0) { d.bg_opa = LV_OPA_TRANSP; d.border_color = ink(obj, k); d.border_width = border; d.border_opa = LV_OPA_COVER; }
  else { d.bg_color = ink(obj, k); d.bg_opa = LV_OPA_COVER; }
  lv_area_t a = {cx + x, cy + y, cx + x + w - 1, cy + y + h - 1 + (foot ? radius : 0)};
  const lv_area_t clip = layer->_clip_area;
  if (foot) layer->_clip_area.y2 = std::min<int32_t>(layer->_clip_area.y2, cy + y + h - 1);
  if (layer->_clip_area.y2 >= layer->_clip_area.y1) lv_draw_rect(layer, &d, &a);
  layer->_clip_area = clip;
}
lv_obj_t *drawn(lv_obj_t *parent, void (*draw)(const Pen &), const char *layout) {
  lv_obj_t *o = box(parent, layout);
  use(o, "fit");  // as tall as wide, but never taller than its parent: the Pen takes the largest square that fits
  lv_obj_add_event_cb(o, on_draw, LV_EVENT_DRAW_MAIN, (void *) draw);
  lv_obj_add_event_cb(o, on_size, LV_EVENT_GET_SELF_SIZE, nullptr);
  lv_obj_add_event_cb(o, on_size, LV_EVENT_SIZE_CHANGED, nullptr);
  return o;
}
lv_obj_t *strip(lv_obj_t *parent, void (*draw)(const Pen &), const char *look) {
  lv_obj_t *o = box(parent, "free");
  use(o, look);
  lv_obj_add_event_cb(o, on_draw, LV_EVENT_DRAW_MAIN, (void *) draw);
  return o;
}
void redraw(lv_obj_t *o, int x, int y, int w, int h) {
  int cx, cy, half;
  square(o, cx, cy, half);
  const int m = 2 * snap_cell(look_of(o)->skin.get());  // snapped: shapes sit up to a cell off, thick runs a cell more
  lv_area_t a = {cx + x - m, cy + y - m, cx + x + w - 1 + m, cy + y + h - 1 + m};
  lv_obj_invalidate_area(o, &a);
}
void at(float deg, int r, int *x, int *y) {
  const float a = deg * 0.01745329f;
  *x = (int) lroundf(cosf(a) * r); *y = (int) lroundf(sinf(a) * r);
}
int half(lv_obj_t *o) { int cx, cy, h; square(o, cx, cy, h); return h; }
// how far a Pen knob reaches from its centre in the live theme: the knob, its halo ring (snapped: whole cells round
// it), the half cell a snapped knob moves to its grid, edges, a shadow, the AA fringe. A moving knob repaints this
// much round its path (2026-10-28: Phosphor's snapped knob + halo outran the dial's knob/2 + 2: ghost edges stayed)
int knob_reach(lv_obj_t *o, const char *sz, const char *ring) {
  const int dm = size(sz), rw = size(ring);
  const PenPart pp = pen_part(o, G::P_KNOB);
  const int sh = pp.b.has & S::K_SHADOW ? std::max(abs(pp.b.shadow.dx.px[DN]), abs(pp.b.shadow.dy.px[DN])) : 0;
  int half = dm / 2 + 1;
  if (const int cs = snap_cell(pp.k)) half = (cells_of(dm, cs) | 1) * cs / 2 + (rw > 0 ? cells_of(rw, cs) : 0) * cs + cs;
  if (pp.b.has & S::K_EDGES) half += 2;
  return half + sh + 2;
}
void redraw_arc(lv_obj_t *o, int r, float a0, float a1, int pad) {
  if (a1 < a0) std::swap(a0, a1);
  int x, y, x1 = INT32_MAX, y1 = INT32_MAX, x2 = INT32_MIN, y2 = INT32_MIN;
  for (float a = a0;; a += 5) {  // the span's bounding box: its ends and every 5 degrees between (plus the pad)
    if (a > a1) a = a1;
    at(a, r, &x, &y);
    x1 = std::min(x1, x); y1 = std::min(y1, y); x2 = std::max(x2, x); y2 = std::max(y2, y);
    if (a >= a1) break;
  }
  redraw(o, x1 - pad, y1 - pad, x2 - x1 + 2 * pad + 1, y2 - y1 + 2 * pad + 1);
}
void pointer(lv_obj_t *o, float *deg, int *dist, int *half) {
  lv_point_t p;
  lv_indev_get_point(lv_indev_active(), &p);
  int cx, cy;
  square(o, cx, cy, *half);
  const float dx = p.x - cx, dy = p.y - cy;
  *deg = fmodf(atan2f(dy, dx) * 57.29578f + 360, 360);
  *dist = (int) lroundf(sqrtf(dx * dx + dy * dy));
}
Page page(lv_obj_t *pg, const char *kind) {
  Page p{pg, nullptr, nullptr};
  use(pg, "page");
  use(pg, "part:page");  // the theme's page frame (face + edges)
  lv_obj_remove_flag(pg, LV_OBJ_FLAG_SCROLLABLE);
  p.head = lv_obj_create(pg);
  lv_obj_remove_style_all(p.head);
  lv_obj_remove_flag(p.head, (lv_obj_flag_t) (LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE));
  use(p.head, "page-head");
  use(p.head, "head-move");  // C45: a flush header bar (empty in Facet)
  p.body = lv_obj_create(pg);
  lv_obj_remove_style_all(p.body);
  use(p.body, "body");
  use(p.body, (std::string("body:") + kind).c_str());
  use(p.body, "body-move");
  use(pg, "page-move");
  if (!strcmp(kind, "center")) lv_obj_remove_flag(p.body, (lv_obj_flag_t) (LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE));
  else {
    lv_obj_add_flag(p.body, (lv_obj_flag_t) (LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE));  // a drag on plain text scrolls too
    lv_obj_set_scroll_dir(p.body, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(p.body, LV_SCROLLBAR_MODE_OFF);
  }
  return p;
}
void tune(const char *name, int px) {  // name "" = every token back; px < 0 = this one back
  for (int i = 0; px >= 0 && *name && i < gen::N_SIZES; i++)  // a LOOK size stays in its range (a layout size is tuned for proofs only)
    if (!strcmp(gen::SIZES[i].name, name) && gen::SIZES[i].look) px = px < gen::SIZES[i].lo ? gen::SIZES[i].lo : px > gen::SIZES[i].hi ? gen::SIZES[i].hi : px;
  if (!*name) tuned.clear(); else if (px < 0) tuned.erase(name); else tuned[name] = px;
  for (Look *L : looks) {
    for (size_t i = 0; i < defs.size(); i++) {
      const Kind k = defs[i].k;
      if ((k >= PAD && k <= WIDTH) || k == SHAPE || k == DERIVED) { lv_style_reset(&L->s[i]); lv_style_init(&L->s[i]); sized(&L->s[i], defs[i]); }
    }
    paint(L, L->t);  // the derived ones
  }
  lv_obj_report_style_change(nullptr);
}
void inset(int px) { tune("layout:inset", px); }
std::string tunables() {  // [[name, token px, now px, kind, lo, hi]]: every size token + the structure ones the page styles read
  std::map<std::string, int> base;
  for (int i = 0; i < gen::N_SIZES; i++) base[gen::SIZES[i].name] = gen::SIZES[i].v;
  for (auto &e : STRUCTURE) base.emplace(e.first, e.second);
  std::string o = "[";
  // [name, token px, now px, "look" | "fixed", lo, hi]: look = themeable within lo..hi, fixed = a layout size
  for (auto &b : base) {
    bool lk = false; int lo = b.second, hi = b.second;
    for (int i = 0; i < gen::N_SIZES; i++) if (b.first == gen::SIZES[i].name) { lk = gen::SIZES[i].look; lo = gen::SIZES[i].lo; hi = gen::SIZES[i].hi; }
    o += std::string(o.size() > 1 ? "," : "") + "[\"" + b.first + "\"," + std::to_string(b.second) + "," + std::to_string(tok(b.first.c_str(), b.second)) +
         (lk ? ",\"look\"," : ",\"fixed\",") + std::to_string(lo) + "," + std::to_string(hi) + "]";
  }
  return o + "]";
}
std::string uses(lv_display_t *d) {  // the size tokens what is on display d reads: its objects' shared styles + the pen
  Look *L = nullptr;
  for (Look *l : looks) if (l->d == d) L = l;
  lv_obj_t *scr = lv_display_get_screen_active(d);
  if (!L || !scr) return "[]";
  std::set<std::string> got;
  reads = &got;
  std::vector<lv_obj_t *> todo{scr};
  while (!todo.empty()) {
    lv_obj_t *o = todo.back();
    todo.pop_back();
    if (lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN)) continue;  // not on screen
    for (uint32_t i = 0; i < o->style_cnt; i++) {
      const lv_style_t *st = o->styles[i].style;
      if (st < L->s.data() || st >= L->s.data() + L->s.size()) continue;
      const int k = (int) (st - L->s.data());
      sized(&L->s[k], defs[k]);  // the same values again: only the reads are new
      paint(L, L->t, k);
    }
    // what the core places from live tokens on a tune (a chart's bars, a toast's slot, a sheet's padding) re-reads them
    // here with the same values: those tokens are listed too
    lv_obj_send_event(o, LV_EVENT_STYLE_CHANGED, nullptr);
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) todo.push_back(lv_obj_get_child(o, i));
  }
  lv_obj_invalidate(scr);
  lv_refr_now(d);  // the pen's fx::size reads while drawing
  reads = nullptr;
  std::string o = "[";
  for (const std::string &n : got) o += (o.size() > 1 ? ",\"" : "\"") + n + "\"";
  return o + "]";
}
bool migrated(const fw::Component *c) { return c && c->meta && strstr(c->meta, "\"contract\": 2"); }
const char *style_name(const lv_style_t *s) {
  for (Look *L : looks)
    if (s >= L->s.data() && s < L->s.data() + L->s.size()) return defs[s - L->s.data()].name.c_str();
  return nullptr;
}
}  // namespace fx

namespace {
struct GridL { int cols = 1, want = 1, min = -1, W = -1, gap = 0, cw = 0, ch = 0, left = 0; bool row = false, wide = false, busy = false; std::string gapn; };
struct GridC { int w = 1, h = 1; bool tap = false; };
std::map<lv_obj_t *, GridL> grids;
std::map<lv_obj_t *, GridC> gcells;
int grid_gap(const std::string &n) {
  using namespace fw;
  if (n == "none") return 0;
  if (n == "near") return tok("space:3", sp::s3);
  if (n == "block") return tok("layout:group", lay::group);
  return tok("layout:gutter", lay::gutter);
}
// the geometry from the root's own content width; true = the cells changed size or count
bool grid_measure(lv_obj_t *r, GridL &g) {
  using namespace fw;
  const int W = lv_obj_get_content_width(r), was = g.cols * 100000 + g.cw * 100 + g.ch;
  g.W = W; g.gap = grid_gap(g.gapn);
  g.cols = g.min >= 0 ? LV_CLAMP(1, (W + g.gap) / (ic::PX[g.min] + g.gap), 12) : g.want;
  g.cw = LV_MAX(0, (W - g.gap * (g.cols - 1)) / g.cols / sp::GRID * sp::GRID);
  g.left = LV_MAX(0, (W - g.cw * g.cols - g.gap * (g.cols - 1)) / 2 / sp::GRID * sp::GRID);
  g.ch = g.wide ? g.cw * 9 / 16 / sp::GRID * sp::GRID : g.cw;
  if (g.row) {
    lv_obj_set_style_pad_left(r, g.left, 0);
    lv_obj_set_style_pad_column(r, g.gap, 0);
    lv_obj_set_style_pad_row(r, g.gap, 0);
  }
  return g.cols * 100000 + g.cw * 100 + g.ch != was;
}
void grid_lay(lv_obj_t *r, GridL &g) {  // every cell into the first free place that fits, in reading order
  std::vector<std::vector<bool>> occ;
  for (uint32_t i = 0; i < lv_obj_get_child_count(r); i++) {
    lv_obj_t *c = lv_obj_get_child(r, (int32_t) i);
    auto it = gcells.find(c);
    if (it == gcells.end()) continue;
    const int w = LV_CLAMP(1, it->second.w, g.cols), h = g.row ? 1 : LV_CLAMP(1, it->second.h, 8);
    const int cw = w * g.cw + (w - 1) * g.gap, chh = h * g.ch + (h - 1) * g.gap;
    if (g.row) lv_obj_set_size(c, cw, LV_SIZE_CONTENT);
    else
      for (int y = 0, done = 0; !done; y++) {
        while ((int) occ.size() < y + h) occ.emplace_back(g.cols, false);
        for (int x = 0; x + w <= g.cols && !done; x++) {
          bool free = true;
          for (int yy = y; yy < y + h && free; yy++)
            for (int xx = x; xx < x + w && free; xx++) free = !occ[yy][xx];
          if (!free) continue;
          for (int yy = y; yy < y + h; yy++)
            for (int xx = x; xx < x + w; xx++) occ[yy][xx] = true;
          lv_obj_set_pos(c, g.left + x * (g.cw + g.gap), y * (g.ch + g.gap));
          lv_obj_set_size(c, cw, chh);
          done = 1;
        }
      }
    const bool tap = it->second.tap && g.cw >= fw::touch::min && (g.row || g.ch >= fw::touch::min);
    if (tap) lv_obj_add_flag(c, LV_OBJ_FLAG_CLICKABLE); else lv_obj_remove_flag(c, LV_OBJ_FLAG_CLICKABLE);
  }
}
void grid_changed(lv_event_t *e) {  // a new size or a tuned token: measure again (only when it changes the geometry)
  lv_obj_t *r = (lv_obj_t *) lv_event_get_current_target(e);
  auto it = grids.find(r);
  if (it == grids.end() || it->second.busy) return;
  GridL &g = it->second;
  if (lv_obj_get_content_width(r) == g.W && grid_gap(g.gapn) == g.gap) return;
  g.busy = true;
  const bool cols = grid_measure(r, g);
  grid_lay(r, g);
  g.busy = false;
  if (cols) lv_obj_send_event(r, LV_EVENT_REFRESH, nullptr);
}
}  // namespace

namespace fx {
void grid(lv_obj_t *r, int cols, const char *min, const char *gap, const char *aspect) {
  const bool fresh = !grids.count(r);
  GridL &g = grids[r];
  g.want = LV_CLAMP(1, cols, 8); g.gapn = gap; g.row = !strcmp(aspect, "row"); g.wide = !strcmp(aspect, "wide"); g.min = -1;
  for (int i = 0; i < fw::ic::N_SIZES; i++) if (!strcmp(fw::ic::SIZE_NAMES[i], min)) g.min = i;
  if (g.row) { lv_obj_set_layout(r, LV_LAYOUT_FLEX); lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW_WRAP); }
  if (fresh) {
    lv_obj_add_event_cb(r, grid_changed, LV_EVENT_SIZE_CHANGED, nullptr);
    lv_obj_add_event_cb(r, grid_changed, LV_EVENT_STYLE_CHANGED, nullptr);
    lv_obj_add_event_cb(r, [](lv_event_t *e) { grids.erase((lv_obj_t *) lv_event_get_current_target(e)); }, LV_EVENT_DELETE, nullptr);
  }
  lv_obj_update_layout(r);  // the own content box now, so the first cells are sized at build (an Icon's fit: cell)
  g.busy = true; grid_measure(r, g); g.busy = false;
}
lv_obj_t *grid_cell(lv_obj_t *r, int w, int h, bool tap) {
  auto it = grids.find(r);
  lv_obj_t *c = box(r, it != grids.end() && it->second.row ? "col" : "centre");
  lv_obj_add_flag(c, fw::spec::SLOT);
  gcells[c] = {w, h, tap};
  lv_obj_add_event_cb(c, [](lv_event_t *e) { gcells.erase((lv_obj_t *) lv_event_get_current_target(e)); }, LV_EVENT_DELETE, nullptr);
  if (it != grids.end()) { it->second.busy = true; grid_lay(r, it->second); it->second.busy = false; }
  return c;
}
void grid_place(lv_obj_t *r) {
  auto it = grids.find(r);
  if (it == grids.end()) return;
  it->second.busy = true; grid_lay(r, it->second); it->second.busy = false;
}
int grid_cols(lv_obj_t *r) { auto it = grids.find(r); return it == grids.end() ? 1 : it->second.cols; }
}  // namespace fx

