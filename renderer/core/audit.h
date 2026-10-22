// Component audit marks (tools/audit.mjs, README "Audit"; Darren 2026-10-22: "tests against them to make sure they
// align with the pixel grids and there's nothing off about them ... don't clip out of their box ... animations follow
// the right path"). The audit checks every component on its own (clipping, grid, symmetry, touch, motion); a
// component only says what the audit cannot know:
//   fw::qa::free(obj, "why")     obj is placed off the 4 px grid on purpose (a knob placed by angle, a value)
//   fw::qa::clips(obj, "why")    obj clips its children on purpose (a bar's square foot, a scroll list)
//   fw::qa::circle(obj, ref, cx, cy, r)  while it moves, obj's centre stays on this circle (ref's local px)
//   fw::qa::inside(obj, ref)     while it moves, obj stays inside ref's box (default ref: the component root)
//   fw::qa::radial(obj, axes, depth)  obj's marks at 12 / 3 / 6 / 9 o'clock must sit on its centre lines
// Workbench (WASM) only: on the panels (ESP-IDF) these are empty inlines, nothing is stored.
#pragma once
#include "lvgl.h"

namespace fw::qa {
#ifdef ESP_PLATFORM
inline void free(lv_obj_t *, const char *) {}
inline void clips(lv_obj_t *, const char *) {}
inline void circle(lv_obj_t *, lv_obj_t *, float, float, float) {}
inline void inside(lv_obj_t *, lv_obj_t * = nullptr) {}
inline void radial(lv_obj_t *, uint8_t, int) {}
#else
void free(lv_obj_t *o, const char *why);
void clips(lv_obj_t *o, const char *why);
void circle(lv_obj_t *o, lv_obj_t *ref, float cx, float cy, float r);
void inside(lv_obj_t *o, lv_obj_t *ref = nullptr);
// a ring of marks centred in o: axes = the marks it has at 12 / 3 / 6 / 9 o'clock (bits 1 / 2 / 4 / 8), each `depth`
// px deep from o's edge; the audit checks their ink sits on the centre lines (fw::tick_ring marks itself)
void radial(lv_obj_t *o, uint8_t axes, int depth);
#endif
}  // namespace fw::qa
