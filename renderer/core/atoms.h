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

// Smooth stroke (experiment, board facet-145; WIP, foundations/strokes.cpp compares it): an anti-aliased stroke of
// `width` px through float points, rasterised once into an A8 coverage mask (1 byte / px, w x h) and shown as an
// lv_image recoloured with `colour` (a token, like the icons). Analytic: every pixel near a segment gets coverage =
// clamp(width/2 + 0.5 - distance to the segment), segments combine by max, so joins are seamless and the ends are
// round caps. Points are in the box's pixel space (pixel centres at integers); keep width/2 + 1 px clear of the edge.
// LVGL's own lv_line snaps every point to whole pixels (lv_draw_sw_line converts to int even with LV_USE_FLOAT)
// and draws each segment on its own, hence the kinks and lumps this replaces.
struct StrokePt { float x, y; };
lv_obj_t *stroke_path(lv_obj_t *parent, int w, int h, const StrokePt *pts, int n, int width, lv_color_t colour);
// the same for a curve given as a function of t = 0..1 (sampled at <= 0.75 px steps along its length)
using StrokeFn = StrokePt (*)(float t, void *user);
lv_obj_t *stroke_fn(lv_obj_t *parent, int w, int h, StrokeFn f, void *user, int width, lv_color_t colour);
int stroke_bytes(lv_obj_t *stroke);  // the mask's bytes (memory cost of one stroke)

}  // namespace fw
