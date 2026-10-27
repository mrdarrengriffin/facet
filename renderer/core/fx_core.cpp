// fx_core.cpp: the contract core (renderer/CONTRACT.md). Token styles are SHARED lv_style_t, one set per display
// (the workbench shows several stages in different themes at once; a panel has one), named like the tokens
// (tools/gen_fx.mjs -> fx_gen.h). A theme / accent / text-size change repaints the display's set and reports one
// style change: no object holds a colour, so none can keep an old one.
#include "fx.h"
#include "fw.h"
#include "fx_gen.h"
#include "spec.h"  // spec::SLOT: a Grid cell is a slot
#include "core/lv_obj_private.h"  // fx::uses walks o->styles
#include "core/lv_obj_style_private.h"
#include "display/lv_display_private.h"  // a display going away: its objects first, then its look (no leak)
#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace fw { void text_metrics(ty::Id s, int &line_space, int &pad_top, int &pad_bottom, int &translate_y); }

namespace {
enum Kind { FG, BG, BORDER, LINE, INK, PAD, GAP, PADX, RADIUS, HEIGHT, MAXH, MINW, MAXW, WIDTH, TYPE, SHAPE, DERIVED,
            TINT, OUTLINE, DIGIT, COLON, RAISE, BASE };
struct Def { std::string name; Kind k; int i; };
std::vector<Def> defs;  // sorted by name
const char *const SHAPES[] = {"box", "col", "row", "stack", "centre", "cell", "grow", "free", "page", "page-head", "body",
                              "body:list", "body:cards", "body:center", "body:page", "bleed", "bg-none", "fit",
                              "fill", "fill-w", "row-top", "fade-press", "fade-release", "gap:gutter", "pair", "wrap",
                              "header", "header-back", "header-title", "header-label", "header-end", "header-time",
                              "hug", "over", "over-cell", "transp", "opaque", "fade-press-ink", "fade-release-ink", "no-outline", "cells", "cell-text",
                              "rule-top", "row-mid", "row-end", "fade-press-fill", "fade-release-fill",
                              "square:ibtn-play", "square:ibtn-round", "square:ibtn-square", "tall:chip-h", "tall:list-row-h"};
// tint:<c> = the colour at chip.tint over what is behind (Status chip; colour.json mixes chip-<tone>); outline:<c> = the
// focus ring (states.json focus: stroke focus, touch focus-offset outside the box)
const char *const TINTS[] = {"ok", "warn", "error", "accent", "blue", "text-dim"};
const char *const OUTLINES[] = {"focus-ring"};
// baseline:<a>:<b>: text b's figures (its glyph `ch`) on text a's baseline (a margin top), the Time's AM / PM + seconds
struct Base { fw::ty::Id a, b; uint32_t ch; };
const Base BASES[] = {{fw::ty::clock_hero_mono, fw::ty::clock_seconds_mono, '0'}, {fw::ty::clock_hero, fw::ty::clock_seconds, '0'},
  {fw::ty::clock_hero_mono, fw::ty::clock_ampm_mono, 'M'}, {fw::ty::clock_hero, fw::ty::clock_ampm, 'M'},
  {fw::ty::clock_mid_mono, fw::ty::clock_ampm_mono, 'M'}, {fw::ty::clock_mid, fw::ty::clock_ampm, 'M'},
  {fw::ty::clock_compact_mono, fw::ty::clock_ampm_sm, 'M'}, {fw::ty::clock_compact, fw::ty::clock_ampm_sm, 'M'}};
const char *type_name(fw::ty::Id id) {
  for (int i = 0; i < fx::gen::N_TYPES; i++) if (fx::gen::TYPES[i].id == id) return fx::gen::TYPES[i].name;
  abort();
}
// derived sizes: one value worked out from other tokens, so a component never does the sum (paint() says how)
const char *const DERIVED_N[] = {"dial-wing-up", "dial-wing-down", "dial-slot", "card-pad", "card-text", "card-gap", "card-gap-s", "card-row", "pair-rule", "label-row",
                                 "list-text-t", "list-text-ts", "list-text-ot", "list-text-ots", "list-slot", "section-label", "empty-pad"};
struct Look { lv_display_t *d; const fw::Theme *t = nullptr; int large = -1; std::vector<lv_style_t> s; };
std::vector<Look *> looks;

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
    if (g == "space") { defs.push_back({"pad:" + v, PAD, i}); defs.push_back({"gap:" + v, GAP, i}); defs.push_back({"pad-x:" + v, PADX, i}); }
    else if (g == "radius") defs.push_back({"radius:" + v, RADIUS, i});
    else if (g == "layout" || g == "touch") {
      defs.push_back({"h:" + v, HEIGHT, i}); defs.push_back({"max-w:" + v, MAXW, i}); defs.push_back({"w:" + v, WIDTH, i});
      defs.push_back({"min-w:" + v, MINW, i});  // a floor: fills its parent but never narrower (a Cells cell)
      defs.push_back({"max-h:" + v, MAXH, i});  // a ceiling: with h: of the same token = exactly that tall (a Cells cell)
      if (g == "layout") defs.push_back({"gap:" + v, GAP, i});  // gap between children = a layout token
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
  std::sort(defs.begin(), defs.end(), [](const Def &a, const Def &b) { return a.name < b.name; });
  for (size_t i = 1; i < defs.size(); i++) if (defs[i].name == defs[i - 1].name) abort();  // a name made twice
}
int index_of(const char *n) {
  init_defs();
  auto it = std::lower_bound(defs.begin(), defs.end(), n, [](const Def &d, const char *k) { return d.name < k; });
  return it != defs.end() && it->name == n ? (int) (it - defs.begin()) : -1;
}
int ix(const char *n) { const int i = index_of(n); if (i < 0) abort(); return i; }
void flex(lv_style_t *s, lv_flex_flow_t f, lv_flex_align_t main, lv_flex_align_t cross, lv_flex_align_t track) {
  lv_style_set_layout(s, LV_LAYOUT_FLEX); lv_style_set_flex_flow(s, f);
  lv_style_set_flex_main_place(s, main); lv_style_set_flex_cross_place(s, cross); lv_style_set_flex_track_place(s, track);
}
// fx::tune (the proofs, the workbench Layout lab): a size token as if it said px; "squeeze" = the Body's max width
std::map<std::string, int> tuned;
std::set<std::string> *reads = nullptr;  // fx::uses: the size tokens read while on (styles + pen)
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
  else if (k == "wrap") flex(s, LV_FLEX_FLOW_ROW_WRAP, S, S, S);  // rows that wrap: a growing child counts its min-w
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
}
lv_color_t colour(int i, const fw::Theme *t) {  // a colour token or a state role, resolved in theme t
  return i < fx::gen::N_COLOURS ? t->*fx::gen::COLOURS[i].f : fx::gen::ROLES[i - fx::gen::N_COLOURS].f(*t);
}
int box_of(fw::ty::Id id) { return fw::ty::STYLES[id].box; }
void paint(Look *L, const fw::Theme *t, int only = -1) {  // colours from the theme, type from the text-size setting (only = one def)
  L->t = t; L->large = fw::text_large();
  for (size_t i = 0; i < defs.size(); i++) {
    if (only >= 0 && (int) i != only) continue;
    lv_style_t *s = &L->s[i];
    const Def &d = defs[i];
    if (d.k == DERIVED) {
      // dial-wing: the Dial's centre column is wing + value + wing, so the value's middle is the dial's middle (the
      // approved geometry). Above: caption + space 7; below: space 2 + unit + space 9 + the button. Both wings take
      // the larger, the caption sits at the top wing's foot and the unit row at the bottom wing's head.
      using namespace fw;
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
    if (d.k == TINT) { lv_style_set_bg_color(s, colour(d.i, t)); lv_style_set_bg_opa(s, (lv_opa_t) lroundf(fw::tokens::comp::chip_tint * 255)); continue; }
    if (d.k == OUTLINE) {
      lv_style_set_outline_color(s, colour(d.i, t)); lv_style_set_outline_opa(s, LV_OPA_COVER);
      lv_style_set_outline_width(s, tok("stroke:focus", fw::stroke::focus)); lv_style_set_outline_pad(s, tok("touch:focus-offset", fw::touch::focus_offset) - tok("stroke:focus", fw::stroke::focus));  // the ring's outer edge at the offset
      continue;
    }
    if (d.k == DIGIT || d.k == COLON || d.k == RAISE) {  // type.json cells: em of the type's size
      const fw::ty::Cells &k = fw::ty::CELLS[d.i];
      const fw::ty::Style &st = fw::ty::STYLES[k.style];
      if (d.k == RAISE) {
        int ls, pt, pb, ty;
        fw::text_metrics(k.style, ls, pt, pb, ty);
        lv_style_set_translate_y(s, ty + (int) lroundf(k.colon_y * st.size));
      } else {
        lv_style_set_width(s, (int) lroundf((d.k == COLON ? k.colon : k.digit) * st.size)); lv_style_set_height(s, st.box);
        flex(s, LV_FLEX_FLOW_ROW, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
      }
      continue;
    }
    if (d.k == BASE) {  // where b's figures end below its box top, against a's (cap height centred in the box)
      const Base &b = BASES[d.i];
      auto cap_bottom = [](fw::ty::Id id, uint32_t ch) {
        const lv_font_t *f = fw::font(id);
        lv_font_glyph_dsc_t g;
        const int cap = lv_font_get_glyph_dsc(f, &g, ch, 0) ? g.box_h : f->line_height / 2;
        return (fw::ty::STYLES[id].box + cap) / 2;
      };
      lv_style_set_margin_top(s, cap_bottom(b.a, '0') - cap_bottom(b.b, b.ch));
      continue;
    }
    if (d.k <= INK) {
      const lv_color_t c = colour(d.i, t);
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
      fw::text_metrics(id, ls, pt, pb, ty);
      lv_style_set_text_font(s, fw::font(id));
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
  else if (d.k == SHAPE) shape(s, d.name.c_str());
}
Look *make(lv_display_t *d, const fw::Theme *t) {
  init_defs();
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
    if (fw::T && (L->t != fw::T || L->large != (int) fw::text_large())) { paint(L, fw::T); lv_obj_report_style_change(nullptr); }
    return L;
  }
  return make(d, fw::T ? fw::T : fw::theme(false));
}
// drawn parts: inks read from the object's look at draw time
lv_color_t ink(lv_obj_t *o, const char *n) {
  const int i = ix(n);
  if (defs[i].k > LINE) abort();
  return colour(defs[i].i, look_of(o)->t);
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
void on_draw(lv_event_t *e) {
  lv_obj_t *o = (lv_obj_t *) lv_event_get_current_target(e);
  fx::Pen p{lv_event_get_layer(e), o, 0, 0, 0};
  square(o, p.cx, p.cy, p.half);
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
    if (L->t != t || L->large != (int) fw::text_large()) { paint(L, t); lv_obj_report_style_change(nullptr); }
    return;
  }
  make(d, t);
}
void use(lv_obj_t *o, const char *t, lv_style_selector_t sel) { lv_obj_add_style(o, &look_of(o)->s[ix(t)], sel); }
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
lv_obj_t *icon(lv_obj_t *p, const char *name, const char *size, const char *ink) {
  const fw::ic::Size sz = !strcmp(size, "card") ? fw::ic::card : !strcmp(size, "glyph") ? fw::ic::glyph
                         : !strcmp(size, "feature") ? fw::ic::feature : fw::ic::icon;
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
  lv_point_t sz;
  lv_text_get_size(&sz, lv_label_get_text(l), lv_obj_get_style_text_font(l, LV_PART_MAIN), lv_obj_get_style_text_letter_space(l, LV_PART_MAIN), 0,
                   LV_COORD_MAX, LV_TEXT_FLAG_NONE);
  lv_obj_set_width(l, (sz.x + fw::sp::GRID - 1) / fw::sp::GRID * fw::sp::GRID);
}
void name(lv_obj_t *o, const char *n) { if (!names.count(o)) lv_obj_add_event_cb(o, forget, LV_EVENT_DELETE, nullptr); names[o] = n; }
int type_box(const char *type) { return box_of(type_id(type)); }
int size(const char *t) {
  for (int i = 0; i < gen::N_SIZES; i++) if (!strcmp(gen::SIZES[i].name, t)) return tok(t, gen::SIZES[i].v);
  abort();
}
bool Pen::sees(int x1, int y1, int x2, int y2) const {
  const lv_area_t &c = layer->_clip_area;
  return cx + std::max(x1, x2) >= c.x1 && cx + std::min(x1, x2) <= c.x2 && cy + std::max(y1, y2) >= c.y1 && cy + std::min(y1, y2) <= c.y2;
}
void Pen::line(int x1, int y1, int x2, int y2, const char *stroke, const char *k, bool round) const {
  const int w = size(stroke);
  if (!sees(std::min(x1, x2) - w, std::min(y1, y2) - w, std::max(x1, x2) + w, std::max(y1, y2) + w)) return;
  lv_draw_line_dsc_t d;
  lv_draw_line_dsc_init(&d);
  d.p1.x = cx + x1; d.p1.y = cy + y1; d.p2.x = cx + x2; d.p2.y = cy + y2;
  d.width = w; d.color = ink(obj, k); d.opa = LV_OPA_COVER; d.round_start = d.round_end = round;
  lv_draw_line(layer, &d);
}
void Pen::arc(int r, float a0, float a1, const char *stroke, const char *k, bool round) const {
  if (r < 1 || a1 - a0 < 0.5f) return;
  lv_draw_arc_dsc_t d;
  lv_draw_arc_dsc_init(&d);
  const int w = size(stroke);
  if (!sees(-r - w, -r - w, r + w, r + w)) return;
  d.center.x = cx; d.center.y = cy; d.radius = r + w / 2; d.width = w; d.rounded = round;
  d.start_angle = fmodf(a0, 360); d.end_angle = fmodf(a1, 360);
  d.color = ink(obj, k); d.opa = LV_OPA_COVER;
  lv_draw_arc(layer, &d);
}
void Pen::dot(int x, int y, const char *diameter, const char *k) const {
  const int dm = size(diameter);
  if (!sees(x - dm, y - dm, x + dm, y + dm)) return;
  lv_draw_rect_dsc_t d;
  lv_draw_rect_dsc_init(&d);
  d.radius = LV_RADIUS_CIRCLE; d.bg_color = ink(obj, k); d.bg_opa = LV_OPA_COVER;
  lv_area_t a = {cx + x - dm / 2, cy + y - dm / 2, cx + x - dm / 2 + dm - 1, cy + y - dm / 2 + dm - 1};
  lv_draw_rect(layer, &d, &a);
}
void Pen::tick(float deg, const char *length, const char *stroke, const char *k) const {
  // the tick ring rule (core/atoms.cpp ring_draw): ends rounded from the edge-grid position, the outer end pulled in
  // so a slanted butt end's corners stay on the circle
  const int w = size(stroke), len = size(length);
  const float R = half + 1.0f, rad = deg * 0.01745329f, c = cosf(rad), s = sinf(rad);
  const float Ro = sqrtf(std::max(0.0f, (R - 0.5f) * (R - 0.5f) - w * w / 4.0f));
  const int x1 = (int) lroundf(c * Ro), y1 = (int) lroundf(s * Ro), x2 = (int) lroundf(c * (R - len)), y2 = (int) lroundf(s * (R - len));
  if (!sees(std::min(x1, x2) - w, std::min(y1, y2) - w, std::max(x1, x2) + w, std::max(y1, y2) + w)) return;
  lv_draw_line_dsc_t d;
  lv_draw_line_dsc_init(&d);
  d.p1.x = cx + x1; d.p1.y = cy + y1; d.p2.x = cx + x2; d.p2.y = cy + y2;
  d.width = w; d.color = ink(obj, k); d.opa = LV_OPA_COVER;
  lv_draw_line(layer, &d);
}
void Pen::knob(int x, int y, const char *sz, const char *k, const char *halo, const char *ring) const {
  const int dm = size(sz), rw = size(ring);
  if (!sees(x - dm, y - dm, x + dm, y + dm)) return;
  lv_draw_rect_dsc_t d;
  lv_draw_rect_dsc_init(&d);
  d.radius = LV_RADIUS_CIRCLE; d.bg_color = ink(obj, k); d.bg_opa = LV_OPA_COVER;
  d.border_width = rw; d.border_color = ink(obj, halo); d.border_opa = LV_OPA_COVER;
  lv_area_t a = {cx + x - dm / 2, cy + y - dm / 2, cx + x - dm / 2 + dm - 1, cy + y - dm / 2 + dm - 1};
  lv_draw_rect(layer, &d, &a);
}
void Pen::text(int x, int y, const char *s, const char *type, const char *k) const {
  const fw::ty::Id id = type_id(type);
  const lv_font_t *f = fw::font(id);
  const int ls = fw::ty::STYLES[id].letter_space;
  lv_point_t sz;
  lv_text_get_size(&sz, s, f, ls, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
  lv_font_glyph_dsc_t g;  // the visual middle of a digit below the line top (cap height centred, the text-box rule)
  float mid = f->line_height / 2.0f;
  if (lv_font_get_glyph_dsc(f, &g, '0', 0)) mid = (f->line_height - f->base_line) - (g.ofs_y + g.box_h / 2.0f);
  const int x0 = x - (int) lroundf(sz.x / 2.0f), y0 = y - (int) lroundf(mid);
  if (!sees(x0, y0, x0 + sz.x, y0 + f->line_height)) return;
  lv_draw_label_dsc_t d;
  lv_draw_label_dsc_init(&d);
  d.font = f; d.color = ink(obj, k); d.letter_space = ls; d.text = s; d.text_local = 1;
  lv_area_t a = {cx + x0, cy + y0, cx + x0 + sz.x, cy + y0 + f->line_height};
  lv_draw_label(layer, &d, &a);
}
lv_obj_t *drawn(lv_obj_t *parent, void (*draw)(const Pen &), const char *layout) {
  lv_obj_t *o = box(parent, layout);
  use(o, "fit");  // as tall as wide, but never taller than its parent: the Pen takes the largest square that fits
  lv_obj_add_event_cb(o, on_draw, LV_EVENT_DRAW_MAIN, (void *) draw);
  lv_obj_add_event_cb(o, on_size, LV_EVENT_GET_SELF_SIZE, nullptr);
  lv_obj_add_event_cb(o, on_size, LV_EVENT_SIZE_CHANGED, nullptr);
  return o;
}
void redraw(lv_obj_t *o, int x, int y, int w, int h) {
  int cx, cy, half;
  square(o, cx, cy, half);
  lv_area_t a = {cx + x, cy + y, cx + x + w - 1, cy + y + h - 1};
  lv_obj_invalidate_area(o, &a);
}
void at(float deg, int r, int *x, int *y) {
  const float a = deg * 0.01745329f;
  *x = (int) lroundf(cosf(a) * r); *y = (int) lroundf(sinf(a) * r);
}
int half(lv_obj_t *o) { int cx, cy, h; square(o, cx, cy, h); return h; }
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
  lv_obj_remove_flag(pg, LV_OBJ_FLAG_SCROLLABLE);
  p.head = lv_obj_create(pg);
  lv_obj_remove_style_all(p.head);
  lv_obj_remove_flag(p.head, (lv_obj_flag_t) (LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE));
  use(p.head, "page-head");
  p.body = lv_obj_create(pg);
  lv_obj_remove_style_all(p.body);
  use(p.body, "body");
  use(p.body, (std::string("body:") + kind).c_str());
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
  using namespace fw;
  const std::pair<const char *, int> extra[] = {{"layout:inset", lay::inset}, {"layout:gutter", lay::gutter}, {"layout:group", lay::group}, {"space:3", sp::s3}, {"layout:top", lay::top},
    {"layout:header-gap", lay::header_gap}, {"layout:row", lay::row}, {"layout:back-gap", lay::back_gap},
    {"layout:header-item-gap", lay::header_item_gap}, {"layout:header-max-w", lay::header_max_w}, {"space:2", sp::s2}, {"space:7", sp::s7}, {"space:9", sp::s9}, {"touch:button-h", touch::button_h}};
  for (auto &e : extra) base.emplace(e.first, e.second);
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

