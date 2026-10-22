// Device runtime (Darren 2026-10-22): the panel as one live thing, the same C++ on the panels and in the WASM
// workbench: a screen stack, Facet's screen transition and the taps that move between screens. Old Facet:
// @facet/ui device.ts createDevice() (go(id), back, Home's settings button, the Content rise on every cut).
//   mount      dev::mount(parent, "home") makes a full-screen root (lay::screen_w x screen_h) and shows that screen.
//              The workbench's "Device" page (screens/device.cpp, /wasm/screens/device) and the panel firmware
//              (POST /fw/show {"id":"device"}, firmware/fw_device.h) both just render that component.
//   navigate   any fw::emit from inside the device (fw::listen on the root): "navigate" <id> (the screen chrome's
//              settings button, Home's cards) = go(id); "back" (the Header's Back button) = back. The tap's own sound
//              already played (states / sound hooks); the runtime adds none. The cut runs on the next LVGL tick (a
//              one-shot timer), never inside the tapped object's own event (it is deleted with its screen).
//   stack      go(id) pushes id (an id already on the stack pops back to it: no loops); back pops (at the root: no-op);
//              home() clears to the root. Ids = registered components: a "screens" component is built as the whole
//              screen (props {}); any other (the placeholder Home lists the foundation pages) is shown as a page: the
//              Header (back + its name, the screen chrome) and the component scrolling under it from lay::top.
//   transition Motion pattern `screen` (the Facet Content rise, fw::rise): the header first, then the content blocks in
//              rows by their top, from below going forward, from above going back / Home; reduced motion = instant.
//              Hidden and floating children (a closed Bottom sheet) are left alone.
// TODO (with the panel host): the Settings `transition` value (rise / instant) and idle -> clock, as createDevice.
#pragma once
#include "fw.h"

namespace fw {
namespace dev {
lv_obj_t *mount(lv_obj_t *parent, const char *start = "home");  // the device root (its screen is its one child)
void go(lv_obj_t *in, const char *id);  // in = the root or anything inside it; queued for the next tick
void back(lv_obj_t *in);
void home(lv_obj_t *in);
const char *current(lv_obj_t *in);  // the id on top of the stack ("" = not a device)
}  // namespace dev
}  // namespace fw
