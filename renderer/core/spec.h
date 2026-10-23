// Screens from specs (one-renderer plan §2.1-2.5, v1 2026-10-23, WIP): a screen is a JSON spec built from the
// registered components (fw::find), the same code on the panels and in the WASM workbench. Header-only (core/*.h is
// bundled as is). Specs never carry positions, sizes, colours, fonts or sounds: the layout and the components decide.
//
// Screen spec  {"v": 1, "id": "office", "name": "Office", "rev": 3,
//               "params": {"entity": "light.x", ...},     the screen's parameters + defaults; "$entity" anywhere
//                                                          in a string = the value it was opened with
//               "header": {"label": "Office", "back": true},
//               "layout": "list" | "center" | "cards",    list = one scrolling column (rows touch), center = one node
//                                                          centred in the content box, cards = the card grid
//               "nodes": [node...]}
// Node         {"c": "<component id>", "p": {prop: value | bind}, "on": {event: action}, "pending": {prop: [on, off]}}
//   bind       {"e": "light.x" | "$param", "a"?: "<attribute>", "fmt"?: on|pct|avail|light|name} (core/ha.h)
//   on         event = what the component emits (or "long": a long press, made here) -> one action:
//   action     {"do": "light.switch", "e"}  (a toggle's on / off)    {"do": "light.toggle", "e"}
//              {"do": "light.brightness", "e"}  (a Dial / Slider value: live while dragging = throttle with a
//                                                trailing send, light-throttle-ms; the release always sends)
//              {"do": "climate.set_temperature", "e"}  (a thermostat set point: live while dragging = throttle with a
//                                                       trailing send, clamped + snapped to the entity's min / max /
//                                                       step; settles when HA reports it)
//              {"do": "climate.set_hvac_mode", "e"} / {"do": "climate.set_preset_mode", "e"}  (the event's value)
//              {"do": "nav.open", "screen": "<id>", "params"?: {k: v}}    {"do": "nav.back"}
//              on "open" (any component's own open event): {"do": "nav.open"} = the screen its detail names
//              The closed whitelist: each component + event allows only the actions in RULES (the catalogue).
//   pending    while a light.switch waits for HA: the prop shows on / off text (a List row's sub "Turning on")
// Deck         {"v": 1, "rev": 2, "screens": [{"id": "living_room", "card": true}, ...]}: the screens, their order
//              and Home's cards (card: true). An id is a spec screen or a built-in screen (settings).
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
#include <string>
#include <vector>
#include "fw.h"
#include "ha.h"
#include "specs_gen.h"

namespace fw {
namespace spec {
constexpr int PROTOCOL = 1;
struct Budget {
  int nodes = 48, binds = 64, bytes = 8192, screens = 16, deck_bytes = 2048;
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
    {"toggle", "toggle", "light.switch"},            {"button", "tap", "nav.open,nav.back,light.toggle"},
    {"more_row", "tap", "nav.open,nav.back"},
    {"thermostat_dial", "change", "climate.set_temperature"}, {"thermostat_dial", "input", "climate.set_temperature"},
    {"thermostat_view", "target", "climate.set_temperature"}, {"thermostat_view", "mode", "climate.set_hvac_mode"},
    {"thermostat_view", "preset", "climate.set_preset_mode"},        // a link to a sub-view spec (moon -> moon_details -> ...)
    {"*", "open", "nav.open"},                       // any component's own "open" (a tapped calendar day, a PR):
                                                     // its detail = the screen id [+ "?" + params JSON]
};
constexpr const char *ACTIONS[] = {"light.switch", "light.toggle", "light.brightness", "climate.set_temperature",
                                   "climate.set_hvac_mode", "climate.set_preset_mode", "nav.open", "nav.back"};
constexpr const char *FMTS[] = {"", "on", "pct", "avail", "light", "name", "num"};
constexpr uint32_t CLIMATE_THROTTLE_MS = 250;  // like light-throttle-ms: a set point while dragging at most this often
constexpr const char *LAYOUTS[] = {"list", "center", "cards"};
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
  static const char *const K[] = {"do", "e", "screen", "params"};
  check_keys(a.as<ArduinoJson::JsonObjectConst>(), K, 4, path, e);
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
  if (d == "nav.open") {
    const std::string s = a["screen"] | "";
    const bool from_event = !strcmp(event, "open") && a["screen"].isNull();  // the component's "open" names the screen
    if (!from_event && !id_ok(s)) err(e, path + ".screen", "bad_screen", "a screen id (a-z, 0-9, _; at most 12)");
    if (!a["params"].isNull() && !a["params"].is<ArduinoJson::JsonObjectConst>()) err(e, path + ".params", "bad_action", "params is an object");
  }
}
inline void check_value(const PropInfo &pi, ArduinoJson::JsonVariantConst v, const std::string &path, Errs &e, int &binds) {
  if (v.is<ArduinoJson::JsonObjectConst>()) {
    static const char *const K[] = {"e", "a", "fmt"};
    check_keys(v.as<ArduinoJson::JsonObjectConst>(), K, 3, path, e);
    if (!entity_ok(v["e"] | "")) err(e, path + ".e", "bad_bind", "a bind needs an entity (domain.object or $param)");
    if (!one_of(FMTS, v["fmt"] | "")) err(e, path + ".fmt", "bad_bind", "fmt is one of on, pct, avail, light, name, num");
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
  static const char *const K[] = {"v", "id", "name", "rev", "params", "header", "layout", "nodes"};
  check_keys(s.as<ArduinoJson::JsonObjectConst>(), K, 8, "", e);
  if ((s["v"] | 0) != PROTOCOL) err(e, ".v", "bad_version", "v must be 1");
  if (!id_ok(s["id"] | "")) err(e, ".id", "bad_id", "id: a-z, 0-9, _; at most 12 characters");
  if (!s["rev"].is<int>() || s["rev"].as<int>() < 0) err(e, ".rev", "bad_rev", "rev is a whole number");
  if (!one_of(LAYOUTS, s["layout"] | "")) err(e, ".layout", "bad_layout", "layout is list, center or cards");
  if (find((s["id"] | ""))) err(e, ".id", "bad_id", "that id is a built-in screen or component");
  if (bytes > (size_t) budget().bytes) err(e, "", "over_budget", "spec over " + std::to_string(budget().bytes) + " bytes");
  if (!s["params"].isNull()) {
    if (!s["params"].is<ArduinoJson::JsonObjectConst>()) err(e, ".params", "bad_params", "params is an object of defaults");
    else for (ArduinoJson::JsonPairConst kv : s["params"].as<ArduinoJson::JsonObjectConst>())
      if (!kv.value().is<const char *>()) err(e, std::string(".params.") + kv.key().c_str(), "bad_params", "a parameter default is text");
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
    static const char *const NK[] = {"c", "p", "on", "pending"};
    check_keys(n.as<ArduinoJson::JsonObjectConst>(), NK, 4, p, e);
    const std::string c = n["c"] | "";
    const std::vector<PropInfo> *pis = props_of(c.c_str());
    const Component *comp = find(c.c_str());
    if (!pis || !comp || strcmp(comp->section, "components")) { err(e, p + ".c", "unknown_component", "no component " + c + " in this build"); continue; }
    for (ArduinoJson::JsonPairConst kv : n["p"].as<ArduinoJson::JsonObjectConst>()) {
      const std::string k = kv.key().c_str();
      auto it = std::find_if(pis->begin(), pis->end(), [&](const PropInfo &x) { return x.name == k; });
      if (it == pis->end()) { err(e, p + ".p." + k, one_of(STYLE_KEYS, k) ? "style_key" : "unknown_prop", c + " has no prop " + k); continue; }
      check_value(*it, kv.value(), p + ".p." + k, e, binds);
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
inline void check_deck(ArduinoJson::JsonVariantConst d, size_t bytes, Errs &e) {
  if (!d.is<ArduinoJson::JsonObjectConst>()) { err(e, "", "bad_spec", "a deck is an object"); return; }
  static const char *const K[] = {"v", "rev", "screens"};
  check_keys(d.as<ArduinoJson::JsonObjectConst>(), K, 3, "", e);
  if ((d["v"] | 0) != PROTOCOL) err(e, ".v", "bad_version", "v must be 1");
  if (!d["rev"].is<int>()) err(e, ".rev", "bad_rev", "rev is a whole number");
  if (bytes > (size_t) budget().deck_bytes) err(e, "", "over_budget", "deck over " + std::to_string(budget().deck_bytes) + " bytes");
  ArduinoJson::JsonArrayConst s = d["screens"].as<ArduinoJson::JsonArrayConst>();
  if (s.isNull() || !s.size()) { err(e, ".screens", "bad_deck", "screens is a non-empty list"); return; }
  if ((int) s.size() > budget().screens) err(e, ".screens", "over_budget", "more than " + std::to_string(budget().screens) + " screens");
  int i = 0;
  for (ArduinoJson::JsonVariantConst x : s) {
    const std::string p = ".screens[" + std::to_string(i++) + "]";
    static const char *const SK[] = {"id", "card"};
    if (!x.is<ArduinoJson::JsonObjectConst>()) { err(e, p, "bad_deck", "a deck entry is {id, card?}"); continue; }
    check_keys(x.as<ArduinoJson::JsonObjectConst>(), SK, 2, p, e);
    const std::string id = x["id"] | "";
    if (!screen_known(id)) err(e, p + ".id", "unknown_screen", "no screen " + id + " (send its spec first)");
  }
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
};
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
inline void tick(lv_timer_t *t) {
  auto *s = (Screen *) lv_timer_get_user_data(t);
  const uint32_t now = lv_tick_get();
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
  for (ArduinoJson::JsonPairConst kv : doc["params"].as<ArduinoJson::JsonObjectConst>()) s->params[kv.key().c_str()] = kv.value().as<std::string>();
  for (ArduinoJson::JsonPairConst kv : opened)
    if (kv.value().is<const char *>()) s->params[kv.key().c_str()] = kv.value().as<std::string>();
  subst_all(doc.as<ArduinoJson::JsonVariant>(), s->params);
  const Theme &t = *T;
  s->root = box(parent);
  lv_obj_set_user_data(s->root, s);
  lv_obj_set_size(s->root, lay::screen_w, lay::screen_h);
  lv_obj_set_style_bg_color(s->root, t.bg, 0);
  lv_obj_set_style_bg_opa(s->root, LV_OPA_COVER, 0);
  lv_obj_add_event_cb(s->root, [](lv_event_t *e) {
    auto *s = (Screen *) lv_event_get_user_data(e);
    if (s->timer) lv_timer_delete(s->timer);
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
  hd["label"] = doc["header"]["label"] | (doc["name"] | "");
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
    lv_obj_add_flag(c, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(c, LV_DIR_VER);
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
  for (ArduinoJson::JsonVariantConst nd : doc["nodes"].as<ArduinoJson::JsonArrayConst>()) {
    Node n;
    n.comp = nd["c"] | "";
    ArduinoJson::JsonDocument p;
    p.set(nd["p"]);
    if (p.isNull()) p.to<ArduinoJson::JsonObject>();
    n.obj = ha::bound(c, n.comp.c_str(), p.as<Props>());
    if (!n.obj) continue;
    if (!nd["on"].isNull()) serializeJson(nd["on"], n.on);
    if (!nd["pending"].isNull()) serializeJson(nd["pending"], n.pending);
    for (ArduinoJson::JsonPairConst kv : nd["on"].as<ArduinoJson::JsonObjectConst>()) {
      const std::string e = kv.value()["e"] | "";
      if (!e.empty()) n.entity = e;
      if (!strcmp(kv.value()["do"] | "", "light.brightness")) n.has_brightness = true;
      if (!strncmp(kv.value()["do"] | "", "climate.", 8) && !e.empty())  // its range + what settles a change
        for (const char *a : {"", "temperature", "min_temp", "max_temp", "target_temp_step", "preset_mode"}) ha::want(e, a);
    }
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
  for (ArduinoJson::JsonVariantConst x : deck["screens"].as<ArduinoJson::JsonArrayConst>()) scan(screen_json(x["id"] | ""));
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
inline bool apply(const char *kind, const std::string &json, lv_obj_t *dry, ArduinoJson::JsonObject out) {
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
  if (reload_hook()) reload_hook()(id.c_str());
  out["ok"] = true;
  out["status"] = "applied";
  return true;
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
