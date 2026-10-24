// Home Assistant data for screens (one-renderer plan §2.3, first slice 2026-10-22): the same code on the panels and in
// the WASM workbench. Header-only (core/*.h is bundled as is).
//   store    the latest value per (entity, attribute) ("" = the state), the only place HA data lands. Hosts put():
//            the panel from its HA API subscriptions (firmware/fw_device.h), the workbench from the hub
//            (/api/panel/entities, polled; core/main.cpp fw_ha_put).
//   wants    the (entity, attribute) pairs bound by anything built (or declared up front: want()). The panel subscribes
//            to each new pair once (on_want; HA can't unsubscribe), the workbench polls fw_ha_wants().
//   binds    a component prop may be {"e": "light.x", "a"?: "brightness", "fmt"?: ...} instead of a value; bound()
//            builds the component with the values from the store and, when one of its pairs changes, moves it through
//            its update path (FW_UPDATE, silent), never a rebuild. fmt: (none) the raw string; "on" state == on (bool);
//            "pct" a 0-255 attribute as 0-100 (missing / None = 0); "avail" "unavailable" when the state is
//            unavailable / unknown, else "default" (no value yet = "default"); "light" the light row's sub text
//            ("Off", "On", "On · 64%"); "name" the friendly_name (else the entity id); "num" a number attribute
//            (a thermostat's temperature / current_temperature; missing = the component's default); "frac" a 0-1
//            attribute as 0-100 (a media player's volume_level; missing = the component's default).
//   hold     instant feedback (simple model): a control the person just moved shows the new value at once (its own
//            animation); hold(obj, ms, e, a, expect) keeps store updates off it until HA reports `expect` for (e, a)
//            (confirmed: settle on the store) or ms pass (timed out / refused: back to what HA says). Pending text is
//            the screen's (the room's "Turning on").
//   actions  act(obj, service, entity, data_json) = a closed whitelist (light.turn_on [+ brightness_pct 0-100],
//            light.turn_off, light.toggle on light.* entities; climate.set_temperature {temperature},
//            climate.set_hvac_mode {hvac_mode: off|heat|cool|heat_cool|auto|dry|fan_only}, climate.set_preset_mode
//            {preset_mode} on climate.* entities; media_player.media_play_pause / media_next_track /
//            media_previous_track, media_player.volume_set {volume_level 0..1} on media_player.* entities;
//            cover.open_cover / close_cover / stop_cover, set_cover_position {position}, set_cover_tilt_position
//            {tilt_position} 0-100 on cover.* entities): emits
//            "ha" {service, entity, data} (the workbench host calls HA through the hub) and calls the action sink (the panel: an HA action over its API).
// TODO (full "wish" model, plan §2.3): wishes per entity shared across screens, drag throttling (brightness while
// dragging), providers, a deck-wide subscription union, big attributes kept raw.
#pragma once
#include <algorithm>
#include <cmath>
#include <map>
#include <string>
#include <utility>
#include <vector>
#include "fw.h"

namespace fw {
namespace ha {
using Pair = std::pair<std::string, std::string>;  // (entity, attribute "" = state)
using WantSink = void (*)(const std::string &e, const std::string &a);
using ActSink = void (*)(const char *service, const char *entity, const char *data_json);

struct Bound {
  lv_obj_t *obj = nullptr;
  const Component *c = nullptr;
  std::string spec;  // the props as given (with binds), JSON
  std::vector<Pair> pairs;
  uint32_t hold_until = 0;  // lv_tick; 0 = not held
  Pair expect_pair;
  std::string expect;
  bool expect_pct = false;  // expect = a 0-100 value of a 0-255 attribute (brightness)
  bool dirty = false;
};
struct State {
  std::map<Pair, std::string> store;
  std::vector<Pair> wants;
  std::vector<Bound *> bounds;
  WantSink want_sink = nullptr;
  ActSink act_sink = nullptr;
  std::vector<void (*)(const std::string &e, const std::string &a, const std::string &v)> put_hooks;  // on_put
  lv_timer_t *timer = nullptr;
};
inline State &S() {
  static State s;
  return s;
}

inline const std::string *get(const std::string &e, const std::string &a = "") {
  auto it = S().store.find({e, a});
  return it == S().store.end() ? nullptr : &it->second;
}
inline void want(const std::string &e, const std::string &a = "") {
  if (e.empty()) return;
  State &s = S();
  if (std::find(s.wants.begin(), s.wants.end(), Pair{e, a}) != s.wants.end()) return;
  s.wants.push_back({e, a});
  if (s.want_sink) s.want_sink(e, a);
}
inline void on_want(WantSink f) {  // the panel: subscribe; called at once for every pair wanted so far
  S().want_sink = f;
  for (const Pair &p : std::vector<Pair>(S().wants)) f(p.first, p.second);
}
inline void on_act(ActSink f) { S().act_sink = f; }
// a runtime that reacts to a pair without a component (the device's toasts: "facet.toast"); called on every change
inline void on_put(void (*f)(const std::string &e, const std::string &a, const std::string &v)) { S().put_hooks.push_back(f); }
inline const std::vector<Pair> &wants() { return S().wants; }

// ---- binds
inline bool is_bind(ArduinoJson::JsonVariantConst v) { return v.is<ArduinoJson::JsonObjectConst>() && v["e"].is<const char *>(); }
inline double num(const std::string *v) {
  if (!v || v->empty()) return NAN;
  char *end = nullptr;
  const double x = strtod(v->c_str(), &end);
  return end == v->c_str() ? NAN : x;
}
inline int pct(const std::string &e, const char *a) {
  const double b = num(get(e, a));
  return std::isfinite(b) ? (int) std::lround(LV_CLAMP(0.0, b, 255.0) * 100 / 255) : 0;
}
// the value of one bind into out[key]; the pairs it reads go to `pairs`
inline void bind_raw(ArduinoJson::JsonVariantConst b, ArduinoJson::JsonObject out, const char *key, std::vector<Pair> *pairs);
// s (the value while the entity is empty: the factory demo), map / else (the value as words or a tone), tpl ("{}")
inline void bind_value(ArduinoJson::JsonVariantConst b, ArduinoJson::JsonObject out, const char *key, std::vector<Pair> *pairs) {
  const std::string e = b["e"] | "";
  const bool sample = e.empty() && b["s"].is<const char *>();
  if (sample) {
    const std::string fmt = b["fmt"] | "";
    const char *sv = b["s"];
    if (fmt == "num" || fmt == "frac" || fmt == "pct") { char *end = nullptr; const double x = strtod(sv, &end); if (end != sv) out[key] = x; }
    else if (fmt == "on") out[key] = !strcmp(sv, "on");
    else out[key] = sv;
  } else bind_raw(b, out, key, pairs);
  if (b["map"].isNull() && b["tpl"].isNull()) return;
  std::string v;
  ArduinoJson::JsonVariant cur = out[key];
  if (cur.is<double>()) { char nb[24]; snprintf(nb, sizeof nb, "%g", cur.as<double>()); v = nb; }
  else if (cur.is<bool>()) v = cur.as<bool>() ? "on" : "off";
  else v = cur | "";
  if (b["map"].is<ArduinoJson::JsonObjectConst>()) {
    ArduinoJson::JsonVariantConst m = b["map"][v.c_str()];
    if (m.is<const char *>()) v = m.as<const char *>();
    else if (b["else"].is<const char *>()) v = b["else"].as<const char *>();
  }
  if (b["tpl"].is<const char *>() && !v.empty()) {
    std::string t = b["tpl"].as<const char *>();
    const size_t at = t.find("{}");
    if (at != std::string::npos) t.replace(at, 2, v);
    v = t;
  }
  out[key] = v;
}
inline void bind_raw(ArduinoJson::JsonVariantConst b, ArduinoJson::JsonObject out, const char *key, std::vector<Pair> *pairs) {
  const std::string e = b["e"] | "", a = b["a"] | "", fmt = b["fmt"] | "";
  auto use = [&](const std::string &aa) { if (pairs && std::find(pairs->begin(), pairs->end(), Pair{e, aa}) == pairs->end()) pairs->push_back({e, aa}); };
  const std::string *st = get(e, "");
  const bool on = st && *st == "on";
  if (fmt == "on") { use(""); out[key] = on; return; }
  if (fmt == "pct") { use(a.empty() ? "brightness" : a); out[key] = pct(e, a.empty() ? "brightness" : a.c_str()); return; }
  if (fmt == "avail") { use(""); out[key] = st && (*st == "unavailable" || *st == "unknown") ? "unavailable" : "default"; return; }
  if (fmt == "light") {
    use(""); use("brightness");
    if (!st || *st == "unavailable" || *st == "unknown") { out[key] = ""; return; }
    if (!on) { out[key] = "Off"; return; }
    const double br = num(get(e, "brightness"));
    out[key] = std::isfinite(br) ? "On · " + std::to_string(pct(e, "brightness")) + "%" : std::string("On");
    return;
  }
  if (fmt == "num") {  // a number attribute (temperature, current_temperature, min_temp): missing / None = no prop
    use(a);
    const double x = num(get(e, a));
    if (std::isfinite(x)) out[key] = x;
    return;
  }
  if (fmt == "frac") {  // a 0-1 attribute (volume_level) as 0-100; missing / None = no prop
    use(a);
    const double x = num(get(e, a));
    if (std::isfinite(x)) out[key] = (int) std::lround(LV_CLAMP(0.0, x, 1.0) * 100);
    return;
  }
  if (fmt == "name") { use("friendly_name"); const std::string *n = get(e, "friendly_name"); out[key] = n && !n->empty() ? *n : e; return; }
  use(a);
  const std::string *v = get(e, a);
  out[key] = v ? *v : "";
}
// props with binds -> props with values (the pairs read go to `pairs`)
inline void resolve(Props in, ArduinoJson::JsonDocument &out, std::vector<Pair> *pairs) {
  ArduinoJson::JsonObject o = out.to<ArduinoJson::JsonObject>();
  for (ArduinoJson::JsonPairConst kv : in) {
    if (is_bind(kv.value())) bind_value(kv.value(), o, kv.key().c_str(), pairs);
    else o[kv.key()] = kv.value();
  }
}

inline Bound *bound_of(lv_obj_t *obj) {
  for (Bound *b : S().bounds)
    if (b->obj == obj) return b;
  return nullptr;
}
// the store values again, through the component's update path (silent)
inline void refresh(Bound *b) {
  b->dirty = false;
  ArduinoJson::JsonDocument spec, p;
  deserializeJson(spec, b->spec);
  resolve(spec.as<Props>(), p, nullptr);
  Quiet q;
  update_child(b->c, b->obj, p.as<Props>());
}
inline void tick(lv_timer_t *) {
  const uint32_t now = lv_tick_get();
  for (Bound *b : std::vector<Bound *>(S().bounds)) {
    if (b->hold_until && (int32_t) (now - b->hold_until) >= 0) { b->hold_until = 0; b->dirty = true; }
    if (b->dirty && !b->hold_until) refresh(b);
  }
}
inline void start_timer() {
  if (!S().timer) S().timer = lv_timer_create(tick, 20, nullptr);
}

inline bool matches(const Bound *b, const std::string &v) {
  if (!b->expect_pct) {
    if (b->expect == v) return true;
    const double x = num(&v), y = num(&b->expect);  // numbers: "21" == "21.0"
    return std::isfinite(x) && std::isfinite(y) && std::fabs(x - y) < 1e-6;
  }
  const double x = num(&v);
  return std::isfinite(x) && (int) std::lround(LV_CLAMP(0.0, x, 255.0) * 100 / 255) == atoi(b->expect.c_str());
}
// the host's new value: kept; bound components that read it repaint on the next tick (from a shallow stack)
inline void put(const std::string &e, const std::string &a, const std::string &v) {
  State &s = S();
  auto it = s.store.find({e, a});
  if (it != s.store.end() && it->second == v) return;
  s.store[{e, a}] = v;
  for (auto f : s.put_hooks) f(e, a, v);  // a changed value only (a toast sends its own id / time)
  for (Bound *b : s.bounds) {
    if (std::find(b->pairs.begin(), b->pairs.end(), Pair{e, a}) == b->pairs.end()) continue;
    if (b->hold_until && b->expect_pair == Pair{e, a} && matches(b, v)) b->hold_until = 0;  // confirmed
    b->dirty = true;
  }
}

// build component `comp` with `spec` (props, binds allowed) into parent; returns its root (nullptr = not built)
inline lv_obj_t *bound(lv_obj_t *parent, const char *comp, Props spec) {
  const Component *c = find(comp);
  if (!c) return nullptr;
  auto *b = new Bound;
  b->c = c;
  serializeJson(spec, b->spec);
  ArduinoJson::JsonDocument p;
  resolve(spec, p, &b->pairs);
  const uint32_t before = lv_obj_get_child_count(parent);
  c->build(parent, p.as<Props>());
  if (lv_obj_get_child_count(parent) <= before) { delete b; return nullptr; }
  b->obj = lv_obj_get_child(parent, -1);
  for (const Pair &pr : b->pairs) want(pr.first, pr.second);
  S().bounds.push_back(b);
  lv_obj_add_event_cb(b->obj, [](lv_event_t *e) {
    auto *b = (Bound *) lv_event_get_user_data(e);
    auto &v = S().bounds;
    v.erase(std::remove(v.begin(), v.end(), b), v.end());
    delete b;
  }, LV_EVENT_DELETE, b);
  start_timer();
  return b->obj;
}
// obj (built by bound()) takes new props / binds (a screen parameter changed: another entity): the pairs are read
// again, wanted, any hold dropped, and the component moves there through its update path (silent)
inline bool rebind(lv_obj_t *obj, Props spec) {
  Bound *b = bound_of(obj);
  if (!b) return false;
  b->spec.clear();
  serializeJson(spec, b->spec);
  b->pairs.clear();
  ArduinoJson::JsonDocument p;
  resolve(spec, p, &b->pairs);
  for (const Pair &pr : b->pairs) want(pr.first, pr.second);
  b->hold_until = 0;
  refresh(b);
  return true;
}
// instant feedback: keep the store off obj until (e, a) == expect or ms pass, then show what HA says
// pct = expect is 0-100 of a 0-255 attribute (brightness_pct sent, brightness reported)
inline void hold(lv_obj_t *obj, uint32_t ms, const std::string &e = "", const std::string &a = "", const std::string &expect = "", bool pct = false) {
  Bound *b = bound_of(obj);
  if (!b) return;
  b->hold_until = lv_tick_get() + ms;
  if (!b->hold_until) b->hold_until = 1;
  b->expect_pair = {e, a};
  b->expect = expect;
  b->expect_pct = pct;
  if (!e.empty() && !expect.empty()) {  // already there (HA was quicker than the tap's animation): settle at once
    const std::string *v = get(e, a);
    if (v && matches(b, *v)) b->hold_until = 0;
  }
}
inline bool held(lv_obj_t *obj) {
  Bound *b = bound_of(obj);
  return b && b->hold_until;
}

// ---- actions (closed whitelist)
inline const char *act(lv_obj_t *from, const char *service, const char *entity, const char *data_json = "") {
  const std::string svc = service ? service : "", ent = entity ? entity : "";
  const bool light = ent.rfind("light.", 0) == 0, climate = ent.rfind("climate.", 0) == 0, media = ent.rfind("media_player.", 0) == 0;
  if (svc.rfind("light.", 0) == 0 && !light) return "not a light";
  if (svc.rfind("climate.", 0) == 0 && !climate) return "not a climate entity";
  if (svc.rfind("media_player.", 0) == 0 && !media) return "not a media player";
  if (svc.rfind("cover.", 0) == 0 && ent.rfind("cover.", 0) != 0) return "not a cover";
  if (svc.rfind("lock.", 0) == 0 && ent.rfind("lock.", 0) != 0) return "not a lock";
  if (svc.rfind("vacuum.", 0) == 0 && ent.rfind("vacuum.", 0) != 0) return "not a vacuum";
  ArduinoJson::JsonDocument data;
  if (data_json && *data_json && deserializeJson(data, data_json)) return "data is not JSON";
  auto only = [&](const char *key) {  // the data holds exactly this one key
    if (data.size() != 1 || data[key].isNull()) return false;
    return true;
  };
  if (svc == "climate.set_temperature") {
    if (!only("temperature") || !data["temperature"].is<double>()) return "climate.set_temperature takes temperature";
    const double t = data["temperature"].as<double>();
    if (!std::isfinite(t) || t < 0 || t > 50) return "temperature outside 0..50";
  } else if (svc == "climate.set_hvac_mode") {
    static const char *const MODES[] = {"off", "heat", "cool", "heat_cool", "auto", "dry", "fan_only"};
    const std::string m = data["hvac_mode"] | "";
    if (!only("hvac_mode") || std::none_of(std::begin(MODES), std::end(MODES), [&](const char *x) { return m == x; })) return "hvac_mode is one of off, heat, cool, heat_cool, auto, dry, fan_only";
  } else if (svc == "climate.set_preset_mode") {
    const std::string m = data["preset_mode"] | "";
    if (!only("preset_mode") || m.empty() || m.size() > 32) return "climate.set_preset_mode takes preset_mode (a short name)";
    for (char c : m) if (!(isalnum((unsigned char) c) || c == '_' || c == ' ' || c == '-')) return "preset_mode: letters, digits, space, _ or -";
  } else if (svc == "light.turn_on") {
    for (ArduinoJson::JsonPairConst kv : data.as<ArduinoJson::JsonObjectConst>())
      if (strcmp(kv.key().c_str(), "brightness_pct")) return "light.turn_on takes brightness_pct only";
    if (data["brightness_pct"].is<double>()) data["brightness_pct"] = (int) LV_CLAMP(0.0, std::round(data["brightness_pct"].as<double>()), 100.0);
  } else if (svc == "media_player.volume_set") {
    if (!only("volume_level") || !data["volume_level"].is<double>()) return "media_player.volume_set takes volume_level";
    const double v = data["volume_level"].as<double>();
    if (!std::isfinite(v) || v < 0 || v > 1) return "volume_level outside 0..1";
  } else if (svc == "cover.set_cover_position" || svc == "cover.set_cover_tilt_position") {
    const char *k = svc == "cover.set_cover_position" ? "position" : "tilt_position";
    if (!only(k) || !data[k].is<double>()) return "cover position: position / tilt_position 0..100";
    data[k] = (int) LV_CLAMP(0.0, std::round(data[k].as<double>()), 100.0);
  } else if (svc == "vacuum.set_fan_speed") {
    const std::string f = data["fan_speed"] | "";
    if (!only("fan_speed") || f.empty() || f.size() > 24) return "vacuum.set_fan_speed takes fan_speed (a short name)";
    for (char c : f) if (!(isalnum((unsigned char) c) || c == '_' || c == ' ' || c == '-')) return "fan_speed: letters, digits, space, _ or -";
  } else if (svc == "vacuum.send_command") {  // only the room clean (Roborock app_segment_clean)
    if (data.size() != 2 || strcmp(data["command"] | "", "app_segment_clean") || !data["params"][0]["segments"].is<ArduinoJson::JsonArrayConst>())
      return "vacuum.send_command: app_segment_clean with segments only";
    for (ArduinoJson::JsonVariantConst x : data["params"][0]["segments"].as<ArduinoJson::JsonArrayConst>())
      if (!x.is<int>() || x.as<int>() < 0 || x.as<int>() > 999) return "segments are room numbers";
  } else if (svc == "cover.open_cover" || svc == "cover.close_cover" || svc == "cover.stop_cover" ||
             svc == "lock.lock" || svc == "lock.unlock" || svc == "lock.open" || svc == "vacuum.start" ||
             svc == "vacuum.pause" || svc == "vacuum.return_to_base" ||
             svc == "light.turn_off" || svc == "light.toggle" || svc == "media_player.media_play_pause" ||
             svc == "media_player.media_next_track" || svc == "media_player.media_previous_track") {
    if (data.size()) return "no data for this action";
  } else return "action not allowed";
  ArduinoJson::JsonDocument ev;
  ev["service"] = svc;
  ev["entity"] = ent;
  ev["data"] = data.isNull() ? ArduinoJson::JsonVariant() : data.as<ArduinoJson::JsonVariant>();
  std::string evs, ds;
  serializeJson(ev, evs);
  if (!data.isNull()) serializeJson(data, ds);
  if (from) emit(from, "ha", evs.c_str());
  if (S().act_sink) S().act_sink(svc.c_str(), ent.c_str(), ds.c_str());
  return "";
}
}  // namespace ha
}  // namespace fw
