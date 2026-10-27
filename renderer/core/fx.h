// fx.h: the contract API (renderer/CONTRACT.md). A migrated component (meta "contract": 2) builds only through this:
// it gets a parent, token NAMES and layout; nothing about the screen, the card it sits in, the theme or the accent.
// Tokens are shared lv_style_t per display, named like the tokens (tools/gen_fx.mjs): fg:/bg:/border:/line:<colour or
// state role> (a role = "<state>-<role>" from tokens/states.json: fg:disabled-ink, bg:pressed-surface), pad:/gap:/pad-x:
// <space step>, radius:<r>, h:<layout token> (min height), max-w:<layout token> (a cap: fills its parent up to it),
// min-w:<layout token> (a floor: fills its parent, never narrower), max-h:<layout token> (with h: = exactly that tall), gap:<layout token> / gap:gutter (between children),
// type:<type style>, tint:<status colour> (the colour at chip.tint over whatever is behind), outline:focus-ring (the
// focus ring outside the box: stroke focus, touch focus-offset), digit:/colon:/colon-raise:<clock type> (a digit cell,
// the colon cell, the colon's raise: type.json cells), baseline:<type>:<small type> (a smaller text's figures on the
// bigger one's baseline), plus the structure styles below and derived ones ("dial-wing": core/fx_core.cpp says how).
#pragma once
#include "lvgl.h"
#include <string>

namespace fw { struct Theme; struct Component; }
namespace fx {
// A bare object, no look of its own: fills its parent's content width, height from its content. layout = "col",
// "row" (cross-centred, gutter gap), "stack" (column, space 2 gap), "centre" (column, centred both ways), "free",
// "wrap" (rows that wrap; a child with "grow" + min-w:<token> takes a track slot of its min-w, then shares the rest).
lv_obj_t *box(lv_obj_t *parent, const char *layout = "col");
// A one-line text in a type style ("type:label") and a colour ("fg:text-dim"); the style's case; ends in "...".
lv_obj_t *text(lv_obj_t *parent, const char *s, const char *type, const char *fg);
void set_text(lv_obj_t *label, const char *s, const char *type);  // new words, the type's case (an update path)
// Apply a shared token style by name (optionally per state / part). Unknown name = abort (a typo crashes the
// workbench, never a silent default).
void use(lv_obj_t *o, const char *token, lv_style_selector_t sel = 0);
void state(lv_obj_t *o, lv_state_t s, bool on);  // o and everything inside it (the styles follow the state)
// A screen / status icon (icons.json) at the icon size "icon", "card", "glyph" (a control's glyph) or "feature" (the
// play / pause glyph), each layer inked by a shared style: the ink named ("fg:text-dim" -> its colour), role layers
// (accent, text-dim, blue, steel) their own token. Follows a live theme switch. The icon box trickles its parent's
// states to its layers.
lv_obj_t *icon(lv_obj_t *parent, const char *name, const char *size, const char *ink);
// a token on every INK layer of an icon (role layers keep their own colours), per state: "ink:on-ink" on CHECKED,
// "fade-press-ink" on PRESSED (the ink's press fade)
void icon_use(lv_obj_t *icon, const char *token, lv_style_selector_t sel = 0);
bool icon_multi(lv_obj_t *icon);  // the icon has role layers (an accent part): its ink alone must not show a state
void pulse(lv_obj_t *o);
// decor (C14): the focus ring (states.json focus: stroke focus, its outer edge touch:focus-offset outside the box) in
// state `show`, hidden by fw::st::OFF; a shared style, so a theme switch repaints it
void focus_ring(lv_obj_t *o, lv_style_selector_t show = LV_STATE_CHECKED);          // Motion pattern "pulse" on o's opacity (forever; reduced motion = still)
lv_obj_t *row(lv_obj_t *parent);   // equal cells with the gutter between: row(), then one cell() per item
lv_obj_t *cell(lv_obj_t *row);
// a one-line text (or a row of fixed boxes) whose box ends on the 4 px grid (what follows it lands on the grid)
void fit_grid(lv_obj_t *label);
// equal parts (a Segmented "fill"): row's children share its content width, each on the 4 px grid, the last takes the
// remainder; again on every size change of row (the sums live here, C8)
void equal(lv_obj_t *row);
// The Grid layout's geometry (C8: a layout arranges; the sums live here so the component holds no size). grid() makes
// `root` a grid of equal cells: `cols` columns, or min = an icon size name ("glyph", "icon" ...): as many min-wide cells
// + gaps as fit, at most 12 ("none" = cols); gap "none" | "near" (space:3) | "group" (layout:gutter) | "block"
// (layout:group); aspect "square" | "wide" (16:9) | "row" (the content's height: cells flow in rows). Everything from
// root's OWN content width (C6) and tokens read live (the Layout lab moves them): cell width snapped down to the 4 px
// grid, half the leftover on the left; re-placed on every size or token change (new cell sizes or a new column count
// send root LV_EVENT_REFRESH: an items grid re-expands, its contents fit the new cells). grid_cell() adds a cell (a spec slot) at the end, spanning w x h cells,
// placed in reading order into the first free place that fits; tap = clickable only while the cell is at least
// touch:min both ways (C32). grid_place() again after cells were deleted; grid_cols() = the columns now.
void grid(lv_obj_t *root, int cols, const char *min, const char *gap, const char *aspect);
lv_obj_t *grid_cell(lv_obj_t *root, int w = 1, int h = 1, bool tap = false);
void grid_place(lv_obj_t *root);
int grid_cols(lv_obj_t *root);
void bleed(lv_obj_t *o);           // the one escape: o's background reaches the page edge (the "bleed" style marks it)
void name(lv_obj_t *o, const char *n);  // a component root's name for the inspector
int size(const char *token);       // a size token ("stroke:hair", "layout:dial-tick") for DRAWING only
int type_box(const char *type);    // a type style's line box ("type:c-meta"), for placing drawn text only

// Drawn parts. drawn() = a box that fills its parent's width, at least as tall as it is wide, calling draw with a
// Pen on every redraw: the largest centred square, coordinates relative to its centre. Strokes name size tokens,
// inks name shared colour styles ("fg:accent"): read at draw time, so a theme / accent switch repaints with no
// component code. Drawing invalidates only what changed: redraw(o, x, y, w, h) (centre-relative, the pen's space)
// marks one area; never lv_obj_invalidate on a moving part.
struct Pen {
  lv_layer_t *layer; lv_obj_t *obj; int cx, cy, half;
  int w = 0, h = 0;  // the box's content width and height (a strip: the centre is (cx, cy), x runs -w / 2 .. w / 2)
  void line(int x1, int y1, int x2, int y2, const char *stroke, const char *ink, bool round = false) const;
  // r = the stroke's middle line; 0 deg = 3 o'clock, clockwise; round = round caps
  void arc(int r, float deg0, float deg1, const char *stroke, const char *ink, bool round = true) const;
  void dot(int x, int y, const char *diameter, const char *ink) const;
  // a scale mark at deg, from the box edge inward `length` long (a size token), butt ends kept on the circle
  void tick(float deg, const char *length, const char *stroke, const char *ink) const;
  // a round handle of diameter `size` centred on x, y, with a `ring` stroke in `halo` (the canvas colour) round it
  void knob(int x, int y, const char *size, const char *ink, const char *halo, const char *ring) const;
  // a short text centred on x, y (its digits' visual middle on y: the text-box rule), in a type style and ink
  void text(int x, int y, const char *s, const char *type, const char *ink) const;
  // false when the box x1..x2, y1..y2 (centre-relative) is outside what this redraw repaints: every primitive above
  // skips itself then, so a partial redraw (redraw / redraw_arc) costs only what crosses it (the static scale is not
  // drawn again while a knob moves)
  bool sees(int x1, int y1, int x2, int y2) const;
};
lv_obj_t *drawn(lv_obj_t *parent, void (*draw)(const Pen &), const char *layout = "col");
// a drawn strip (the Slider): a box styled by `look` (a structure style that gives its size, "slider"), the Pen on
// its content box centre; Pen::w / h = the content size (Pen::half = the largest centred square's half, as drawn())
lv_obj_t *strip(lv_obj_t *parent, void (*draw)(const Pen &), const char *look);
void redraw(lv_obj_t *drawn_box, int x, int y, int w, int h);
// mark only the span of a ring between deg0 and deg1 (either order) at radius r, widened by `pad` px each side (the
// knob's and the stroke's reach): what a moving arc end + knob needs, never the whole box
void redraw_arc(lv_obj_t *drawn_box, int r, float deg0, float deg1, int pad);
void at(float deg, int r, int *x, int *y);  // the point at deg (0 = 3 o'clock, clockwise) and radius r, centre-relative
int half(lv_obj_t *drawn_box);              // the Pen's half for this box now (the hit geometry matches the drawing)
void pointer(lv_obj_t *o, float *deg, int *dist, int *half);

// ---- the shell only (core/spec.h, core/device.cpp, core/fw_core.cpp): never in a component ----
// The look of a display: its shared styles painted from theme t (+ the type fonts of the text-size setting). The ONE
// place a theme / accent / text-size change lands: repaints that display's styles, one report_style_change.
void look(lv_display_t *d, const fw::Theme *t);
// Screen > Page > Header slot + Body. head = inset top/sides, lay::top tall (the Header
// component sits at its content origin); body = the rest, inset padding INSIDE it. body: "list" (scrolling column,
// rows touch), "cards" (scrolling wrap, gutter both ways), "center" (one node centred, no scroll), "page" (scrolling
// column, children centred: a foundation page).
// `page` (an object the caller sized) BECOMES the Page: its children are then head + body, in that order.
struct Page { lv_obj_t *page, *head, *body; };
Page page(lv_obj_t *page, const char *body);
bool migrated(const fw::Component *c);    // meta "contract": 2
void inset(int px);                       // proofs only: the page inset as if its token were px (-1 = the token)
// proofs + the workbench Layout lab: size token `name` as if it were px (px < 0 = the token again; name "" = all
// back; "squeeze" = the Body's max width): every look's size / structure styles again, live, no rebuild
void tune(const char *name, int px);
std::string uses(lv_display_t *d);       // JSON ["name"]: the size tokens display d's objects read (styles + pen), lab
std::string tunables();                   // JSON [[name, token px, now px, "look" | "fixed", lo, hi]]
const char *style_name(const lv_style_t *s);  // the token a shared style is (inspector), nullptr = local / foreign
}  // namespace fx
