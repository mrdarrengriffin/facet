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

}  // namespace fw
