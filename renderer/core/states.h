// In-place state changes (Darren 2026-10-22, update path phase 2): a component's update moves to a new state
// (default <-> disabled, a choice on / off) through ONE shared helper and animates in place, never a rebuild.
// Included at the end of fw.h, after states_gen.h (the fw::st:: tokens it extends).
//
// How: components declare every state look as LVGL state styles (selector st::OFF, st::ON, ...) on each part, as the
// state tokens say. fw::st::set_state() adds / removes the state on the object AND every descendant (no trickle
// flags needed, so a row's press never reaches its children), and every colour or opacity that changes because of it
// fades from what was on screen to the new look with Motion pattern `fade` (150 ms, reduced motion: instant). The
// fade is an LVGL style transition (lv_obj_style_create_transition, the press-fade machinery: the object's own
// local styles are never touched, a fade still running is reversed from where it is), so the state styles own the
// look again when it ends. LVGL's own transitions (press fades) run as before.
#pragma once
#include "fw.h"

namespace fw {
namespace st {
// add (on) / remove `state` on o and its whole subtree; animate = fade the looks that change (false = build time)
void set_state(lv_obj_t *o, lv_state_t state, bool on, bool animate = true);
// default <-> disabled (and the colour side of unavailable): st::OFF
inline void set_off(lv_obj_t *o, bool off, bool animate = true) { set_state(o, OFF, off, animate); }
// a part swapped in place (an unavailable word for a control): out hidden at once, in fades in (Motion fade)
void swap(lv_obj_t *out, lv_obj_t *in, bool animate = true);
// after an update made a component's contents again for a new state (fw::refill: a card going unavailable, a row's
// trailing swapped): its children fade in from transparent (Motion fade), the root's own box stays (no flash)
void fade_in_children(lv_obj_t *root, bool animate = true);
// The common update of a pressable component whose looks are built from its props (card, tile, rows, ...):
// fill(root, p) styles the root and makes its children. built() at build time remembers the props; update():
// only "state" default <-> pressed changed = the press look moves in place (LVGL press fade); anything else =
// fw::refill + fill (snap), and when the state goes to or from disabled / unavailable (st::OFF) the new contents fade
// in (fade_in_children). Silent (fw::update's Quiet).
using Fill = void (*)(lv_obj_t *root, ArduinoJson::JsonObjectConst p);
void built(lv_obj_t *root, ArduinoJson::JsonObjectConst p);
bool update(lv_obj_t *root, ArduinoJson::JsonObjectConst p, Fill fill);
}  // namespace st
}  // namespace fw
