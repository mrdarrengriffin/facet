// Home Assistant (Settings > Home Assistant): the panel's own link to Home Assistant (core/device.h
// ha_link), so the entity pickers have lists on a bare panel. A screen spec of real components only, built by the spec
// renderer (core/spec.h: Page > Header + list Body, C1-C3), laid out as Settings:
//   header   the Header (back + "Home Assistant" + the screen chrome)
//   body     two groups, each a Section header (size label) over read-only List rows (act none):
//            "connection"  List row "Address" (the URL, or "Not set") and List row "Token" (the last 4 characters
//                          only, "Not set"): set by the action ha.connect (an agent, HA, the workbench's props)
//            "entities"    List row "Entity lists", its sub line the status (Not set up / Ready / Reading Home
//                          Assistant / "N entities" / the failure in words), a Button "Read now" (secondary) in its
//                          end slot: the action ha.entities over the domains already read (the pickers read only their
//                          own domain(s) when they open, screens/screen_set.cpp; nothing read yet = a connection check
//                          over "zone", a few entities)
//   The spec follows the link while the page shows (a 250 ms timer compares ha_link::rev() and builds it again).
//   Read now shows only when the link is set up and not reading.
// Props (the workbench): url, token = ha.connect on build when given (a token typed here stays in the module; the
// workbench's own fetch goes through its host with the host's token). Update path: none needed (rows follow the link).
#include "fw.h"
#include "device.h"
#include "spec.h"

namespace {
using Doc = ArduinoJson::JsonDocument;
namespace hl = fw::dev::ha_link;
constexpr const char *PICK_DOMAINS[] = {"light", "sensor", "binary_sensor", "media_player", "calendar", "weather", "climate", "cover", "lock", "vacuum", "camera",
                                         "switch", "fan", "scene", "script", "select", "input_boolean", "input_select", "automation", "button", "number", "zone"};
// Read now: the domains read so far (each picker reads only its own; sensors alone are ~1000 rows), else a check
std::string read_again() {
  std::string d;
  for (const char *x : PICK_DOMAINS) if (hl::list(x)) d += std::string(d.empty() ? "" : ",") + x;
  return d.empty() ? "zone" : d;
}
std::string status_words() {
  switch (hl::status()) {
    case hl::OFF: return "Not set up";
    case hl::IDLE: return "Ready";
    case hl::LOADING: return "Reading Home Assistant";
    case hl::OK: return std::to_string(hl::count()) + " entities";
    default: return "Not reached: " + hl::why();
  }
}
ArduinoJson::JsonObject node(ArduinoJson::JsonArray n, const char *c) {
  ArduinoJson::JsonObject o = n.add<ArduinoJson::JsonObject>();
  o["c"] = c;
  return o;
}
void group(ArduinoJson::JsonArray n, const char *title) {
  ArduinoJson::JsonObject p = node(n, "section_header")["p"].to<ArduinoJson::JsonObject>();
  p["title"] = title, p["size"] = "label", p["badge"] = false, p["right"] = "none";
}
ArduinoJson::JsonObject row(ArduinoJson::JsonArray n, const char *title) {
  ArduinoJson::JsonObject p = node(n, "list_row")["p"].to<ArduinoJson::JsonObject>();
  p["title"] = title, p["icon"] = "none", p["act"] = "none";
  return p;
}
// the screen as a spec, from the link as it is now
std::string spec_json() {
  Doc sp;
  sp["v"] = 1, sp["id"] = "ha_link", sp["name"] = "Home Assistant", sp["layout"] = "list";
  sp["header"]["label"] = "Home Assistant";
  ArduinoJson::JsonArray n = sp["nodes"].to<ArduinoJson::JsonArray>();
  group(n, "connection");
  ArduinoJson::JsonObject a = row(n, "Address");
  a["trailing"] = "value", a["value"] = hl::url().empty() ? "Not set" : hl::url().c_str();
  ArduinoJson::JsonObject t = row(n, "Token");
  t["trailing"] = "value", t["value"] = hl::has_token() ? hl::token_hint().c_str() : "Not set";
  group(n, "entities");
  const int at = (int) n.size();
  row(n, "Entity lists")["sub"] = status_words();
  if (hl::ready() && hl::status() != hl::LOADING) {
    ArduinoJson::JsonObject b = node(n, "button");
    b["in"] = at, b["slot"] = 0;
    b["p"]["kind"] = "secondary", b["p"]["label"] = "Read now";
    b["on"]["tap"]["do"] = "ha.entities", b["on"]["tap"]["domains"] = read_again();
  }
  std::string out;
  serializeJson(sp, out);
  return out;
}

const char *META = R"({
  "role": "detail",
  "about": "Settings > Home Assistant: the panel's own link to Home Assistant (its address and a long-lived token, kept on the panel; the token is never shown back) and the entity lists the screens' pickers offer, read from Home Assistant on demand. Set with the action ha.connect; Read now = ha.entities.",
  "frame": [480, 480],
  "icons": ["back@glyph"],
  "props": [
    {"name": "url", "about": "Home Assistant's address (ha.connect on open; empty = leave it).", "type": "text", "default": ""},
    {"name": "token", "about": "A long-lived access token (ha.connect on open; empty = keep the one set). The workbench reads through its host with the host's own token.", "type": "text", "default": ""}
  ]
})";

// the spec root, built again in its place when the link changes (hl::rev); its timer goes with it (fx::every)
void mount(lv_obj_t *parent, int32_t at) {
  Doc none;
  std::string error;
  const uint32_t rev = hl::rev();
  lv_obj_t *root = fw::spec::build(parent, "ha_link", none.as<fw::Props>(), &error, spec_json().c_str());
  if (!root) return;
  if (at >= 0) lv_obj_move_to_index(root, at);
  fx::every(root, 250, [parent, root, rev] {
    if (rev == hl::rev()) return;
    const int32_t at = lv_obj_get_index(root);
    lv_obj_delete(root);
    mount(parent, at);
  });
}
void build_page(lv_obj_t *parent, fw::Props p) {
  const std::string pu = p["url"] | "", pt = p["token"] | "";
  if (!pu.empty() && (pu != hl::url() || !pt.empty())) hl::configure(pu, pt);
  mount(parent, -1);
}
const int reg = fw::add({"ha_link", "Home Assistant", "screens", META, build_page});
}  // namespace

FW_UPDATE(ha_link) { return false; }  // url / token = ha.connect on build
