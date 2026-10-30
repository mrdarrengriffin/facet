// Flight hero: the Flight radar's hero, the lead aircraft (docs: @facet/ui screens/overhead.ts aircraftHero(), 656 x
// 264, CSS .fs-oh-*), on the contract (renderer/CONTRACT.md, "contract": 2). Specific to the Flight radar.
//   row      the words column (grow) and a real Sky radar (fw::find("sky_radar"), C8b, size hero) on the right, top
//            aligned, layout:sky-gap apart (docs 400 + 48 + 208 = 656)
//   column   gap:4 between its lines (docs 16), fixed boxes so the stats never move:
//            look line (type:label caps, fg:accent-text from 30 degrees up, else fg:text-dim: "high in the NW · 38°")
//              with a real Status chip dot + its words after it when there is a status (military, helicopter,
//              "7700 emergency"), gap:4
//            route (type:headline, fg:text, two lines then "...", h:sky-route-h: "Catania → Leeds")
//            who (type:body, fg:text-dim, one line: "Jet2.com · LS13DK · Boeing 737-800")
//            three real Stats gap:2 apart: altitude (w:sky-stat; label climbing / descending), speed
//            (w:sky-stat-s), distance (the rest: docs 144 / 120 / 120)
//   empty    no aircraft: the quiet words (type:headline "Clear skies", type:body text-dim under it) beside the empty
//            radar (docs .fs-oh-emptybox)
// Feed: bind every prop to panel.flights (core/flights_source.cpp). Tap (with an aircraft): emits "tap" + sound open
// (the Flight radar opens the aircraft's details: flights.select); no press look (docs: the hero is live, not a button). Update path: the words, stats and radar in place (silent); empty <-> aircraft or a status
// coming / going or the look line's ink changing rebuilds with a fade (fw::st::refill_fade). Contrast: text, label, stat, status_chip, overhead.
#include "fw.h"
#include "fx.h"

namespace {
bool empty_of(fw::Props p) { const char *e = p["empty"] | ""; return *e && strcmp(e, "0") && strcmp(e, "false"); }
const char *look_ink(fw::Props p) { return !strcmp(p["look_tone"] | "text-dim", "accent-text") ? "fg:accent-text" : "fg:text-dim"; }
std::string sig(fw::Props p) { return std::string(empty_of(p) ? "e" : "-") + (*(p["status"] | "") ? "s" : "-") + look_ink(p); }
void radar_props(fw::Props p, ArduinoJson::JsonDocument &d) { d["planes"] = p["planes"] | "[]", d["size"] = "hero", d["unit"] = p["unit"] | "km"; }
void stat_props(ArduinoJson::JsonDocument &d, const char *label, const char *value) { d["label"] = label, d["value"] = value, d["tone"] = "dim", d["size"] = "body"; }  // docs: body values (64 tall)
lv_obj_t *line(lv_obj_t *col, const char *s, const char *type, const char *ink) {
  lv_obj_t *l = fx::text(col, s, type, ink);
  fx::use(l, "fill-w");
  return l;
}
void fill(lv_obj_t *root, fw::Props p) {
  for (const char *t : {"box", "row-top", "fill-w", "gap:sky-gap"}) fx::use(root, t);
  fw::shape(root, sig(p), "hero");
  lv_obj_t *col = fx::box(root, "col");
  for (const char *t : {"grow", "gap:4"}) fx::use(col, t);
  if (empty_of(p)) {
    lv_obj_t *t = line(col, p["empty_title"] | "Clear skies", "type:headline", "fg:text");
    lv_label_set_long_mode(t, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_t *b = line(col, p["empty_body"] | "", "type:body", "fg:text-dim");
    lv_label_set_long_mode(b, LV_LABEL_LONG_MODE_WRAP);
  } else {
    lv_obj_t *look = fx::box(col, "row");
    for (const char *t : {"fill-w", "gap:4", "label-row"}) fx::use(look, t);
    fx::text(look, p["look"] | "", "type:label", look_ink(p));
    if (*(p["status"] | "")) {
      ArduinoJson::JsonDocument d;
      d["tone"] = p["status_tone"] | "info", d["size"] = "dot";
      fw::find("status_chip")->build(look, d.as<fw::Props>());
      fw::qa::free(lv_obj_get_child(look, 1), "the status dot after the look line: its x follows the text's width");
      fx::text(look, p["status"] | "", "type:label", "fg:text-dim");
    }
    lv_obj_t *route = line(col, p["route"] | "", "type:headline", "fg:text");
    fx::use(route, "h:sky-route-h");
    lv_label_set_long_mode(route, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_t *who = line(col, p["who"] | "", "type:body", "fg:text-dim");
    lv_label_set_long_mode(who, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_t *stats = fx::box(col, "row-top");
    for (const char *t : {"fill-w", "gap:2"}) fx::use(stats, t);
    const char *const L[3] = {p["alt_label"] | "altitude", "speed", "distance"}, *const V[3] = {p["alt"] | "--", p["speed"] | "--", p["away"] | "--"};
    for (int i = 0; i < 3; i++) {
      lv_obj_t *cell = fx::box(stats, "col");
      fx::use(cell, i == 0 ? "w:sky-stat" : i == 1 ? "w:sky-stat-s" : "grow");
      ArduinoJson::JsonDocument d;
      stat_props(d, L[i], V[i]);
      fw::find("stat")->build(cell, d.as<fw::Props>());
    }
  }
  ArduinoJson::JsonDocument d;
  radar_props(p, d);
  fw::find("sky_radar")->build(root, d.as<fw::Props>());
}
}  // namespace

FW_COMPONENT(flight_hero, "Flight hero", "components", R"({
  "about": "The aircraft that leads on the Flight radar: where to look, its route, who flies it, its height, speed and distance, beside the sky radar.",
  "contract": 2,
  "parts": [],
  "frame": [656, 280],
  "props": [
    {"name": "look", "about": "Where to look, such as high in the NW · 38°.", "type": "text", "default": "high in the NW · 54°"},
    {"name": "look_tone", "about": "accent-text when the aircraft is high enough to look up at, else text-dim.", "type": "select", "options": ["text-dim", "accent-text"], "default": "accent-text"},
    {"name": "status", "about": "A status after the look line: military, helicopter or an emergency squawk. Empty for none.", "type": "text", "default": ""},
    {"name": "status_tone", "about": "The status dot's colour: error for an emergency, info otherwise.", "type": "select", "options": ["info", "error", "warn", "ok", "working", "idle"], "default": "info"},
    {"name": "route", "about": "Where it flies from and to, such as Amsterdam → Edinburgh, or its callsign.", "type": "text", "default": "Amsterdam → Edinburgh"},
    {"name": "who", "about": "The airline, flight and aircraft, such as easyJet · U245UK · Airbus A320neo.", "type": "text", "default": "easyJet · U245UK · Airbus A320neo"},
    {"name": "alt_label", "about": "altitude, or climbing / descending while it does.", "type": "text", "default": "altitude"},
    {"name": "alt", "about": "Its height, such as 29,000 ft.", "type": "text", "default": "29,000 ft"},
    {"name": "speed", "about": "Its ground speed, such as 529 mph.", "type": "text", "default": "529 mph"},
    {"name": "away", "about": "How far away and which way, such as 7 km NW.", "type": "text", "default": "7 km NW"},
    {"name": "planes", "about": "The aircraft on the sky radar (the Sky radar's planes).", "type": "text", "default": "[[26,118,302,0],[12,200,210,0],[33,42,318,0],[38,140,255,0],[41,250,75,0],[6.5,300,330,1]]"},
    {"name": "unit", "about": "The sky radar's rings: km or mi, as Settings > Units says.", "type": "select", "options": ["km", "mi"], "default": "km"},
    {"name": "empty", "about": "Shows the quiet words instead of an aircraft, when nothing is flying nearby.", "type": "bool", "default": false},
    {"name": "empty_title", "about": "The quiet words' headline, such as Clear skies.", "type": "text", "default": "Clear skies"},
    {"name": "empty_body", "about": "The line under it, such as Nothing flying within 50 km right now.", "type": "text", "default": "Nothing flying within 50 km right now."}
  ],
  "variants": [
    {"label": "high overhead", "props": {}},
    {"label": "military", "props": {"look": "low in the W · 8°", "look_tone": "text-dim", "status": "military", "route": "TARTN24", "who": "Royal Air Force · Airbus Voyager KC2", "alt": "18,000 ft", "speed": "380 mph", "away": "41 km W", "planes": "[[26,118,302,0],[41,250,75,1]]"}},
    {"label": "clear skies", "props": {"empty": true, "planes": "[]"}}
  ]
})") {
  lv_obj_t *root = lv_obj_create(parent);
  lv_obj_remove_style_all(root);
  lv_obj_remove_flag(root, LV_OBJ_FLAG_SCROLLABLE);
  fx::name(root, "flight_hero");
  fill(root, p);
  fw::st::on_tap(root, [](lv_event_t *e) {  // the aircraft's details (nothing to open on the quiet words)
    if (!fw::st::tap_now(e)) return;
    auto *o = (lv_obj_t *) lv_event_get_current_target(e);
    if (lv_obj_get_child_count(lv_obj_get_child(o, 0)) < 4) return;  // the quiet words (2 lines), not an aircraft (4)
    fw::emit(o, "tap", "");
    fw::sound(o, "open");
  }, nullptr);
}

FW_UPDATE(flight_hero) {
  lv_obj_t *col = lv_obj_get_child(root, 0), *radar = lv_obj_get_child(root, 1);
  if (!col || !radar || !fw::shape(root, sig(p), "hero")) {
    fw::st::refill_fade(root, [&] { fw::refill(root); fill(root, p); }, true, true);
    return true;
  }
  ArduinoJson::JsonDocument d;
  radar_props(p, d);
  fw::update_child(fw::find("sky_radar"), radar, d.as<fw::Props>());
  if (empty_of(p)) {
    fx::set_text(lv_obj_get_child(col, 0), p["empty_title"] | "", "type:headline");
    fx::set_text(lv_obj_get_child(col, 1), p["empty_body"] | "", "type:body");
    return true;
  }
  lv_obj_t *look = lv_obj_get_child(col, 0);
  lv_obj_t *lt = lv_obj_get_child(look, 0);
  fx::set_text(lt, p["look"] | "", "type:label");  // its ink is in the shape: a change rebuilds
  if (*(p["status"] | "")) {
    ArduinoJson::JsonDocument s;
    s["tone"] = p["status_tone"] | "info", s["size"] = "dot";
    fw::update_child(fw::find("status_chip"), lv_obj_get_child(look, 1), s.as<fw::Props>());
    fx::set_text(lv_obj_get_child(look, 2), p["status"] | "", "type:label");
  }
  fx::set_text(lv_obj_get_child(col, 1), p["route"] | "", "type:headline");
  fx::set_text(lv_obj_get_child(col, 2), p["who"] | "", "type:body");
  lv_obj_t *stats = lv_obj_get_child(col, 3);
  const char *const L[3] = {p["alt_label"] | "altitude", "speed", "distance"}, *const V[3] = {p["alt"] | "--", p["speed"] | "--", p["away"] | "--"};
  for (int i = 0; i < 3; i++) {
    ArduinoJson::JsonDocument s;
    stat_props(s, L[i], V[i]);
    fw::update_child(fw::find("stat"), lv_obj_get_child(lv_obj_get_child(stats, i), 0), s.as<fw::Props>());
  }
  return true;
}
