// toasts.h: the toasts' behaviour (renderer/CONTRACT.md C4: toasts live on the Overlay layer, never in the Body). The
// core owns what moves and what a finger does, so the Toast and Toast layer components (components/toast.cpp,
// toast_layer.cpp) only build their looks from token names through fx. Moved out of those components when they went
// on the contract (2026-10-28); the motion, the gestures and the perf notes are as they were there.
//   toast   fw::toasts::own(o) once at build (o = the Toast's root): finger down pauses the timer; a sideways drag
//           follows the finger (translate_x only: an opacity under 100 % would render the whole toast through a layer
//           on every move, the slow part on the Guition); released past toast.swipe = dismissed by swipe, moved less
//           than toast.tap-slop = a tap (dismiss), anything between springs back (Motion toast-in) and the timer
//           resumes. The panel's key (core/device.h BUTTON_ALERT: the newest toast shown) dismisses ("button").
//           Dismiss = event "dismiss" (detail tap | swipe-left | swipe-right | timeout | action | pushed | button)
//           and, for a tap, swipe or the key, sound "back"; then it leaves (Motion toast-out: fade + slide back to
//           its edge, or sideways by toast.swipe-out after a swipe; reduced = fade). In a layer it is deleted when
//           gone; on its own page it stays hidden until an update brings it back (Motion toast-in, silent).
//   arm     fw::toasts::arm(o, bar, ...) after the build's fill (again(...) after an update's, left(...) before it): the timer bar (bar = a box the component made, its look
//           from tokens) placed along the bottom inside the edge, the tone colour, shrinking left over the life
//           (layout.json toast life 6 s, life-urgent 12 s), linear; timer off = a full bar, no timeout. An update
//           keeps the life left when the timer and life stay.
//   action  fw::toasts::action(o, button): a tap on the toast's Button (the real one, fw::find) dismisses it
//   layer   fw::toasts::layer(host) = a Toast layer overlay on host (full size, floating, NOT clickable: only the
//           toasts take taps); up to toast.max (2) toasts, newest nearest the edge they enter from, lay::gutter apart;
//           top = the first at lay::toast_top, bottom = the first ends at the bottom inset; x = inset, content width.
//   show    fw::toasts::show(layer, props, detail): builds a Toast (fw::find, internal props layer + edge), which
//           fades in and slides from its edge (Motion toast-in; reduced = fade); the others move one slot along (y,
//           Motion toast-in); over the max the oldest is pushed out (reason "pushed"). detail = the arrival's event
//           "toast" detail + sound toast.sound (notify); nullptr = quiet (no event, no sound, and the push is silent)
//   push    fw::toasts::push(layer, toast, top): the device runtime's toast = {tone, title, body, icon?, action?,
//           life? normal|urgent}; icon defaults by tone (ok done, warn / error alert, working agent, info / idle
//           bell); top = where a new stack enters (Settings > Toasts), kept while toasts are showing
// Perf (Guition): small invalidation only. A sitting toast redraws its timer bar strip (stroke::edge high); an arriving
// / leaving toast redraws its own box while it fades; a restack moves whole toasts by y. Nothing full-screen.
#pragma once
#include "fw.h"

namespace fw {
namespace toasts {
void own(lv_obj_t *toast);
void arm(lv_obj_t *toast, lv_obj_t *bar, bool timer, bool urgent);  // after the build's fill: the bar full
int32_t left(lv_obj_t *toast, bool timer, bool urgent);  // before an update's refill: the bar paused, the width to keep (-1 = full)
void again(lv_obj_t *toast, lv_obj_t *bar, bool timer, bool urgent, int32_t keep_w);  // after it: a dismissed one comes back
void action(lv_obj_t *toast, lv_obj_t *button);

lv_obj_t *layer(lv_obj_t *host, bool top = toast::top, bool still = false);
lv_obj_t *show(lv_obj_t *layer, Props toast, const char *detail);
void clear(lv_obj_t *layer);     // every toast showing leaves quietly
void set_top(lv_obj_t *layer, bool top);  // taken only while no toast is showing (a stack never flips)
void push(lv_obj_t *layer, Props toast, bool top);
// o lies under a shown toast (and is not part of one): the contrast proofs skip it (covered on purpose, like the scrim's)
bool under(lv_obj_t *o);
}  // namespace toasts
}  // namespace fw
