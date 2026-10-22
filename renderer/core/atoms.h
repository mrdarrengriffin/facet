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
// The atom owns the returned image's user data (its Stroke state): never lv_obj_set_user_data on it; keep a caller's
// state on another object (2026-10-22: the Motion page clock hand did, stroke_redraw read garbage, panels crashed).
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

// ---- Dials and rings (facet-129, 2026-10-22): the shared geometry of every round control and face. The Dial, the
// Thermostat dial, the Tick ring, the Arc, the Media disc and the clock faces all draw with these, so a scale, a value
// arc and "where on the ring is the finger" are one rule. Angles are LVGL degrees (0 = 3 o'clock, clockwise); a ring
// spans `range` degrees from `start` (the Facet dials: 135 + 270, open at the bottom; a full face: 270 + 360 = from 12).
inline float ring_deg(float start, float range, double frac) { return start + (float) frac * range; }
inline StrokePt polar(float cx, float cy, float r, float deg) {
  const float a = deg * 3.14159265f / 180.0f;
  return {cx + std::cos(a) * r, cy + std::sin(a) * r};
}
// "nice" scale intervals for a value span (the Dial rule, Darren 2026-10-21): the label interval is the smallest
// 1 / 2 / 5 x 10^k that keeps the numbers to at most max_labels; minor ticks split it in 5 (a 2-interval in 4).
// 0-100 = every 10, minor 2 (the docs dial ring).
void nice_intervals(double span, double *major, double *minor, int max_labels = 11);
int decimals_of(double v);  // 0, 1 or 2 decimals that show v exactly (0.5, 0.25 steps)

// Tick ring: ticks (and optional numbers or labels) drawn inward from the edge of a size x size box in ONE draw event
// (no object per tick). Ticks sit on multiples of `minor` in [lo, hi], majors on multiples of `major` (0 = none);
// a count-based ring (Astro tickRing count / stride) = lo 0, hi count - 1, minor 1, major stride. lit: ticks at or below
// that value take lit_c (a progress ring, the seconds gone); NAN = none. dots: a round dot of `width` at the edge
// instead of a line (the Media disc's dotted ring). Numbers: majors carry their value (num_dec decimals) or
// labels[i] for the i-th major (clock faces), num_gap inside the tick, centred on the cap height (the text-box rule).
struct TickRing {
  double lo = 0, hi = 100, minor = 2, major = 10;
  float start = 135.0f, range = 270.0f;
  int minor_len = 0, major_len = 0, width = 0;
  lv_color_t minor_c, major_c;
  bool dots = false;
  double lit = NAN;
  lv_color_t lit_c;
  bool numbers = false;
  int num_gap = 0, num_dec = 0;
  ty::Id num_style = ty::c_meta;
  lv_color_t num_c;
  const char *const *labels = nullptr;  // one per major (from lo), nullptr = the value
  int n_labels = 0;
};
lv_obj_t *tick_ring(lv_obj_t *parent, int size, const TickRing &spec);  // owns its user data (the spec)
// the ring's spec, to change in place (colours for a state, lit for progress): call tick_ring_changed() after
TickRing *tick_ring_spec(lv_obj_t *ring);
void tick_ring_changed(lv_obj_t *ring);

// Value arc: an lv_arc (anti-aliased by LVGL, invalidates only the swept part + the knob; Darren 2026-10-22: no stroke
// atom for dial arcs) on a d x d box, non-clickable (the owner takes the touch), range 0..steps. track_w 0 = no track;
// knob 0 = none, else a round knob of that diameter in knob_c ringed by knob_ring px of knob_ring_c (the canvas: it
// separates the knob from the arc under it). Value = lv_arc_set_value (indicator from start) or, for a free span (the
// Thermostat's heat / cool span), lv_arc_set_angles on the indicator.
struct ArcLook {
  float start = 135.0f, range = 270.0f;
  int track_w = 0;
  lv_color_t track_c;
  int ind_w = 0;
  lv_color_t ind_c;
  bool round = true;
  int knob = 0, knob_ring = 0;
  lv_color_t knob_c, knob_ring_c;
  int32_t steps = 100;
};
lv_obj_t *value_arc(lv_obj_t *parent, int d, const ArcLook &look);
// where the finger is on a ring centred in o: frac 0..1 along [start, start + range]; in the gap (range < 360) the
// nearer end (or, while dragging from `held` 0..1, the end it is nearest: a drag never jumps across the gap). dist =
// px from the centre (dials keep their centre free).
float ring_frac(lv_obj_t *o, lv_point_t pt, float start, float range, float *dist = nullptr, float held = -1.0f);

}  // namespace fw
// Audit marks (core/audit.h): what the component audit cannot know by itself
#include "audit.h"
