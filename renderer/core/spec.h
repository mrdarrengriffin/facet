// Screens from specs (one-renderer plan §2.1-2.5, v1 2026-10-23, WIP): a screen is a JSON spec built from the
// registered components (fw::find), the same code on the panels and in the WASM workbench. Header-only (core/*.h is
// bundled as is). Specs never carry positions, sizes, colours, fonts or sounds: the layout and the components decide.
//
// Screen spec  {"v": 1, "id": "office", "name": "Office", "rev": 3,
//               "params": {"entity": "light.x", ...},     the screen's parameters + defaults; "$entity" anywhere
//                                                          in a string = the value it was opened with. Typed
//                                                          (2026-10-23): {"type": text|select|entity|entities|
//                                                          toggle|hidden, "default", "label", "domain", "options", "about"}:
//                                                          Settings > Screens and the workbench build their controls
//                                                          from it (entity / entities = one / a comma list of
//                                                          `domain` ids, filled by the host's discovery)
//               "header": {"label": "Office", "settings"?: false},   settings = the chrome's settings button (a
//                                                          Home). Back is the renderer's (facet-188): every screen
//                                                          but the device's root has it; "back" is accepted, ignored
//               "layout": "list" | "center" | "cards",    list = one scrolling column (rows touch), center = one node
//                                                          centred in the content box, cards = the card grid
//               "nodes": [node...]}
// Node         {"c": "<component id>", "p": {prop: value | bind}, "on": {event: action}, "pending": {prop: [on, off]},
//               "in"?: k, "slot"?: n, "each"?: "<param>", "sample"?: [{prop: value}...], "show"?: bind}
//   show       (2026-10-24) a bind: the node shows only while its value is set (hidden for "", 0, off, false), e.g. the
//              vacuum's room tiles only while it isn't cleaning: {"e": "$entity", "map": {"cleaning": ""}, "else": "1"}.
//              Re-read with the node's binds (a hidden node takes no room in its column).
//   in header  (2026-10-24) "in": "header" builds the node into the screen's header row, before the screen chrome
//              (docs header({right: [...]}): the Vacuum's Rooms | Map Segmented)
//              slot "title" holds one part (2026-10-28): header.label is the shorthand (the Header builds a Title from
//              it; a bound label follows the entity), {"c": "title", "p": {"text": ...}, "in": "header", "slot":
//              "title"} the explicit form (or a Segmented); a node there replaces the label's Title
//   in overlay (2026-10-29) "in": "overlay" builds a bottom_sheet over the whole page (CONTRACT.md C4: the Overlay
//              layer, never the Body); its list is a slot ("in": k, "slot": 0) the spec fills (the Calendar's sheets),
//              its open prop a bind ({"e": "panel.calendar", "a": "sheet", "map": {"day": "open"}, "else": "closed"})
//   in / slot  (2026-10-23, pages from components only) build this node into slot n (default 0) of node k (an earlier
//              node whose component is a LAYOUT: its meta says "slots": N and it marks its slot objects with
//              fw::spec::SLOT, in order) instead of the screen's content box: Split = two columns, Group = a wrapping
//              row. k counts the nodes as written; a layout node can't repeat (each).
//   span       (2026-10-25, Grid) [cols, rows]: a node in a static Grid's cell takes that many cells (the grid places
//              its cells in reading order into the first free place that fits; spans stay inside its cols)
//   cell       (2026-10-25, Grid items mode) a Grid with "items" repeats this one node {c, p} per item ("$item" = the
//              item, "$item.<key>" = a field of an object item); not a layout (no grids in grids)
//   each       repeat the node once per item of the comma list parameter (at most EACH_MAX), "$item" in its props /
//              actions = the item. With the list empty, once per "sample" entry (its props over "p": the factory
//              spec's demo, so a screen shows something before a host fills the list). Expanded when the screen opens
//              (a later screen.param doesn't add or drop copies).
//   spacing    in the list layout and in a layout's column slots, a section_header that isn't first gets lay::group
//              above it (Layout rule "more space between groups"); in the list layout so does any node after a non-row
//              node (hero_block, gauge, readout ...); rows touch (they have hairlines).
//   bind       {"e": "light.x" | "$param", "a"?: "<attribute>", "fmt"?: on|pct|avail|light|name|num|frac,
//               "s"?: sample, "map"?: {value: text}, "else"?: text, "tpl"?: "battery {}%"} (core/ha.h): s = the value
//              while the entity is empty (the factory demo), map = the value as words / a tone ("else" for the rest,
//              default the value itself), tpl = the text around it ({} = the value; empty value = empty text).
//              e "" with s = a bind on a screen parameter, not an entity: {"e": "", "s": "$view", "map": {...}}.
//              fmt (2026-10-24, the panel lab's Vacuum + Door lock lines): count = the number of items in a comma
//              list ("Kitchen:3,Corridor:1" -> 2; a map value's "{}" = the one item's name when there is one, an
//              else's "{}" = the number: "Clean {}" / "Clean {} rooms"); time = an ISO time as "09:03" today,
//              "Mon 22:05" before; day = "today 07:59", "yesterday 18:42", "Mon 18:42" 
//   on         event = what the component emits (or "long": a long press, made here) -> one action:
//   action     {"do": "light.switch", "e"}  (a toggle's on / off)    {"do": "light.toggle", "e"}
//              {"do": "light.brightness", "e"}  (a Dial / Slider value: live while dragging = throttle with a
//                                                trailing send, light-throttle-ms; the release always sends)
//              {"do": "climate.set_temperature", "e"}  (a thermostat set point: live while dragging = throttle with a
//                                                       trailing send, clamped + snapped to the entity's min / max /
//                                                       step; settles when HA reports it)
//              {"do": "climate.set_hvac_mode", "e"} / {"do": "climate.set_preset_mode", "e"}  (the event's value)
//              {"do": "media.control", "e"}  (a Media view's "media" event: previous | play_pause | next |
//                                              volume 0-100 -> media_player.media_previous_track / media_play_pause /
//                                              media_next_track / volume_set)
//              {"do": "cover.control", "e"}  (position N | tilt N | open | stop | close -> (on panel.covers: the covers feed,
//                                              core/covers_source.cpp, says which; the window, the slats, the buttons)
//                                              cover.set_cover_position / set_cover_tilt_position / open_cover /
//                                              stop_cover / close_cover)
//              {"do": "lock.control", "e"}  (a Lock view's "lock": lock | unlock | open -> lock.lock / lock.unlock /
//                                             lock.open; the view emits unlock / open only after its hold-to-confirm)
//              {"do": "vacuum.control", "e", "rooms"?: "$rooms", "mode"?, "mop"?}  (a Button tap by its label: Clean / Start (with
//                rooms = a comma list of segment ids: vacuum.send_command app_segment_clean, the Roborock form; else
//                vacuum.start) | Resume | Pause | Dock -> vacuum.start / pause / return_to_base; a Segmented "change"
//                = the suction: vacuum.set_fan_speed with its option id; a Vacuum mode "change" = "fan <id>" (the
//                suction), "mode <id>" / "mop <id>" = select.select_option on the action's "mode" / "mop" select entity;
//                rooms items may be "Name:id": the id after the last colon is the segment)
//              {"do": "screen.toggle", "param": "rooms"}  (the event's detail goes in or out of the comma list
//                parameter `param`, at most EACH_MAX items; every node is bound again: room picks)
//              {"do": "screen.param", "param": "entity", "from"?: "speakers"}  (the event's detail becomes the
//                screen's parameter `param` and every node is bound again in place, through its update path: a
//                picked speaker re-binds the Media screen; with "from" the value must be one of that parameter's
//                comma list, the same domain as before; with "auto": "<param>" naming an entity (a sensor whose
//                state is an entity id: HA's media target), the parameter follows that state while in Auto, a
//                pick of "auto" starts following again; the screen's "$mode" = auto | manual | "" (no Auto)).
//                With that sensor unset, Auto follows what's playing (2026-10-26): the first entity of the "from"
//                list whose state is playing (none playing = it stays where it is); Auto is offered whenever the
//                sensor is set or that list has entities)
//                "to": "week" sets that value instead of the event's (a More row opening a view of the same screen);
//                "back": true = the screen's Back (the Header's, or a nav.back) first returns the parameter to the
//                value before (Energy > last 7 days > Back = Energy, the docs subView), then leaves as usual
//              {"do": "nav.open", "screen": "<id>", "params"?: {k: v}}    {"do": "nav.back"}
//              timers (2026-10-28, the panel's own: core/device.h "the panel's timers"; no entity):
//              {"do": "timer.start", "seconds": 300, "name"?: "Pasta"}  (no name = its length, "5 min"; a 5th timer is
//                refused: a warn toast says why)
//              {"do": "timer.extend", "s": 60}  {"do": "timer.cancel"}  {"do": "timer.stop"}  (stop = a done one)
//              {"do": "timer.select", "name"?: "Pasta" | "id"?: "2"}  (the one the ring shows)
//              {"do": "timer.new"} / {"do": "timer.back"}  (+ New: panel.timer reads "picking" until a start / back)
//                Which timer: the action's "id" / "name", else the event's detail for a Timer chip's "select" / "stop"
//                (detail = its name; also from chips in an items Grid), else the shown one. Button "tap" takes any of
//                them; timer_chip select -> timer.select, stop -> timer.stop.
//              on "open" (any component's own open event): {"do": "nav.open"} = the screen its detail names
//              The closed whitelist: each component + event allows only the actions in RULES (the catalogue).
//   pending    while a light.switch waits for HA: the prop shows on / off text (a List row's sub "Turning on")
// local       (2026-10-28) an entity the panel fills itself, bound like any other (core/ha.h "local sources"; never
//  sources    sent to or asked of Home Assistant). "panel.timer" = the panel's timers (core/device.h): its state =
//              the shown timer's running | done | idle (none); attributes: name, left ("4:32" / "1:02:05"), left_s,
//              total (s), share (time left 0..1000: a Progress ring's value), line ("of 10 min · ends 14:52",
//              "of 10 min · done"), id, count (timers), running, free (TIMERS_MAX - count: "show" the presets while
//              there is room), others (a JSON list of the OTHER timers, soonest first, for a Grid's items:
//              [{"id", "name", "left" s, "total" s, "state" running | done, "time" "4:32" | "done"}]).
//              e.g. {"e": "panel.timer", "a": "left"}; show while one runs: {"e": "panel.timer", "map": {"running":
//              "1"}, "else": ""}. Moving values (left, share, others) are put once a second only while a bind reads it.
// feeds       (2026-10-29, CONTRACT.md C51) "feeds": {"panel.bins": {"source": "calendar", "calendar": "$calendar"}}:
//              which adapter fills each feed this screen reads, and its settings (texts; "$param" = the screen's
//              parameter). Applied when the screen is built (fw::ha::set_feed_config: the adapter refetches on a change).
// card        (2026-10-29, Darren: every screen comes with its Home card) "card": {"icon": "bins", "preview": "bins",
//              "text": {"e": "panel.bins", "a": "when"}, ...}: the Card a Home page card for this screen shows (the
//              Card's props; binds on the screen's feeds). Home applies the page's "feeds" too, so the card shows the
//              same data as the screen. No card = the icon + the page's name only.
// Spec role   "role": "page" | "detail" | "both" (default page; screen model 2026-10-23): a page is a place in the deck
//              (Home links to it); a detail screen is opened from something on a page, for one entity (Light,
//              Blinds), never in the deck; both = either (Media, Thermostat). Built-in screens: "home" is a page
//              kind, the rest (Settings) are system screens, never in the deck.
// Deck v2      {"v": 2, "rev": 7, "start": "living", "pages": [
//                {"id": "living", "kind": "home", "name": "Living room", "cards": [
//                  {"t": "light", "e": "light.x", "size": "s", "tap": "toggle", "label": "Ceiling"},
//                  {"t": "page", "page": "media", "size": "m"}]},
//                {"id": "media", "kind": "media", "name"?, "source"?, "params"?: {k: text}}]}
//              This panel's pages in order (notes: screen-model.md). id = unique, a-z 0-9 _, at most 12; kind = a
//              page-role spec or "home"; name = header label + card title (else the spec's name); source = an id
//              of the kind's sources (renderer/sources/sources.json); params = the page opens with them over the
//              spec's defaults (a host's discovery fills them). cards only on a home: t = a card type (CARD_TYPES),
//              e = its entity (a domain the type takes), size s | m | l, tap toggle | detail | page | none (the
//              type's first by default), page = a page id (t page), label (else the entity's name). A home may set
//              header_actions {button: "settings" (default) | a page id (that page's shortcut in the settings
//              button's place; Settings = a long press on the time), status: "none" | "timer" (the panel's timer,
//              time left beside the time)}. start = the
//              page shown first. A v1 deck ({"v": 1, "screens": [...]}) is refused: a panel holding one falls back
//              to the factory deck. No "on": a page you don't want is not in the deck (Settings > Screens and the
//              editor list the other page kinds as off; turning one on adds it at the end).
//
// Validation (one implementation for the panels, the WASM workbench and the hub's dry render): unknown keys (style
// keys such as colour / x / width / font get `style_key`), unknown components, props a component doesn't have,
// values outside the prop's options / range, binds, actions outside the whitelist, budgets (nodes, binds, bytes).
// Errors: [{path, code, msg}].
//
// Store: factory specs (generated from renderer/specs/*.json into specs_gen.h, compiled in: a panel works with no
// hub), overrides (sent screens and the deck) each kept with a CRC and its last good copy through a Storage the host
// gives (panel: NVS; WASM: memory with fault injection for the power-loss test). Apply = validate fully, build it
// once off screen, then store and swap; a half-sent or failing spec never shows; the same content (canonical JSON CRC,
// whatever its rev) is "unchanged"; different content applies (rev is the sender's label).
// Boot: an override whose record fails its CRC or validation falls back to its last good copy, then to factory.
#pragma once
#include <algorithm>
#include "fx.h"
#include <cmath>
#include <cstdint>
#include <cstring>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "fw.h"
#include "fx.h"
#include "ha.h"
#include "device.h"
#include "specs_gen.h"

namespace fw {
namespace spec {
// set by the device runtime while it builds the root of its stack (core/device.cpp show): the root screen has no Back
// (nowhere to go back to; the panel's root is Home, which has none anyway; the docs demo a screen as a device's root)
inline bool at_root = false;
constexpr int PROTOCOL = 1;
constexpr int DECK_V = 2;  // the deck's own version (v1 decks are refused)
struct Budget {
  int nodes = 48, binds = 64, bytes = 8192, screens = 16, deck_bytes = 8192, pages = 16, homes = 4, cards = 24;  // start values (screen model): the bench sets them
};
inline const Budget &budget() {
  static Budget b;
  return b;
}
constexpr uint32_t LIGHT_THROTTLE_MS = 200;  // light-throttle-ms: live brightness at most this often while dragging (Astro light.ts)
constexpr uint32_t LIGHT_HOLD_MS = 3000;     // a light's own change holds HA's echoes off this long (Astro light.ts: no jump back)
constexpr uint32_t PENDING_MS = 5000;        // a switch / brightness waits this long for HA before showing HA's state

// ---- the action whitelist per component and event (the catalogue's "actions")
struct Rule { const char *comp, *event, *actions; };
constexpr Rule RULES[] = {
    {"list_row", "toggle", "light.switch"},          {"list_row", "tap", "nav.open,nav.back,light.toggle,flights.select"},
    {"list_row", "long", "nav.open"},                {"card", "tap", "nav.open,light.toggle"},
    {"card", "long", "nav.open"},                    {"dial", "change", "light.brightness"},
    {"entity_tile", "tap", "nav.open,light.toggle"},
    {"icon_button", "tap", "nav.open,nav.back,calendar.control,media.control,cover.control"},  {"icon_button", "toggle", "media.control"},  // 2026-10-29: a header's settings / shortcut icon (the end slot takes icon buttons only); the Calendar's calendars button
    {"dial", "input", "light.brightness"},           {"dial", "toggle", "light.switch"},
    {"slider", "change", "light.brightness,cover.control"}, {"slider", "input", "light.brightness"},
    {"slider_row", "change", "media.control,cover.control"}, {"slider_row", "input", "media.control,cover.control"},
    {"toggle", "toggle", "light.switch"},
    {"more_row", "tap", "nav.open,nav.back,screen.param"},  // screen.param + to: a view of the same screen (Energy > last 7 days)
    {"thermostat_dial", "change", "climate.set_temperature"}, {"thermostat_dial", "input", "climate.set_temperature"},
    {"thermostat_view", "target", "climate.set_temperature"}, {"thermostat_view", "mode", "climate.set_hvac_mode"},
    {"thermostat_view", "preset", "climate.set_preset_mode"},
    {"cover_visual", "change", "cover.control"},  // the Covers window: a drag released (its position)
    {"lock_ring", "lock", "lock.control"},           {"button", "confirm", "lock.control"},
    {"room_tile", "pick", "screen.toggle"},          {"camera_tile", "tap", "nav.back"},          {"tile", "tap", "nav.open"},          {"button", "tap", "nav.open,nav.back,light.toggle,vacuum.control,timer.start,timer.extend,timer.cancel,timer.stop,timer.select,timer.new,timer.back,cover.control"},
    {"timer_chip", "select", "timer.select"},       {"timer_chip", "stop", "timer.stop"},
    {"grid", "select", "timer.select"},             {"grid", "stop", "timer.stop"},  // Timer chips in an items Grid
    {"segmented", "change", "vacuum.control,screen.param"}, {"iso_floorplan", "pick", "screen.toggle"},
    {"vacuum_mode", "change", "vacuum.control"},     {"grid", "pick", "screen.param,nav.open,calendar.control,media.control"},
    {"sky_radar", "pick", "flights.select"},         {"flight_hero", "tap", "flights.select"},  // the Flight radar: a plane / the hero
    {"calendar_month", "pick", "calendar.control"},  {"calendar_month", "month", "calendar.control"},
    {"bottom_sheet", "close", "calendar.control,media.control,cover.control"}, {"picker_row", "tap", "media.control,cover.control"},        // a link to a sub-view spec (moon -> moon_details -> ...)
    {"*", "open", "nav.open"},                       // any component's own "open" (a tapped calendar day, a PR):
                                                     // its detail = the screen id [+ "?" + params JSON]
};
constexpr const char *ACTIONS[] = {"light.switch", "light.toggle", "light.brightness", "climate.set_temperature",
                                   "climate.set_hvac_mode", "climate.set_preset_mode", "media.control", "cover.control", "lock.control", "vacuum.control", "screen.toggle", "screen.param",
                                   "nav.open", "nav.back", "timer.start", "timer.extend", "timer.cancel", "timer.stop", "timer.select", "timer.new", "timer.back",
                                   "calendar.control", "flights.select"};
// The one closed action list, documented (CONTRACT.md C50): every action a spec node, an agent (POST /fw/act) or Home
// Assistant (facet_act) can run, with its fields and one example. GET /fw/act, the catalogue's "act" and the agent docs
// are generated from it; act() checks "value" against `value` (outside a spec the value a component's event would give
// is a field). fields = "name|type|need|limits" joined by ';'. value = alternatives joined by '|': a word, "word #a..b"
// (the word, a space, a number a..b), "#a..b" (a number), "*n" (a short name: letters, digits, space, _ or -, at most
// n); "" = no value. screen = needs a spec screen (its parameters): a spec node only.
struct ActionDoc { const char *id, *about, *fields, *value, *example; bool screen; };
constexpr ActionDoc ACTION_DOCS[] = {
    {"light.switch", "Turn a light on or off.", "e|entity|required|light.x", "on|off", R"({"do":"light.switch","e":"light.office","value":"on"})", false},
    {"light.toggle", "Toggle a light.", "e|entity|required|light.x", "", R"({"do":"light.toggle","e":"light.office"})", false},
    {"light.brightness", "Set a light's brightness in percent (0 = off).", "e|entity|required|light.x", "#0..100", R"({"do":"light.brightness","e":"light.office","value":"40"})", false},
    {"climate.set_temperature", "Set a thermostat's target (kept inside the entity's range and step).", "e|entity|required|climate.x", "#0..50", R"({"do":"climate.set_temperature","e":"climate.hall","value":"20.5"})", false},
    {"climate.set_hvac_mode", "Set a thermostat's mode.", "e|entity|required|climate.x", "off|heat|cool|heat_cool|auto|dry|fan_only", R"({"do":"climate.set_hvac_mode","e":"climate.hall","value":"heat"})", false},
    {"climate.set_preset_mode", "Set a thermostat's preset.", "e|entity|required|climate.x", "*32", R"({"do":"climate.set_preset_mode","e":"climate.hall","value":"eco"})", false},
    {"media.control", "Control a media player (volume in percent).", "e|entity|required|media_player.x", "previous|play_pause|next|volume #0..100", R"({"do":"media.control","e":"media_player.kitchen","value":"volume 30"})", false},
    {"cover.control", "Open, close or stop a cover, or set its position / tilt in percent.", "e|entity|required|cover.x", "open|close|stop|position #0..100|tilt #0..100", R"({"do":"cover.control","e":"cover.office_blind","value":"position 50"})", false},
    {"lock.control", "Lock, unlock or open a lock.", "e|entity|required|lock.x", "lock|unlock|open", R"({"do":"lock.control","e":"lock.front_door","value":"lock"})", false},
    {"vacuum.control", "Start (optionally some rooms), pause, resume or dock a vacuum.", "e|entity|required|vacuum.x;rooms|text|optional|segment ids, a comma list (with clean)", "clean|start|resume|pause|dock", R"({"do":"vacuum.control","e":"vacuum.robot","value":"clean","rooms":"1,3"})", false},
    {"screen.toggle", "A spec screen's tapped item in / out of a comma list parameter (spec nodes only).", "param|id|required|the screen parameter", "", R"({"do":"screen.toggle","param":"rooms"})", true},
    {"screen.param", "Set a spec screen's parameter to the picked value (spec nodes only).", "param|id|required|the screen parameter;from|id|optional|the parameter holding the allowed values;auto|id|optional|the parameter Auto follows;to|id|optional|the value to set instead of the event's (a view);back|bool|optional|true = the screen's Back returns to the value before", "", R"({"do":"screen.param","param":"speaker","from":"speakers"})", true},
    {"nav.open", "Open a screen.", "screen|id|required|a screen id (a-z, 0-9, _, at most 12);params|object|optional|the screen's parameters", "", R"({"do":"nav.open","screen":"timer"})", false},
    {"nav.back", "Go back one screen.", "", "", R"({"do":"nav.back"})", false},
    {"timer.start", "Start a timer (at most 4 at once).", "seconds|int|required|1..86400;name|text|optional|at most 24", "", R"({"do":"timer.start","seconds":300,"name":"Pasta"})", false},
    {"timer.extend", "Add time to a timer (none named = the one shown).", "s|int|required|1..86400;id|text|optional|a timer id;name|text|optional|a timer name, at most 24", "", R"({"do":"timer.extend","name":"Pasta","s":60})", false},
    {"timer.cancel", "Cancel a timer, running or done (none named = the one shown).", "id|text|optional|a timer id;name|text|optional|a timer name, at most 24", "", R"({"do":"timer.cancel","name":"Pasta"})", false},
    {"timer.stop", "Clear a timer that is done (none named = the one shown).", "id|text|optional|a timer id;name|text|optional|a timer name, at most 24", "", R"({"do":"timer.stop","name":"Pasta"})", false},
    {"timer.select", "Show that timer on the Timer screen.", "id|text|optional|a timer id;name|text|optional|a timer name, at most 24", "", R"({"do":"timer.select","name":"Pasta"})", false},
    {"timer.new", "Show the Timer screen's length picker.", "", "", R"({"do":"timer.new"})", false},
    {"timer.back", "Leave the length picker.", "", "", R"({"do":"timer.back"})", false},
    {"calendar.control", "On the Calendar: show the previous, next or this month, open a day or the calendars sheet, close the sheet, or turn a calendar on or off (its id: the entity without calendar.).", "", "previous|next|today|calendars|close|*48", R"({"do":"calendar.control","value":"day 2026-10-23"})", false},
    {"flights.select", "On the Flight radar: select an aircraft (by hex, callsign or route) so the hero and the card show it; the selected one again opens its details (spec nodes only).", "", "*48", R"({"do":"flights.select"})", true},
};
static_assert(sizeof ACTION_DOCS / sizeof ACTION_DOCS[0] == sizeof ACTIONS / sizeof ACTIONS[0], "every action has its ACTION_DOCS row (CONTRACT.md C50)");
inline const ActionDoc *action_doc(const std::string &d) {
  for (const ActionDoc &a : ACTION_DOCS)
    if (d == a.id) return &a;
  return nullptr;
}
constexpr const char *FMTS[] = {"", "on", "pct", "avail", "light", "name", "num", "frac", "count", "time", "day", "level", "state", "dim"};
constexpr uint32_t CLIMATE_THROTTLE_MS = 250;  // like light-throttle-ms: a set point while dragging at most this often
constexpr const char *LAYOUTS[] = {"list", "center", "cards"};
constexpr int EACH_MAX = 12;                         // copies one "each" node makes at most
constexpr lv_obj_flag_t SLOT = LV_OBJ_FLAG_USER_2;  // a layout component's slot object (in / slot), in order
// slot n of a layout node: itself if marked, then its descendants marked SLOT (pre-order, not inside another slot:
// a layout nested in a slot keeps its own slots), nullptr = none
inline lv_obj_t *slot(lv_obj_t *o, int &n) {
  for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) {
    lv_obj_t *c = lv_obj_get_child(o, (int32_t) i);
    if (lv_obj_has_flag(c, SLOT)) {  // a slot: its contents (nested layouts' slots) belong to them, not to o
      if (n-- == 0) return c;
      continue;
    }
    if (lv_obj_t *d = slot(c, n)) return d;
  }
  return nullptr;
}
inline lv_obj_t *slot_of(lv_obj_t *o, int n) {  // the layout node itself may be its only slot (Group)
  if (lv_obj_has_flag(o, SLOT) && n-- == 0) return o;
  return slot(o, n);
}
// Slots with allowed components (facet-205, CONTRACT.md C8a): a component's meta "slots" declares every place a spec
// may put other components. Either a count (legacy layouts: N numbered slots, any component, any number) or a list of
// declarations, one per slot: {"name", "about", "accepts": [component ids] | "any", "max": n (0 = no limit),
// "count": n (n numbered slots of this kind: "in": k, "slot": i), "template": true (the Grid's "cell" template)}. A
// declaration with no count and no template is a NAMED slot (the Header's title / end: "in": "header", "slot": name).
// check_screen enforces accepts and max with a path error ("bad_slot"); the workbench / editor read the same meta.
struct SlotDecl { std::string name; std::vector<std::string> accepts; int count = 0, max = 0; bool any = true, tmpl = false; };
inline const std::vector<SlotDecl> &slot_decls(const char *comp) {
  static std::map<std::string, std::vector<SlotDecl>> cache;  // metas are constant: parsed once per component
  auto it = cache.find(comp);
  if (it != cache.end()) return it->second;
  std::vector<SlotDecl> &v = cache[comp];
  const Component *c = find(comp);
  ArduinoJson::JsonDocument d;
  if (!c || !c->meta || deserializeJson(d, c->meta)) return v;
  if (d["slots"].is<int>()) { SlotDecl s; s.count = d["slots"] | 0; v.push_back(s); return v; }
  for (ArduinoJson::JsonObjectConst o : d["slots"].as<ArduinoJson::JsonArrayConst>()) {
    SlotDecl s;
    s.name = o["name"] | ""; s.count = o["count"] | 0; s.max = o["max"] | 0; s.tmpl = o["template"] | false;
    if (o["accepts"].is<ArduinoJson::JsonArrayConst>()) {
      s.any = false;
      for (ArduinoJson::JsonVariantConst a : o["accepts"].as<ArduinoJson::JsonArrayConst>()) s.accepts.push_back(a | "");
    }
    v.push_back(s);
  }
  return v;
}
// a component's numbered slots ("in": k, "slot": i), 0 = not a layout
inline int slots_of(const char *comp) {
  int n = 0;
  for (const SlotDecl &s : slot_decls(comp)) n += s.tmpl ? 0 : s.count;
  return n;
}
inline const SlotDecl *slot_named(const char *comp, const std::string &name) {  // a named slot (count 0) or the template
  for (const SlotDecl &s : slot_decls(comp)) if (!s.count && s.name == name) return &s;
  return nullptr;
}
inline const SlotDecl *slot_at(const char *comp, int i) {  // numbered slot i
  for (const SlotDecl &s : slot_decls(comp)) if (!s.tmpl && s.count) { if (i < s.count) return &s; i -= s.count; }
  return nullptr;
}
inline bool slot_takes(const SlotDecl &s, const std::string &c) { return s.any || std::find(s.accepts.begin(), s.accepts.end(), c) != s.accepts.end(); }
inline std::string slot_list(const SlotDecl &s) {  // "icon_button, status_chip or segmented"
  std::string o;
  for (size_t i = 0; i < s.accepts.size(); i++) o += (i ? (i + 1 == s.accepts.size() ? " or " : ", ") : "") + s.accepts[i];
  return o;
}
constexpr const char *PARAM_TYPES[] = {"text", "select", "entity", "entities", "toggle", "hidden"};
constexpr const char *ROLES[] = {"page", "detail", "both"};
// Home card type catalogue (deck v2, screen model piece 2): per type the entity domains it takes, its sizes, its taps
// (the default first), the detail screen "detail" opens, and the component that draws it at each size (s | m | l; ""
// = not offered). Sizes: s = a tile (tile-w x tile-h, three across), m = a card (card-w x card-h, two across),
// l = a wide card (content-w x card-h). A sensor is read-only, in either size (Darren 2026-10-25): s = an Entity tile
// with no press (act none: value + label), m = a State readout (no box, no tap), two in a row sharing one card slot,
// stacked (docs readoutSlot). Taps: toggle = the
// entity's toggle, detail = its detail screen for that entity, page = a deck page, none = reports only.
struct CardType { const char *t, *domains, *sizes, *taps, *detail, *comp_s, *comp_m, *comp_l; };
constexpr CardType CARD_TYPES[] = {
    {"light", "light,switch", "s,m", "toggle,detail", "light", "entity_tile", "card", ""},
    {"cover", "cover", "s,m", "detail", "covers", "entity_tile", "card", ""},
    {"climate", "climate", "m", "detail", "thermostat", "", "card", ""},
    {"media", "media_player", "m,l", "detail,page", "media", "", "card", "card"},
    {"sensor", "sensor,binary_sensor", "s,m", "none", "", "entity_tile", "readout", ""},
    {"page", "", "s,m,l", "page", "", "entity_tile", "card", "card"}};
// a parameter's default: its text, or a typed parameter's "default"
inline std::string param_default(ArduinoJson::JsonVariantConst v) {
  if (v.is<const char *>()) return v.as<std::string>();
  return v["default"] | "";
}
constexpr const char *STYLE_KEYS[] = {"color", "colour", "bg", "x", "y", "w", "h", "width", "height", "size", "font",
                                      "radius", "pad", "padding", "margin", "align", "sound", "opacity"};
inline bool in_list(const char *csv, const std::string &w) {
  for (const char *p = csv; *p;) {
    const char *e = strchr(p, ',');
    const size_t n = e ? (size_t) (e - p) : strlen(p);
    if (w.size() == n && !strncmp(p, w.c_str(), n)) return true;
    if (!e) break;
    p = e + 1;
  }
  return false;
}
inline const char *allowed(const char *comp, const char *event) {
  for (const Rule &r : RULES)
    if ((!strcmp(r.comp, comp) || !strcmp(r.comp, "*")) && !strcmp(r.event, event)) return r.actions;
  return nullptr;
}
template <size_t N> bool one_of(const char *const (&set)[N], const std::string &v) {
  for (const char *s : set)
    if (v == s) return true;
  return false;
}

// ---- component props from the registry meta (parsed once per component)
struct PropInfo { std::string name, type; std::vector<std::string> options; double min = 0, max = 0; };
inline const std::vector<PropInfo> *props_of(const char *comp) {
  static std::map<std::string, std::vector<PropInfo>> cache;
  auto it = cache.find(comp);
  if (it != cache.end()) return &it->second;
  const Component *c = find(comp);
  if (!c) return nullptr;
  ArduinoJson::JsonDocument m;
  deserializeJson(m, c->meta ? c->meta : "{}");
  std::vector<PropInfo> v;
  for (ArduinoJson::JsonObjectConst p : m["props"].as<ArduinoJson::JsonArrayConst>()) {
    PropInfo pi;
    pi.name = p["name"] | "";
    pi.type = p["type"] | "text";
    for (ArduinoJson::JsonVariantConst o : p["options"].as<ArduinoJson::JsonArrayConst>()) pi.options.push_back(o.as<std::string>());
    pi.min = p["min"] | -1e9;
    pi.max = p["max"] | 1e9;
    v.push_back(pi);
  }
  return &cache.emplace(comp, std::move(v)).first->second;
}

// ---- validation
struct Err { std::string path, code, msg; };
using Errs = std::vector<Err>;
inline void err(Errs &e, const std::string &path, const char *code, const std::string &msg) {
  if (e.size() < 24) e.push_back({path, code, msg});
}
inline bool entity_ok(const std::string &s) {
  if (!s.empty() && s[0] == '$') return s.size() > 1;
  const size_t dot = s.find('.');
  if (dot == std::string::npos || dot == 0 || dot + 1 >= s.size() || s.size() > 64) return false;
  for (char c : s)
    if (!(islower((unsigned char) c) || isdigit((unsigned char) c) || c == '_' || c == '.')) return false;
  return true;
}
inline bool word_ok(const std::string &s, size_t max) {  // a-z, 0-9, _; 1..max
  if (s.empty() || s.size() > max) return false;
  for (char c : s)
    if (!(islower((unsigned char) c) || isdigit((unsigned char) c) || c == '_')) return false;
  return true;
}
inline bool id_ok(const std::string &s) {
  if (s.empty() || s.size() > 12) return false;
  for (char c : s)
    if (!(islower((unsigned char) c) || isdigit((unsigned char) c) || c == '_')) return false;
  return true;
}
inline void check_keys(ArduinoJson::JsonObjectConst o, const char *const *keys, int n, const std::string &path, Errs &e) {
  for (ArduinoJson::JsonPairConst kv : o) {
    const std::string k = kv.key().c_str();
    bool ok = false;
    for (int i = 0; i < n; i++)
      if (k == keys[i]) ok = true;
    if (ok) continue;
    if (one_of(STYLE_KEYS, k)) err(e, path + "." + k, "style_key", "specs carry no style: the components and the layout decide " + k);
    else err(e, path + "." + k, "unknown_key", "unknown key " + k);
  }
}
inline void check_action(ArduinoJson::JsonVariantConst a, const char *comp, const char *event, const std::string &path, Errs &e) {
  if (!a.is<ArduinoJson::JsonObjectConst>()) { err(e, path, "bad_action", "an action is an object {do, ...}"); return; }
  static const char *const K[] = {"do", "e", "screen", "params", "param", "from", "auto", "rooms", "mode", "mop", "seconds", "name", "s", "id", "value", "to", "back"};
  check_keys(a.as<ArduinoJson::JsonObjectConst>(), K, 17, path, e);
  const std::string d = a["do"] | "";
  if (!one_of(ACTIONS, d)) { err(e, path + ".do", "bad_action", "unknown action " + d); return; }
  if (comp) {  // a spec node: its component's event allows it; nullptr = act() (an agent, HA: no component)
    const char *ok = allowed(comp, event);
    if (!ok || !in_list(ok, d)) err(e, path + ".do", "not_allowed", std::string(comp) + " " + event + " allows " + (ok ? ok : "no action"));
    if (!a["value"].isNull()) err(e, path + ".value", "bad_action", "value: in a spec the component's event gives it (value is for /fw/act and facet_act)");
  }
  if (d.rfind("light.", 0) == 0) {
    const std::string en = a["e"] | "";
    if (!entity_ok(en) || (en[0] != '$' && en.rfind("light.", 0) != 0)) err(e, path + ".e", "bad_entity", "a light entity (light.x or $param)");
  }
  if (d.rfind("climate.", 0) == 0) {
    const std::string en = a["e"] | "";
    if (!entity_ok(en) || (en[0] != '$' && en.rfind("climate.", 0) != 0)) err(e, path + ".e", "bad_entity", "a climate entity (climate.x or $param)");
  }
  if (d == "media.control") {
    const std::string en = a["e"] | "";
    if (!entity_ok(en) || (en[0] != '$' && en != "panel.media" && en.rfind("media_player.", 0) != 0)) err(e, path + ".e", "bad_entity", "a media player (media_player.x, panel.media or $param)");
  }
  if (d == "cover.control") {
    const std::string en = a["e"] | "";
    if (!entity_ok(en) || (en[0] != '$' && en != "panel.covers" && en.rfind("cover.", 0) != 0)) err(e, path + ".e", "bad_entity", "a cover (cover.x, panel.covers or $param)");
  }
  if (d == "vacuum.control") {
    const std::string en = a["e"] | "";
    if (!entity_ok(en) || (en[0] != '$' && en.rfind("vacuum.", 0) != 0)) err(e, path + ".e", "bad_entity", "a vacuum (vacuum.x or $param)");
    if (!a["rooms"].isNull() && !a["rooms"].is<const char *>()) err(e, path + ".rooms", "bad_action", "rooms: a comma list of segment ids or $param");
    for (const char *k : {"mode", "mop"})
      if (!a[k].isNull()) {
        const std::string sel = a[k] | "";
        if (!entity_ok(sel) || (sel[0] != '$' && sel.rfind("select.", 0) != 0)) err(e, path + "." + k, "bad_entity", std::string(k) + ": a select entity (select.x or $param)");
      }
  }
  if (d == "timer.start" && !(a["seconds"].is<int>() && (int) a["seconds"] >= 1 && (int) a["seconds"] <= 86400))
    err(e, path + ".seconds", "bad_action", "seconds: the timer's length, 1 to 86400");
  if (d == "timer.extend" && !(a["s"].is<int>() && (int) a["s"] >= 1 && (int) a["s"] <= 86400))
    err(e, path + ".s", "bad_action", "s: seconds to add, 1 to 86400");
  for (const char *k : {"name", "id"}) {
    if (a[k].isNull()) continue;
    const bool here = d.rfind("timer.", 0) == 0 && !(!strcmp(k, "id") && d == "timer.start");  // a new timer has no id yet
    if (!here || !a[k].is<const char *>() || strlen(a[k] | "") > 24)
      err(e, path + "." + k, "bad_action", std::string(k) + ": a timer's " + k + " (text, at most 24), on a timer action");
  }
  if (d.rfind("timer.", 0) != 0 && (!a["seconds"].isNull() || !a["s"].isNull())) err(e, path, "bad_action", "seconds / s go on timer actions");
  if (d == "screen.toggle" && !id_ok(a["param"] | "")) err(e, path + ".param", "bad_action", "param: the screen parameter to toggle the detail in (a-z, 0-9, _)");
  if (d == "lock.control") {
    const std::string en = a["e"] | "";
    if (!entity_ok(en) || (en[0] != '$' && en.rfind("lock.", 0) != 0)) err(e, path + ".e", "bad_entity", "a lock (lock.x or $param)");
  }
  if (d == "screen.param") {
    if (!id_ok(a["param"] | "")) err(e, path + ".param", "bad_action", "param: the screen parameter to set (a-z, 0-9, _)");
    if (!a["from"].isNull() && !id_ok(a["from"] | "")) err(e, path + ".from", "bad_action", "from: the parameter holding the allowed values");
    if (!a["auto"].isNull() && !id_ok(a["auto"] | "")) err(e, path + ".auto", "bad_action", "auto: the parameter naming the entity Auto follows");
    if (!a["to"].isNull() && !id_ok(a["to"] | "")) err(e, path + ".to", "bad_action", "to: the value to set (a word: a-z, 0-9, _), instead of the event's");
    if (!a["back"].isNull() && !a["back"].is<bool>()) err(e, path + ".back", "bad_action", "back: true = Back returns to the value before");
  } else if (!a["to"].isNull() || !a["back"].isNull()) err(e, path, "bad_action", "to / back go on screen.param");
  if (d == "nav.open") {
    const std::string s = a["screen"] | "";
    const bool from_event = !strcmp(event, "open") && a["screen"].isNull();  // the component's "open" names the screen
    if (!from_event && !id_ok(s)) err(e, path + ".screen", "bad_screen", "a screen id (a-z, 0-9, _; at most 12)");
    if (!a["params"].isNull() && !a["params"].is<ArduinoJson::JsonObjectConst>()) err(e, path + ".params", "bad_action", "params is an object");
  }
}
inline void check_value(const PropInfo &pi, ArduinoJson::JsonVariantConst v, const std::string &path, Errs &e, int &binds) {
  if (v.is<ArduinoJson::JsonObjectConst>()) {
    static const char *const K[] = {"e", "a", "fmt", "s", "map", "else", "tpl"};
    check_keys(v.as<ArduinoJson::JsonObjectConst>(), K, 7, path, e);
    if (!v["map"].isNull()) {
      if (!v["map"].is<ArduinoJson::JsonObjectConst>() || v["map"].size() > 12) err(e, path + ".map", "bad_bind", "map is {value: text}, at most 12");
      else for (ArduinoJson::JsonPairConst kv : v["map"].as<ArduinoJson::JsonObjectConst>())
        if (!kv.value().is<const char *>() || strlen(kv.value().as<const char *>()) > 64 || strlen(kv.key().c_str()) > 32) err(e, path + ".map", "bad_bind", "map values are text (64), keys at most 32");
    }
    for (const char *k : {"s", "else", "tpl"}) {
      const size_t cap = (!strcmp(k, "s") && pi.name == "items") ? 256 : 64;  // an items sample is a whole list (2026-10-26)
      if (!v[k].isNull() && (!v[k].is<const char *>() || strlen(v[k].as<const char *>()) > cap)) err(e, path + "." + k, "bad_bind", std::string(k) + " is text (at most " + std::to_string(cap) + ")");
    }
    if (!entity_ok(v["e"] | "") && !(!strcmp(v["e"] | "x", "") && v["s"].is<const char *>()))
      err(e, path + ".e", "bad_bind", "a bind needs an entity (domain.object or $param), or e \"\" with s (a screen parameter)");
    if (!one_of(FMTS, v["fmt"] | "")) err(e, path + ".fmt", "bad_bind", "fmt is one of on, pct, avail, dim, light, name, num, frac, count, time, day");
    binds++;
    return;
  }
  if (v.is<const char *>() && v.as<const char *>()[0] == '$') return;  // a parameter (checked when opened)
  if (pi.type == "bool") { if (!v.is<bool>()) err(e, path, "bad_prop", pi.name + " is true / false"); return; }
  if (pi.type == "range") {
    if (!v.is<double>()) { err(e, path, "bad_prop", pi.name + " is a number"); return; }
    const double x = v.as<double>();
    if (x < pi.min || x > pi.max) err(e, path, "bad_prop", pi.name + " outside its range");
    return;
  }
  if (!v.is<const char *>()) { err(e, path, "bad_prop", pi.name + " is text"); return; }
  if (pi.type == "select" && std::find(pi.options.begin(), pi.options.end(), v.as<std::string>()) == pi.options.end())
    err(e, path, "bad_prop", pi.name + ": " + v.as<std::string>() + " is not one of its options");
  else if (strlen(v.as<const char *>()) > (pi.name == "items" ? 256u : pi.name == "src" ? 512u : 64u))  // a static items list: 256, as its bind sample
    err(e, path, "bad_prop", pi.name + ": text over " + (pi.name == "items" ? "256" : pi.name == "src" ? "512" : "64") + " characters");
}
// a screen spec: errors into e (empty = valid)
// Grid + Icon rules a prop check can't see (board facet-171): span only in a static grid, inside its cols; cell only
// with items, one non-layout node; cols xor min; an Icon's step xor fit: cell
inline void check_grid(ArduinoJson::JsonVariantConst n, ArduinoJson::JsonArrayConst nodes, int i, const std::string &c, const std::string &p, Errs &e, int &binds) {
  if (!n["span"].isNull()) {
    const int k = n["in"].is<int>() ? (n["in"] | -1) : -1;
    const bool g = k >= 0 && k < i - 1 && !strcmp(nodes[k]["c"] | "", "grid") && nodes[k]["p"]["items"].isNull();
    const int cols = g && nodes[k]["p"]["min"].isNull() ? (nodes[k]["p"]["cols"] | 3) : 8;
    ArduinoJson::JsonArrayConst sa = n["span"].as<ArduinoJson::JsonArrayConst>();
    if (!g) err(e, p + ".span", "bad_span", "span goes on a node in a static grid (in: a grid with no items)");
    else if (sa.size() != 2 || !sa[0].is<int>() || !sa[1].is<int>() || sa[0].as<int>() < 1 || sa[0].as<int>() > cols || sa[1].as<int>() < 1 || sa[1].as<int>() > 8)
      err(e, p + ".span", "bad_span", "span is [columns 1.." + std::to_string(cols) + ", rows 1..8]");
  }
  if (c == "grid") {
    if (!n["p"]["cols"].isNull() && !n["p"]["min"].isNull()) err(e, p + ".p.min", "bad_prop", "cols or min, not both");
    if (!n["cell"].isNull() && n["p"]["items"].isNull()) err(e, p + ".cell", "bad_cell", "cell goes with items");
  } else if (!n["cell"].isNull()) err(e, p + ".cell", "bad_cell", "only a grid has a cell");
  if ((c == "icon" || c == "image") && !n["p"]["step"].isNull() && strcmp(n["p"]["fit"] | "step", "step")) err(e, p + ".p.fit", "bad_prop", "step or fit: cell, not both");
  if (c == "image" && !n["p"]["aspect"].isNull() && !strcmp(n["p"]["fit"] | "step", "cell")) err(e, p + ".p.aspect", "bad_prop", "aspect goes with step; a cell keeps its own shape");
  if (c == "image" && n["p"]["src"].is<const char *>()) {  // a URL (http / https) or an item field; binds are checked as binds
    const char *s = n["p"]["src"];
    if (*s && strncmp(s, "http://", 7) && strncmp(s, "https://", 8) && !strstr(s, "$item")) err(e, p + ".p.src", "bad_prop", "src is an http(s) URL");
  }
  if (n["cell"].isNull() || c != "grid") return;
  ArduinoJson::JsonVariantConst cn = n["cell"];
  const std::string cc = cn["c"] | "";
  const std::vector<PropInfo> *cps = props_of(cc.c_str());
  if (!cn.is<ArduinoJson::JsonObjectConst>() || !cps) { err(e, p + ".cell", "bad_cell", "cell is one node {c, p} of a component"); return; }
  if (slots_of(cc.c_str())) { err(e, p + ".cell.c", "bad_cell", "a cell can't be a layout (no grids in grids)"); return; }
  if (const SlotDecl *t = slot_named("grid", "item"); t && !slot_takes(*t, cc))
    err(e, p + ".cell.c", "bad_slot", "a grid's item cell takes " + slot_list(*t) + ", not " + cc);
  static const char *const CK[] = {"c", "p"};
  check_keys(cn.as<ArduinoJson::JsonObjectConst>(), CK, 2, p + ".cell", e);
  for (ArduinoJson::JsonPairConst kv : cn["p"].as<ArduinoJson::JsonObjectConst>()) {
    const std::string k = kv.key().c_str();
    auto it = std::find_if(cps->begin(), cps->end(), [&](const PropInfo &x) { return x.name == k; });
    if (it == cps->end()) err(e, p + ".cell.p." + k, one_of(STYLE_KEYS, k) ? "style_key" : "unknown_prop", cc + " has no prop " + k);
    else if (!(kv.value().is<const char *>() && strstr(kv.value() | "", "$item")))  // "$item..." is filled per item
      check_value(*it, kv.value(), p + ".cell.p." + k, e, binds);
  }
}
inline void check_screen(ArduinoJson::JsonVariantConst s, size_t bytes, Errs &e) {
  if (!s.is<ArduinoJson::JsonObjectConst>()) { err(e, "", "bad_spec", "a screen spec is an object"); return; }
  static const char *const K[] = {"v", "id", "name", "rev", "role", "params", "header", "layout", "nodes", "feeds", "card"};
  check_keys(s.as<ArduinoJson::JsonObjectConst>(), K, 11, "", e);
  if (!s["feeds"].isNull()) {  // {"panel.<feed>": {"source": "<adapter>", <text settings, "$param" allowed>}} (C51)
    if (!s["feeds"].is<ArduinoJson::JsonObjectConst>()) err(e, ".feeds", "bad_feeds", "feeds is an object: feed entity -> its settings");
    else for (ArduinoJson::JsonPairConst kv : s["feeds"].as<ArduinoJson::JsonObjectConst>()) {
      const std::string fp = std::string(".feeds.") + kv.key().c_str();
      if (strncmp(kv.key().c_str(), "panel.", 6)) err(e, fp, "bad_feeds", "a feed is a panel.<feed> entity");
      if (!kv.value().is<ArduinoJson::JsonObjectConst>()) { err(e, fp, "bad_feeds", "a feed's settings are an object of texts"); continue; }
      for (ArduinoJson::JsonPairConst f : kv.value().as<ArduinoJson::JsonObjectConst>())
        if (!f.value().is<const char *>()) err(e, fp + "." + f.key().c_str(), "bad_feeds", "a feed setting is text");
    }
  }
  if (!s["role"].isNull() && !one_of(ROLES, s["role"] | "")) err(e, ".role", "bad_role", "role is page, detail or both");
  if ((s["v"] | 0) != PROTOCOL) err(e, ".v", "bad_version", "v must be 1");
  if (!id_ok(s["id"] | "")) err(e, ".id", "bad_id", "id: a-z, 0-9, _; at most 12 characters");
  if (!s["rev"].is<int>() || s["rev"].as<int>() < 0) err(e, ".rev", "bad_rev", "rev is a whole number");
  if (!one_of(LAYOUTS, s["layout"] | "")) err(e, ".layout", "bad_layout", "layout is list, center or cards");
  if (find((s["id"] | ""))) err(e, ".id", "bad_id", "that id is a built-in screen or component");
  if (bytes > (size_t) budget().bytes) err(e, "", "over_budget", "spec over " + std::to_string(budget().bytes) + " bytes");
  if (!s["params"].isNull()) {
    if (!s["params"].is<ArduinoJson::JsonObjectConst>()) err(e, ".params", "bad_params", "params is an object of defaults");
    else for (ArduinoJson::JsonPairConst kv : s["params"].as<ArduinoJson::JsonObjectConst>()) {
      const std::string pp = std::string(".params.") + kv.key().c_str();
      if (kv.value().is<const char *>()) continue;
      if (!kv.value().is<ArduinoJson::JsonObjectConst>()) { err(e, pp, "bad_params", "a parameter is its default (text) or {type, default, label, ...}"); continue; }
      static const char *const PK[] = {"type", "default", "label", "domain", "options", "about"};
      check_keys(kv.value().as<ArduinoJson::JsonObjectConst>(), PK, 6, pp, e);
      if (!one_of(PARAM_TYPES, kv.value()["type"] | "")) err(e, pp + ".type", "bad_params", "type is text, select, entity, entities, toggle or hidden");
      if (!kv.value()["default"].isNull() && !kv.value()["default"].is<const char *>()) err(e, pp + ".default", "bad_params", "a parameter default is text");
      if (!strcmp(kv.value()["type"] | "", "select") && !kv.value()["options"].is<ArduinoJson::JsonArrayConst>()) err(e, pp + ".options", "bad_params", "a select lists its options");
    }
  }
  if (!s["header"].isNull()) {
    static const char *const H[] = {"label", "back", "settings", "actions", "clock"};
    if (!s["header"].is<ArduinoJson::JsonObjectConst>()) err(e, ".header", "bad_header", "header is {label, back, settings?, actions?}");
    else check_keys(s["header"].as<ArduinoJson::JsonObjectConst>(), H, 5, ".header", e);
  }
  ArduinoJson::JsonArrayConst nodes = s["nodes"].as<ArduinoJson::JsonArrayConst>();
  if (nodes.isNull()) { err(e, ".nodes", "bad_nodes", "nodes is a list"); return; }
  if ((int) nodes.size() > budget().nodes) err(e, ".nodes", "over_budget", "more than " + std::to_string(budget().nodes) + " nodes");
  int body = 0;  // nodes in the body: one placed in another's slot ("in": k | "header") isn't laid out by the body
  for (ArduinoJson::JsonVariantConst n : nodes) body += n["in"].isNull();
  if (!strcmp(s["layout"] | "", "center") && body != 1) err(e, ".nodes", "bad_nodes", "a center layout has one node (not counting slot items)");
  int binds = 0, i = 0;
  if (!s["card"].isNull()) {  // the screen's Home card: Card props (binds on its feeds), checked like a node's
    const std::vector<PropInfo> *cps = props_of("card");
    if (!s["card"].is<ArduinoJson::JsonObjectConst>() || !cps) err(e, ".card", "bad_card", "card is the Card's props {label?, icon, preview, value, sub, ...}");
    else for (ArduinoJson::JsonPairConst kv : s["card"].as<ArduinoJson::JsonObjectConst>()) {
      const std::string k = kv.key().c_str();
      auto it = std::find_if(cps->begin(), cps->end(), [&](const PropInfo &x) { return x.name == k; });
      if (it == cps->end()) err(e, ".card." + k, one_of(STYLE_KEYS, k) ? "style_key" : "unknown_prop", "card has no prop " + k);
      else check_value(*it, kv.value(), ".card." + k, e, binds);
    }
  }
  // items per slot so far; the header's end slot starts with its own items (settings, a shortcut, the timer status)
  std::map<std::string, int> fill;
  ArduinoJson::JsonVariantConst hd = s["header"];
  const int fill0 = ((hd["settings"] | false) ? 1 : 0) + (*(hd["actions"]["shortcut"] | "") ? 1 : 0) + (!strcmp(hd["actions"]["status"] | "", "timer") ? 1 : 0);
  fill["header's end"] = fill0;
  for (ArduinoJson::JsonVariantConst n : nodes) {
    const std::string p = ".nodes[" + std::to_string(i++) + "]";
    if (!n.is<ArduinoJson::JsonObjectConst>()) { err(e, p, "bad_node", "a node is an object {c, p, on}"); continue; }
    static const char *const NK[] = {"c", "p", "on", "pending", "in", "slot", "each", "sample", "show", "span", "cell"};
    check_keys(n.as<ArduinoJson::JsonObjectConst>(), NK, 11, p, e);
    if (!n["show"].isNull()) {
      if (!ha::is_bind(n["show"])) err(e, p + ".show", "bad_bind", "show is a bind {e, ...}");
      else { PropInfo si; si.name = "show"; si.type = "text"; check_value(si, n["show"], p + ".show", e, binds); }
    }
    const std::string c = n["c"] | "";
    const SlotDecl *sd = nullptr;  // the slot this node goes in (its declaration), its fill key and its words
    std::string sk, sw;
    if (n["in"].is<const char *>()) {  // into the header row
      if (!strcmp(n["in"] | "", "overlay")) {  // a sheet over the whole screen (C4: the Overlay layer)
        if (strcmp(n["c"] | "", "bottom_sheet")) err(e, p + ".in", "bad_in", "the overlay takes a bottom_sheet");
        if (!n["slot"].isNull()) err(e, p + ".slot", "bad_in", "the overlay has no slots");
      } else {
      if (strcmp(n["in"] | "", "header")) err(e, p + ".in", "bad_in", "in: \"header\", \"overlay\" or an earlier layout node");
      const std::string hs = n["slot"] | "end";
      sd = n["slot"].isNull() || n["slot"].is<const char *>() ? slot_named("header", hs) : nullptr;
      if (!sd)
        err(e, p + ".slot", "bad_in", hs == "start" ? "the header has no start slot (Back is fixed at the start): use slot end"
                                                   : "the header's slots are title and end");
      else { sk = "header's " + hs; sw = "the header's " + hs + " slot"; }
      }
    } else if (!n["in"].isNull()) {  // into an earlier layout node's slot
      const int k = n["in"] | -1, sl = n["slot"] | 0;
      const std::string kc = k >= 0 && k < i - 1 ? (nodes[k]["c"] | "") : "";
      const int ns = kc.empty() ? 0 : slots_of(kc.c_str());
      if (k < 0 || k >= i - 1 || !ns) err(e, p + ".in", "bad_in", "in: an earlier node that is a layout (has slots)");
      else if (!nodes[k]["each"].isNull()) err(e, p + ".in", "bad_in", "a repeated (each) node can't hold others");
      else if (!n["slot"].isNull() && (!n["slot"].is<int>() || sl < 0 || sl >= ns)) err(e, p + ".slot", "bad_in", kc + " has slots 0.." + std::to_string(ns - 1));
      else if ((sd = slot_at(kc.c_str(), sl))) { sk = std::to_string(k) + "." + std::to_string(sl); sw = kc + "'s " + (sd->name.empty() ? "slot" : sd->name) + " " + std::to_string(sl); }
    } else if (!n["slot"].isNull()) err(e, p + ".slot", "bad_in", "slot goes with in");
    if (!n["each"].isNull() && !id_ok(n["each"] | "")) err(e, p + ".each", "bad_each", "each: a screen parameter (a comma list)");
    if (!n["sample"].isNull()) {
      if (n["each"].isNull()) err(e, p + ".sample", "bad_each", "sample goes with each");
      else if (!n["sample"].is<ArduinoJson::JsonArrayConst>() || n["sample"].size() > (size_t) EACH_MAX) err(e, p + ".sample", "bad_each", "sample is a list of prop objects, at most " + std::to_string(EACH_MAX));
    }
    const std::vector<PropInfo> *pis = props_of(c.c_str());
    const Component *comp = find(c.c_str());
    if (!pis || !comp || strcmp(comp->section, "components")) { err(e, p + ".c", "unknown_component", "no component " + c + " in this build"); continue; }
    if (sd) {  // the slot's allow list and max count (facet-205)
      if (!slot_takes(*sd, c)) err(e, p + ".c", "bad_slot", sw + " takes " + slot_list(*sd) + ", not " + c);
      if (sd->max && !n["each"].isNull()) err(e, p + ".each", "bad_slot", sw + " holds at most " + std::to_string(sd->max) + ": a repeated (each) node can't go in it");
      else if (sd->max && ++fill[sk] > sd->max)
        err(e, p + ".in", "bad_slot", sw + " holds at most " + std::to_string(sd->max) + (sk == "header's end" && fill0 ? " (the header's own settings / actions count)" : "") + ": this is item " + std::to_string(fill[sk]));
    }
    for (ArduinoJson::JsonPairConst kv : n["p"].as<ArduinoJson::JsonObjectConst>()) {
      const std::string k = kv.key().c_str();
      auto it = std::find_if(pis->begin(), pis->end(), [&](const PropInfo &x) { return x.name == k; });
      if (it == pis->end()) { err(e, p + ".p." + k, one_of(STYLE_KEYS, k) ? "style_key" : "unknown_prop", c + " has no prop " + k); continue; }
      // a row list (List row stack, Picker row group, Scroll track: items, 2026-10-26) fills "$item..." per row
      if (c != "grid" && !n["p"]["items"].isNull() && kv.value().is<const char *>() && strstr(kv.value() | "", "$item")) continue;
      check_value(*it, kv.value(), p + ".p." + k, e, binds);
    }
    check_grid(n, nodes, i, c, p, e, binds);
    int si = 0;
    for (ArduinoJson::JsonVariantConst sm : n["sample"].as<ArduinoJson::JsonArrayConst>()) {
      const std::string sp = p + ".sample[" + std::to_string(si++) + "]";
      if (!sm.is<ArduinoJson::JsonObjectConst>()) { err(e, sp, "bad_each", "a sample is a prop object"); continue; }
      for (ArduinoJson::JsonPairConst kv : sm.as<ArduinoJson::JsonObjectConst>()) {
        auto it = std::find_if(pis->begin(), pis->end(), [&](const PropInfo &x) { return x.name == kv.key().c_str(); });
        if (it == pis->end()) err(e, sp + "." + kv.key().c_str(), "unknown_prop", c + " has no prop " + kv.key().c_str());
        else check_value(*it, kv.value(), sp + "." + kv.key().c_str(), e, binds);
      }
    }
    for (ArduinoJson::JsonPairConst kv : n["on"].as<ArduinoJson::JsonObjectConst>())
      check_action(kv.value(), c.c_str(), kv.key().c_str(), p + ".on." + kv.key().c_str(), e);
    for (ArduinoJson::JsonPairConst kv : n["pending"].as<ArduinoJson::JsonObjectConst>()) {
      ArduinoJson::JsonArrayConst a = kv.value().as<ArduinoJson::JsonArrayConst>();
      if (a.size() != 2 || !a[0].is<const char *>() || !a[1].is<const char *>()) err(e, p + ".pending." + kv.key().c_str(), "bad_pending", "pending is [on text, off text]");
      else if (std::find_if(pis->begin(), pis->end(), [&](const PropInfo &x) { return x.name == kv.key().c_str(); }) == pis->end())
        err(e, p + ".pending." + kv.key().c_str(), "unknown_prop", c + " has no prop " + kv.key().c_str());
    }
  }
  if (binds > budget().binds) err(e, ".nodes", "over_budget", "more than " + std::to_string(budget().binds) + " binds");
}
inline bool screen_known(const std::string &id);
inline std::string role_of(const std::string &id);
// a comma list holds x
inline bool csv_has(const char *list, const std::string &x) {
  for (const char *p = list; *p;) {
    const char *c = strchr(p, ',');
    const size_t n = c ? (size_t) (c - p) : strlen(p);
    if (n == x.size() && !strncmp(p, x.c_str(), n)) return true;
    if (!c) break;
    p = c + 1;
  }
  return false;
}
inline const CardType *card_type(const std::string &t) {
  for (const CardType &c : CARD_TYPES) if (t == c.t) return &c;
  return nullptr;
}
inline void check_card(ArduinoJson::JsonVariantConst c, const std::string &p, const std::set<std::string> &ids, Errs &e) {
  static const char *const CK[] = {"t", "e", "size", "tap", "page", "label"};
  if (!c.is<ArduinoJson::JsonObjectConst>()) { err(e, p, "bad_card", "a card is {t, e, size, tap?, page?, label?}"); return; }
  check_keys(c.as<ArduinoJson::JsonObjectConst>(), CK, 6, p, e);
  const std::string t = c["t"] | "", ent = c["e"] | "", size = c["size"] | "", tap = c["tap"] | "", pg = c["page"] | "";
  const CardType *ct = card_type(t);
  if (!ct) { err(e, p + ".t", "bad_card", "t is a card type (light, cover, climate, media, sensor, page)"); return; }
  if (*ct->domains) {
    const size_t dot = ent.find('.');
    if (dot == std::string::npos || !csv_has(ct->domains, ent.substr(0, dot))) err(e, p + ".e", "bad_entity", std::string("a ") + t + " card takes " + ct->domains + " entities");
  } else if (!ent.empty()) err(e, p + ".e", "bad_card", "a page card has no entity");
  if (!csv_has(ct->sizes, size)) err(e, p + ".size", "bad_size", std::string("a ") + t + " card is size " + ct->sizes);
  if (!tap.empty() && !csv_has(ct->taps, tap)) err(e, p + ".tap", "bad_tap", std::string("a ") + t + " card's tap is " + ct->taps);
  if (!pg.empty() && !ids.count(pg)) err(e, p + ".page", "bad_page", "no page " + pg + " in this deck");
  if (t == "page" && pg.empty()) err(e, p + ".page", "bad_page", "a page card names a page");
  if (!c["label"].isNull() && (!c["label"].is<const char *>() || strlen(c["label"] | "") > 32)) err(e, p + ".label", "bad_card", "label is text (at most 32)");
}
inline void check_deck(ArduinoJson::JsonVariantConst d, size_t bytes, Errs &e) {
  if (!d.is<ArduinoJson::JsonObjectConst>()) { err(e, "", "bad_spec", "a deck is an object"); return; }
  if ((d["v"] | 0) != DECK_V) { err(e, ".v", "bad_version", "deck v must be 2 (v1 decks are retired)"); return; }
  static const char *const K[] = {"v", "rev", "start", "pages"};
  check_keys(d.as<ArduinoJson::JsonObjectConst>(), K, 4, "", e);
  if (!d["rev"].is<int>()) err(e, ".rev", "bad_rev", "rev is a whole number");
  if (bytes > (size_t) budget().deck_bytes) err(e, "", "over_budget", "deck over " + std::to_string(budget().deck_bytes) + " bytes");
  ArduinoJson::JsonArrayConst s = d["pages"].as<ArduinoJson::JsonArrayConst>();
  if (s.isNull() || !s.size()) { err(e, ".pages", "bad_deck", "pages is a non-empty list"); return; }
  if ((int) s.size() > budget().pages) err(e, ".pages", "over_budget", "more than " + std::to_string(budget().pages) + " pages");
  std::set<std::string> ids;
  int i = 0;
  for (ArduinoJson::JsonVariantConst x : s) {
    const std::string id = x["id"] | "", p = ".pages[" + std::to_string(i++) + "]";
    if (!id_ok(id)) err(e, p + ".id", "bad_id", "id: a-z, 0-9, _; at most 12 characters");
    else if (!ids.insert(id).second) err(e, p + ".id", "bad_id", "two pages are " + id);
  }
  const std::string start = d["start"] | "";
  if (!ids.count(start)) err(e, ".start", "bad_start", "start names a page of this deck");
  int homes = 0;
  i = 0;
  for (ArduinoJson::JsonVariantConst x : s) {
    const std::string p = ".pages[" + std::to_string(i++) + "]";
    static const char *const SK[] = {"id", "kind", "name", "source", "params", "cards", "header_actions"};
    if (!x.is<ArduinoJson::JsonObjectConst>()) { err(e, p, "bad_deck", "a page is {id, kind, name?, source?, params?, cards?}"); continue; }
    check_keys(x.as<ArduinoJson::JsonObjectConst>(), SK, 7, p, e);
    const std::string kind = x["kind"] | "";
    if (!screen_known(kind)) { err(e, p + ".kind", "unknown_screen", "no screen " + kind + " (send its spec first)"); continue; }
    const std::string role = role_of(kind);
    if (role == "detail") err(e, p + ".kind", "not_a_page", kind + " is a detail screen: it opens from a card, not the deck");
    else if (role == "system") err(e, p + ".kind", "not_a_page", kind + " is part of the panel, not a page");
    if (!x["name"].isNull() && (!x["name"].is<const char *>() || strlen(x["name"] | "") > 32)) err(e, p + ".name", "bad_deck", "name is text (at most 32)");
    if (!x["source"].isNull() && !word_ok(x["source"] | "", 24)) err(e, p + ".source", "bad_deck", "source: a source id (a-z, 0-9, _; at most 24)");
    if (!x["params"].isNull()) {
      if (!x["params"].is<ArduinoJson::JsonObjectConst>()) err(e, p + ".params", "bad_deck", "params is {name: text}");
      else for (ArduinoJson::JsonPairConst kv : x["params"].as<ArduinoJson::JsonObjectConst>())
        if (!kv.value().is<const char *>() || strlen(kv.value().as<const char *>()) > 1024) err(e, p + ".params." + kv.key().c_str(), "bad_deck", "a parameter is text (at most 1024)");
    }
    if (kind == "home") homes++;
    if (!x["header_actions"].isNull()) {  // a Home's header actions: {button: "settings" | a page id, status: none | timer}
      ArduinoJson::JsonVariantConst ha_ = x["header_actions"];
      static const char *const HK[] = {"button", "status"};
      if (kind != "home" || !ha_.is<ArduinoJson::JsonObjectConst>()) err(e, p + ".header_actions", "bad_deck", "header_actions is {button?, status?} on a home");
      else {
        check_keys(ha_.as<ArduinoJson::JsonObjectConst>(), HK, 2, p + ".header_actions", e);
        const std::string b = ha_["button"] | "settings", st = ha_["status"] | "none";
        if (b != "settings" && !ids.count(b)) err(e, p + ".header_actions.button", "bad_page", "button is settings or a page id of this deck");
        if (st != "none" && st != "timer") err(e, p + ".header_actions.status", "bad_deck", "status is none or timer");
      }
    }
    if (x["cards"].isNull()) continue;
    if (kind != "home") { err(e, p + ".cards", "bad_deck", "only a home has cards"); continue; }
    ArduinoJson::JsonArrayConst cs = x["cards"].as<ArduinoJson::JsonArrayConst>();
    if (cs.isNull()) { err(e, p + ".cards", "bad_deck", "cards is a list"); continue; }
    if ((int) cs.size() > budget().cards) err(e, p + ".cards", "over_budget", "more than " + std::to_string(budget().cards) + " cards on a home");
    int k = 0;
    for (ArduinoJson::JsonVariantConst c : cs) check_card(c, p + ".cards[" + std::to_string(k++) + "]", ids, e);
  }
  if (homes > budget().homes) err(e, ".pages", "over_budget", "more than " + std::to_string(budget().homes) + " homes");
}
inline std::string errs_json(const Errs &e) {
  ArduinoJson::JsonDocument d;
  ArduinoJson::JsonArray a = d.to<ArduinoJson::JsonArray>();
  for (const Err &x : e) { auto o = a.add<ArduinoJson::JsonObject>(); o["path"] = x.path; o["code"] = x.code; o["msg"] = x.msg; }
  std::string s;
  serializeJson(d, s);
  return s;
}

// ---- storage (host-given) + the store
struct Storage {
  std::function<bool(const std::string &key, std::string &out)> load;
  std::function<bool(const std::string &key, const std::string &val)> save;
  std::function<void(const std::string &key)> erase;
};
inline uint32_t crc32(const std::string &s) {
  uint32_t c = 0xFFFFFFFFu;
  for (unsigned char b : s) {
    c ^= b;
    for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
  }
  return ~c;
}
inline std::string record(const std::string &json) {
  char h[12];
  snprintf(h, sizeof h, "%08x\n", (unsigned) crc32(json));
  return h + json;
}
inline bool unrecord(const std::string &rec, std::string &json) {
  if (rec.size() < 10 || rec[8] != '\n') return false;
  json = rec.substr(9);
  char h[12];
  snprintf(h, sizeof h, "%08x", (unsigned) crc32(json));
  return !strncmp(h, rec.c_str(), 8);
}
struct Store {
  Storage io;
  std::map<std::string, std::string> over;  // id -> the live override (JSON); "" key unused
  std::string deck_over;
  std::vector<std::string> log;             // what boot / apply did (hello)
};
inline Store &S() {
  static Store s;
  return s;
}
inline const char *factory(const std::string &id) {
  for (const auto &f : FACTORY)
    if (id == f.id) return f.json;
  return nullptr;
}
// a sent screen (facet-188): a live spec screen with no factory copy (an agent's / the AI's). It opens one level
// above Home and replaces the previous sent screen (core/device.cpp "Navigation").
inline bool sent(const std::string &id) { return !factory(id) && S().over.count(id); }
inline const char *screen_json(const std::string &id) {
  auto it = S().over.find(id);
  if (it != S().over.end()) return it->second.c_str();
  return factory(id);
}
inline bool screen_known(const std::string &id) { return screen_json(id) || find(id.c_str()); }
inline const char *deck_json() { return S().deck_over.empty() ? FACTORY_DECK : S().deck_over.c_str(); }
// what nav.open can open: a screen, or a deck page id (a page card / shortcut) whose kind is a screen this build has
inline bool opens_known(const std::string &id) {
  if (screen_known(id)) return true;
  ArduinoJson::JsonDocument deck;
  if (deserializeJson(deck, deck_json())) return false;
  for (ArduinoJson::JsonVariantConst pg : deck["pages"].as<ArduinoJson::JsonArrayConst>())
    if (id == (pg["id"] | "")) { const std::string k = pg["kind"] | ""; return k == "home" || screen_known(k); }
  return false;
}
// a screen's role: its spec's "role" (page when it has none), "page" for the built-in Home, "system" for the other
// built-ins (Settings), "" = unknown
inline std::string role_of(const std::string &id) {
  if (const char *j = screen_json(id)) {
    ArduinoJson::JsonDocument d;
    if (deserializeJson(d, j)) return "";
    return d["role"] | "page";
  }
  if (id == "home") return "page";
  return find(id.c_str()) ? "system" : "";
}
inline bool page_role(const std::string &id) { const std::string r = role_of(id); return r == "page" || r == "both"; }
// the page with that id in a deck's JSON, else the first page of that kind; false = none
inline bool deck_page(const char *json, const std::string &id, ArduinoJson::JsonDocument &out) {
  ArduinoJson::JsonDocument d;
  if (!json || deserializeJson(d, json)) return false;
  ArduinoJson::JsonArrayConst ps = d["pages"].as<ArduinoJson::JsonArrayConst>();
  for (ArduinoJson::JsonVariantConst x : ps) if (id == (x["id"] | "")) { out.set(x); return true; }
  for (ArduinoJson::JsonVariantConst x : ps) if (id == (x["kind"] | "")) { out.set(x); return true; }
  return false;
}
// this panel's deck page for id into out: the page with that id, else the first page of that kind (a screen opened
// by its spec id finds its page); false = not in the deck. A saved deck's page takes the params it does not set from
// the factory deck's page of the same id (2026-10-26: a deck saved before a parameter existed, e.g. the Media
// screen's speakers, still gets the build's list; a param set to "" stays cleared)
inline bool deck_entry(const std::string &id, ArduinoJson::JsonDocument &out) {
  if (!deck_page(deck_json(), id, out)) return false;
  if (S().deck_over.empty()) return true;
  ArduinoJson::JsonDocument f;
  if (!deck_page(FACTORY_DECK, out["id"] | id.c_str(), f) || strcmp(f["kind"] | "", out["kind"] | "")) return true;
  for (ArduinoJson::JsonPairConst kv : f["params"].as<ArduinoJson::JsonObjectConst>())
    if (out["params"][kv.key()].isNull()) out["params"][kv.key()] = kv.value();
  return true;
}
// did this panel pick param key for deck page id (the saved deck's own value, not a factory fallback)?
inline bool deck_picked(const std::string &id, const std::string &key) {
  ArduinoJson::JsonDocument d;
  return deck_page(deck_json(), id, d) && d["params"][key.c_str()].is<const char *>();
}
// is this deck page there (deck_entry found one)? A page you don't want is not in the deck.
inline bool deck_on(ArduinoJson::JsonVariantConst entry) { return entry.is<ArduinoJson::JsonObjectConst>(); }
// the page kinds the deck has a page of
inline std::set<std::string> deck_kinds() {
  ArduinoJson::JsonDocument d;
  deserializeJson(d, deck_json());
  std::set<std::string> used;
  for (ArduinoJson::JsonVariantConst x : d["pages"].as<ArduinoJson::JsonArrayConst>()) used.insert(x["kind"] | "");
  return used;
}
// built-in page kinds this panel has no page of (Settings > Screens and the editor show them as off)
inline std::vector<std::string> off_kinds() {
  const std::set<std::string> used = deck_kinds();
  std::vector<std::string> v;
  for (const auto &f : FACTORY) if (!used.count(f.id) && page_role(f.id)) v.push_back(f.id);
  return v;
}
// sent page screens (a spec sent by Claude, the API or an agent, not a built-in) the deck has no page of: Settings >
// Screens' "Sent" group (Add to deck / Remove = revert), the editor's Sent list (facet-166)
inline std::vector<std::string> sent_kinds() {
  const std::set<std::string> used = deck_kinds();
  std::vector<std::string> v;
  for (const auto &kv : S().over) if (!factory(kv.first) && !used.count(kv.first) && page_role(kv.first)) v.push_back(kv.first);
  return v;
}
inline int rev_of(const char *json) {
  ArduinoJson::JsonDocument d;
  return json && !deserializeJson(d, json) ? (d["rev"] | -1) : -1;
}
// the override ids kept (storage key "idx": comma list)
inline std::vector<std::string> index_ids() {
  std::string s;
  std::vector<std::string> v;
  if (!S().io.load || !S().io.load("idx", s)) return v;
  for (size_t p = 0; p < s.size();) {
    size_t c = s.find(',', p);
    if (c == std::string::npos) c = s.size();
    if (c > p) v.push_back(s.substr(p, c - p));
    p = c + 1;
  }
  return v;
}
inline bool save_index(const std::vector<std::string> &v) {
  std::string s;
  for (const auto &x : v) s += (s.empty() ? "" : ",") + x;
  return S().io.save && S().io.save("idx", s);
}
// a stored record for key k that is intact and valid, else ""
inline std::string good(const std::string &k, bool deck) {
  std::string rec, json;
  if (!S().io.load || !S().io.load(k, rec) || !unrecord(rec, json)) return "";
  ArduinoJson::JsonDocument d;
  if (deserializeJson(d, json)) return "";
  Errs e;
  if (deck) check_deck(d.as<ArduinoJson::JsonVariantConst>(), json.size(), e);
  else check_screen(d.as<ArduinoJson::JsonVariantConst>(), json.size(), e);
  return e.empty() ? json : "";
}
// boot: every override from storage (current, else its last good copy, else factory); then the deck
inline void load() {
  Store &s = S();
  s.over.clear();
  s.deck_over.clear();
  s.log.clear();
  for (const std::string &id : index_ids()) {
    if (id == "deck") continue;
    std::string j = good("s." + id, false);
    if (j.empty()) { j = good("g." + id, false); s.log.push_back(id + (j.empty() ? ": no intact copy, factory" : ": current copy bad, last good")); }
    if (!j.empty()) s.over[id] = j;
  }
  std::string d = good("s.deck", true);
  if (d.empty()) { d = good("g.deck", true); if (!d.empty()) s.log.push_back("deck: current copy bad, last good"); }
  s.deck_over = d;
}

// ---- building a screen from its spec
struct Node {
  lv_obj_t *obj = nullptr;
  std::string comp, on, pending, entity;  // on / pending = JSON; entity = the light its actions act on
  std::string raw_p, raw_on;               // props / on as the spec has them ("$param" kept): screen.param binds again
  std::vector<std::string> params;         // anchors: the screen parameters this node shows / acts on (spec order)
  std::map<std::string, std::vector<std::string>> prop_params;  // per prop: the parameters it is fed by
  bool has_brightness = false;
  double c_sent = NAN, c_want = NAN;  // climate set point: last sent, the latest waiting for the interval
  uint32_t c_at = 0;
  int sent = -1, want = -1;  // brightness throttle (light-throttle-ms)
  uint32_t sent_at = 0;
  bool drag_sent = false;
  int8_t pend = 0;  // a pending text to show after the toggle's slide: 1 on, -1 off
};
constexpr int ON_ONLY = 1000;
struct Screen {
  lv_obj_t *root = nullptr;
  std::vector<Node> nodes;
  lv_timer_t *timer = nullptr;  // throttle trail + pending texts
  std::map<std::string, std::string> params;
  // Auto (screen.param "auto"): the parameter follow_param follows the state of entity follow_src (HA's media target
  // sensor) while following; a pick of a real value stops it, a pick of "auto" starts it again
  // a pick (Auto or a real one, here or in Settings > Screens) is kept in the deck (params.<follow_param>: "auto" =
  // Auto), so the screen opens on it next time; id = the deck screen id
  // follow_from = the action's "from" list parameter: with no follow_src, Auto follows the first of it that plays
  std::string follow_param, follow_src, follow_from, follow_last, id;
  bool following = false;
  std::vector<std::pair<std::string, std::string>> undo;  // screen.param "back": (param, value before), Back pops one
};
// the spec screens alive now (the editor's hit-test walks them)
inline std::vector<Screen *> &live() { static std::vector<Screen *> v; return v; }

// ---- anchors (2026-10-23, the editor's click-to-edit): which screen parameters a part of the screen shows.
// Every node is anchored to the "$param"s its props and actions use (found when the screen is built, no spec
// changes). A component may also mark one of its parts with the prop that part shows (fw::spec::anchor(part,
// "visual")): the editor can then pick that part alone (its parameters = the ones feeding that prop). Hosts read them
// with hits(): [{n, c, prop?, params, x, y, w, h}] in device px, nodes first then parts; hit() = the smallest at a point.
inline std::map<lv_obj_t *, std::string> &anchors() { static std::map<lv_obj_t *, std::string> m; return m; }
inline void anchor(lv_obj_t *part, const char *prop) {
  if (!anchors().count(part))
    lv_obj_add_event_cb(part, [](lv_event_t *e) { anchors().erase((lv_obj_t *) lv_event_get_current_target(e)); }, LV_EVENT_DELETE, nullptr);
  anchors()[part] = prop;
}
// the "$name" refs inside v that name a parameter (not the derived "$mode")
inline void refs(ArduinoJson::JsonVariantConst v, const std::map<std::string, std::string> &pm, std::set<std::string> &out) {
  if (v.is<const char *>()) {
    const char *t = v.as<const char *>();
    for (const char *c = strchr(t, '$'); c; c = strchr(c + 1, '$')) {
      const char *j = c + 1;
      while (isalnum((unsigned char) *j) || *j == '_') j++;
      std::string k(c + 1, j - c - 1);
      if (k != "mode" && pm.count(k)) out.insert(k);
    }
  } else if (v.is<ArduinoJson::JsonObjectConst>()) {
    for (ArduinoJson::JsonPairConst kv : v.as<ArduinoJson::JsonObjectConst>()) refs(kv.value(), pm, out);
  } else if (v.is<ArduinoJson::JsonArrayConst>()) {
    for (ArduinoJson::JsonVariantConst x : v.as<ArduinoJson::JsonArrayConst>()) refs(x, pm, out);
  }
}
inline std::vector<std::string> ordered(const std::set<std::string> &got, ArduinoJson::JsonObjectConst spec_params) {
  std::vector<std::string> v;
  for (ArduinoJson::JsonPairConst kv : spec_params)
    if (got.count(kv.key().c_str())) v.push_back(kv.key().c_str());
  return v;
}
inline void hit_box(ArduinoJson::JsonObject r, lv_obj_t *o) {
  lv_area_t a;
  lv_obj_get_coords(o, &a);
  r["x"] = a.x1;
  r["y"] = a.y1;
  r["w"] = lv_area_get_width(&a);
  r["h"] = lv_area_get_height(&a);
}
inline bool shown(lv_obj_t *o, lv_obj_t *upto) {
  for (; o && o != upto; o = lv_obj_get_parent(o))
    if (lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN)) return false;
  return true;
}
inline void hits(lv_display_t *d, ArduinoJson::JsonArray arr) {
  lv_obj_t *act = lv_display_get_screen_active(d);
  for (Screen *s : live()) {
    if (lv_obj_get_screen(s->root) != act || !shown(s->root, nullptr)) continue;
    for (size_t i = 0; i < s->nodes.size(); i++) {
      const Node &n = s->nodes[i];
      if (!n.obj || n.params.empty() || !shown(n.obj, s->root)) continue;
      auto r = arr.add<ArduinoJson::JsonObject>();
      r["n"] = i;
      r["c"] = n.comp;
      for (const std::string &p : n.params) r["params"].add(p);
      hit_box(r, n.obj);
    }
    for (auto &kv : anchors()) {
      Node *n = nullptr;
      for (lv_obj_t *o = kv.first; o && !n; o = lv_obj_get_parent(o))
        for (Node &x : s->nodes) if (x.obj == o) { n = &x; break; }
      if (!n || !shown(kv.first, s->root)) continue;
      auto it = n->prop_params.find(kv.second);
      if (it == n->prop_params.end() || it->second.empty()) continue;
      auto r = arr.add<ArduinoJson::JsonObject>();
      r["n"] = n - s->nodes.data();
      r["c"] = n->comp;
      r["prop"] = kv.second;
      for (const std::string &p : it->second) r["params"].add(p);
      hit_box(r, kv.first);
    }
  }
}
// the smallest anchored box at (x, y), or false
inline bool hit(lv_display_t *d, int x, int y, ArduinoJson::JsonObject out) {
  ArduinoJson::JsonDocument all;
  hits(d, all.to<ArduinoJson::JsonArray>());
  ArduinoJson::JsonVariantConst best;
  long area = -1;
  for (ArduinoJson::JsonVariantConst r : all.as<ArduinoJson::JsonArrayConst>()) {
    const int rx = r["x"], ry = r["y"], rw = r["w"], rh = r["h"];
    if (x < rx || y < ry || x >= rx + rw || y >= ry + rh) continue;
    if (area < 0 || (long) rw * rh < area) { area = (long) rw * rh; best = r; }
  }
  if (area < 0) return false;
  out.set(best.as<ArduinoJson::JsonObjectConst>());
  return true;
}
// "$name" -> the parameter (whole-string refs and inside texts)
inline std::string subst(const std::string &v, const std::map<std::string, std::string> &pm) {
  if (v.find('$') == std::string::npos) return v;
  std::string out;
  for (size_t i = 0; i < v.size();) {
    if (v[i] == '$') {
      size_t j = i + 1;
      while (j < v.size() && (isalnum((unsigned char) v[j]) || v[j] == '_')) j++;
      auto it = pm.find(v.substr(i + 1, j - i - 1));
      if (it != pm.end()) { out += it->second; i = j; continue; }
    }
    out += v[i++];
  }
  return out;
}
inline void subst_all(ArduinoJson::JsonVariant v, const std::map<std::string, std::string> &pm) {
  if (v.is<const char *>()) { v.set(subst(v.as<std::string>(), pm)); return; }
  if (v.is<ArduinoJson::JsonObject>()) for (ArduinoJson::JsonPair kv : v.as<ArduinoJson::JsonObject>()) subst_all(kv.value(), pm);
  if (v.is<ArduinoJson::JsonArray>()) for (ArduinoJson::JsonVariant x : v.as<ArduinoJson::JsonArray>()) subst_all(x, pm);
}
inline Node *node_of(Screen *s, lv_obj_t *o) {
  for (; o; o = lv_obj_get_parent(o))
    for (Node &n : s->nodes)
      if (n.obj == o) return &n;
  return nullptr;
}
// every HA call a node's action makes goes through here: a refusal's reason is kept for act() (fw::spec::act answers it)
inline std::string &act_why() { static std::string w; return w; }
inline const char *ha_act(Node &n, const char *service, const char *entity, const char *data_json = "") {
  const char *w = ha::act(n.obj, service, entity, data_json);
  if (*w) act_why() = w;
  return w;
}
inline void send_brightness(Node &n, int v) {
  if (v == n.sent) return;
  char data[40];
  snprintf(data, sizeof data, "{\"brightness_pct\":%d}", v);
  if (*(v > 0 ? ha_act(n, "light.turn_on", n.entity.c_str(), data) : ha_act(n, "light.turn_off", n.entity.c_str()))) return;
  n.sent = v;
  n.sent_at = lv_tick_get();
}
// a set point within the entity's own range and step (HA min_temp / max_temp / target_temp_step, else 5..35 by 0.5)
inline double climate_clamp(const std::string &e, double v) {
  const double mn = ha::num(ha::get(e, "min_temp")), mx = ha::num(ha::get(e, "max_temp")), st = ha::num(ha::get(e, "target_temp_step"));
  const double lo = std::isfinite(mn) ? mn : 5, hi = std::isfinite(mx) ? mx : 35, step = std::isfinite(st) && st > 0 ? st : 0.5;
  return LV_CLAMP(lo, lo + std::round((v - lo) / step) * step, hi);
}
inline void send_temperature(Node &n, double v) {
  v = climate_clamp(n.entity, v);
  if (std::isfinite(n.c_sent) && std::fabs(v - n.c_sent) < 1e-6) return;
  char data[48];
  snprintf(data, sizeof data, "{\"temperature\":%g}", v);
  if (*ha_act(n, "climate.set_temperature", n.entity.c_str(), data)) return;
  n.c_sent = v;
  n.c_at = lv_tick_get();
}
inline void follow_now(Screen *s);
inline bool deck_set(const std::string &id, const std::string &key, ArduinoJson::JsonVariantConst value);
inline void tick(lv_timer_t *t) {
  auto *s = (Screen *) lv_timer_get_user_data(t);
  const uint32_t now = lv_tick_get();
  follow_now(s);
  for (Node &n : s->nodes) {
    if (n.want == ON_ONLY && lv_tick_diff(now, n.sent_at) >= 300) {
      n.want = -1;
      if (!*ha_act(n, "light.turn_on", n.entity.c_str())) { n.sent = -1; n.sent_at = now; }
      ha::hold(n.obj, LIGHT_HOLD_MS, n.entity, "", "on");
    } else if (n.want >= 0 && n.want != ON_ONLY && lv_tick_diff(now, n.sent_at) >= LIGHT_THROTTLE_MS) {
      send_brightness(n, n.want);
      n.want = -1;
    }
    if (std::isfinite(n.c_want) && lv_tick_diff(now, n.c_at) >= CLIMATE_THROTTLE_MS) {
      send_temperature(n, n.c_want);
      n.c_want = NAN;
    }
    if (n.pend && lv_tick_diff(now, n.sent_at) >= mo::duration(n.obj, st::SWITCH, mo::MOVE) + 20) {
      const bool on = n.pend > 0;
      n.pend = 0;
      if (!ha::held(n.obj)) continue;  // HA already answered
      ha::Bound *b = ha::bound_of(n.obj);
      ArduinoJson::JsonDocument spec, p, pend;
      deserializeJson(spec, b->spec);
      ha::resolve(spec.as<Props>(), p, nullptr);
      deserializeJson(pend, n.pending);
      for (ArduinoJson::JsonPairConst kv : pend.as<ArduinoJson::JsonObjectConst>()) p[kv.key()] = kv.value()[on ? 0 : 1];
      if (p["on"].is<bool>()) p["on"] = on;
      Quiet q;
      update_child(b->c, n.obj, p.as<Props>());
    }
  }
}
// what a node's actions act on (from its "on"): the entity, a brightness drag, the climate attributes it needs
inline void node_actions(Node &n) {
  ArduinoJson::JsonDocument on;
  deserializeJson(on, n.on.empty() ? "{}" : n.on);
  n.has_brightness = false;
  for (ArduinoJson::JsonPairConst kv : on.as<ArduinoJson::JsonObjectConst>()) {
    const std::string e = kv.value()["e"] | "";
    if (!e.empty()) n.entity = e;
    if (!strcmp(kv.value()["do"] | "", "light.brightness")) n.has_brightness = true;
    if (!strncmp(kv.value()["do"] | "", "climate.", 8) && !e.empty())  // its range + what settles a change
      for (const char *a : {"", "temperature", "min_temp", "max_temp", "target_temp_step", "preset_mode"}) ha::want(e, a);
  }
}
// every node bound again from its spec as written, with the screen's parameters as they are now (update path)
inline void rebind_all(Screen *s);
// screen.param: the parameter takes `v` (checked) and every node is bound again (update path); false = refused
inline bool set_param(Screen *s, const std::string &param, const std::string &from, const std::string &v) {
  auto cur = s->params.find(param);
  if (cur == s->params.end() || v.empty() || v[0] == '$' || !(entity_ok(v) || word_ok(v, 24))) return false;  // an entity or a word (a view)
  if (cur->second == v) { rebind_all(s); return true; }
  const std::string &old = cur->second;
  if (old.find('.') != std::string::npos && v.substr(0, v.find('.')) != old.substr(0, old.find('.'))) return false;  // same domain
  if (!from.empty()) {  // one of that parameter's comma list
    auto l = s->params.find(from);
    if (l == s->params.end() || !in_list(l->second.c_str(), v)) return false;
  }
  cur->second = v;
  rebind_all(s);
  return true;
}
inline void rebind_all(Screen *s) {
  for (Node &n : s->nodes) {
    ArduinoJson::JsonDocument p, on;
    deserializeJson(p, n.raw_p.empty() ? "{}" : n.raw_p);
    subst_all(p.as<ArduinoJson::JsonVariant>(), s->params);
    if (!n.raw_on.empty()) {
      deserializeJson(on, n.raw_on);
      subst_all(on.as<ArduinoJson::JsonVariant>(), s->params);
      n.on.clear();
      serializeJson(on, n.on);
      node_actions(n);
    }
    ha::rebind(n.obj, p.as<Props>());
  }
}
// Auto is on offer: the target sensor is set, or the "from" list has entities (Auto then follows what's playing)
inline bool auto_ok(Screen *s) {
  if (!s->follow_src.empty()) return true;
  if (s->follow_from.empty()) return false;
  const std::string &l = s->params[s->follow_from];
  return l.find('.') != std::string::npos && l[0] != '$';
}
// what Auto follows now: the target sensor's state, else the first entity of the "from" list that is playing; "" =
// nothing to follow (stay where it is)
inline std::string follow_target(Screen *s) {
  if (!s->follow_src.empty()) { const std::string *v = ha::get(s->follow_src); return v ? *v : ""; }
  if (s->follow_from.empty()) return "";
  const std::string l = s->params[s->follow_from];
  for (size_t i = 0; i < l.size();) {
    size_t j = l.find(',', i);
    if (j == std::string::npos) j = l.size();
    std::string e = l.substr(i, j - i);
    while (!e.empty() && e[0] == ' ') e.erase(0, 1);
    while (!e.empty() && e.back() == ' ') e.pop_back();
    if (entity_ok(e)) {
      ha::want(e);
      const std::string *st = ha::get(e);
      if (st && *st == "playing") return e;
    }
    i = j + 1;
  }
  return "";
}
// Auto: the followed entity (the target's state, or the player that plays), if it names one of the parameter's domain
inline void follow_now(Screen *s) {
  if (!s->following || !auto_ok(s)) return;
  const std::string t = follow_target(s);
  const std::string *v = t.empty() ? nullptr : &t;
  if (!v || *v == s->follow_last) return;
  s->follow_last = *v;
  const std::string &cur = s->params[s->follow_param];
  if (!entity_ok(*v) || (*v)[0] == '$' || v->substr(0, v->find('.')) != cur.substr(0, cur.find('.'))) return;
  set_param(s, s->follow_param, "", *v);
}
// one checked action `a` for node n (an event's detail = its value): THE code path of every action, a spec tap (run)
// and act() alike (CONTRACT.md C50). why = nullptr: a spec tap (a refused timer says so in a toast); else act() wants
// the reason a refusal has ("" = done)
inline bool run_action(Screen *s, Node &n, ArduinoJson::JsonVariantConst a, const char *event, const char *detail, std::string *why_out) {
  const std::string d = a["do"] | "";
  auto refuse = [&](const char *w) { if (why_out && why_out->empty()) *why_out = w; return true; };
  if (d == "nav.back") { emit(n.obj, "back"); return true; }
  if (d == "calendar.control") {  // the calendar feed's own state (core/calendar_source.cpp): a tap's event says what
    const std::string v = !strcmp(event, "tap") ? "calendars" : !strcmp(event, "close") ? "close" : detail ? detail : "";
    const char *why = dev::calendar_source::control(v);
    if (*why) return refuse(why);
    return true;
  }
  if (d.rfind("timer.", 0) == 0) {  // the panel's timers (core/device.h): which one = id / name, a chip's detail, the shown
    namespace tm = dev::timers;
    const char *why = "";
    if (d == "timer.start") why = tm::start(a["name"] | "", a["seconds"] | 0);
    else if (d == "timer.new" || d == "timer.back") tm::picking(d == "timer.new");  // + New: the lengths; back: cancel that
    else {
      int id = 0;
      if (a["id"].is<const char *>()) id = tm::find(a["id"] | "");
      else if (a["name"].is<const char *>()) id = tm::find(a["name"] | "");
      else if (!strcmp(event, "select") || !strcmp(event, "stop")) id = tm::find(detail);
      else if (const dev::TimerItem *t = tm::shown()) id = t->id;
      if (!id) return refuse(a["id"].isNull() && a["name"].isNull() ? "no timer" : "no timer by that name");
      if (d == "timer.extend") why = tm::extend(id, a["s"] | 60);
      else if (d == "timer.cancel") why = tm::cancel(id);
      else if (d == "timer.stop") why = tm::stop(id);
      else why = tm::select(id);
    }
    if (*why && why_out) return refuse(why);  // act(): the caller hears why
    if (*why && d == "timer.start") {  // full / a bad length: say so where the person is looking
      ArduinoJson::JsonDocument t;
      t["tone"] = "warn"; t["icon"] = "timer"; t["title"] = "No timer started"; t["body"] = why;
      dev::toast(t.as<Props>());
    }
    return true;
  }
  if (d == "flights.select") {  // core/flights_source.cpp: the lead again (or the hero, no detail) = its detail view
    if (!s) return refuse("a spec screen's action");
    const bool again = dev::flights_source::select(detail);
    const std::string view = s->params["view"];
    const char *to = again ? "detail" : (view == "list" || view == "detail") ? "now" : nullptr;  // a row on the list: back to Now
    if (to && view != to && set_param(s, "view", "", to)) {
      if (again) s->undo.push_back({"view", view});  // Back closes the detail view first
      else s->undo.erase(std::remove_if(s->undo.begin(), s->undo.end(), [](const auto &u) { return u.first == "view"; }), s->undo.end());
      emit(s->root, "param", (std::string("view=") + to).c_str());
    }
    return true;
  }
  if (d == "nav.open") {
    std::string id = a["screen"] | "";
    if (id.empty()) id = detail;  // the component's own "open": its detail is "<screen>[?{params}]"
    if (id.empty() || !id_ok(id.substr(0, id.find('?')))) return refuse("no screen id");
    if (!opens_known(id.substr(0, id.find('?')))) return refuse("no such screen on this panel");  // a screen this build lacks (its feature is off): nothing opens
    if (a["params"].is<ArduinoJson::JsonObjectConst>()) {
      std::string pj;
      serializeJson(a["params"], pj);
      id += "?" + pj;
    }
    if (!strcmp(event, "long")) sound(n.obj, "open");
    emit(n.obj, "navigate", id.c_str());
    return true;
  }
  // an entity that is unavailable / unknown takes no call (BF-049): the component shows it blocked (its unavailable
  // state); this is the net under every HA action, whatever the component does
  static const char *const HA_DOS[] = {"media.", "cover.", "vacuum.", "lock.", "light.", "climate."};  // (light.toggle covers switches)
  if (std::any_of(std::begin(HA_DOS), std::end(HA_DOS), [&](const char *p) { return d.rfind(p, 0) == 0; })) {
    const std::string *es = ha::get(n.entity);
    if (es && (*es == "unavailable" || *es == "unknown")) { emit(n.obj, st::BLOCKED_EVENT, "unavailable"); return refuse("the entity is unavailable"); }
  }
  if (d == "media.control" && n.entity == "panel.media") {  // the media feed (core/media_source.cpp): it shows the tap at once and
    // names the media_player service to call (none on its sample adapter); a tap's event says what
    const std::string ev = event ? event : "", det = detail ? detail : "";
    if (ev == "input") {  // live only under a finger (an update's sweep moves the value too)
      lv_obj_t *f = lv_indev_get_active_obj();
      while (f && f != n.obj) f = lv_obj_get_parent(f);
      if (!f) return true;
    }
    const std::string v = ev == "close" ? "close" : ev == "toggle" ? "play_pause" : ev == "pick" ? "pick " + det
                        : ev == "change" ? "volume " + det : ev == "input" ? "live " + det
                        : n.comp == "picker_row" ? "pick " + det  // a speaker row: its name
                        : ev == "tap" ? (det == "prev" ? "previous" : det == "speaker" ? "speakers" : det) : det;
    std::string svc, ent, data;
    const char *why = dev::media_source::control(v, &svc, &ent, &data);
    if (*why) return refuse(why);
    if (!svc.empty()) ha_act(n, svc.c_str(), ent.c_str(), data.c_str());
    return true;
  }
  if (d == "media.control") {  // previous | play_pause | next | volume 0-100 (the view shows it at once itself)
    static const char *const CMD[][2] = {{"previous", "media_player.media_previous_track"},
                                         {"play_pause", "media_player.media_play_pause"},
                                         {"next", "media_player.media_next_track"}};
    for (auto &c : CMD)
      if (!strcmp(detail, c[0])) { ha_act(n, c[1], n.entity.c_str()); return true; }
    if (!strncmp(detail, "volume ", 7)) {
      char data[40];
      snprintf(data, sizeof data, "{\"volume_level\":%.2f}", LV_CLAMP(0, atoi(detail + 7), 100) / 100.0);
      ha_act(n, "media_player.volume_set", n.entity.c_str(), data);
    }
    return true;
  }
  if (d == "cover.control" && n.entity == "panel.covers") {  // the covers feed (core/covers_source.cpp): it shows the
    // tap at once and names the cover service to call (none on its sample adapter); a tap's event says what
    const std::string ev = event ? event : "", det = detail ? detail : "";
    std::string v = ev == "close" ? "sheet_close" : n.comp == "picker_row" ? "pick " + det : n.comp == "icon_button" ? "windows"
                  : n.comp == "slider_row" || n.comp == "slider" ? (ev == "input" ? "live " : "tilt ") + det
                  : n.comp == "cover_visual" ? "position " + det : det;
    for (char &ch : v) ch = (char) tolower((unsigned char) ch);
    if (n.comp == "picker_row") v = "pick " + det;  // the name as it is
    std::string svc, ent, data;
    const char *why = dev::covers_source::control(v, &svc, &ent, &data);
    if (*why) return refuse(why);
    if (!svc.empty()) ha_act(n, svc.c_str(), ent.c_str(), data.c_str());
    return true;
  }
  if (d == "cover.control") {  // position N | tilt N | open | stop | close (the view shows it at once itself)
    char data[40];
    if (!strncmp(detail, "position ", 9)) {
      snprintf(data, sizeof data, "{\"position\":%d}", (int) LV_CLAMP(0, atoi(detail + 9), 100));
      ha_act(n, "cover.set_cover_position", n.entity.c_str(), data);
    } else if (!strncmp(detail, "tilt ", 5)) {
      snprintf(data, sizeof data, "{\"tilt_position\":%d}", (int) LV_CLAMP(0, atoi(detail + 5), 100));
      ha_act(n, "cover.set_cover_tilt_position", n.entity.c_str(), data);
    } else if (!strcmp(detail, "open")) ha_act(n, "cover.open_cover", n.entity.c_str());
    else if (!strcmp(detail, "close")) ha_act(n, "cover.close_cover", n.entity.c_str());
    else if (!strcmp(detail, "stop")) ha_act(n, "cover.stop_cover", n.entity.c_str());
    return true;
  }
  if (d == "vacuum.control") {  // a Button's label (Clean / Start / Resume, Pause, Dock; any case) or a Segmented's
    std::string w = detail;      // "change" = the suction (its option id = the fan speed)
    for (char &ch : w) ch = (char) tolower((unsigned char) ch);
    if (!strcmp(event, "change")) {
      if (w.empty() || w.size() > 32) return true;
      const size_t sp = w.find(' ');
      if (sp != std::string::npos) {  // a Vacuum mode change: "fan turbo", "mode mop", "mop high"
        const std::string key = w.substr(0, sp), opt = detail + sp + 1;
        if (key == "mode" || key == "mop") {
          const std::string sel = a[key.c_str()] | "";
          if (sel.empty() || sel[0] == '$') return true;
          ArduinoJson::JsonDocument od;
          od["option"] = opt;
          std::string data;
          serializeJson(od, data);
          ha_act(n, "select.select_option", sel.c_str(), data.c_str());
          return true;
        }
        if (key != "fan") return true;
        w = w.substr(sp + 1);
      }
      ArduinoJson::JsonDocument fd;
      fd["fan_speed"] = w;
      std::string data;
      serializeJson(fd, data);
      ha_act(n, "vacuum.set_fan_speed", n.entity.c_str(), data.c_str());
      return true;
    }
    const std::string rooms = a["rooms"] | "";
    if (w.rfind("clean", 0) == 0 || w == "start") {  // Clean all / Clean Kitchen / Clean 2 rooms
      std::string segs;  // "1,3" or "Kitchen:3,Corridor:1" -> [3,1] (the id after an item's last colon)
      for (size_t i = 0; i < rooms.size();) {
        size_t j = rooms.find(',', i);
        if (j == std::string::npos) j = rooms.size();
        std::string it = rooms.substr(i, j - i);
        const size_t c = it.rfind(':');
        if (c != std::string::npos) it = it.substr(c + 1);
        const char *q = it.c_str();
        while (*q && !isdigit((unsigned char) *q)) q++;
        if (*q) segs += (segs.empty() ? "" : ",") + std::to_string(atoi(q));
        i = j + 1;
      }
      if (segs.empty()) { ha_act(n, "vacuum.start", n.entity.c_str()); return true; }
      const std::string data = "{\"command\":\"app_segment_clean\",\"params\":[{\"segments\":[" + segs + "],\"repeat\":1}]}";
      ha_act(n, "vacuum.send_command", n.entity.c_str(), data.c_str());
    } else if (w == "resume") ha_act(n, "vacuum.start", n.entity.c_str());
    else if (w == "pause") ha_act(n, "vacuum.pause", n.entity.c_str());
    else if (w == "dock") ha_act(n, "vacuum.return_to_base", n.entity.c_str());
    return true;
  }
  if (d == "screen.toggle") {  // the detail in / out of the comma list parameter, then every node bound again
    const std::string param = a["param"] | "", item = detail;
    auto it = s->params.find(param);
    if (it == s->params.end() || item.empty() || item.size() > 24 || item.find(',') != std::string::npos) return true;
    std::vector<std::string> l;
    bool had = false;
    for (size_t i = 0; i < it->second.size();) {
      size_t j = it->second.find(',', i);
      if (j == std::string::npos) j = it->second.size();
      const std::string x = it->second.substr(i, j - i);
      if (x == item) had = true;
      else if (!x.empty()) l.push_back(x);
      i = j + 1;
    }
    if (!had && (int) l.size() < EACH_MAX) l.push_back(item);
    std::string v;
    for (const std::string &x : l) v += (v.empty() ? "" : ",") + x;
    it->second = v;
    rebind_all(s);
    emit(s->root, "param", (param + "=" + v).c_str());  // the host sees the picks
    return true;
  }
  if (d == "lock.control") {  // lock | unlock | open: the view sends unlock / open only after its hold (the confirm step)
    if (!strcmp(detail, "lock")) ha_act(n, "lock.lock", n.entity.c_str());
    else if (!strcmp(detail, "unlock")) ha_act(n, "lock.unlock", n.entity.c_str());
    else if (!strcmp(detail, "open")) ha_act(n, "lock.open", n.entity.c_str());
    return true;
  }
  if (d == "screen.param") {
    if (auto_ok(s) && !strcmp(detail, "auto")) {  // Auto again: follow the target entity (or what's playing)
      s->following = true;
      s->params["mode"] = "auto";
      s->follow_last.clear();
      const std::string t = follow_target(s);
      if (t.empty() || !set_param(s, s->follow_param, "", t)) rebind_all(s);
      if (!t.empty()) s->follow_last = t;
      ArduinoJson::JsonDocument kv;
      kv.set("auto");
      deck_set(s->id, "params." + s->follow_param, kv.as<ArduinoJson::JsonVariantConst>());  // Auto sticks
      return true;
    }
    if (auto_ok(s)) {  // a real pick: manual until Auto is picked again
      const bool was = s->following;
      s->following = false;
      s->params["mode"] = "manual";
      if (!set_param(s, a["param"] | "", a["from"] | "", detail)) { s->following = was; s->params["mode"] = was ? "auto" : "manual"; rebind_all(s); return true; }
      emit(s->root, "param", (std::string(a["param"] | "") + "=" + detail).c_str());  // the host sees the pick
      ArduinoJson::JsonDocument kv;
      kv.set(detail);
      deck_set(s->id, "params." + s->follow_param, kv.as<ArduinoJson::JsonVariantConst>());  // the pick sticks
      return true;
    }
    const char *v = a["to"].is<const char *>() ? a["to"].as<const char *>() : detail;  // a fixed value (a view)
    const std::string param = a["param"] | "";
    auto cur = s->params.find(param);
    const std::string before = cur != s->params.end() ? cur->second : "";
    if (set_param(s, param, a["from"] | "", v)) {
      if ((a["back"] | false) && before != v) s->undo.push_back({param, before});  // Back returns here first
      emit(s->root, "param", (param + "=" + v).c_str());  // the host sees it (a view, a speaker)
    }
    return true;
  }
  if (d == "light.toggle") {  // a light, or a switch (a Home light card takes both)
    const char *svc = n.entity.rfind("switch.", 0) == 0 ? "switch.toggle" : "light.toggle";
    // hold for HA's answer = the other side (facet-181: a bare hold kept HA's answer off the card's state line)
    const std::string *cur = ha::get(n.entity, "");
    const std::string want = cur && *cur == "on" ? "off" : "on";
    if (!*ha_act(n, svc, n.entity.c_str())) ha::hold(n.obj, PENDING_MS, n.entity, "", want);
    return true;
  }
  if (d == "light.switch") {
    const bool v = !strcmp(detail, "on");
    const bool sw = n.entity.rfind("switch.", 0) == 0;  // the light screen takes switch.* too (an on / off dial)
    if (v && n.has_brightness && !sw) { n.want = ON_ONLY; n.sent_at = lv_tick_get(); return true; }  // a drag may follow
    if (!v && n.has_brightness && n.sent == 0) return true;                                   // the released 0 did it
    if (*ha_act(n, sw ? (v ? "switch.turn_on" : "switch.turn_off") : v ? "light.turn_on" : "light.turn_off", n.entity.c_str())) return true;
    if (n.has_brightness) n.sent = v ? -1 : 0;
    n.sent_at = lv_tick_get();
    ha::hold(n.obj, LIGHT_HOLD_MS, n.entity, "", v ? "on" : "off");
    if (!n.pending.empty()) n.pend = v ? 1 : -1;
    return true;
  }
  if (d == "climate.set_temperature") {  // live while dragging: throttle with a trailing send; the release / a step now
    const double v = atof(detail);
    if (!strcmp(event, "input")) {
      ha::hold(n.obj, PENDING_MS);
      if (!std::isfinite(n.c_sent) || lv_tick_elaps(n.c_at) >= CLIMATE_THROTTLE_MS) { n.c_want = NAN; send_temperature(n, v); }
      else n.c_want = v;
      return true;
    }
    n.c_want = NAN;
    send_temperature(n, v);
    char want[24];
    snprintf(want, sizeof want, "%g", climate_clamp(n.entity, v));
    ha::hold(n.obj, PENDING_MS, n.entity, "temperature", want);  // settles when HA reports the set point
    return true;
  }
  if (d == "climate.set_hvac_mode" || d == "climate.set_preset_mode") {
    const bool hvac = d == "climate.set_hvac_mode";
    ArduinoJson::JsonDocument data;
    data[hvac ? "hvac_mode" : "preset_mode"] = detail;
    std::string ds;
    serializeJson(data, ds);
    if (!*ha_act(n, d.c_str(), n.entity.c_str(), ds.c_str()))
      ha::hold(n.obj, PENDING_MS, n.entity, hvac ? "" : "preset_mode", detail);  // the state = the hvac mode in HA
    return true;
  }
  if (d == "light.brightness") {
    const int v = (int) std::lround(atof(detail));
    if (!strcmp(event, "input")) {  // live: throttle with a trailing send
      ha::hold(n.obj, LIGHT_HOLD_MS);
      if (v == 0 && !n.drag_sent) return true;
      n.drag_sent = true;
      if (n.want == ON_ONLY || n.sent < 0 || lv_tick_elaps(n.sent_at) >= LIGHT_THROTTLE_MS) { n.want = -1; send_brightness(n, v); }
      else n.want = v;
      return true;
    }
    n.want = -1;  // released: the final value now
    n.drag_sent = false;
    send_brightness(n, v);
    if (v > 0) ha::hold(n.obj, LIGHT_HOLD_MS, n.entity, "brightness", std::to_string(v), true);
    else ha::hold(n.obj, LIGHT_HOLD_MS, n.entity, "", "off");
    return true;
  }
  return false;
}
// an event from inside node n -> its action (the whitelist was checked when the spec was accepted)
inline bool run(Screen *s, Node &n, const char *event, const char *detail) {
  ArduinoJson::JsonDocument on;
  deserializeJson(on, n.on);
  ArduinoJson::JsonVariantConst a = on[event];
  if (a.isNull()) return false;
  return run_action(s, n, a, event, detail, nullptr);
}
// ---- act (CONTRACT.md C50): ONE action from outside a spec (an agent's POST /fw/act, Home Assistant's facet_act).
// Checked exactly like a spec node's action (check_action with no component: every action of the list), plus its
// "value" against ACTION_DOCS (what a component's event would give), then run through run_action() like a tap. Spec-
// only actions (ACTION_DOCS screen) and $param entities are refused (no screen here). true = done; else why says why.
// state (optional) = what it acted on now: the timers for timer.*, the entity's state, the screen opened.
inline bool value_ok(const char *g, const std::string &v) {
  auto num = [](const std::string &x, double lo, double hi) {
    if (x.empty() || x.size() > 12) return false;
    char *end = nullptr;
    const double d = strtod(x.c_str(), &end);
    return end && !*end && std::isfinite(d) && d >= lo && d <= hi;
  };
  for (const char *p = g; *p;) {
    const char *bar = strchr(p, '|');
    const std::string alt(p, bar ? bar - p : strlen(p));
    p = bar ? bar + 1 : p + alt.size();
    const size_t hash = alt.find('#');
    if (alt[0] == '*') {  // a short name
      const size_t cap = (size_t) atoi(alt.c_str() + 1);
      if (v.empty() || v.size() > cap) continue;
      if (std::all_of(v.begin(), v.end(), [](char c) { return isalnum((unsigned char) c) || c == '_' || c == ' ' || c == '-'; })) return true;
    } else if (hash != std::string::npos) {  // [word ]#lo..hi
      const std::string word = alt.substr(0, hash);
      const double lo = atof(alt.c_str() + hash + 1), hi = atof(alt.c_str() + alt.find("..") + 2);
      if (v.compare(0, word.size(), word) == 0 && num(v.substr(word.size()), lo, hi)) return true;
    } else if (v == alt) return true;
  }
  return false;
}
inline bool act(ArduinoJson::JsonVariantConst a, std::string &why, ArduinoJson::JsonObject state = ArduinoJson::JsonObject()) {
  why.clear();
  Errs e;
  check_action(a, nullptr, "act", "", e);
  if (!e.empty()) { why = e[0].msg; return false; }
  const std::string d = a["do"] | "";
  const ActionDoc *doc = action_doc(d);
  if (doc->screen) { why = d + " needs a spec screen (its parameters): it runs from a spec node's event only"; return false; }
  for (const char *k : {"e", "rooms", "mode", "mop", "screen", "name", "id"})
    if ((a[k] | "")[0] == '$') { why = std::string(k) + ": no screen parameters outside a spec (name it)"; return false; }
  std::string v;
  if (a["value"].is<const char *>()) v = a["value"].as<const char *>();
  else if (a["value"].is<double>()) { char b[24]; snprintf(b, sizeof b, "%g", a["value"].as<double>()); v = b; }
  else if (!a["value"].isNull()) { why = "value: text (or a number)"; return false; }
  if (*doc->value && !value_ok(doc->value, v)) { why = std::string("value: one of ") + doc->value; return false; }
  if (!*doc->value && !a["value"].isNull()) { why = d + " takes no value"; return false; }
  Node n;
  n.obj = dev::first_root();  // the panel's screen stack (nav.*); HA calls emit their "ha" event from it
  n.entity = a["e"] | "";
  if (d.rfind("nav.", 0) == 0 && !n.obj) { why = "no panel screen is shown"; return false; }
  act_why().clear();
  run_action(nullptr, n, a, "act", v.c_str(), &why);
  if (why.empty()) why = act_why();
  if (!state.isNull()) {
    if (d.rfind("timer.", 0) == 0) {
      ArduinoJson::JsonArray ts = state["timers"].to<ArduinoJson::JsonArray>();
      for (const dev::TimerItem &t : dev::timers::all()) {
        ArduinoJson::JsonObject j = ts.add<ArduinoJson::JsonObject>();
        j["id"] = std::to_string(t.id);
        j["name"] = t.named ? t.name : dev::timers::length_name(t.total);
        j["left"] = dev::timers::left(t);
        j["total"] = t.total;
        j["state"] = t.state == 1 ? "running" : "done";
      }
      if (const dev::TimerItem *t = dev::timers::shown()) state["shown"] = std::to_string(t->id);
    } else if (!n.entity.empty()) {
      state["entity"] = n.entity;
      if (const std::string *st = ha::get(n.entity)) state["state"] = *st;
    } else if (d == "nav.open") state["screen"] = a["screen"] | "";
  }
  return why.empty();
}
inline bool act(const std::string &json, std::string &why, ArduinoJson::JsonObject state = ArduinoJson::JsonObject()) {
  ArduinoJson::JsonDocument a;
  if (json.size() > 1024 || deserializeJson(a, json)) { why = "not JSON (an action is an object {\"do\": ..., ...}, at most 1 KB)"; return false; }
  return act(a.as<ArduinoJson::JsonVariantConst>(), why, state);
}
// every action of the list with its fields, value, example (GET /fw/act, the catalogue's "act", the agent docs)
inline void actions_json(ArduinoJson::JsonArray out) {
  for (const ActionDoc &x : ACTION_DOCS) {
    ArduinoJson::JsonObject o = out.add<ArduinoJson::JsonObject>();
    o["do"] = x.id;
    o["about"] = x.about;
    ArduinoJson::JsonArray fs = o["fields"].to<ArduinoJson::JsonArray>();
    for (const char *p = x.fields; *p;) {
      const char *sc = strchr(p, ';');
      const std::string f(p, sc ? sc - p : strlen(p));
      p = sc ? sc + 1 : p + f.size();
      ArduinoJson::JsonObject fo = fs.add<ArduinoJson::JsonObject>();
      const char *keys[] = {"name", "type", "need", "limits"};
      size_t i = 0;
      for (const char *k : keys) {
        const size_t j = f.find('|', i);
        fo[k] = f.substr(i, j == std::string::npos ? std::string::npos : j - i);
        if (j == std::string::npos) break;
        i = j + 1;
      }
    }
    if (*x.value) {
      ArduinoJson::JsonObject fo = fs.add<ArduinoJson::JsonObject>();
      fo["name"] = "value"; fo["type"] = "text"; fo["need"] = "required";
      fo["limits"] = x.value;
    }
    if (x.screen) o["spec_only"] = true;
    ArduinoJson::JsonDocument ex;
    deserializeJson(ex, x.example);
    o["example"] = ex;
  }
}
inline lv_obj_t *box(lv_obj_t *parent) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  return o;
}
// build screen `id` (its live spec) into parent with the parameters it was opened with; returns its root or nullptr
// the header row of the spec screen being built (nullptr outside a build): a node may put its own controls in it,
// before the screen chrome, while it builds (docs header({right: [...]}): the Calendar's Agenda | Month + calendars
// button). Only during its build; the header is the screen's, so they go with the screen.
// the Header's slot "title" | "end" (components/header.cpp SLOTS; nullptr if `header` is not a Header or no such slot)
lv_obj_t *header_slot(lv_obj_t *header, const char *name);
int header_tail(lv_obj_t *end);       // how many of the end slot's last items the Header built itself (Home's settings ...)
void header_hide_time(lv_obj_t *o);   // a control takes the time's place (o = the Header or its end slot)
// the Header itself while a spec screen builds (building_header() = its END slot, where C++ views put their controls)
inline lv_obj_t *&building_header_root() {
  static lv_obj_t *h = nullptr;
  return h;
}
inline lv_obj_t *&building_header() {
  static lv_obj_t *h = nullptr;
  return h;
}
// `item`, just built into building_header() (the end slot), goes before the Header's own end items (Home's settings
// button, header actions); the time is after the slot anyway
inline void header_place(lv_obj_t *item) {
  lv_obj_t *e = item ? lv_obj_get_parent(item) : nullptr;
  const int n = e ? (int) lv_obj_get_child_count(e) : 0, k = n - 1 - header_tail(e);
  if (k >= 0 && k < n - 1) lv_obj_move_to_index(item, k);
  if (e) lv_obj_remove_flag(e, LV_OBJ_FLAG_HIDDEN);  // an empty end slot is hidden (no gap): it has an item now
}
// a header label's width = its text rounded up to the grid (docs fit8), so what follows it stays on the grid; measured
// from the text (the header is not laid out yet while the screen builds)
inline void header_fit(lv_obj_t *label) {
  lv_point_t sz;
  lv_text_get_size(&sz, lv_label_get_text(label), lv_obj_get_style_text_font(label, LV_PART_MAIN),
                   lv_obj_get_style_text_letter_space(label, LV_PART_MAIN), 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
  lv_obj_set_width(label, (sz.x + sp::GRID - 1) / sp::GRID * sp::GRID);
}
inline lv_obj_t *build(lv_obj_t *parent, const std::string &id, Props opened, std::string *error = nullptr, const char *json = nullptr) {
  if (!json) json = screen_json(id);
  ArduinoJson::JsonDocument doc;
  if (!json || deserializeJson(doc, json)) { if (error) *error = "no spec " + id; return nullptr; }
  auto *s = new Screen;
  s->id = doc["id"] | id.c_str();
  for (ArduinoJson::JsonPairConst kv : doc["params"].as<ArduinoJson::JsonObjectConst>()) s->params[kv.key().c_str()] = param_default(kv.value());
  ArduinoJson::JsonDocument de;  // this panel's picks (the deck entry's params), then what it was opened with
  {
    if (deck_entry(s->id, de))
      for (ArduinoJson::JsonPairConst kv : de["params"].as<ArduinoJson::JsonObjectConst>())
        if (kv.value().is<const char *>()) s->params[kv.key().c_str()] = kv.value().as<std::string>();
  }
  for (ArduinoJson::JsonPairConst kv : opened)
    if (kv.value().is<const char *>()) s->params[kv.key().c_str()] = kv.value().as<std::string>();
  for (ArduinoJson::JsonPairConst kv : doc["feeds"].as<ArduinoJson::JsonObjectConst>()) {  // C51: the feeds' adapters + settings
    ArduinoJson::JsonDocument fc;
    for (ArduinoJson::JsonPairConst f : kv.value().as<ArduinoJson::JsonObjectConst>()) fc[f.key().c_str()] = subst(f.value() | "", s->params);
    std::string cfg;
    serializeJson(fc, cfg);
    ha::set_feed_config(kv.key().c_str(), cfg);
  }
  for (ArduinoJson::JsonVariantConst nd : doc["nodes"].as<ArduinoJson::JsonArrayConst>())  // Auto (screen.param "auto")
    for (ArduinoJson::JsonPairConst kv : nd["on"].as<ArduinoJson::JsonObjectConst>()) {
      if (strcmp(kv.value()["do"] | "", "screen.param") || !kv.value()["auto"].is<const char *>()) continue;
      s->follow_param = kv.value()["param"] | "";
      s->follow_from = kv.value()["from"] | "";
      s->follow_src = s->params[kv.value()["auto"] | ""];
      if (!entity_ok(s->follow_src) || s->follow_src[0] == '$') s->follow_src.clear();
      std::string &fv = s->params[s->follow_param];
      const bool picked = deck_picked(s->id, s->follow_param);  // nothing picked (here, not the factory's) = Auto
      const bool asked = fv == "auto" || !picked;
      if (fv == "auto") fv = param_default(doc["params"][s->follow_param.c_str()]);  // a real one until the target says
      s->following = auto_ok(s) && asked && opened[s->follow_param.c_str()].isNull();  // opened on one: manual
      if (s->following) {
        if (!s->follow_src.empty()) ha::want(s->follow_src);
        const std::string t = follow_target(s);
        const std::string &cur = s->params[s->follow_param];
        if (!t.empty() && entity_ok(t) && t[0] != '$' && t.substr(0, t.find('.')) == cur.substr(0, cur.find('.'))) s->params[s->follow_param] = t;
        if (!t.empty()) s->follow_last = t;
      }
      s->params["mode"] = !auto_ok(s) ? "" : s->following ? "auto" : "manual";
    }
  // "each": a node per item of its list parameter (or per sample entry), "$item" = the item; orig = the node as written
  std::vector<int> orig;
  int n_orig = 0;
  {
    ArduinoJson::JsonDocument ex;
    ArduinoJson::JsonArray arr = ex.to<ArduinoJson::JsonArray>();
    for (ArduinoJson::JsonVariantConst nd : doc["nodes"].as<ArduinoJson::JsonArrayConst>()) {
      const int oi = n_orig++;
      if (nd["each"].isNull()) { arr.add(nd); orig.push_back(oi); continue; }
      std::vector<std::string> items;
      const std::string l = s->params[nd["each"] | ""];
      for (size_t i = 0; i < l.size() && (int) items.size() < EACH_MAX;) {
        size_t j = l.find(',', i);
        if (j == std::string::npos) j = l.size();
        std::string it = l.substr(i, j - i);
        while (!it.empty() && it.front() == ' ') it.erase(0, 1);
        while (!it.empty() && it.back() == ' ') it.pop_back();
        if (!it.empty()) items.push_back(it);
        i = j + 1;
      }
      std::map<std::string, std::string> pm;
      auto one = [&](ArduinoJson::JsonVariantConst over, const std::string &item) {
        ArduinoJson::JsonDocument d1;
        d1.set(nd);
        d1.remove("each");
        d1.remove("sample");
        for (ArduinoJson::JsonPairConst kv : over.as<ArduinoJson::JsonObjectConst>()) d1["p"][kv.key()] = kv.value();
        pm["item"] = item;
        subst_all(d1.as<ArduinoJson::JsonVariant>(), pm);
        arr.add(d1);
        orig.push_back(oi);
      };
      if (!items.empty()) for (const std::string &it : items) one(ArduinoJson::JsonVariantConst(), it);
      else {
        int k = 0;
        for (ArduinoJson::JsonVariantConst sm : nd["sample"].as<ArduinoJson::JsonArrayConst>()) if (k++ < EACH_MAX) one(sm, "");
      }
    }
    doc["nodes"] = ex.as<ArduinoJson::JsonArrayConst>();
  }
  std::vector<lv_obj_t *> orig_obj(n_orig, nullptr);
  ArduinoJson::JsonDocument raw;  // the nodes as written ("$param" kept): screen.param binds them again (kept only then)
  const bool rebinds = strstr(json, "\"screen.param\"") != nullptr || strstr(json, "\"screen.toggle\"") != nullptr;
  if (rebinds) raw.set(doc["nodes"]);
  std::vector<std::pair<std::vector<std::string>, std::map<std::string, std::vector<std::string>>>> anc;  // per node
  for (ArduinoJson::JsonVariantConst nd : doc["nodes"].as<ArduinoJson::JsonArrayConst>()) {
    std::set<std::string> all;
    std::map<std::string, std::vector<std::string>> per;
    for (ArduinoJson::JsonPairConst kv : nd["p"].as<ArduinoJson::JsonObjectConst>()) {
      std::set<std::string> one;
      refs(kv.value(), s->params, one);
      if (!one.empty()) per[kv.key().c_str()] = ordered(one, doc["params"].as<ArduinoJson::JsonObjectConst>());
      all.insert(one.begin(), one.end());
    }
    refs(nd["on"], s->params, all);
    anc.emplace_back(ordered(all, doc["params"].as<ArduinoJson::JsonObjectConst>()), std::move(per));
  }
  subst_all(doc.as<ArduinoJson::JsonVariant>(), s->params);
  const Theme &t = *T;
  s->root = box(parent);
  lv_obj_set_user_data(s->root, s);
  live().push_back(s);
  lv_obj_set_size(s->root, lay::screen_w, lay::screen_h);
  fx::look(lv_obj_get_display(parent), T);  // the contract's shared styles (CONTRACT.md): the screen is a Page
  fx::use(s->root, "bg:bg");
  lv_obj_add_event_cb(s->root, [](lv_event_t *e) {
    auto *s = (Screen *) lv_event_get_user_data(e);
    if (s->timer) lv_timer_delete(s->timer);
    live().erase(std::remove(live().begin(), live().end(), s), live().end());
    delete s;
  }, LV_EVENT_DELETE, s);
  listen(s->root, [](lv_obj_t *from, const char *name, const char *detail, void *user) {
    auto *s = (Screen *) user;
    if (!strcmp(name, "back") && !s->undo.empty()) {  // a view opened with screen.param "back": Back closes it first
      const auto u = s->undo.back();
      s->undo.pop_back();
      s->params[u.first] = u.second;
      rebind_all(s);
      emit(s->root, "param", (u.first + "=" + u.second).c_str());
      return;
    }
    Node *n = node_of(s, from);
    if (n && run(s, *n, name, detail)) return;
    if (lv_obj_t *up = lv_obj_get_parent(s->root)) notify(up, name, detail);  // the Header's back: the device
  }, s);
  // header (the Header component + the screen chrome)
  ArduinoJson::JsonDocument hd;
  // no label key = no title (the Title is a part the editor removes, Darren 2026-10-28); a label left empty by its
  // parameter ("$name" with no name given) = the spec's name, as before
  const char *hl = doc["header"]["label"] | "";
  hd["label"] = doc["header"]["label"].isNull() ? "" : *hl ? hl : (doc["name"] | "");
  hd["back"] = !at_root;  // the renderer owns Back (facet-188): a spec cannot remove it
  hd["settings"] = doc["header"]["settings"] | false;  // the settings button in the chrome (Home)
  hd["clock"] = doc["header"]["clock"] | true;  // false: no time (the Media screen, docs header clock: false)
  if (doc["header"]["actions"].is<ArduinoJson::JsonObjectConst>()) hd["actions"] = doc["header"]["actions"];  // a Home's header actions
  {  // opened for one entity (a Home card's detail): the header names it, the card's label else its friendly name
    const std::string oe = opened["entity"] | "", on = opened["name"] | "";
    if (!oe.empty() && oe[0] != '$' && entity_ok(oe)) {
      if (!on.empty()) hd["label"] = on;
      else { hd["label"].to<ArduinoJson::JsonObject>(); hd["label"]["e"] = oe; hd["label"]["fmt"] = "name"; }
    }
  }
  // Screen > Page > Header slot + Body (fx::page): the inset is padding INSIDE the body, from the structure
  const std::string layout = doc["layout"] | "list";
  const fx::Page pg = fx::page(s->root, layout == "center" ? "center" : layout == "cards" ? "cards" : "list");
  if (lv_obj_t *h = ha::bound(pg.head, "header", hd.as<Props>()))  // a bound label follows the entity's name
  { building_header_root() = h; building_header() = header_slot(h, "end"); }
  lv_obj_t *c = pg.body;
  size_t ni = 0;
  bool prev_row = true;  // the last node placed straight in the list column was a row
  for (ArduinoJson::JsonVariantConst nd : doc["nodes"].as<ArduinoJson::JsonArrayConst>()) {
    ArduinoJson::JsonVariantConst rn = raw[ni++];
    Node n;
    n.comp = nd["c"] | "";
    ArduinoJson::JsonDocument p;
    p.set(nd["p"]);
    if (p.isNull()) p.to<ArduinoJson::JsonObject>();
    if (!nd["show"].isNull()) p[ha::SHOW_KEY] = nd["show"];  // read with the binds, applied as visibility (ha.h)
    if (n.comp == "grid") {  // the grid lays its cells out once: the items template, its static cells' count + spans
      if (!nd["cell"].isNull()) p["_cell"] = nd["cell"];
      if (!nd["p"]["items"].isNull()) p["_items"] = true;
      const int me = ni - 1 < orig.size() ? orig[ni - 1] : -1;
      int cells = 0;
      for (ArduinoJson::JsonVariantConst o : doc["nodes"].as<ArduinoJson::JsonArrayConst>()) {
        if (!o["in"].is<int>() || (o["in"] | -1) != me) continue;
        cells = std::max(cells, (o["slot"] | 0) + 1);
        if (o["span"].is<ArduinoJson::JsonArrayConst>()) p["_spans"][std::to_string(o["slot"] | 0)] = o["span"];
      }
      if (cells) p["_cells"] = cells;
    }
    lv_obj_t *into = c;  // "in": an earlier layout node's slot, the header row or the overlay
    const bool in_ov = nd["in"].is<const char *>() && !strcmp(nd["in"] | "", "overlay");
    if (in_ov) { into = pg.page; p["_overlay"] = true; }  // a sheet over the whole page (C4), never in the Body
    const bool in_hdr = !in_ov && nd["in"].is<const char *>() && building_header();
    const std::string hslot = nd["slot"] | "end";  // "in": "header" with no slot = the end slot, before the chrome
    if (in_hdr) {
      into = header_slot(building_header_root(), hslot.c_str());
      if (!into) into = building_header();
      if (hslot == "title")  // a node in the title REPLACES the Header's own Title (header.label's; max 1: C8a)
        lv_obj_clean(into);
    }
    const int k = nd["in"] | -1;
    if (k >= 0 && k < n_orig && orig_obj[k])
      if (lv_obj_t *sl = slot_of(orig_obj[k], nd["slot"] | 0)) into = sl;
    n.obj = ha::bound(into, n.comp.c_str(), p.as<Props>());
    if (!n.obj) continue;
    if (in_hdr) {
      if (hslot == "end") header_place(n.obj);
      lv_obj_remove_flag(into, LV_OBJ_FLAG_HIDDEN);  // an empty start / end slot is hidden (no gap)
    }
    if (ni - 1 < orig.size()) orig_obj[orig[ni - 1]] = n.obj;
    if (into != c && !in_hdr && !in_ov) {  // a full-width node (list row, section header) in a narrower slot: the slot's width
      const int32_t sw = lv_obj_get_style_width(into, LV_PART_MAIN), w = lv_obj_get_style_width(n.obj, LV_PART_MAIN);
      if (sw > 0 && !LV_COORD_IS_SPEC(sw) && w > sw && !LV_COORD_IS_SPEC(w)) lv_obj_set_width(n.obj, sw);
    }
    const bool after_block = into == c && layout == "list" && lv_obj_get_index(n.obj) > 0 && !prev_row;
    if ((n.comp == "section_header" && lv_obj_get_index(n.obj) > 0 && (into != c || layout == "list")) || after_block)
      lv_obj_set_style_margin_top(n.obj, lay::group, 0);  // more space between groups (Layout rule)
    if (into == c) {  // a row (list_row, plant_row, section_header ...) touches the next; any other block doesn't
      const size_t L = n.comp.size();
      prev_row = n.comp == "section_header" || (L > 4 && n.comp.compare(L - 4, 4, "_row") == 0);
    }
    if (!nd["on"].isNull()) serializeJson(nd["on"], n.on);
    if (!nd["pending"].isNull()) serializeJson(nd["pending"], n.pending);
    if (rebinds && (!rn["p"].isNull() || !rn["show"].isNull())) {
      ArduinoJson::JsonDocument rp;
      rp.set(rn["p"]);
      if (!rn["show"].isNull()) rp[ha::SHOW_KEY] = rn["show"];
      serializeJson(rp, n.raw_p);
    }
    if (rebinds && !rn["on"].isNull()) serializeJson(rn["on"], n.raw_on);
    if (ni - 1 < anc.size()) { n.params = anc[ni - 1].first; n.prop_params = anc[ni - 1].second; }
    node_actions(n);
    if (!nd["on"]["long"].isNull())
      lv_obj_add_event_cb(n.obj, [](lv_event_t *e) {  // a long press (no tap after it)
        auto *o = (lv_obj_t *) lv_event_get_current_target(e);
        if (lv_obj_has_state(o, st::OFF)) return;
        if (lv_indev_t *in = lv_indev_active()) lv_indev_wait_release(in);
        lv_obj_remove_state(o, LV_STATE_PRESSED);
        notify(o, "long", "");
      }, LV_EVENT_LONG_PRESSED, nullptr);
    s->nodes.push_back(std::move(n));
  }
  building_header() = nullptr;
  building_header_root() = nullptr;
  s->timer = lv_timer_create(tick, 20, s);
  return s->root;
}

// every concrete entity the specs bind (the deck's union, subscribed at boot; "$param" binds come when opened)
// only the panel's real deck (facet-169): a Home's cards, a page's own params ("$name" -> the page's value; no value =
// the spec default = a demo id, skipped) and the literal ids of a SENT spec; a factory spec's literal ids are demo
// entities (light.living_room_ceiling ...), so the panel never subscribes to them at boot (a page still wants what it
// binds once it is built)
inline void want_all() {
  auto scan = [](const char *json, ArduinoJson::JsonObjectConst params, bool literal) {
    ArduinoJson::JsonDocument d;
    if (!json || deserializeJson(d, json)) return;
    for (ArduinoJson::JsonVariantConst n : d["nodes"].as<ArduinoJson::JsonArrayConst>())
      for (ArduinoJson::JsonPairConst kv : n["p"].as<ArduinoJson::JsonObjectConst>()) {
        std::string e = kv.value()["e"] | "";
        if (e.empty()) continue;
        if (e[0] == '$') e = params[e.substr(1)] | "";
        else if (!literal) continue;
        if (e.empty() || e.find('.') == std::string::npos) continue;
        const std::string fmt = kv.value()["fmt"] | "", a = kv.value()["a"] | "";
        size_t from = 0;  // an "entities" param = a comma list
        while (from <= e.size()) {
          size_t to = e.find(',', from);
          if (to == std::string::npos) to = e.size();
          std::string one = e.substr(from, to - from);
          while (!one.empty() && one[0] == ' ') one.erase(0, 1);
          while (!one.empty() && one.back() == ' ') one.pop_back();
          from = to + 1;
          if (one.find('.') == std::string::npos) continue;
          ha::want(one, "");
          if (fmt == "pct" || fmt == "light") ha::want(one, a.empty() ? "brightness" : a);
          else if (fmt == "name") ha::want(one, "friendly_name");
          else if (!a.empty()) ha::want(one, a);
        }
      }
  };
  ArduinoJson::JsonDocument deck;
  deserializeJson(deck, deck_json());
  for (ArduinoJson::JsonVariantConst x : deck["pages"].as<ArduinoJson::JsonArrayConst>()) {
    const std::string kind = x["kind"] | "";
    scan(screen_json(kind), x["params"].as<ArduinoJson::JsonObjectConst>(), S().over.count(kind) > 0);
    for (ArduinoJson::JsonVariantConst c : x["cards"].as<ArduinoJson::JsonArrayConst>())  // a Home's cards
      if (const char *e = c["e"]) { ha::want(e, ""); ha::want(e, "friendly_name"); }
  }
}

// the spec as one canonical string (parsed and written again: the sender's spacing doesn't count), its CRC
inline uint32_t canonical(const std::string &json) {
  ArduinoJson::JsonDocument d;
  if (deserializeJson(d, json)) return crc32(json);
  std::string s;
  serializeJson(d, s);
  return crc32(s);
}
// ---- apply (atomic): validate fully, build once off screen, then store (last good first) and swap
using Reload = void (*)(const char *id);  // the host rebuilds what shows id ("deck" = Home and the stack)
inline Reload &reload_hook() {
  static Reload r = nullptr;
  return r;
}
// out: {ok, status: applied|unchanged|refused, id, rev, errors?}; dry = a hidden parent for the off-screen build
// kind "check" / "check_deck" (facet-163, agents): validate (+ the off-screen build for a screen) and answer, store
// nothing: {ok, status: valid|refused, id, rev, errors?}
// built-in screens (Darren 2026-10-29, facet-209): a sent screen may be anything an agent wants, under its own id; a
// factory id is refused (built_in: a built-in only takes a theme). kind "screen_dev" / "check_dev" = the hub's own dev
// path (Send specs, the house's copies, the tests), which may still replace a built-in without a firmware build.
inline void no_built_in(const std::string &id, bool dev, Errs &e) {
  if (!dev && !id.empty() && factory(id)) err(e, ".id", "built_in", "a built-in screen can't be replaced: send yours under its own id (built-ins only take a theme)");
}
inline bool apply(const char *kind, const std::string &json, lv_obj_t *dry, ArduinoJson::JsonObject out, bool quiet = false) {
  const bool only_check = !strcmp(kind, "check") || !strcmp(kind, "check_deck") || !strcmp(kind, "check_dev");
  const bool deck = !strcmp(kind, "deck") || !strcmp(kind, "check_deck");
  const bool dev = !strcmp(kind, "screen_dev") || !strcmp(kind, "check_dev");
  ArduinoJson::JsonDocument d;
  Errs e;
  if (deserializeJson(d, json)) err(e, "", "bad_json", "not JSON");
  else if (deck) check_deck(d.as<ArduinoJson::JsonVariantConst>(), json.size(), e);
  else { check_screen(d.as<ArduinoJson::JsonVariantConst>(), json.size(), e); no_built_in(d["id"] | "", dev, e); }
  const std::string id = deck ? "deck" : (d["id"] | "");
  out["id"] = id;
  out["rev"] = d["rev"] | -1;
  if (!e.empty()) {
    out["ok"] = false;
    out["status"] = "refused";
    ArduinoJson::JsonDocument ed;
    deserializeJson(ed, errs_json(e));
    out["errors"] = ed;
    return false;
  }
  const char *live = deck ? (S().deck_over.empty() ? nullptr : S().deck_over.c_str()) : (S().over.count(id) ? S().over[id].c_str() : nullptr);
  if (!live) live = deck ? FACTORY_DECK : factory(id);
  if (!only_check && live && canonical(live) == canonical(json)) {  // the same content (Darren 2026-10-23: not the rev) = a no-op
    out["ok"] = true;
    out["status"] = "unchanged";
    out["why"] = "the panel already has this exact spec";
    return true;
  }
  if (!deck && dry) {  // built once off screen: a spec that can't build never goes live
    std::string be;
    ArduinoJson::JsonDocument none;
    none.to<ArduinoJson::JsonObject>();
    lv_obj_t *r = build(dry, id, none.as<Props>(), &be, json.c_str());
    if (!r) { out["ok"] = false; out["status"] = "refused"; out["errors"][0]["code"] = "build"; out["errors"][0]["msg"] = be; return false; }
    lv_obj_update_layout(r);
    lv_obj_delete(r);
  }
  if (only_check) { out["ok"] = true; out["status"] = "valid"; return true; }
  Store &s = S();
  bool ok = true;
  if (s.io.save) {
    std::string cur;
    if (s.io.load && s.io.load("s." + id, cur)) ok = s.io.save("g." + id, cur);  // the previous copy = last good
    ok = ok && s.io.save("s." + id, record(json));
    std::vector<std::string> idx = index_ids();
    if (ok && std::find(idx.begin(), idx.end(), id) == idx.end()) { idx.push_back(id); ok = save_index(idx); }
  }
  if (!ok) { out["ok"] = false; out["status"] = "storage"; return false; }
  if (deck) s.deck_over = json;
  else s.over[id] = json;
  want_all();
  if (reload_hook() && !quiet) reload_hook()(id.c_str());
  out["ok"] = true;
  out["status"] = "applied";
  return true;
}
// this panel's pick for page id (Settings > Screens; a page id, else the first page of that kind): key "source" /
// "params.<name>" = value; "on" = false takes the page out of the deck (never the start page), true adds a page of
// kind id at the end. Kept as the deck (a new rev, validated and stored like a sent one) without rebuilding what
// shows; false = refused
inline bool deck_set(const std::string &id, const std::string &key, ArduinoJson::JsonVariantConst value) {
  ArduinoJson::JsonDocument d;
  if (deserializeJson(d, deck_json())) return false;
  ArduinoJson::JsonArray ps = d["pages"].as<ArduinoJson::JsonArray>();
  int at = -1, i = 0;
  for (ArduinoJson::JsonVariant x : ps) { if (id == (x["id"] | "")) { at = i; break; } i++; }
  if (at < 0) { i = 0; for (ArduinoJson::JsonVariant x : ps) { if (id == (x["kind"] | "")) { at = i; break; } i++; } }
  if (key == "on") {
    if (value.as<bool>()) {
      if (at >= 0 || !page_role(id)) return at >= 0;
      ArduinoJson::JsonObject n = ps.add<ArduinoJson::JsonObject>();
      n["id"] = id, n["kind"] = id;
    } else {
      if (at < 0) return true;
      if (id == (d["start"] | "") || (ps[at]["id"] | "") == std::string(d["start"] | "")) return false;
      ps.remove(at);
    }
  } else {
    if (at < 0) return false;
    if (key.rfind("params.", 0) == 0) ps[at]["params"][key.substr(7)] = value;
    else ps[at][key] = value;
  }
  d["rev"] = (d["rev"] | 0) + 1;
  std::string js;
  serializeJson(d, js);
  ArduinoJson::JsonDocument o;
  return apply("deck", js, nullptr, o.to<ArduinoJson::JsonObject>(), true);
}
// revert one override (screen id, "deck") or "all" to factory
inline void revert(const std::string &id, ArduinoJson::JsonObject out) {
  Store &s = S();
  std::vector<std::string> idx = index_ids(), keep;
  ArduinoJson::JsonArray done = out["reverted"].to<ArduinoJson::JsonArray>();
  for (const std::string &x : idx) {
    if (id != "all" && x != id) { keep.push_back(x); continue; }
    if (s.io.erase) { s.io.erase("s." + x); s.io.erase("g." + x); }
    if (x == "deck") s.deck_over.clear();
    else s.over.erase(x);
    done.add(x);
  }
  if (keep.size() != idx.size()) save_index(keep);
  out["ok"] = true;
  if (reload_hook()) reload_hook()("deck");  // rebuild what shows (a reverted screen gone from the panel leaves the stack)
}

// ---- catalogue (components, props, binds, actions; one source for the hub, the schema and validation)
inline void catalogue(ArduinoJson::JsonObject out) {
  out["protocol"] = PROTOCOL;
  ArduinoJson::JsonObject comps = out["components"].to<ArduinoJson::JsonObject>();
  for (const Component &c : reg()) {
    if (strcmp(c.section, "components")) continue;
    ArduinoJson::JsonObject o = comps[c.id].to<ArduinoJson::JsonObject>();
    o["name"] = c.name;
    o["update"] = c.update != nullptr;
    ArduinoJson::JsonArray ps = o["props"].to<ArduinoJson::JsonArray>();
    for (const PropInfo &pi : *props_of(c.id)) {
      ArduinoJson::JsonObject p = ps.add<ArduinoJson::JsonObject>();
      p["name"] = pi.name;
      p["type"] = pi.type;
      p["bindable"] = true;
      if (!pi.options.empty()) { auto a = p["options"].to<ArduinoJson::JsonArray>(); for (auto &x : pi.options) a.add(x); }
      if (pi.type == "range") { p["min"] = pi.min; p["max"] = pi.max; }
    }
    ArduinoJson::JsonObject ev = o["events"].to<ArduinoJson::JsonObject>();
    for (const Rule &r : RULES)
      if (!strcmp(r.comp, c.id) || !strcmp(r.comp, "*")) {
        auto a = ev[r.event].to<ArduinoJson::JsonArray>();
        for (const char *p = r.actions; *p;) { const char *e = strchr(p, ','); a.add(std::string(p, e ? e - p : strlen(p))); if (!e) break; p = e + 1; }
      }
  }
  auto acts = out["actions"].to<ArduinoJson::JsonArray>();
  for (const char *a : ACTIONS) acts.add(a);
  actions_json(out["act"].to<ArduinoJson::JsonArray>());  // the documented list (C50): /fw/act, the agent docs
  auto fmts = out["fmt"].to<ArduinoJson::JsonArray>();
  for (const char *f : FMTS) if (*f) fmts.add(f);
  auto lays = out["layouts"].to<ArduinoJson::JsonArray>();
  for (const char *l : LAYOUTS) lays.add(l);
  ArduinoJson::JsonObject b = out["budget"].to<ArduinoJson::JsonObject>();
  b["nodes"] = budget().nodes; b["binds"] = budget().binds; b["bytes"] = budget().bytes;
  b["screens"] = budget().screens; b["deck_bytes"] = budget().deck_bytes;
  b["pages"] = budget().pages; b["homes"] = budget().homes; b["cards"] = budget().cards;
  out["deck_v"] = DECK_V;
  auto rl = out["roles"].to<ArduinoJson::JsonArray>();
  for (const char *r : ROLES) rl.add(r);
  ArduinoJson::JsonObject ct = out["cards"].to<ArduinoJson::JsonObject>();
  for (const CardType &c : CARD_TYPES) { auto o = ct[c.t].to<ArduinoJson::JsonObject>(); o["domains"] = c.domains; o["sizes"] = c.sizes; o["taps"] = c.taps; o["detail"] = c.detail;
    auto cm = o["component"].to<ArduinoJson::JsonObject>(); if (*c.comp_s) cm["s"] = c.comp_s; if (*c.comp_m) cm["m"] = c.comp_m; if (*c.comp_l) cm["l"] = c.comp_l; }
}
inline uint32_t catalogue_hash() {
  static uint32_t h = 0;
  if (h) return h;
  ArduinoJson::JsonDocument d;
  catalogue(d.to<ArduinoJson::JsonObject>());
  std::string s;
  serializeJson(d, s);
  return h = crc32(s);
}
// the capability hello (facet_hello): protocol, catalogue hash, components, budgets, overrides, deck rev
inline void hello(ArduinoJson::JsonObject out) {
  out["protocol"] = PROTOCOL;
  char h[12];
  snprintf(h, sizeof h, "%08x", (unsigned) catalogue_hash());
  out["catalogue"] = h;
  auto comps = out["components"].to<ArduinoJson::JsonArray>();
  for (const Component &c : reg()) if (!strcmp(c.section, "components")) comps.add(c.id);
  auto scr = out["screens"].to<ArduinoJson::JsonObject>();
  for (const auto &f : FACTORY) { auto o = scr[f.id].to<ArduinoJson::JsonObject>(); o["rev"] = rev_of(screen_json(f.id)); o["override"] = S().over.count(f.id) > 0; }
  for (const auto &kv : S().over) if (!factory(kv.first)) { auto o = scr[kv.first].to<ArduinoJson::JsonObject>(); o["rev"] = rev_of(kv.second.c_str()); o["override"] = true; o["sent"] = true; ArduinoJson::JsonDocument sd; if (!deserializeJson(sd, kv.second)) { o["name"] = sd["name"] | kv.first.c_str(); o["role"] = sd["role"] | "page"; } }
  out["deck_rev"] = rev_of(deck_json());
  out["deck_override"] = !S().deck_over.empty();
  ArduinoJson::JsonObject b = out["budget"].to<ArduinoJson::JsonObject>();
  b["nodes"] = budget().nodes; b["binds"] = budget().binds; b["bytes"] = budget().bytes; b["screens"] = budget().screens;
  auto lg = out["log"].to<ArduinoJson::JsonArray>();
  for (const auto &x : S().log) lg.add(x);
}
}  // namespace spec
}  // namespace fw
