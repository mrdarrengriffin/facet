// API keys (Settings > API keys): the keys scripts, automations and agents use to change this panel (core/keys.h), laid out as Settings
// (Screen > Page > Header + list Body, C1-C3), real components only:
//   header   the Header (back + "API keys" + the screen chrome)
//   body     a Section header (size label) per group:
//            "off"      while Allow API access is off: why nothing works, in words (Text body dim)
//            "keys"     a read-out List row per key (act none): its name, the sub line "Used 2 h ago · Ends in 29 days";
//                       none yet = a Text body dim line saying how one is made; with keys, a Button "Revoke all"
//                       (secondary, hold to confirm: a key must not go by accident), its "confirm" = keys::revoke("all")
//                       + an ok toast
//            "how"      one Text body dim line: what a key can do, how it is asked for
//   The page follows the keys and the switch (a 250 ms timer compares keys::rev() and builds it again in its place;
//   used / ends words move with the clock on the next change). Props: none. Update path: none needed.
#include "fw.h"
#include "fx.h"
#include "device.h"
#include "keys.h"

namespace {
using Doc = ArduinoJson::JsonDocument;
namespace k = fw::keys;

lv_obj_t *part(const char *id, lv_obj_t *parent, Doc &p) {
  const fw::Component *c = fw::find(id);
  if (!c) return nullptr;
  const uint32_t n = lv_obj_get_child_count(parent);
  c->build(parent, p.as<fw::Props>());
  return lv_obj_get_child_count(parent) > n ? lv_obj_get_child(parent, -1) : nullptr;
}
void group(lv_obj_t *body, const char *title, bool first) {
  Doc sh;
  sh["title"] = title, sh["size"] = "label", sh["badge"] = false, sh["right"] = "none";
  lv_obj_t *h = part("section_header", body, sh);
  if (h && !first) fx::use(h, "above:group");
}
void words(lv_obj_t *body, const std::string &s) {
  Doc t;
  t["text"] = s.c_str(), t["style"] = "body", t["tone"] = "dim", t["lines"] = 3;
  part("text", body, t);
}

const char *META = R"({
  "role": "detail",
  "about": "Settings > API keys: the keys scripts, automations and agents use to change this panel, each with its name, when it was last used and when it ends, and Revoke all. A script, automation or agent asks the panel for a key; the person taps Allow on the glass.",
  "frame": [480, 480],
  "icons": ["back@glyph"],
  "props": []
})";

// the page as the keys are now; a Revoke all hold (event "confirm") revokes every key, anything else goes on up (the
// device runtime: the Header's back)
void page(lv_obj_t *root) {
  const fx::Page fp = fx::page(root, "list");
  Doc hd;
  hd["label"] = "API keys";
  part("header", fp.head, hd);
  lv_obj_t *c = fp.body;
  bool first = true;
  if (!k::on()) {
    group(c, "off", true), first = false;
    words(c, "Allow API access is off, so no key works and none can be asked for. Turn it on in Settings.");
  }
  group(c, "keys", first);
  if (k::list().empty()) words(c, "No keys yet. A script, automation or agent asks the panel for one with its name, and you tap Allow here.");
  for (const k::Key &x : k::list()) {
    Doc r;
    const std::string sub = k::used_words(x) + " \xC2\xB7 " + k::ends_words(x);
    r["title"] = x.name.c_str(), r["icon"] = "none", r["act"] = "none", r["sub"] = sub.c_str();
    part("list_row", c, r);
  }
  if (!k::list().empty()) {
    Doc b;
    b["kind"] = "secondary", b["label"] = "Revoke all", b["hold"] = true, b["hold_label"] = "Revoking", b["value"] = "revoke_all";
    part("button", c, b);
  }
  group(c, "how", false);
  words(c, "A key can change screens and settings, as the panel's editor can. Plain HTTP on your own Wi-Fi.");
}
void mount(lv_obj_t *parent, int32_t at) {
  lv_obj_t *root = fx::box(parent, "fill");
  if (at >= 0) lv_obj_move_to_index(root, at);
  page(root);
  fw::listen(root, [](lv_obj_t *from, const char *name, const char *detail, void *user) {
    if (!strcmp(name, "confirm") && !strcmp(detail, "revoke_all")) {
      const int n = k::revoke("all");
      Doc t;
      const std::string body = n == 1 ? "1 key revoked. Each needs a new one to change this panel." : std::to_string(n) + " keys revoked. Each needs a new one to change this panel.";
      t["tone"] = "ok", t["icon"] = "check", t["title"] = "Keys revoked", t["body"] = body.c_str();
      fw::dev::toast(t.as<fw::Props>());
      return;
    }
    if (lv_obj_t *up = lv_obj_get_parent((lv_obj_t *) user)) fw::notify(up, name, detail);
  }, root);
  const uint32_t rev = k::rev();
  fx::every(root, 250, [parent, root, rev] {
    if (rev == k::rev()) return;
    const int32_t at = lv_obj_get_index(root);
    lv_obj_delete(root);
    mount(parent, at);
  });
}
void build_page(lv_obj_t *parent, fw::Props) { mount(parent, -1); }
const int reg = fw::add({"agent_keys", "API keys", "screens", META, build_page});
}  // namespace

FW_UPDATE(agent_keys) { return false; }  // the page follows the keys itself
