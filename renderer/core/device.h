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
//   settings   the panel's personalisation values (tokens/settings.json, fw::set::), ONE set() path for the Settings
//              screen, Home Assistant (the ESPHome config entities: firmware/settings_yaml.mjs + fw_device.h) and the
//              workbench, so the three never disagree. set() checks the value (a select takes its option id or label,
//              a number is clamped + snapped to its step, a switch takes true/false/on/off), keeps it, applies it to
//              every mounted device, tells the observers (the firmware's entities + NVS) and emits "setting" <id>=<value>
//              (from the screen's control, else from each device root). An unchanged value does nothing (no echo loops).
//              Applied now: theme + accent restyle the device live (the current screen is built again in place with
//              the new theme after its running animations end, scroll kept, no rise), transition (instant = no rise),
//              any change from outside the screen moves the Settings screen's controls (its update path). Theme auto =
//              light while the host says it is day (set_daylight: the panel's sun.sun), else the stage's theme (the
//              workbench theme, the panel's /fw/show theme). Accent: the stage's set until one is set (workbench).
//              The rest (sounds, sound pack, volume, idle, units, toasts) is kept and reported for their consumers;
//              the workbench host switches its sound pack on the "setting" event.
// TODO (with the panel host): idle -> clock, as createDevice; Text size (a type-scale rebuild).
#pragma once
#include "fw.h"

namespace fw {
namespace dev {
lv_obj_t *mount(lv_obj_t *parent, const char *start = "home");  // the device root (its screen is its one child)
void go(lv_obj_t *in, const char *id);  // in = the root or anything inside it; queued for the next tick
void back(lv_obj_t *in);
void home(lv_obj_t *in);
const char *current(lv_obj_t *in);  // the id on top of the stack ("" = not a device)

// settings (above)
enum From : uint8_t { SCREEN, HA, HOST, BOOT };  // who set it: the Settings screen, Home Assistant, a host / API, restore
const char *setting(const char *id);  // the value now (option id, "true" / "false", the number), nullptr = no such id
// "" = applied, "same" = already that value, else the error; obj = the control that changed it (emits from there)
const char *set(const char *id, const char *value, From from, lv_obj_t *obj = nullptr);
const char *set(const char *id, double value, From from);  // numbers (HA number entities)
const char *option_label(const char *id, const char *value);  // a select's label for an option id ("Dark"), else value
using Observer = void (*)(const char *id, const char *value, From from);
void observe(Observer cb);  // every applied change (firmware: publish to the entity, which keeps it in NVS)
// backup: {"facet":"settings","version":1,"values":{id: value, ...}} (no secrets: there are none in settings)
void settings_json(std::string &out);
std::string restore(const char *json, From from);  // a backup's values through set(); "" = ok, else the first error
void set_daylight(int day);                          // the sun for theme auto: 1 day, 0 night, -1 unknown
const Theme *look(const Theme *stage);               // the theme the device shows over a stage theme
void on_look(void (*cb)());  // the look may have changed (theme / accent set, the sun): hosts redraw what they show
}  // namespace dev
}  // namespace fw
