// Atoms (docs @facet/ui atoms.ts focusRing() / cornerTicks(), CSS .f-ring / .f-ticks): the two accent marks every
// component shares for "on / active" (states.json on.mark + focus). Included at the end of fw.h, after states_gen.h.
// Each has its own workbench page (components/focus_ring.cpp, components/corner_ticks.cpp).
#pragma once
#include "fw.h"  // self-contained: ESPHome includes every component header on its own (alphabetical), fw.h includes this one at its end

namespace fw {

// Focus ring: a stroke::focus (1 px) accent ring `offset` px outside o's border box (LVGL outline: its radius follows
// o's radius + the offset, docs radius + offset). Shown for selector `show` (0 = always), hidden while disabled /
// unavailable (st::OFF), even when active. Drawn with fw::bleed so no parent clips it. Call once o is in its parent.
// Facet: Button primary active and the playing play Icon button (st::FOCUS_OFFSET = touch focus-offset 8 / 4).
void focus_ring(lv_obj_t *o, lv_style_selector_t show = st::ON, int offset = st::FOCUS_OFFSET);

// Corner ticks: L-shaped accent marks (st::on_mark; st::on_mark_pressed = accent-text while the owner is pressed) on o's border-box corners, `len` long and `thick` wide (two border
// sides of a len x len box each), shown for selector `show` (0 = always). all = the four corners (Button secondary
// active), tr_bl = top-right + bottom-left (docs: cards and tiles; no approved WASM component uses that yet). The ticks
// are children of o: with a `show` state, o needs LV_OBJ_FLAG_STATE_TRICKLE (Button has it) so they follow. Placed
// past o's padding + border, so call after o's padding and border are set. Inside the box: no bleed needed.
enum class Corners { all, tr_bl };
void corner_ticks(lv_obj_t *o, Corners c = Corners::all, lv_style_selector_t show = st::ON,
                  int len = lay::corner_tick, int thick = stroke::corner_tick);

// Smooth stroke = how Facet draws every curve, arc, circle and diagonal (Darren 2026-10-22, board facet-145: "atom for
// everything"; never lv_line / lv_arc / lv_draw_line for them): an anti-aliased stroke of `width` px through float
// points, rasterised once into an A8 coverage mask (1 byte / px, w x h) and shown as an lv_image recoloured with
// `colour` (a token, like the icons). Analytic: every pixel near a segment gets coverage = clamp(width/2 + 0.5 -
// distance to the segment), segments combine by max, so joins are seamless (always round). Ends: Cap::round (the
// default; lv_line / lv_arc "rounded") or Cap::butt (cut square at the end point, AA'd; lv_line / lv_draw_line /
// lv_arc default). Points are in the box's pixel space (pixel centres at integers); keep width/2 + 1 px clear of the
// edge. LVGL's own lv_line snaps every point to whole pixels (lv_draw_sw_line converts to int even with LV_USE_FLOAT)
// and draws each segment on its own, hence the kinks and lumps this replaces. Animate by moving / fading the image;
// a shape that changes (a value arc, a clock hand, a growing z) re-rasters into its own mask (stroke_redraw /
// stroke_set_arc), only for small or occasional changes (the lab page foundations/strokes.cpp measures).
enum class Cap : uint8_t { round, butt };
struct StrokePt { float x, y; };
lv_obj_t *stroke_path(lv_obj_t *parent, int w, int h, const StrokePt *pts, int n, int width, lv_color_t colour,
                      Cap cap = Cap::round);
// the same for a curve given as a function of t = 0..1 (sampled at <= 0.75 px steps along its length)
using StrokeFn = StrokePt (*)(float t, void *user);
lv_obj_t *stroke_fn(lv_obj_t *parent, int w, int h, StrokeFn f, void *user, int width, lv_color_t colour,
                    Cap cap = Cap::round);
std::vector<StrokePt> stroke_sample(StrokeFn f, void *user);  // the points stroke_fn draws
// points given in the PARENT's content px (where an lv_line's points would be): the mask is sized to the path's
// bounds + the stroke and placed there (so overshoot past a plot is still drawn)
lv_obj_t *stroke_at(lv_obj_t *parent, const StrokePt *pts, int n, int width, lv_color_t colour, Cap cap = Cap::round);
// re-raster a stroke's mask with new points (its box, width and cap kept); dirty = the area to redraw in the mask's
// px (nullptr = the whole image)
void stroke_redraw(lv_obj_t *stroke, const StrokePt *pts, int n, const lv_area_t *dirty = nullptr);
// circle arc, lv_arc geometry: drawn inward from a d x d box at (x, y) of the parent (the stroke's outer edge on the
// box edge), from angle a0 to a1 in degrees, LVGL angles (0 = 3 o'clock, clockwise; a1 < a0 wraps past 360, a0 == a1
// = nothing, like lv_arc). The image is the box + 1 px each side (the AA fringe).
lv_obj_t *stroke_arc(lv_obj_t *parent, int x, int y, int d, float a0, float a1, int width, lv_color_t colour,
                     Cap cap = Cap::round);
// move an arc's ends (a value arc): re-rasters; when a0 is unchanged only the swept part is invalidated
void stroke_set_arc(lv_obj_t *stroke, float a0, float a1);
int stroke_bytes(lv_obj_t *stroke);  // the mask's bytes (memory cost of one stroke)
// a bare mask for a draw event (no object; e.g. many small marks drawn in one event): separate segments (2 points
// each, `cap` on both ends); free with lv_draw_buf_destroy. stroke_draw = the mask at (x, y) of the layer, recoloured.
lv_draw_buf_t *stroke_segments(int w, int h, const StrokePt *ends, int n_segments, int width, Cap cap = Cap::round);
void stroke_draw(lv_layer_t *layer, lv_draw_buf_t *mask, int x, int y, lv_color_t colour);

}  // namespace fw
