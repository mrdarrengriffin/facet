// sheet.h: the Bottom sheet's overlay layer (renderer/CONTRACT.md C4: sheets live on the Overlay layer, never in the
// Body). The core owns the layer's geometry and motion, so the Bottom sheet component (components/bottom_sheet.cpp)
// only fills the panel with real components through fx.
//   overlay  fills the box it is made on (floating, ignores its layout), hidden while closed; marked for the audit
//            (fw::qa::free "overlay: bottom sheet") and LV_OBJ_FLAG_USER_4 (the component's update finds it)
//   scrim    the whole overlay, bg:scrim at Motion opacity scrim / scrim-light (dark / light theme); a tap on it or
//            the panel's key (core/device.h BUTTON_SHEET) closes the sheet: event "close" detail "scrim" / "button" +
//            sound back
//   panel    bg:bg, a border:line-hi hair edge with radius:ring corners, one hair past the left / right edges and
//            radius:ring past the bottom (only its top corners and top edge show); padding inset (top less the hair);
//            a column with the gutter between its children. Its height follows its content (fit), capped at
//            sheet-top from the top; bottom-anchored, so a short list opens lower
// Motion (motion.json): open = sheet-open (the scrim fades in, the panel fades in and rises by the offset), close =
// sheet-close; reduced motion = fade only; no fades (fw::mo::fades, Guition) = the scrim snaps, the opaque panel
// slides by its own height. Both run from wherever they are (a close during the opening turns it round).
// Bench options (the panel flash session, README "Bottom sheet frame cost"): flatten (the screen under the sheet as one
// opaque image while it animates), solid (the panel opaque, rising only).
#pragma once
#include "lvgl.h"

namespace fw {
namespace sheet {
// the overlay on host, closed; returns the overlay (its children: scrim, panel). The overlay's user data is the caller's.
lv_obj_t *make(lv_obj_t *host, bool flatten = false, bool solid = false);
lv_obj_t *panel(lv_obj_t *ov);
// the panel's height for its head + n rows of row_h px (0 = layout sheet-row-h), capped at sheet-top
void fit(lv_obj_t *ov, int rows, int row_h = 0);
void open(lv_obj_t *ov, bool still = false);  // still = shown open at once (no motion)
void close(lv_obj_t *ov);
bool is_open(lv_obj_t *ov);
int open_close_ms();  // sheet-in + sheet-out (the frame-rate bench's loop period)
// the open sheet's overlay on screen scr (searched in its tree), or nullptr: the pixel contrast proof measures only it
// (what is under the scrim is dimmed on purpose, C4)
lv_obj_t *open_on(lv_obj_t *scr);
}  // namespace sheet
}  // namespace fw
