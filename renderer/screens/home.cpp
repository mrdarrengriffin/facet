// Home: a room's page (screen model §4, piece 2). Each Home in the deck (core/spec.h deck v2) holds its own cards; this
// screen turns the Home page into a screen spec made only of real components and builds it with the spec renderer
// (core/spec.h build: binds to the entities' live state, the tap actions, the header):
//   header   the Header (label = the page name; Back unless it is the start page) + the screen chrome
//   content  one Grid (components/grid.cpp): square cells, the size class's column count (tokens/layout.json
//            homeCols, 3 on the square panels), rows half a square; each card spans its size: 1x1, 2x1, 1x2, 2x2 or
//            half (a half under a half shares one square); 1x2 is placed but not offered (core/spec.h GRID_SIZES).
//            Every type at every size: 1x1 = the Entity tile,
//            half = the Entity tile's half (icon + name in a small head, the state under it), bigger = the Card (the full step shows the
//            type's preview, the compact step drops it); the State readout (type readout) = itself, half only (its
//            meta sizes). Old sizes: s = 1x1, m = 2x1 (a sensor's or a readout's m = half), l = 2x2.
//            What the Card shows per type:
//              light   the level (accent while on), the state; no preview (a card holds no toggle)
//              cover   the state, the window preview at the position
//              media   text mode: the track, the record preview (the album art on it), the artist
//              action  (script, scene, automation, button) its verb
//              sensor  and anything else: its state (read-only: tap none)
//              page    the screen's own card (its spec's "card"), else its name
//              entity  ANY entity (after HA's tile card): drawn like its domain's nearest type (light,
//                      cover, sensor; a script / scene / button with its verb; anything else its state); taps by
//                      domain (core/spec.h ENTITY_TAPS): toggle, detail (the domain's screen), run, none
//            label = the card's label, else the entity's friendly name (a bind). Unavailable entities show the
//            components' own unavailable look.
//   taps     toggle = light.toggle (a light or a switch); detail = nav.open of the type's detail screen for that
//            entity; page = nav.open of that deck page; none = nothing (sensors); run = script.run / scene.activate /
//            automation.trigger / button.press (an action card)
// Header actions (the deck page's header_actions, screen model §10): button settings (default) or a page id = that
// page's shortcut, a gutter before the settings button (which always stays; a long press on the time opens Settings
// too); status timer = the panel's timer, time left beside the time; buttons = square Icon buttons in the header's end
// (each with a card's tap: its entity's domain's, or a page; one whose screen this build lacks is left out).
// Prop page = the Home's page id ("" = the deck's start page if it is a Home, else the first Home). With no Home in the
// deck: the header and an empty grid.
// Update path: the deck changing rebuilds the screen (the device reloads it); entity changes move each card through
// its own update path (binds), so nothing else changes with props.
#include <string>
#include "fw.h"
#include "fx.h"
#include "spec.h"
#include "device.h"

namespace {
using Doc = ArduinoJson::JsonDocument;
using Var = ArduinoJson::JsonVariantConst;

const char *META = R"({
  "about": "A room's Home: its cards from the deck on a grid of squares (1 by 1, 2 by 1, 1 by 2, 2 by 2 or a half), live from the entities. Tap a tile to toggle, a card to open its screen.",
  "frame": [480, 480],
  "icons": ["screen-settings@icon", "bulb@icon", "power@icon"],
  "props": [
    {"name": "page", "about": "Which page of the deck to show, by its id, such as kitchen. Leave empty for the start page.", "type": "text", "default": ""},
    {"name": "editing", "about": "In the editor: an empty Home shows only its grid (the editor draws the squares to add to), not the Add your cards box.", "type": "bool", "default": false, "debug": true}
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
// a card's (or a header button's) tap as the node's on.tap: toggle, detail (its domain's screen for the entity), run
// (the domain's action), page (that deck page); none = no tap
void set_tap(ArduinoJson::JsonObject n, const std::string &tap, const std::string &e, const std::string &detail, const std::string &pid, const std::string &label) {
  const std::string dom = e.substr(0, e.find('.'));
  if (tap == "toggle") {
    ArduinoJson::JsonObject a = n["on"]["tap"].to<ArduinoJson::JsonObject>();
    a["do"] = dom == "light" || dom == "switch" ? "light.toggle" : "entity.toggle";
    a["e"] = e;
  } else if (tap == "detail" && !detail.empty()) {
    ArduinoJson::JsonObject a = n["on"]["tap"].to<ArduinoJson::JsonObject>();
    a["do"] = "nav.open";
    a["screen"] = detail;
    a["params"]["entity"] = e;
    if (!label.empty()) a["params"]["name"] = label;
  } else if (tap == "run") {
    ArduinoJson::JsonObject a = n["on"]["tap"].to<ArduinoJson::JsonObject>();
    a["do"] = dom == "script" ? "script.run" : dom == "scene" ? "scene.activate" : dom == "automation" ? "automation.trigger" : "button.press";
    a["e"] = e;
  } else if (tap == "page" && !pid.empty()) {
    ArduinoJson::JsonObject a = n["on"]["tap"].to<ArduinoJson::JsonObject>();
    a["do"] = "nav.open";
    a["screen"] = pid;
  }
}
// a header button (header_actions.buttons) as a square Icon button node in the header's end; false = left out (its
// screen isn't in this build)
bool button_node(Var b, Var pages, ArduinoJson::JsonArray nodes) {
  const std::string t = b["t"] | "entity", e = b["e"] | "", pid = b["page"] | "";
  const fw::spec::DomainTaps &dt = fw::spec::entity_taps(e.substr(0, e.find('.')));
  std::string tap = t == "page" ? "page" : (b["tap"] | "");
  if (tap.empty()) tap = std::string(dt.taps).substr(0, std::string(dt.taps).find(','));
  std::string to = tap == "detail" ? dt.detail : "";
  if (tap == "page") for (Var pg : pages.as<ArduinoJson::JsonArrayConst>()) if (pid == (pg["id"] | "")) to = pg["kind"] | "";
  if ((tap == "page" || tap == "detail") && (to.empty() || !fw::spec::screen_known(to))) return false;
  ArduinoJson::JsonObject n = nodes.add<ArduinoJson::JsonObject>();
  n["c"] = "icon_button";
  n["p"]["style"] = "square";
  if (tap == "page") {  // a screen button wears its screen's own icon: its Home card's icon (spec "card", C52), else its kind's
    Doc sp;
    const char *j = fw::spec::screen_json(to);
    const std::string ci = j && !deserializeJson(sp, j) ? std::string(sp["card"]["icon"] | "") : std::string();
    n["p"]["icon"] = "screen-" + (!ci.empty() ? ci : has_icon(to) ? to : std::string("home"));
  } else n["p"]["icon"] = b["icon"] | "next";
  n["in"] = "header";
  n["slot"] = "end";
  set_tap(n, tap, e, dt.detail, pid, "");
  return true;
}
// one deck card -> one spec node in slot `slot` of node 0 (the Grid); returns false when it is left out
bool card_node(Var cd, Var pages, int slot, ArduinoJson::JsonArray nodes) {
  const std::string t = cd["t"] | "", e = cd["e"] | "", label = cd["label"] | "", size = fw::spec::card_grid_size(t, cd["size"] | "m");
  const fw::spec::CardType *ct = fw::spec::card_type(t);
  if (!ct) return false;
  std::string tap = cd["tap"] | "";
  const std::string edom = e.substr(0, e.find('.'));
  const fw::spec::DomainTaps &dt = fw::spec::entity_taps(edom);
  const std::string taps = t == "entity" ? dt.taps : ct->taps, detail = t == "entity" ? dt.detail : ct->detail;
  if (tap.empty()) tap = taps.substr(0, taps.find(','));
  // every type at every size: a square or a half = the Entity tile, bigger = the Card; a State readout = itself (half)
  const char *comp = fw::spec::card_comp(t, size);
  {  // a card that opens a screen this build lacks (its feature is off, features.json; a custom deck can still name
     // one) is left out, so no card leads nowhere
    std::string to = tap == "detail" ? detail : "";
    if (t == "page" || tap == "page") {
      const std::string id = cd["page"] | "";
      to.clear();
      for (Var pg : pages.as<ArduinoJson::JsonArrayConst>())
        if (id == (pg["id"] | "")) to = pg["kind"] | "";
      if (to.empty()) return false;
    }
    if (!to.empty() && !fw::spec::screen_known(to)) return false;
  }
  ArduinoJson::JsonObject n = nodes.add<ArduinoJson::JsonObject>();
  n["c"] = comp;
  n["in"] = 0;
  n["slot"] = slot;
  {  // the span in the grid's half rows: a square is two
    ArduinoJson::JsonArray sp = n["span"].to<ArduinoJson::JsonArray>();
    sp.add(size[0] == '2' ? 2 : 1);
    sp.add(size == "half" ? 1 : size == "1x2" || size == "2x2" ? 4 : 2);
  }
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
  // the Entity tile (any entity) draws like the type its domain is closest to: a light / switch / fan / input boolean /
  // automation as a light (on = accent), a cover as a cover, a sensor as a sensor, a script / scene / button as an
  // action (its verb), anything else its state
  const std::string vt = t != "entity" ? t
      : (edom == "light" || edom == "switch" || edom == "fan" || edom == "input_boolean" || edom == "automation") ? "light"
      : edom == "cover" ? "cover" : (edom == "sensor" || edom == "binary_sensor") ? "sensor"
      : (edom == "script" || edom == "scene" || edom == "button") ? "action" : "other";
  if (comp_s == "readout") {  // a State readout (half only): no box, no tap; the state, the accent while on, offline = mute
    sensor_icon(p, e);
    bind(p, "state", e, "state");
    ArduinoJson::JsonObject tone = p["tone"].to<ArduinoJson::JsonObject>();
    tone["e"] = e;
    tone["map"]["on"] = "accent";
    tone["map"]["unavailable"] = "mute";
    tone["else"] = "text";
  } else if (comp_s == "entity_tile") {
    if (size == "half") p["shape"] = "half";
    if (vt == "sensor") {  // read-only: the value as the status, the device class icon, no press
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
    } else if (vt == "action") {  // run it: the domain's icon, the verb as the status, no on look
      p["icon"] = dom == "script" ? "play" : dom == "scene" ? "house" : dom == "automation" ? "auto" : "bolt";
      p["status"] = dom == "script" ? "Run" : dom == "scene" ? "Activate" : dom == "automation" ? "Trigger" : "Press";
      p["on"] = false;
    } else if (vt == "other") {  // its state (a lock, a climate, a media player, anything else)
      p["icon"] = dom == "climate" ? "thermometer" : dom == "media_player" ? "speaker" : dom == "lock" ? "door" : dom == "vacuum" ? "vacuum" : "check";
      bind(p, "status", e, "state");
      p["on"] = false;
    } else if (vt == "cover") {
      p["icon"] = "screen-covers";
      bind(p, "status", e, "state");
      ArduinoJson::JsonObject on = p["on"].to<ArduinoJson::JsonObject>();
      on["e"] = e;
      on["map"]["open"] = "on";
      on["map"]["opening"] = "on";
      on["else"] = "off";
    } else {
      p["icon"] = dom == "light" ? "bulb" : dom == "automation" ? "auto" : "power";
      bind(p, "status", e, "level");
      bind(p, "on", e, "on");
    }
    p["act"] = (t == "sensor" || tap == "none") ? "none" : tap == "toggle" ? "toggle" : "open";
    if (t != "page") bind(p, "state", e, "avail");
  } else if (comp_s == "card") {  // the card fills its cell (no size prop: the Card grid gives it its size)
    p["preview"] = "none";
    p["sub"] = "";
    p["value"] = "";
    p["mode"] = "value";
    if (t == "page") {
      p["icon"] = has_icon(kind) ? kind : std::string("none");
      page_card(p, target, kind);  // the screen's own card (its spec's "card", on its feeds), when it has one
    } else if (vt == "light") {  // the level, the state under it (no preview: a card holds no toggle)
      p["icon"] = "light";
      bind(p, "value", e, "level");
      bind(p, "accent", e, "on");
      bind(p, "sub", e, "state");
    } else if (t == "media" || dom == "media_player") {  // the track, the record preview, the artist
      p["icon"] = "media";
      p["preview"] = "media";
      p["mode"] = "text";
      bind(p, "text", e, "", "media_title");
      bind(p, "sub", e, "", "media_artist");
      bind(p, "art", e, "art", "entity_picture");  // the album art on the record (the Media feed's art)
    } else if (vt == "cover") {  // the state, the window preview at the position
      p["icon"] = "covers";
      p["preview"] = "covers";
      bind(p, "value", e, "state");
      bind(p, "position", e, "num", "current_position");
    } else if (vt == "action") {  // the verb (what a tap does)
      p["icon"] = "none";
      p["value"] = dom == "script" ? "Run" : dom == "scene" ? "Activate" : dom == "automation" ? "Trigger" : "Press";
    } else {  // a sensor, a climate, anything else: its state
      p["icon"] = dom == "climate" ? "thermostat" : "none";
      bind(p, "value", e, "state");
    }
    if (t != "page") bind(p, "state", e, "avail");
  }
  set_tap(n, tap, e, detail, pid, label);
  return true;
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
    sp["header"]["settings"] = true;  // Settings stays a real button
    if (sc || st == "timer") {
      if (sc) { sp["header"]["actions"]["shortcut"] = kind; sp["header"]["actions"]["to"] = b; }
      sp["header"]["actions"]["status"] = st;
    }
  }
  sp["layout"] = "cards";
  ArduinoJson::JsonArray nodes = sp["nodes"].to<ArduinoJson::JsonArray>();
  {  // node 0: the Grid, the size class's column count (tokens/layout.json homeCols), rows half a square
    ArduinoJson::JsonObject g = nodes.add<ArduinoJson::JsonObject>();
    g["c"] = "grid";
    g["p"]["cols"] = fx::home_cols();
    g["p"]["aspect"] = "half";
  }
  int slot = 0;
  for (Var cd : page["cards"].as<ArduinoJson::JsonArrayConst>())
    if (card_node(cd, deck["pages"], slot, nodes)) slot++;
  if (!slot && !props["editing"].as<bool>()) {  // no cards yet (the editor draws its own empty squares: none here) (a fresh panel, Back to factory): where to add them, the panel's own address
    ArduinoJson::JsonObject n = nodes.add<ArduinoJson::JsonObject>();
    n["c"] = "empty_state";
    n["p"]["icon"] = "plus";
    n["p"]["tone"] = "idle";
    n["p"]["title"] = "Add your cards";
    std::string at = fw::dev::ha_link::page();  // http://<the panel>/link once on Wi-Fi ("" = not known: the workbench)
    if (at.size() > 5 && at.compare(at.size() - 5, 5, "/link") == 0) at.resize(at.size() - 5);
    n["p"]["body"] = at.empty() ? std::string("Open the panel's address in a browser") : "Open " + at + " in a browser";
  }
  for (Var b : page["header_actions"]["buttons"].as<ArduinoJson::JsonArrayConst>()) button_node(b, deck["pages"], nodes);
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
