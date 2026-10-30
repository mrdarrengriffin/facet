// Home: a room's page (screen model §4, piece 2). Each Home in the deck (core/spec.h deck v2) holds its own cards; this
// screen turns the Home page into a screen spec made only of real components and builds it with the spec renderer
// (core/spec.h build: binds to the entities' live state, the tap actions, the header):
//   header   the Header (label = the page name; Back unless it is the start page) + the screen chrome
//   content  one Cells layout (components/cells.cpp) per run of same-size cards, in the deck's order: tiles (min
//            tile-w x tile-h, three to a row), cards (min home-card-min-w x card-h, two to a row; two readouts in a row
//            share one cell, a pair), wide cards (wide: one per row); one node per card, from the card type catalogue (CARD_TYPES: the component per size):
//              light   s Entity tile (bulb / power icon, "64%" / "Off", on = accent) | m Card (value = the level)
//              cover   s Entity tile (opens Blinds) | m Card (position, state)
//              climate m Card (current temperature, state)
//              media   m / l Card in text mode (the track, the state)
//              sensor  s Entity tile, read-only (act none: the value + label, no press) | m State readout (no box,
//                      no tap); two m sensors in a row share one card slot
//              page    s Entity tile / m Card / l wide Card: the page's name (opens it)
//              action  s Entity tile: a script / scene / automation / button (its icon, Run / Activate / Trigger / Press)
//            label = the card's label, else the entity's friendly name (a bind). Unavailable entities show the
//            components' own unavailable look.
//   taps     toggle = light.toggle (a light or a switch); detail = nav.open of the type's detail screen for that
//            entity; page = nav.open of that deck page; none = nothing (sensors); run = script.run / scene.activate /
//            automation.trigger / button.press (an action card; its "ask" = the panel asks "Run <name>?" first)
// Header actions (the deck page's header_actions, screen model §10): button settings (default) or a page id = that
// page's shortcut, a gutter before the settings button (which always stays; a long press on the time opens Settings
// too); status timer = the panel's timer, time left beside the time.
// Prop page = the Home's page id ("" = the deck's start page if it is a Home, else the first Home). With no Home in the
// deck: the header and an empty grid.
// Update path: the deck changing rebuilds the screen (the device reloads it); entity changes move each card through
// its own update path (binds), so nothing else changes with props.
#include <string>
#include "fw.h"
#include "spec.h"

namespace {
using Doc = ArduinoJson::JsonDocument;
using Var = ArduinoJson::JsonVariantConst;

const char *META = R"({
  "about": "A room's Home: its cards from the deck (tiles, cards, wide cards and read-only readouts) in rows of one size, live from the entities. Tap a tile to toggle, a card to open its screen.",
  "frame": [480, 480],
  "icons": ["screen-settings@icon", "bulb@icon", "power@icon"],
  "props": [
    {"name": "page", "about": "Which page of the deck to show, by its id, such as kitchen. Leave empty for the start page.", "type": "text", "default": ""}
  ],
  "variants": [
    {"label": "start Home · Living room", "props": {}},
    {"label": "Kitchen", "props": {"page": "kitchen"}}
  ]
})";

// a bind on the card's entity
void bind(ArduinoJson::JsonObject p, const char *key, const std::string &e, const char *fmt, const char *a = nullptr) {
  ArduinoJson::JsonObject b = p[key].to<ArduinoJson::JsonObject>();
  b["e"] = e;
  if (fmt && *fmt) b["fmt"] = fmt;
  if (a) b["a"] = a;
}
// screen icons the Card and the Entity tile draw (screen-<kind>); others draw none
bool has_icon(const std::string &k) {
  static const char *const K[] = {"weather", "media", "timer", "energy", "light", "thermostat", "covers", "calendar", "clock", "switch",
                                  "agents", "github", "moon", "lightning", "overhead", "bins", "print", "home"};
  for (const char *x : K) if (k == x) return true;
  return false;
}
// the page's display name: its own, else its kind's spec name
std::string page_name(Var pg) {
  std::string n = pg["name"] | "";
  if (!n.empty()) return n;
  const std::string kind = pg["kind"] | "";
  Doc sp;
  if (const char *j = fw::spec::screen_json(kind)) { deserializeJson(sp, j); return sp["name"] | kind.c_str(); }
  if (kind == "home") return "Home";
  return kind;
}

// a sensor's icon from its device class (the State readout's and the Entity tile's icon sets)
void sensor_icon(ArduinoJson::JsonObject p, const std::string &e) {
  ArduinoJson::JsonObject ic = p["icon"].to<ArduinoJson::JsonObject>();
  ic["e"] = e;
  ic["a"] = "device_class";
  static const char *const MAP[][2] = {{"temperature", "thermometer"}, {"door", "door"}, {"garage_door", "door"},
      {"opening", "door"}, {"window", "window"}, {"motion", "person"}, {"occupancy", "person"}, {"presence", "person"},
      {"moisture", "alert"}, {"power", "bolt"}, {"energy", "bolt"}, {"current", "bolt"}, {"voltage", "bolt"}};
  for (auto &m : MAP) ic["map"][m[0]] = m[1];
  ic["else"] = "house";
}

// the page's feed settings (its spec's "feeds", "$param" = the page's parameters over the spec's defaults)
std::map<std::string, std::string> page_params(Var sp, Var page) {
  std::map<std::string, std::string> pm;
  for (ArduinoJson::JsonPairConst kv : sp["params"].as<ArduinoJson::JsonObjectConst>()) pm[kv.key().c_str()] = fw::spec::param_default(kv.value());
  for (ArduinoJson::JsonPairConst kv : page["params"].as<ArduinoJson::JsonObjectConst>())
    if (kv.value().is<const char *>()) pm[kv.key().c_str()] = kv.value().as<std::string>();
  return pm;
}
void page_card_feeds(const Doc &sp, Var page) {
  const std::map<std::string, std::string> pm = page_params(sp.as<Var>(), page);
  for (ArduinoJson::JsonPairConst kv : sp["feeds"].as<ArduinoJson::JsonObjectConst>()) {
    Doc fc;
    for (ArduinoJson::JsonPairConst f : kv.value().as<ArduinoJson::JsonObjectConst>()) fc[f.key().c_str()] = fw::spec::subst(f.value() | "", pm);
    std::string cfg;
    serializeJson(fc, cfg);
    fw::ha::set_feed_config(kv.key().c_str(), cfg);
  }
}
// a page card = the screen's own Home card (its spec's "card": Card props bound to its feeds, "$param" = the page's
// parameters) and the page's feed settings applied (its spec's "feeds"), so the card shows what the screen shows
void page_card(ArduinoJson::JsonObject p, Var page, const std::string &kind) {
  Doc sp;
  const char *j = fw::spec::screen_json(kind);
  if (!j || deserializeJson(sp, j) || !sp["card"].is<ArduinoJson::JsonObjectConst>()) return;
  const std::map<std::string, std::string> pm = page_params(sp.as<Var>(), page);
  for (ArduinoJson::JsonPairConst kv : sp["card"].as<ArduinoJson::JsonObjectConst>()) {
    if (kv.value().is<const char *>()) p[kv.key().c_str()] = fw::spec::subst(kv.value().as<std::string>(), pm);
    else p[kv.key().c_str()] = kv.value();
  }
  page_card_feeds(sp, page);
}

// one deck card -> one spec node in slot `slot` of node 0 (the Card grid); returns its cell size ('s', 'm', 'l', 'h')
char card_node(Var cd, Var pages, int slot, ArduinoJson::JsonArray nodes) {
  const std::string t = cd["t"] | "", e = cd["e"] | "", size = cd["size"] | "m", label = cd["label"] | "";
  const fw::spec::CardType *ct = fw::spec::card_type(t);
  if (!ct) return 0;
  std::string tap = cd["tap"] | "";
  if (tap.empty()) tap = std::string(ct->taps).substr(0, std::string(ct->taps).find(','));
  const char *comp = size == "s" ? ct->comp_s : size == "l" ? ct->comp_l : ct->comp_m;
  if (!*comp) return 0;
  {  // a card that opens a screen this build lacks (its feature is off, features.json; a custom deck can still name
     // one) is left out, so no card leads nowhere
    std::string to = tap == "detail" ? ct->detail : "";
    if (t == "page" || tap == "page") {
      const std::string id = cd["page"] | "";
      to.clear();
      for (Var pg : pages.as<ArduinoJson::JsonArrayConst>())
        if (id == (pg["id"] | "")) to = pg["kind"] | "";
      if (to.empty()) return 0;
    }
    if (!to.empty() && !fw::spec::screen_known(to)) return 0;
  }
  ArduinoJson::JsonObject n = nodes.add<ArduinoJson::JsonObject>();
  n["c"] = comp;
  n["in"] = 0;
  n["slot"] = slot;
  ArduinoJson::JsonObject p = n["p"].to<ArduinoJson::JsonObject>();
  const std::string dom = e.substr(0, e.find('.'));
  // the label: the card's, else the entity's friendly name; a page card: the page's name
  std::string pid = cd["page"] | "", kind;
  Var target;
  for (Var pg : pages.as<ArduinoJson::JsonArrayConst>())
    if (pid == (pg["id"] | "")) target = pg;
  if (t == "page") { p["label"] = label.empty() ? page_name(target) : label; kind = target["kind"] | ""; }
  else if (label.empty()) bind(p, "label", e, "name");
  else p["label"] = label;
  const std::string comp_s = comp;
  if (comp_s == "entity_tile") {
    if (t == "sensor") {  // read-only: the value as the status, the device class icon, no press (Darren 2026-10-25)
      sensor_icon(p, e);
      bind(p, "status", e, "state");
      p["on"] = false;
    } else if (t == "page") {
      p["icon"] = has_icon(kind) ? "screen-" + kind : std::string("screen-home");
      p["status"] = "";
      {  // the screen's card's main value (its spec "card" text / value) as the small tile's status (C52)
        Doc sp;
        const char *j = fw::spec::screen_json(kind);
        if (j && !deserializeJson(sp, j)) {
          Var main = !sp["card"]["text"].isNull() ? sp["card"]["text"] : sp["card"]["value"];
          if (!main.isNull()) { p["status"] = main; page_card_feeds(sp, target); }
        }
      }
      p["on"] = false;
    } else if (t == "action") {  // run it (2026-10-30): the domain's icon, the verb as the status, no on look
      p["icon"] = dom == "script" ? "play" : dom == "scene" ? "house" : dom == "automation" ? "auto" : "bolt";
      p["status"] = dom == "script" ? "Run" : dom == "scene" ? "Activate" : dom == "automation" ? "Trigger" : "Press";
      p["on"] = false;
    } else if (t == "cover") {
      p["icon"] = "screen-covers";
      bind(p, "status", e, "state");
      ArduinoJson::JsonObject on = p["on"].to<ArduinoJson::JsonObject>();
      on["e"] = e;
      on["map"]["open"] = "on";
      on["map"]["opening"] = "on";
      on["else"] = "off";
    } else {
      p["icon"] = dom == "switch" ? "power" : "bulb";
      bind(p, "status", e, "level");
      bind(p, "on", e, "on");
    }
    p["act"] = t == "sensor" ? "none" : tap == "toggle" ? "toggle" : "open";
    if (t != "page") bind(p, "state", e, "avail");
  } else if (comp_s == "card") {  // the card fills its cell (no size prop: the Card grid gives it its size)
    p["preview"] = "none";
    p["sub"] = "";
    p["value"] = "";
    p["mode"] = "value";
    if (t == "page") {
      p["icon"] = has_icon(kind) ? kind : std::string("none");
      page_card(p, target, kind);  // the screen's own card (its spec's "card", on its feeds), when it has one
    } else if (t == "light") { p["icon"] = "light"; bind(p, "value", e, "level"); bind(p, "accent", e, "on"); }
    else if (t == "cover") {
      p["icon"] = "covers";
      bind(p, "value", e, "num", "current_position");
      p["value"]["tpl"] = "{}%";
      bind(p, "sub", e, "state");
    } else if (t == "climate") {
      p["icon"] = "thermostat";
      bind(p, "value", e, "num", "current_temperature");
      p["value"]["tpl"] = "{}\xC2\xB0";
      bind(p, "sub", e, "state");
    } else if (t == "media") {
      p["icon"] = "media";
      p["mode"] = "text";
      bind(p, "text", e, "", "media_title");
      bind(p, "sub", e, "state");
    }
    if (t != "page") bind(p, "state", e, "avail");
  } else if (comp_s == "readout") {
    bind(p, "state", e, "state");
    sensor_icon(p, e);
    ArduinoJson::JsonObject tone = p["tone"].to<ArduinoJson::JsonObject>();
    tone["e"] = e;
    tone["map"]["unavailable"] = "mute";
    tone["map"]["unknown"] = "mute";
    tone["else"] = "text";
  }
  // the tap
  if (tap == "toggle") {
    ArduinoJson::JsonObject a = n["on"]["tap"].to<ArduinoJson::JsonObject>();
    a["do"] = "light.toggle";
    a["e"] = e;
  } else if (tap == "detail" && *ct->detail) {
    ArduinoJson::JsonObject a = n["on"]["tap"].to<ArduinoJson::JsonObject>();
    a["do"] = "nav.open";
    a["screen"] = ct->detail;
    a["params"]["entity"] = e;
    if (!label.empty()) a["params"]["name"] = label;
  } else if (tap == "run") {  // the Action tile: its domain's action; ask = "Run <name>?" on the panel first
    ArduinoJson::JsonObject a = n["on"]["tap"].to<ArduinoJson::JsonObject>();
    a["do"] = dom == "script" ? "script.run" : dom == "scene" ? "scene.activate" : dom == "automation" ? "automation.trigger" : "button.press";
    a["e"] = e;
    if (cd["ask"] | false) { a["ask"] = true; if (!label.empty()) a["name"] = label; }
  } else if (tap == "page" && !pid.empty()) {
    ArduinoJson::JsonObject a = n["on"]["tap"].to<ArduinoJson::JsonObject>();
    a["do"] = "nav.open";
    a["screen"] = pid;
  }
  return t == "sensor" && size == "m" ? 'h' : size[0];
}

void build_home(lv_obj_t *parent, fw::Props props) {
  Doc deck;
  deserializeJson(deck, fw::spec::deck_json());
  const std::string want = props["page"] | "", start = deck["start"] | "";
  Var page, first;
  for (Var pg : deck["pages"].as<ArduinoJson::JsonArrayConst>()) {
    if (strcmp(pg["kind"] | "", "home")) continue;
    if (first.isNull()) first = pg;
    const std::string id = pg["id"] | "";
    if (want.empty() ? id == start : id == want) page = pg;
  }
  if (page.isNull()) page = first;
  Doc sp;  // the Home as a screen spec
  sp["v"] = 1;
  sp["id"] = "home";
  sp["name"] = page.isNull() ? std::string("Home") : page_name(page);
  sp["header"]["label"] = sp["name"];
  sp["header"]["back"] = !page.isNull() && start != (page["id"] | "");
  {  // header actions (§10): the settings button, or a page's shortcut in its place; the timer's time left beside the time
    const std::string b = page["header_actions"]["button"] | "settings", st = page["header_actions"]["status"] | "none";
    std::string kind;
    for (Var pg : deck["pages"].as<ArduinoJson::JsonArrayConst>())
      if (b == (pg["id"] | "")) kind = pg["kind"] | "";
    const bool sc = b != "settings" && has_icon(kind);
    sp["header"]["settings"] = true;  // Settings stays a real button (Darren 2026-10-25)
    if (sc || st == "timer") {
      if (sc) { sp["header"]["actions"]["shortcut"] = kind; sp["header"]["actions"]["to"] = b; }
      sp["header"]["actions"]["status"] = st;
    }
  }
  sp["layout"] = "cards";  // the Cells layouts one under another, a gutter apart (each fills the width)
  ArduinoJson::JsonArray nodes = sp["nodes"].to<ArduinoJson::JsonArray>();
  // every card's node first (in its own list), then one Cells per run of same-size cards (h counts as m: two readouts
  // in a row share one cell, a "p" pair), each followed by its cards with "in" = that Cells and their slot in it
  Doc tmp;
  ArduinoJson::JsonArray cards = tmp.to<ArduinoJson::JsonArray>();
  std::string kinds;
  for (Var cd : page["cards"].as<ArduinoJson::JsonArrayConst>()) {
    const char k = card_node(cd, deck["pages"], 0, cards);
    if (k) kinds += k;
  }
  size_t i = 0;
  while (i < kinds.size()) {
    const char run = kinds[i] == 'h' ? 'm' : kinds[i];
    const int at = (int) nodes.size();
    ArduinoJson::JsonObject lay = nodes.add<ArduinoJson::JsonObject>();
    lay["c"] = "cells";
    lay["p"]["sample"] = false;
    lay["p"]["min-w"] = run == 's' ? "tile-w" : "home-card-min-w";
    lay["p"]["min-h"] = run == 's' ? "tile-h" : "card-h";
    if (run == 'l') lay["p"]["wide"] = true;
    else if (run != 's') lay["p"]["last"] = "fill";  // a lone card on the last row goes full width (tiles keep columns)
    std::string cells;
    int slot = 0;
    for (; i < kinds.size() && (kinds[i] == 'h' ? 'm' : kinds[i]) == run; i++) {
      const bool half = kinds[i] == 'h';
      if (half) {  // a pair cell: this readout on top, the next one (if a readout) under it
        cells += cells.empty() ? "p" : ",p";
        for (int h = 0; h < 2 && i < kinds.size() && kinds[i] == 'h'; h++, i++) {
          ArduinoJson::JsonObject n = nodes.add<ArduinoJson::JsonObject>();
          n.set(cards[i]);
          n["in"] = at;
          n["slot"] = slot + h;
        }
        slot += 2;
        i--;  // the for loop's i++
        continue;
      }
      cells += cells.empty() ? "c" : ",c";
      ArduinoJson::JsonObject n = nodes.add<ArduinoJson::JsonObject>();
      n.set(cards[i]);
      n["in"] = at;
      n["slot"] = slot++;
    }
    lay["p"]["cells"] = cells;
  }
  std::string json, error;
  serializeJson(sp, json);
  Doc none;
  none.to<ArduinoJson::JsonObject>();
  fw::spec::build(parent, "home", none.as<fw::Props>(), &error, json.c_str());
}
const int reg = fw::add({"home", "Home", "screens", META, build_home});
}  // namespace

// Update path: an entity's change reaches each card through its binds; the deck changing rebuilds the screen (the
// device's reload), so props never change what is built here.
FW_UPDATE(home) { return lv_obj_get_child_count(root) > 0; }
