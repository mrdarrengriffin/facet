// Card previews (docs @facet/ui previews.ts + radar.ts previewLightning(), Components > Graphics > Card previews): the
// artwork at the top right of a Home card, in the ic::card box (72 / 48), plain shapes only. Built from the atoms and
// the components' own parts: the mascots (fw::mascot), value arcs (fw::value_arc), bars (fw::ch::bars), strokes
// (fw::stroke_at), the lightning data (core/lightning.h). The docs' 72 px geometry is scaled to the box and put on the
// 4 px grid (centred shapes stay centred; marks placed by angle or data say fw::qa::free).
//   agents     four mascots: working, done, needs you, working
//   media      a small record: face, groove, label spot, hole, the blue marker
//   dial       a small dial: value arc (track line, accent) from 135 over 270 degrees, blue hub      value 0..100
//   switch     a pill with its knob: on = accent fill, on-accent knob right; off = line-hi outline,
//              steel knob left; unavailable = line outline, no knob                                  on, unavailable
//   clock      two rings with dots on them (accent on the outer one at 12, blue + steel on the inner)
//   timer      a full ring that empties as the timer runs, blue hub                                  value 0..100 (% left)
//   github     two commits and a branch merging into an accent dot
//   energy     the last half-hours as 8 px bars by price band (6 at 720, 4 at 480)              bands
//   lightning  rings (100 / 25 km), home, the last 15 minutes of strikes as small dots              now, quiet
// Update path (card_preview_set): value sweeps (Motion value), the switch slides (Motion toggle), bars move, strikes
// redraw; a different kind = rebuild (returns false). Silent.
#pragma once
#include "fw.h"

namespace fw {
constexpr const char *PREVIEW_KINDS[] = {"agents", "media", "dial", "switch", "clock", "timer", "github", "energy", "lightning"};
constexpr int N_PREVIEW_KINDS = 9;
bool is_preview(const char *kind);
// the preview `kind` in an ic::card box, as the last child of parent; props: value, on, unavailable, values, bands, now, quiet
lv_obj_t *card_preview(lv_obj_t *parent, const char *kind, Props p);
bool card_preview_set(lv_obj_t *preview, const char *kind, Props p);
}  // namespace fw
