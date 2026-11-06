// Home Assistant (Settings > Home Assistant): the panel's own link to Home Assistant (core/device.h
// ha_link), so the entity pickers have lists on a bare panel (each picker reads its own domain when it opens, and again
// in the background when its list is older than ha_link::STALE_MS: screens/screen_set.cpp; nothing to read by hand). A screen spec of real components only, built by the spec
// renderer (core/spec.h: Page > Header + list Body, C1-C3), laid out as Settings:
//   header   the Header (back + "Home Assistant" + the screen chrome)
//   body     the groups, each a Section header (size label) over read-only List rows (act none):
//            "not reached"  when the link's check or a read failed: why, whole (Text body dim, up to 3 lines)
//            "link this panel"  while the panel is not linked (no address + token, or the check failed) and it serves
//                          its link page (ha_link::page): List row "Link page" (its address) + the QR block to it
//            "connection"  List row "Address" (the URL, or "Not set") and List row "Token" (the last 4 characters
//                          only, "Not set"): set by the action ha.connect (the panel's link page /link, an agent,
//                          HA, the workbench's props)
//            "pairing"     Home Assistant's own connection to the panel (the ESPHome API, ha_link::pairing), only on a
//                          build that has one: List row "Encryption key" (Set by Home Assistant / None / Built in), and
//                          with a key Home Assistant set a Button "Reset pairing" (secondary, hold to confirm: Home
//                          Assistant loses the panel until it adopts it again), its "confirm" = ha_link::reset_pairing()
//                          + a toast; a Text body dim line says what to do next
//   The spec follows the link while the page shows (a 250 ms timer compares ha_link::rev() and the pairing, and builds
//   it again); the spec sits in a fill box that takes the Reset pairing hold (no action of the list: glass only, and
//   POST /fw/pairing/reset on the firmware).
// Props (the workbench): url, token = ha.connect on build when given (a token typed here stays in the module; the
// workbench's own fetch goes through its host with the host's token); pairing = a stand-in for the firmware's key
// (none | unpaired | paired | fixed; only where no firmware set one). Update path: none needed (rows follow the link).
#include "fw.h"
#include "fx.h"
#include "device.h"
#include "spec.h"

namespace {
using Doc = ArduinoJson::JsonDocument;
namespace hl = fw::dev::ha_link;
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
  if (hl::status() == hl::FAIL) {  // why, whole (a row's sub line cuts it): "Home Assistant refused the token."
    group(n, "not reached");
    std::string w = hl::why();
    if (!w.empty()) w[0] = (char) toupper((unsigned char) w[0]), w += ".";
    ArduinoJson::JsonObject x = node(n, "text")["p"].to<ArduinoJson::JsonObject>();
    x["text"] = w.c_str(), x["style"] = "body", x["tone"] = "dim", x["lines"] = 3;
  }
  if ((!hl::ready() || hl::status() == hl::FAIL) && !hl::page().empty()) {  // not linked: the panel's own link page
    group(n, "link this panel");
    ArduinoJson::JsonObject l = row(n, "Link page");
    l["trailing"] = "value", l["value"] = hl::page().c_str();
    ArduinoJson::JsonObject q = node(n, "qr_block")["p"].to<ArduinoJson::JsonObject>();
    q["text"] = hl::page().c_str(), q["caption"] = "open on your phone";
  }
  group(n, "connection");
  ArduinoJson::JsonObject a = row(n, "Address");
  a["trailing"] = "value", a["value"] = hl::url().empty() ? "Not set" : hl::url().c_str();
  ArduinoJson::JsonObject t = row(n, "Token");
  t["trailing"] = "value", t["value"] = hl::has_token() ? hl::token_hint().c_str() : "Not set";
  const hl::Pairing pr = hl::pairing();
  if (pr != hl::NO_PAIRING) {
    group(n, "pairing");
    ArduinoJson::JsonObject k = row(n, "Encryption key");
    k["trailing"] = "value", k["value"] = pr == hl::PAIRED ? "Set by Home Assistant" : pr == hl::FIXED ? "Built in" : "None";
    if (pr == hl::PAIRED) {
      ArduinoJson::JsonObject b = node(n, "button")["p"].to<ArduinoJson::JsonObject>();
      b["kind"] = "secondary", b["label"] = "Reset pairing", b["hold"] = true, b["hold_label"] = "Resetting", b["value"] = "reset_pairing";
    }
    ArduinoJson::JsonObject x = node(n, "text")["p"].to<ArduinoJson::JsonObject>();
    x["style"] = "body", x["tone"] = "dim", x["lines"] = 3;
    x["text"] = pr == hl::PAIRED ? "Home Assistant says the encryption key is invalid? Hold Reset pairing, then add the panel again in Home Assistant."
              : pr == hl::FIXED ? "The key is part of this firmware: change it in the firmware's settings."
                                : "Home Assistant can add this panel now and give it a new key (Settings > Devices & services).";
  }
  std::string out;
  serializeJson(sp, out);
  return out;
}

const char *META = R"({
  "role": "detail",
  "about": "Settings > Home Assistant: the panel's own link to Home Assistant (its address and a long-lived token, kept on the panel; the token is never shown back) Set with the action ha.connect. The entity pickers read their own lists when they open (again in the background when a list is old).",
  "frame": [480, 480],
  "icons": ["back@glyph"],
  "props": [
    {"name": "url", "about": "Home Assistant's address (ha.connect on open; empty = leave it).", "type": "text", "default": ""},
    {"name": "token", "about": "A long-lived access token (ha.connect on open; empty = keep the one set). The workbench reads through its host with the host's own token.", "type": "text", "default": ""},
    {"name": "page", "about": "The panel's own link page, shown as a QR code while it is not linked (the panel sets it: http://<its address>/link).", "type": "text", "default": ""},
    {"name": "pairing", "about": "Home Assistant's encryption key on the panel, as a stand-in where the firmware has not set it: none (no pairing group), unpaired, paired (Reset pairing shows) or fixed.", "type": "select", "options": ["none", "unpaired", "paired", "fixed"], "default": "none"}
  ]
})";

// the workbench's stand-in key (the pairing prop): kept here, Reset pairing clears it like the firmware's
hl::Pairing fake = hl::NO_PAIRING;
hl::Pairing fake_state() { return fake; }
bool fake_reset() { return fake = hl::UNPAIRED, true; }

// the spec in a fill box, built again in its place when the link or the pairing changes (hl::rev); its timer goes
// with it (fx::every); the box takes Reset pairing's hold, anything else goes on up (the Header's back: the device)
void mount(lv_obj_t *parent, int32_t at) {
  Doc none;
  std::string error;
  const uint32_t rev = hl::rev();
  const hl::Pairing pr = hl::pairing();
  lv_obj_t *root = fx::box(parent, "fill");
  if (at >= 0) lv_obj_move_to_index(root, at);
  if (!fw::spec::build(root, "ha_link", none.as<fw::Props>(), &error, spec_json().c_str())) { lv_obj_delete(root); return; }
  fw::listen(root, [](lv_obj_t *from, const char *name, const char *detail, void *user) {
    if (!strcmp(name, "confirm") && !strcmp(detail, "reset_pairing")) {
      const bool ok = hl::reset_pairing();
      Doc t;
      t["tone"] = ok ? "ok" : "warn", t["icon"] = ok ? "check" : "alert", t["title"] = ok ? "Pairing reset" : "Nothing to reset";
      t["body"] = ok ? "Add the panel again in Home Assistant: it asks for no key now and sets a new one." : "This panel holds no key from Home Assistant.";
      fw::dev::toast(t.as<fw::Props>());
      return;
    }
    if (lv_obj_t *up = lv_obj_get_parent((lv_obj_t *) user)) fw::notify(up, name, detail);
  }, root);
  fx::every(root, 250, [parent, root, rev, pr] {
    if (rev == hl::rev() && pr == hl::pairing()) return;
    const int32_t at = lv_obj_get_index(root);
    lv_obj_delete(root);
    mount(parent, at);
  });
}
void build_page(lv_obj_t *parent, fw::Props p) {
  const std::string pu = p["url"] | "", pt = p["token"] | "", pg = p["page"] | "", pp = p["pairing"] | "";
  if (!pg.empty()) hl::set_page(pg);
  if (!pp.empty() && (hl::pairing() == hl::NO_PAIRING || fake != hl::NO_PAIRING)) {  // never over the firmware's key
    fake = pp == "paired" ? hl::PAIRED : pp == "fixed" ? hl::FIXED : pp == "unpaired" ? hl::UNPAIRED : hl::NO_PAIRING;
    hl::on_pairing(fake_state, fake_reset);
  }
  if (!pu.empty() && (pu != hl::url() || !pt.empty())) hl::configure(pu, pt);
  mount(parent, -1);
}
const int reg = fw::add({"ha_link", "Home Assistant", "screens", META, build_page});
}  // namespace

FW_UPDATE(ha_link) { return false; }  // url / token = ha.connect on build
