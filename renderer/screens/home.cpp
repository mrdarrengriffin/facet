// Home: a room's page (screen model §4, piece 2). Each Home in the deck (core/spec.h deck v2) holds its own cards; this
// screen turns the Home page into a screen spec made only of real components and builds it with the spec renderer
// (core/spec.h build: binds to the entities' live state, the tap actions, the header):
//   header   the Header (label = the page name; Back unless it is the start page) + the screen chrome
//   content  one Card grid (components/card_grid.cpp: the §4 packing, s / m / l rows) holding one node per card, in
//            the deck's order, from the card type catalogue (CARD_TYPES: the component per size):
//              light   s Entity tile (bulb / power icon, "64%" / "Off", on = accent) | m Card (value = the level)
//              cover   s Entity tile (opens Blinds) | m Card (position, state)
//              climate m Card (current temperature, state)
//              media   m / l Card in text mode (the track, the state)
//              sensor  m State readout (read-only: no box, no tap); two in a row share one card slot
//              page    s Entity tile / m Card / l wide Card: the page's name (opens it)
//            label = the card's label, else the entity's friendly name (a bind). Unavailable entities show the
//            components' own unavailable look.
//   taps     toggle = light.toggle (a light or a switch); detail = nav.open of the type's detail screen for that
//            entity; page = nav.open of that deck page; none = nothing (sensors)
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
    {"name": "page", "type": "text", "default": ""}
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

// one deck card -> one spec node in slot `slot` of node 0 (the Card grid); returns its cell size ('s', 'm', 'l', 'h')
char card_node(Var cd, Var pages, int slot, ArduinoJson::JsonArray nodes) {
  const std::string t = cd["t"] | "", e = cd["e"] | "", size = cd["size"] | "m", label = cd["label"] | "";
  const fw::spec::CardType *ct = fw::spec::card_type(t);
  if (!ct) return 0;
  std::string tap = cd["tap"] | "";
  if (tap.empty()) tap = std::string(ct->taps).substr(0, std::string(ct->taps).find(','));
  const char *comp = size == "s" ? ct->comp_s : size == "l" ? ct->comp_l : ct->comp_m;
  if (!*comp) return 0;
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
    if (t == "page") {
      p["icon"] = has_icon(kind) ? "screen-" + kind : std::string("screen-home");
      p["status"] = "";
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
    p["act"] = tap == "toggle" ? "toggle" : "open";
    if (t != "page") bind(p, "state", e, "avail");
  } else if (comp_s == "card") {
    p["size"] = size;
    p["preview"] = "none";
    p["sub"] = "";
    p["value"] = "";
    p["mode"] = "value";
    if (t == "page") p["icon"] = has_icon(kind) ? kind : std::string("none");
    else if (t == "light") { p["icon"] = "light"; bind(p, "value", e, "level"); bind(p, "accent", e, "on"); }
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
    p["half"] = true;
    bind(p, "state", e, "state");
    ArduinoJson::JsonObject ic = p["icon"].to<ArduinoJson::JsonObject>();
    ic["e"] = e;
    ic["a"] = "device_class";
    static const char *const MAP[][2] = {{"temperature", "thermometer"}, {"door", "door"}, {"garage_door", "door"},
        {"opening", "door"}, {"window", "window"}, {"motion", "person"}, {"occupancy", "person"}, {"presence", "person"},
        {"moisture", "alert"}, {"power", "bolt"}, {"energy", "bolt"}, {"current", "bolt"}, {"voltage", "bolt"}};
    for (auto &m : MAP) ic["map"][m[0]] = m[1];
    ic["else"] = "house";
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
  } else if (tap == "page" && !pid.empty()) {
    ArduinoJson::JsonObject a = n["on"]["tap"].to<ArduinoJson::JsonObject>();
    a["do"] = "nav.open";
    a["screen"] = pid;
  }
  return t == "sensor" ? 'h' : size[0];
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
  sp["header"]["settings"] = true;
  sp["layout"] = "list";
  ArduinoJson::JsonArray nodes = sp["nodes"].to<ArduinoJson::JsonArray>();
  ArduinoJson::JsonObject grid = nodes.add<ArduinoJson::JsonObject>();
  grid["c"] = "card_grid";
  grid["p"]["sample"] = false;
  std::string cells;
  int slot = 0;
  for (Var cd : page["cards"].as<ArduinoJson::JsonArrayConst>()) {
    const char k = card_node(cd, deck["pages"], slot, nodes);
    if (!k) continue;
    cells += (cells.empty() ? "" : ",") + std::string(1, k);
    slot++;
  }
  grid["p"]["cells"] = cells;
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
