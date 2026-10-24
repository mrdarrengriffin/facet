// Screens from specs (one-renderer plan §2.1-2.5, v1 2026-10-23, WIP): a screen is a JSON spec built from the
// registered components (fw::find), the same code on the panels and in the WASM workbench. Header-only (core/*.h is
// bundled as is). Specs never carry positions, sizes, colours, fonts or sounds: the layout and the components decide.
//
// Screen spec  {"v": 1, "id": "office", "name": "Office", "rev": 3,
//               "params": {"entity": "light.x", ...},     the screen's parameters + defaults; "$entity" anywhere
//                                                          in a string = the value it was opened with. Typed
//                                                          (2026-10-23): {"type": text|select|entity|entities|
//                                                          toggle|hidden, "default", "label", "domain", "options"}:
//                                                          Settings > Screens and the workbench build their controls
//                                                          from it (entity / entities = one / a comma list of
//                                                          `domain` ids, filled by the host's discovery)
//               "header": {"label": "Office", "back": true},
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
//   in / slot  (2026-10-23, pages from components only) build this node into slot n (default 0) of node k (an earlier
//              node whose component is a LAYOUT: its meta says "slots": N and it marks its slot objects with
//              fw::spec::SLOT, in order) instead of the screen's content box: Split = two columns, Group = a wrapping
//              row. k counts the nodes as written; a layout node can't repeat (each).
//   each       repeat the node once per item of the comma list parameter (at most EACH_MAX), "$item" in its props /
//              actions = the item. With the list empty, once per "sample" entry (its props over "p": the factory
//              spec's demo, so a screen shows something before a host fills the list). Expanded when the screen opens
//              (a later screen.param doesn't add or drop copies).
//   spacing    in the list layout and in a layout's column slots, a section_header that isn't first gets lay::group
//              above it (Layout rule "more space between groups"); everything else touches (rows have hairlines).
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
//              {"do": "cover.control", "e"}  (a Covers view's "cover": position N | tilt N | open | stop | close ->
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
//                pick of "auto" starts following again; the screen's "$mode" = auto | manual | "" (no Auto))
//              {"do": "nav.open", "screen": "<id>", "params"?: {k: v}}    {"do": "nav.back"}
//              on "open" (any component's own open event): {"do": "nav.open"} = the screen its detail names
//              The closed whitelist: each component + event allows only the actions in RULES (the catalogue).
//   pending    while a light.switch waits for HA: the prop shows on / off text (a List row's sub "Turning on")
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
//              type's first by default), page = a page id (t page), label (else the entity's name). start = the
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
#include <cmath>
#include <cstdint>
#include <cstring>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "fw.h"
#include "ha.h"
#include "specs_gen.h"

namespace fw {
namespace spec {
constexpr int PROTOCOL = 1;
constexpr int DECK_V = 2;  // the deck's own version (v1 decks are refused)
struct Budget {
  int nodes = 48, binds = 64, bytes = 8192, screens = 16, deck_bytes = 8192, pages = 16, homes = 4, cards = 24;  // start values (screen model): the bench sets them
};
inline const Budget &budget() {
  static Budget b;
  return b;
}
constexpr uint32_t LIGHT_THROTTLE_MS = 250;  // light-throttle-ms: live brightness at most this often while dragging
constexpr uint32_t PENDING_MS = 5000;        // a switch / brightness waits this long for HA before showing HA's state

// ---- the action whitelist per component and event (the catalogue's "actions")
struct Rule { const char *comp, *event, *actions; };
constexpr Rule RULES[] = {
    {"list_row", "toggle", "light.switch"},          {"list_row", "tap", "nav.open,nav.back,light.toggle"},
    {"list_row", "long", "nav.open"},                {"card", "tap", "nav.open,light.toggle"},
    {"card", "long", "nav.open"},                    {"dial", "change", "light.brightness"},
    {"dial", "input", "light.brightness"},           {"dial", "toggle", "light.switch"},
    {"slider", "change", "light.brightness"},        {"slider", "input", "light.brightness"},
    {"toggle", "toggle", "light.switch"},
    {"more_row", "tap", "nav.open,nav.back"},
    {"thermostat_dial", "change", "climate.set_temperature"}, {"thermostat_dial", "input", "climate.set_temperature"},
    {"thermostat_view", "target", "climate.set_temperature"}, {"thermostat_view", "mode", "climate.set_hvac_mode"},
    {"thermostat_view", "preset", "climate.set_preset_mode"},
    {"media_view", "media", "media.control"},        {"media_view", "speaker", "screen.param"},
    {"covers_view", "cover", "cover.control"},       {"covers_view", "window", "screen.param"},
    {"lock_ring", "lock", "lock.control"},           {"hold_button", "confirm", "lock.control"},
    {"room_tile", "pick", "screen.toggle"},          {"camera_tile", "tap", "nav.back"},          {"button", "tap", "nav.open,nav.back,light.toggle,vacuum.control"},
    {"segmented", "change", "vacuum.control,screen.param"}, {"iso_floorplan", "pick", "screen.toggle"},
    {"vacuum_mode", "change", "vacuum.control"},        // a link to a sub-view spec (moon -> moon_details -> ...)
    {"*", "open", "nav.open"},                       // any component's own "open" (a tapped calendar day, a PR):
                                                     // its detail = the screen id [+ "?" + params JSON]
};
constexpr const char *ACTIONS[] = {"light.switch", "light.toggle", "light.brightness", "climate.set_temperature",
                                   "climate.set_hvac_mode", "climate.set_preset_mode", "media.control", "cover.control", "lock.control", "vacuum.control", "screen.toggle", "screen.param",
                                   "nav.open", "nav.back"};
constexpr const char *FMTS[] = {"", "on", "pct", "avail", "light", "name", "num", "frac", "count", "time", "day"};
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
// a component's slots (its meta "slots"), 0 = not a layout
inline int slots_of(const char *comp) {
  const Component *c = find(comp);
  if (!c || !c->meta) return 0;
  const char *m = strstr(c->meta, "\"slots\":");
  return m ? atoi(m + 8) : 0;
}
constexpr const char *PARAM_TYPES[] = {"text", "select", "entity", "entities", "toggle", "hidden"};
constexpr const char *ROLES[] = {"page", "detail", "both"};
// Home card types (deck v2; piece 2 moves them to renderer/cards/cards.json): domains its entity may be, sizes,
// taps (the default first)
struct CardType { const char *t, *domains, *sizes, *taps; };
constexpr CardType CARD_TYPES[] = {
    {"light", "light,switch", "s,m", "toggle,detail"},          {"cover", "cover", "s,m", "detail,toggle"},
    {"climate", "climate", "m", "detail"},                      {"media", "media_player", "m,l", "page,detail,toggle"},
    {"sensor", "sensor,binary_sensor", "s,m", "detail,none"},   {"page", "", "s,m,l", "page"}};
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
  static const char *const K[] = {"do", "e", "screen", "params", "param", "from", "auto", "rooms", "mode", "mop"};
  check_keys(a.as<ArduinoJson::JsonObjectConst>(), K, 10, path, e);
  const std::string d = a["do"] | "";
  if (!one_of(ACTIONS, d)) { err(e, path + ".do", "bad_action", "unknown action " + d); return; }
  const char *ok = allowed(comp, event);
  if (!ok || !in_list(ok, d)) err(e, path + ".do", "not_allowed", std::string(comp) + " " + event + " allows " + (ok ? ok : "no action"));
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
    if (!entity_ok(en) || (en[0] != '$' && en.rfind("media_player.", 0) != 0)) err(e, path + ".e", "bad_entity", "a media player (media_player.x or $param)");
  }
  if (d == "cover.control") {
    const std::string en = a["e"] | "";
    if (!entity_ok(en) || (en[0] != '$' && en.rfind("cover.", 0) != 0)) err(e, path + ".e", "bad_entity", "a cover (cover.x or $param)");
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
  if (d == "screen.toggle" && !id_ok(a["param"] | "")) err(e, path + ".param", "bad_action", "param: the screen parameter to toggle the detail in (a-z, 0-9, _)");
  if (d == "lock.control") {
    const std::string en = a["e"] | "";
    if (!entity_ok(en) || (en[0] != '$' && en.rfind("lock.", 0) != 0)) err(e, path + ".e", "bad_entity", "a lock (lock.x or $param)");
  }
  if (d == "screen.param") {
    if (!id_ok(a["param"] | "")) err(e, path + ".param", "bad_action", "param: the screen parameter to set (a-z, 0-9, _)");
    if (!a["from"].isNull() && !id_ok(a["from"] | "")) err(e, path + ".from", "bad_action", "from: the parameter holding the allowed values");
    if (!a["auto"].isNull() && !id_ok(a["auto"] | "")) err(e, path + ".auto", "bad_action", "auto: the parameter naming the entity Auto follows");
  }
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
    for (const char *k : {"s", "else", "tpl"})
      if (!v[k].isNull() && (!v[k].is<const char *>() || strlen(v[k].as<const char *>()) > 64)) err(e, path + "." + k, "bad_bind", std::string(k) + " is text (at most 64)");
    if (!entity_ok(v["e"] | "") && !(!strcmp(v["e"] | "x", "") && v["s"].is<const char *>()))
      err(e, path + ".e", "bad_bind", "a bind needs an entity (domain.object or $param), or e \"\" with s (a screen parameter)");
    if (!one_of(FMTS, v["fmt"] | "")) err(e, path + ".fmt", "bad_bind", "fmt is one of on, pct, avail, light, name, num, frac, count, time, day");
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
  else if (strlen(v.as<const char *>()) > 64) err(e, path, "bad_prop", pi.name + ": text over 64 characters");
}
// a screen spec: errors into e (empty = valid)
inline void check_screen(ArduinoJson::JsonVariantConst s, size_t bytes, Errs &e) {
  if (!s.is<ArduinoJson::JsonObjectConst>()) { err(e, "", "bad_spec", "a screen spec is an object"); return; }
  static const char *const K[] = {"v", "id", "name", "rev", "role", "params", "header", "layout", "nodes"};
  check_keys(s.as<ArduinoJson::JsonObjectConst>(), K, 9, "", e);
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
      static const char *const PK[] = {"type", "default", "label", "domain", "options"};
      check_keys(kv.value().as<ArduinoJson::JsonObjectConst>(), PK, 5, pp, e);
      if (!one_of(PARAM_TYPES, kv.value()["type"] | "")) err(e, pp + ".type", "bad_params", "type is text, select, entity, entities, toggle or hidden");
      if (!kv.value()["default"].isNull() && !kv.value()["default"].is<const char *>()) err(e, pp + ".default", "bad_params", "a parameter default is text");
      if (!strcmp(kv.value()["type"] | "", "select") && !kv.value()["options"].is<ArduinoJson::JsonArrayConst>()) err(e, pp + ".options", "bad_params", "a select lists its options");
    }
  }
  if (!s["header"].isNull()) {
    static const char *const H[] = {"label", "back"};
    if (!s["header"].is<ArduinoJson::JsonObjectConst>()) err(e, ".header", "bad_header", "header is {label, back}");
    else check_keys(s["header"].as<ArduinoJson::JsonObjectConst>(), H, 2, ".header", e);
  }
  ArduinoJson::JsonArrayConst nodes = s["nodes"].as<ArduinoJson::JsonArrayConst>();
  if (nodes.isNull()) { err(e, ".nodes", "bad_nodes", "nodes is a list"); return; }
  if ((int) nodes.size() > budget().nodes) err(e, ".nodes", "over_budget", "more than " + std::to_string(budget().nodes) + " nodes");
  if (!strcmp(s["layout"] | "", "center") && nodes.size() != 1) err(e, ".nodes", "bad_nodes", "a center layout has one node");
  int binds = 0, i = 0;
  for (ArduinoJson::JsonVariantConst n : nodes) {
    const std::string p = ".nodes[" + std::to_string(i++) + "]";
    if (!n.is<ArduinoJson::JsonObjectConst>()) { err(e, p, "bad_node", "a node is an object {c, p, on}"); continue; }
    static const char *const NK[] = {"c", "p", "on", "pending", "in", "slot", "each", "sample", "show"};
    check_keys(n.as<ArduinoJson::JsonObjectConst>(), NK, 9, p, e);
    if (!n["show"].isNull()) {
      if (!ha::is_bind(n["show"])) err(e, p + ".show", "bad_bind", "show is a bind {e, ...}");
      else { PropInfo si; si.name = "show"; si.type = "text"; check_value(si, n["show"], p + ".show", e, binds); }
    }
    const std::string c = n["c"] | "";
    if (n["in"].is<const char *>()) {  // into the header row
      if (strcmp(n["in"] | "", "header")) err(e, p + ".in", "bad_in", "in: \"header\" or an earlier layout node");
      if (!n["slot"].isNull()) err(e, p + ".slot", "bad_in", "the header has no slots");
    } else if (!n["in"].isNull()) {  // into an earlier layout node's slot
      const int k = n["in"] | -1, sl = n["slot"] | 0;
      const std::string kc = k >= 0 && k < i - 1 ? (nodes[k]["c"] | "") : "";
      const int ns = kc.empty() ? 0 : slots_of(kc.c_str());
      if (k < 0 || k >= i - 1 || !ns) err(e, p + ".in", "bad_in", "in: an earlier node that is a layout (has slots)");
      else if (!nodes[k]["each"].isNull()) err(e, p + ".in", "bad_in", "a repeated (each) node can't hold others");
      else if (!n["slot"].isNull() && (!n["slot"].is<int>() || sl < 0 || sl >= ns)) err(e, p + ".slot", "bad_in", kc + " has slots 0.." + std::to_string(ns - 1));
    } else if (!n["slot"].isNull()) err(e, p + ".slot", "bad_in", "slot goes with in");
    if (!n["each"].isNull() && !id_ok(n["each"] | "")) err(e, p + ".each", "bad_each", "each: a screen parameter (a comma list)");
    if (!n["sample"].isNull()) {
      if (n["each"].isNull()) err(e, p + ".sample", "bad_each", "sample goes with each");
      else if (!n["sample"].is<ArduinoJson::JsonArrayConst>() || n["sample"].size() > (size_t) EACH_MAX) err(e, p + ".sample", "bad_each", "sample is a list of prop objects, at most " + std::to_string(EACH_MAX));
    }
    const std::vector<PropInfo> *pis = props_of(c.c_str());
    const Component *comp = find(c.c_str());
    if (!pis || !comp || strcmp(comp->section, "components")) { err(e, p + ".c", "unknown_component", "no component " + c + " in this build"); continue; }
    for (ArduinoJson::JsonPairConst kv : n["p"].as<ArduinoJson::JsonObjectConst>()) {
      const std::string k = kv.key().c_str();
      auto it = std::find_if(pis->begin(), pis->end(), [&](const PropInfo &x) { return x.name == k; });
      if (it == pis->end()) { err(e, p + ".p." + k, one_of(STYLE_KEYS, k) ? "style_key" : "unknown_prop", c + " has no prop " + k); continue; }
      check_value(*it, kv.value(), p + ".p." + k, e, binds);
    }
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
    static const char *const SK[] = {"id", "kind", "name", "source", "params", "cards"};
    if (!x.is<ArduinoJson::JsonObjectConst>()) { err(e, p, "bad_deck", "a page is {id, kind, name?, source?, params?, cards?}"); continue; }
    check_keys(x.as<ArduinoJson::JsonObjectConst>(), SK, 6, p, e);
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
inline const char *screen_json(const std::string &id) {
  auto it = S().over.find(id);
  if (it != S().over.end()) return it->second.c_str();
  return factory(id);
}
inline bool screen_known(const std::string &id) { return screen_json(id) || find(id.c_str()); }
inline const char *deck_json() { return S().deck_over.empty() ? FACTORY_DECK : S().deck_over.c_str(); }
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
// this panel's deck page for id into out: the page with that id, else the first page of that kind (a screen opened
// by its spec id finds its page); false = not in the deck
inline bool deck_entry(const std::string &id, ArduinoJson::JsonDocument &out) {
  ArduinoJson::JsonDocument d;
  if (deserializeJson(d, deck_json())) return false;
  ArduinoJson::JsonArrayConst ps = d["pages"].as<ArduinoJson::JsonArrayConst>();
  for (ArduinoJson::JsonVariantConst x : ps) if (id == (x["id"] | "")) { out.set(x); return true; }
  for (ArduinoJson::JsonVariantConst x : ps) if (id == (x["kind"] | "")) { out.set(x); return true; }
  return false;
}
// is this deck page there (deck_entry found one)? A page you don't want is not in the deck.
inline bool deck_on(ArduinoJson::JsonVariantConst entry) { return entry.is<ArduinoJson::JsonObjectConst>(); }
// page kinds this panel could add but has no page of (Settings > Screens and the editor show them as off)
inline std::vector<std::string> off_kinds() {
  ArduinoJson::JsonDocument d;
  deserializeJson(d, deck_json());
  std::set<std::string> used;
  for (ArduinoJson::JsonVariantConst x : d["pages"].as<ArduinoJson::JsonArrayConst>()) used.insert(x["kind"] | "");
  std::vector<std::string> v;
  for (const auto &f : FACTORY) if (!used.count(f.id) && page_role(f.id)) v.push_back(f.id);
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
  std::string follow_param, follow_src, follow_last, id;
  bool following = false;
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
inline void send_brightness(Node &n, int v) {
  if (v == n.sent) return;
  char data[40];
  snprintf(data, sizeof data, "{\"brightness_pct\":%d}", v);
  if (*(v > 0 ? ha::act(n.obj, "light.turn_on", n.entity.c_str(), data) : ha::act(n.obj, "light.turn_off", n.entity.c_str()))) return;
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
  if (*ha::act(n.obj, "climate.set_temperature", n.entity.c_str(), data)) return;
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
      if (!*ha::act(n.obj, "light.turn_on", n.entity.c_str())) { n.sent = -1; n.sent_at = now; }
      ha::hold(n.obj, PENDING_MS, n.entity, "", "on");
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
// Auto: the followed entity's state, if it names an entity of the parameter's domain
inline void follow_now(Screen *s) {
  if (!s->following || s->follow_src.empty()) return;
  const std::string *v = ha::get(s->follow_src);
  if (!v || *v == s->follow_last) return;
  s->follow_last = *v;
  const std::string &cur = s->params[s->follow_param];
  if (!entity_ok(*v) || (*v)[0] == '$' || v->substr(0, v->find('.')) != cur.substr(0, cur.find('.'))) return;
  set_param(s, s->follow_param, "", *v);
}
// an event from inside node n -> its action (the whitelist was checked when the spec was accepted)
inline bool run(Screen *s, Node &n, const char *event, const char *detail) {
  ArduinoJson::JsonDocument on;
  deserializeJson(on, n.on);
  ArduinoJson::JsonVariantConst a = on[event];
  if (a.isNull()) return false;
  const std::string d = a["do"] | "";
  if (d == "nav.back") { emit(n.obj, "back"); return true; }
  if (d == "nav.open") {
    std::string id = a["screen"] | "";
    if (id.empty()) id = detail;  // the component's own "open": its detail is "<screen>[?{params}]"
    if (id.empty() || !id_ok(id.substr(0, id.find('?')))) return true;
    if (a["params"].is<ArduinoJson::JsonObjectConst>()) {
      std::string pj;
      serializeJson(a["params"], pj);
      id += "?" + pj;
    }
    if (!strcmp(event, "long")) sound(n.obj, "open");
    emit(n.obj, "navigate", id.c_str());
    return true;
  }
  if (d == "media.control") {  // previous | play_pause | next | volume 0-100 (the view shows it at once itself)
    static const char *const CMD[][2] = {{"previous", "media_player.media_previous_track"},
                                         {"play_pause", "media_player.media_play_pause"},
                                         {"next", "media_player.media_next_track"}};
    for (auto &c : CMD)
      if (!strcmp(detail, c[0])) { ha::act(n.obj, c[1], n.entity.c_str()); return true; }
    if (!strncmp(detail, "volume ", 7)) {
      char data[40];
      snprintf(data, sizeof data, "{\"volume_level\":%.2f}", LV_CLAMP(0, atoi(detail + 7), 100) / 100.0);
      ha::act(n.obj, "media_player.volume_set", n.entity.c_str(), data);
    }
    return true;
  }
  if (d == "cover.control") {  // position N | tilt N | open | stop | close (the view shows it at once itself)
    char data[40];
    if (!strncmp(detail, "position ", 9)) {
      snprintf(data, sizeof data, "{\"position\":%d}", (int) LV_CLAMP(0, atoi(detail + 9), 100));
      ha::act(n.obj, "cover.set_cover_position", n.entity.c_str(), data);
    } else if (!strncmp(detail, "tilt ", 5)) {
      snprintf(data, sizeof data, "{\"tilt_position\":%d}", (int) LV_CLAMP(0, atoi(detail + 5), 100));
      ha::act(n.obj, "cover.set_cover_tilt_position", n.entity.c_str(), data);
    } else if (!strcmp(detail, "open")) ha::act(n.obj, "cover.open_cover", n.entity.c_str());
    else if (!strcmp(detail, "close")) ha::act(n.obj, "cover.close_cover", n.entity.c_str());
    else if (!strcmp(detail, "stop")) ha::act(n.obj, "cover.stop_cover", n.entity.c_str());
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
          ha::act(n.obj, "select.select_option", sel.c_str(), data.c_str());
          return true;
        }
        if (key != "fan") return true;
        w = w.substr(sp + 1);
      }
      ArduinoJson::JsonDocument fd;
      fd["fan_speed"] = w;
      std::string data;
      serializeJson(fd, data);
      ha::act(n.obj, "vacuum.set_fan_speed", n.entity.c_str(), data.c_str());
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
      if (segs.empty()) { ha::act(n.obj, "vacuum.start", n.entity.c_str()); return true; }
      const std::string data = "{\"command\":\"app_segment_clean\",\"params\":[{\"segments\":[" + segs + "],\"repeat\":1}]}";
      ha::act(n.obj, "vacuum.send_command", n.entity.c_str(), data.c_str());
    } else if (w == "resume") ha::act(n.obj, "vacuum.start", n.entity.c_str());
    else if (w == "pause") ha::act(n.obj, "vacuum.pause", n.entity.c_str());
    else if (w == "dock") ha::act(n.obj, "vacuum.return_to_base", n.entity.c_str());
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
    if (!strcmp(detail, "lock")) ha::act(n.obj, "lock.lock", n.entity.c_str());
    else if (!strcmp(detail, "unlock")) ha::act(n.obj, "lock.unlock", n.entity.c_str());
    else if (!strcmp(detail, "open")) ha::act(n.obj, "lock.open", n.entity.c_str());
    return true;
  }
  if (d == "screen.param") {
    if (!s->follow_src.empty() && !strcmp(detail, "auto")) {  // Auto again: follow the target entity
      s->following = true;
      s->params["mode"] = "auto";
      s->follow_last.clear();
      const std::string *v = ha::get(s->follow_src);
      if (!v || !set_param(s, s->follow_param, "", *v)) rebind_all(s);
      if (v) s->follow_last = *v;
      ArduinoJson::JsonDocument kv;
      kv.set("auto");
      deck_set(s->id, "params." + s->follow_param, kv.as<ArduinoJson::JsonVariantConst>());  // Auto sticks
      return true;
    }
    if (!s->follow_src.empty()) {  // a real pick: manual until Auto is picked again
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
    if (set_param(s, a["param"] | "", a["from"] | "", detail)) emit(s->root, "param", (std::string(a["param"] | "") + "=" + detail).c_str());  // the host sees it (a view, a speaker)
    return true;
  }
  if (d == "light.toggle") { if (!*ha::act(n.obj, "light.toggle", n.entity.c_str())) ha::hold(n.obj, PENDING_MS); return true; }
  if (d == "light.switch") {
    const bool v = !strcmp(detail, "on");
    if (v && n.has_brightness) { n.want = ON_ONLY; n.sent_at = lv_tick_get(); return true; }  // a drag may follow
    if (!v && n.has_brightness && n.sent == 0) return true;                                   // the released 0 did it
    if (*ha::act(n.obj, v ? "light.turn_on" : "light.turn_off", n.entity.c_str())) return true;
    if (n.has_brightness) n.sent = v ? -1 : 0;
    n.sent_at = lv_tick_get();
    ha::hold(n.obj, PENDING_MS, n.entity, "", v ? "on" : "off");
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
    if (!*ha::act(n.obj, d.c_str(), n.entity.c_str(), ds.c_str()))
      ha::hold(n.obj, PENDING_MS, n.entity, hvac ? "" : "preset_mode", detail);  // the state = the hvac mode in HA
    return true;
  }
  if (d == "light.brightness") {
    const int v = (int) std::lround(atof(detail));
    if (!strcmp(event, "input")) {  // live: throttle with a trailing send
      ha::hold(n.obj, PENDING_MS);
      if (v == 0 && !n.drag_sent) return true;
      n.drag_sent = true;
      if (n.want == ON_ONLY || n.sent < 0 || lv_tick_elaps(n.sent_at) >= LIGHT_THROTTLE_MS) { n.want = -1; send_brightness(n, v); }
      else n.want = v;
      return true;
    }
    n.want = -1;  // released: the final value now
    n.drag_sent = false;
    send_brightness(n, v);
    if (v > 0) ha::hold(n.obj, PENDING_MS, n.entity, "brightness", std::to_string(v), true);
    else ha::hold(n.obj, PENDING_MS, n.entity, "", "off");
    return true;
  }
  return false;
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
inline lv_obj_t *&building_header() {
  static lv_obj_t *h = nullptr;
  return h;
}
// `item`, just built into building_header(), goes before the screen chrome (the time stays at the right edge)
inline void header_place(lv_obj_t *item) {
  lv_obj_t *h = item ? lv_obj_get_parent(item) : nullptr;
  const uint32_t n = h ? lv_obj_get_child_count(h) : 0;
  if (n >= 2) lv_obj_move_to_index(item, (int32_t) n - 2);
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
  for (ArduinoJson::JsonVariantConst nd : doc["nodes"].as<ArduinoJson::JsonArrayConst>())  // Auto (screen.param "auto")
    for (ArduinoJson::JsonPairConst kv : nd["on"].as<ArduinoJson::JsonObjectConst>()) {
      if (strcmp(kv.value()["do"] | "", "screen.param") || !kv.value()["auto"].is<const char *>()) continue;
      s->follow_param = kv.value()["param"] | "";
      s->follow_src = s->params[kv.value()["auto"] | ""];
      if (!entity_ok(s->follow_src) || s->follow_src[0] == '$') s->follow_src.clear();
      std::string &fv = s->params[s->follow_param];
      const bool picked = de["params"][s->follow_param.c_str()].is<const char *>();  // nothing picked = Auto
      const bool asked = fv == "auto" || !picked;
      if (fv == "auto") fv = param_default(doc["params"][s->follow_param.c_str()]);  // a real one until the target says
      s->following = !s->follow_src.empty() && asked && opened[s->follow_param.c_str()].isNull();  // opened on one: manual
      if (s->following) {
        ha::want(s->follow_src);
        const std::string *v = ha::get(s->follow_src);
        const std::string &cur = s->params[s->follow_param];
        if (v && entity_ok(*v) && (*v)[0] != '$' && v->substr(0, v->find('.')) == cur.substr(0, cur.find('.'))) s->params[s->follow_param] = *v;
        if (v) s->follow_last = *v;
      }
      s->params["mode"] = s->follow_src.empty() ? "" : s->following ? "auto" : "manual";
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
  lv_obj_set_style_bg_color(s->root, t.bg, 0);
  lv_obj_set_style_bg_opa(s->root, LV_OPA_COVER, 0);
  lv_obj_add_event_cb(s->root, [](lv_event_t *e) {
    auto *s = (Screen *) lv_event_get_user_data(e);
    if (s->timer) lv_timer_delete(s->timer);
    live().erase(std::remove(live().begin(), live().end(), s), live().end());
    delete s;
  }, LV_EVENT_DELETE, s);
  listen(s->root, [](lv_obj_t *from, const char *name, const char *detail, void *user) {
    auto *s = (Screen *) user;
    Node *n = node_of(s, from);
    if (n && run(s, *n, name, detail)) return;
    if (lv_obj_t *up = lv_obj_get_parent(s->root)) notify(up, name, detail);  // the Header's back: the device
  }, s);
  // header (the Header component + the screen chrome)
  ArduinoJson::JsonDocument hd;
  const char *hl = doc["header"]["label"] | "";
  hd["label"] = *hl ? hl : (doc["name"] | "");  // "$name" left empty = the spec's name
  hd["back"] = doc["header"]["back"] | true;
  if (const Component *h = find("header")) {
    h->build(s->root, hd.as<Props>());
    lv_obj_set_pos(lv_obj_get_child(s->root, -1), lay::inset, lay::inset);
    building_header() = lv_obj_get_child(s->root, -1);
  }
  const std::string layout = doc["layout"] | "list";
  lv_obj_t *c = box(s->root);
  if (layout == "center") {  // the one node centred in the content box (lay::top .. bottom inset), on the 4 px grid
    lv_obj_set_pos(c, 0, lay::top);
    lv_obj_set_size(c, lay::screen_w, lay::screen_h - lay::top - lay::inset);
    lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(c, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  } else {
    lv_obj_add_flag(c, (lv_obj_flag_t) (LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE));  // clickable: a drag on a
    lv_obj_set_scroll_dir(c, LV_DIR_VER);  // readout (a plant row, a camera still) scrolls the list too
    lv_obj_set_scrollbar_mode(c, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_pos(c, lay::inset, lay::top);
    lv_obj_set_size(c, lay::content_w, lay::screen_h - lay::top);
    lv_obj_set_style_pad_bottom(c, lay::inset, 0);
    lv_obj_set_flex_flow(c, layout == "cards" ? LV_FLEX_FLOW_ROW_WRAP : LV_FLEX_FLOW_COLUMN);
    if (layout == "cards") {
      lv_obj_set_style_pad_column(c, lay::gutter, 0);
      lv_obj_set_style_pad_row(c, lay::gutter, 0);
    }
  }
  size_t ni = 0;
  for (ArduinoJson::JsonVariantConst nd : doc["nodes"].as<ArduinoJson::JsonArrayConst>()) {
    ArduinoJson::JsonVariantConst rn = raw[ni++];
    Node n;
    n.comp = nd["c"] | "";
    ArduinoJson::JsonDocument p;
    p.set(nd["p"]);
    if (p.isNull()) p.to<ArduinoJson::JsonObject>();
    if (!nd["show"].isNull()) p[ha::SHOW_KEY] = nd["show"];  // read with the binds, applied as visibility (ha.h)
    lv_obj_t *into = c;  // "in": an earlier layout node's slot, or the header row
    const bool in_hdr = nd["in"].is<const char *>() && building_header();
    if (in_hdr) into = building_header();
    const int k = nd["in"] | -1;
    if (k >= 0 && k < n_orig && orig_obj[k])
      if (lv_obj_t *sl = slot_of(orig_obj[k], nd["slot"] | 0)) into = sl;
    n.obj = ha::bound(into, n.comp.c_str(), p.as<Props>());
    if (!n.obj) continue;
    if (in_hdr) header_place(n.obj);
    if (ni - 1 < orig.size()) orig_obj[orig[ni - 1]] = n.obj;
    if (into != c && !in_hdr) {  // a full-width node (list row, section header) in a narrower slot: the slot's width
      const int32_t sw = lv_obj_get_style_width(into, LV_PART_MAIN), w = lv_obj_get_style_width(n.obj, LV_PART_MAIN);
      if (sw > 0 && !LV_COORD_IS_SPEC(sw) && w > sw && !LV_COORD_IS_SPEC(w)) lv_obj_set_width(n.obj, sw);
    }
    if (n.comp == "section_header" && lv_obj_get_index(n.obj) > 0 && (into != c || layout == "list"))
      lv_obj_set_style_margin_top(n.obj, lay::group, 0);  // more space between groups (Layout rule)
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
  s->timer = lv_timer_create(tick, 20, s);
  return s->root;
}

// every concrete entity the specs bind (the deck's union, subscribed at boot; "$param" binds come when opened)
inline void want_all() {
  auto scan = [](const char *json) {
    ArduinoJson::JsonDocument d;
    if (!json || deserializeJson(d, json)) return;
    for (ArduinoJson::JsonVariantConst n : d["nodes"].as<ArduinoJson::JsonArrayConst>())
      for (ArduinoJson::JsonPairConst kv : n["p"].as<ArduinoJson::JsonObjectConst>()) {
        const std::string e = kv.value()["e"] | "";
        if (e.empty() || e[0] == '$') continue;
        const std::string fmt = kv.value()["fmt"] | "", a = kv.value()["a"] | "";
        ha::want(e, "");
        if (fmt == "pct" || fmt == "light") ha::want(e, a.empty() ? "brightness" : a);
        else if (fmt == "name") ha::want(e, "friendly_name");
        else if (!a.empty()) ha::want(e, a);
      }
  };
  ArduinoJson::JsonDocument deck;
  deserializeJson(deck, deck_json());
  for (ArduinoJson::JsonVariantConst x : deck["pages"].as<ArduinoJson::JsonArrayConst>()) {
    scan(screen_json(x["kind"] | ""));
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
inline bool apply(const char *kind, const std::string &json, lv_obj_t *dry, ArduinoJson::JsonObject out, bool quiet = false) {
  const bool deck = !strcmp(kind, "deck");
  ArduinoJson::JsonDocument d;
  Errs e;
  if (deserializeJson(d, json)) err(e, "", "bad_json", "not JSON");
  else if (deck) check_deck(d.as<ArduinoJson::JsonVariantConst>(), json.size(), e);
  else check_screen(d.as<ArduinoJson::JsonVariantConst>(), json.size(), e);
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
  if (live && canonical(live) == canonical(json)) {  // the same content (Darren 2026-10-23: not the rev) = a no-op
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
  for (const CardType &c : CARD_TYPES) { auto o = ct[c.t].to<ArduinoJson::JsonObject>(); o["domains"] = c.domains; o["sizes"] = c.sizes; o["taps"] = c.taps; }
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
  for (const auto &kv : S().over) if (!factory(kv.first)) { auto o = scr[kv.first].to<ArduinoJson::JsonObject>(); o["rev"] = rev_of(kv.second.c_str()); o["override"] = true; }
  out["deck_rev"] = rev_of(deck_json());
  out["deck_override"] = !S().deck_over.empty();
  ArduinoJson::JsonObject b = out["budget"].to<ArduinoJson::JsonObject>();
  b["nodes"] = budget().nodes; b["binds"] = budget().binds; b["bytes"] = budget().bytes; b["screens"] = budget().screens;
  auto lg = out["log"].to<ArduinoJson::JsonArray>();
  for (const auto &x : S().log) lg.add(x);
}
}  // namespace spec
}  // namespace fw
