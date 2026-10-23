// The device's toast layer (Darren 2026-10-23, the Toasts screen for WASM parity): the Toast layer component's stack
// (components/toast_layer.cpp) as an API for the device runtime, so toasts arrive over whatever screen shows.
//   layer   fw::toasts::layer(host) = a Toast layer overlay on host (full size, not clickable, only toasts take taps)
//   push    fw::toasts::push(layer, toast, top): toast = {tone, title, body, icon?, action?, life? normal|urgent};
//           icon defaults by tone (ok done, warn / error alert, working agent, info / idle bell); top = where a new
//           stack enters (Settings > Toasts), kept while toasts are showing
#pragma once
#include "fw.h"

namespace fw {
namespace toasts {
lv_obj_t *layer(lv_obj_t *host);
void push(lv_obj_t *layer, Props toast, bool top);
}  // namespace toasts
}  // namespace fw
