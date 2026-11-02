// Screens (Settings > Screens; the panel lab's Screens screen in WASM): which screens this panel has and
// where each gets its data. On the contract: Screen > Page (fx::page) > Header + list Body, real components only (C1-C3):
//   header   the Header (back + "Screens" + the screen chrome) in the Page's head
//   body     a Section header "on this panel" (size label, as every settings group), then one List row per deck page
//            (deck v2; not the Homes), then one
//            per page kind the deck has none of (off; core/spec.h off_kinds): its name,
//            the source as the sub line (core/sources.h: the deck entry's source, else the screen's first; "Built in"
//            when it has none), and a real Toggle in its end slot (on = the screen is on: a Home card)
//   taps     the Toggle = on / off: kept in this panel's deck (fw::spec::deck_set "on", stored like a sent deck, Home
//            follows the next time it shows), event "deck" <id> on=true|false; the rest of the row opens the screen's
//            page (navigate "screen_set?{"id": <id>}")
// Off = not in the deck (a page you don't want is deleted, deck v2): switching one on adds it at the end, with its spec's
// defaults; the start page can't be switched off.
// Sent: screens sent to this panel (the API, an agent; core/spec.h sent_kinds) that the deck has
// no page of are not built-in kinds, so not on / off: after the list a Section header "sent" a group apart (above:group,
// the spec renderer's rule), one List row each (its name, sub "Sent to this panel") with two real Buttons: "Add to deck"
// (secondary, icon plus: deck_set "on", event "deck" <id> on=true; it then shows in the list above) and "Remove"
// (ghost; first tap turns it into "Confirm" primary, the second reverts that sent screen: fw::spec::revert, event
// "spec" "revert <id>"). The device rebuilds the page after either (spec reload "deck").
// Update path: none needed (the device builds it each time it opens; props: none). Pairs: the components'.
#include "fw.h"
#include "fx.h"
#include "spec.h"
#include "sources.h"

namespace {
using Doc = ArduinoJson::JsonDocument;
lv_obj_t *build(const char *id, lv_obj_t *parent, Doc &props) {
  const fw::Component *c = fw::find(id);
  if (!c) return nullptr;
  const uint32_t before = lv_obj_get_child_count(parent);
  c->build(parent, props.as<fw::Props>());
  return lv_obj_get_child_count(parent) > before ? lv_obj_get_child(parent, -1) : nullptr;
}
// a List row's end slot (shown once it holds something), where its trailing component goes (C8a)
lv_obj_t *end_of(lv_obj_t *row) {
  lv_obj_t *e = fw::spec::slot_of(row, 0);
  if (!e) return row;
  lv_obj_remove_flag(e, LV_OBJ_FLAG_HIDDEN);
  return e;
}
// a group's Section header (size label, as every settings group); not the first: a group apart
void group(lv_obj_t *body, const char *title, bool first) {
  Doc sh;
  sh["title"] = title, sh["size"] = "label", sh["badge"] = false, sh["right"] = "none";
  lv_obj_t *h = build("section_header", body, sh);
  if (h && !first) fx::use(h, "above:group");
}
// a screen's name: its spec's, else the built-in screen's
std::string name_of(const std::string &id) {
  if (const char *j = fw::spec::screen_json(id)) {
    Doc sp;
    deserializeJson(sp, j);
    return sp["name"] | id.c_str();
  }
  if (const fw::Component *c = fw::find(id.c_str())) return c->name;
  return id;
}
struct Row { std::string id; lv_obj_t *toggle; };
struct Sent { std::string id; lv_obj_t *add, *remove; bool armed = false; };
struct Page { lv_obj_t *root = nullptr; std::vector<Row> rows; std::vector<Sent> sent; };
// is o (an emitter) part of b
bool within(lv_obj_t *o, lv_obj_t *b) { for (; o; o = lv_obj_get_parent(o)) if (o == b) return true; return false; }

const char *META = R"({
  "role": "detail",
  "about": "Settings > Screens: every screen of this panel with where its data comes from and an on / off toggle; a screen off leaves Home. Tap a screen for its source and options.",
  "frame": [480, 480],
  "icons": ["back@glyph"],
  "props": []
})";

void build_screens(lv_obj_t *parent, fw::Props) {
  auto *pg = new Page;
  lv_obj_t *root = pg->root = fx::box(parent, "fill");
  lv_obj_add_event_cb(root, [](lv_event_t *e) { delete (Page *) lv_event_get_user_data(e); }, LV_EVENT_DELETE, pg);
  const fx::Page fp = fx::page(root, "list");
  Doc hd;
  hd["label"] = "Screens";
  build("header", fp.head, hd);
  lv_obj_t *c = fp.body;
  group(c, "on this panel", true);

  Doc deck, cat;
  deserializeJson(deck, fw::spec::deck_json());
  fw::src::load(cat);
  // the deck's pages (on, deck order; not the Homes), then the page kinds it has none of (off)
  std::vector<std::pair<std::string, ArduinoJson::JsonVariantConst>> list;
  for (ArduinoJson::JsonVariantConst x : deck["pages"].as<ArduinoJson::JsonArrayConst>())
    if (strcmp(x["kind"] | "", "home")) list.push_back({x["kind"] | "", x});
  for (const std::string &k : fw::spec::off_kinds()) list.push_back({k, ArduinoJson::JsonVariantConst()});
  for (const auto &it : list) {
    const std::string &id = it.first;
    ArduinoJson::JsonVariantConst x = it.second;
    Doc rd;
    rd["title"] = name_of(id).c_str();
    rd["icon"] = "none";
    Doc sp;  // the spec's source parameter: the source shown is the one the screen really uses (sample or not)
    if (const char *j = fw::spec::screen_json(id)) deserializeJson(sp, j);
    ArduinoJson::JsonVariantConst sparam = sp["params"]["source"];
    const char *sv = x["params"]["source"] | (const char *) nullptr;
    rd["sub"] = fw::src::line(cat, id, x, sparam, sv ? std::string(sv) : fw::spec::param_default(sparam)).c_str();
    rd["trailing"] = "none";
    lv_obj_t *r = build("list_row", c, rd);
    if (!r) continue;
    Doc tp;
    tp["on"] = fw::spec::deck_on(x);
    lv_obj_t *tg = build("toggle", end_of(r), tp);  // the real Toggle in the row's end slot
    pg->rows.push_back({id, tg});
    char *nav = strdup(("screen_set?{\"id\":\"" + id + "\"}").c_str());  // freed with the row
    lv_obj_add_event_cb(r, [](lv_event_t *e) { free(lv_event_get_user_data(e)); }, LV_EVENT_DELETE, nav);
    lv_obj_add_event_cb(r, [](lv_event_t *e) {  // after the List row's own tap (event tap + sound)
      auto *o = (lv_obj_t *) lv_event_get_current_target(e);
      if (lv_obj_has_state(o, fw::st::OFF)) return;
      fw::emit(o, "navigate", (const char *) lv_event_get_user_data(e));
    }, LV_EVENT_CLICKED, nav);
  }
  const std::vector<std::string> sent = fw::spec::sent_kinds();
  if (!sent.empty()) {  // the Sent group: screens sent to this panel, not built-in kinds
    group(c, "sent", false);
    for (const std::string &id : sent) {
      Doc rd;
      rd["title"] = name_of(id).c_str();
      rd["icon"] = "none";
      rd["sub"] = "Sent to this panel";
      rd["trailing"] = "none";
      lv_obj_t *r = build("list_row", c, rd);
      if (!r) continue;
      Doc ad, rm;
      ad["kind"] = "secondary", ad["label"] = "Add to deck", ad["icon"] = "plus";
      rm["kind"] = "ghost", rm["label"] = "Remove";
      lv_obj_t *en = end_of(r);
      lv_obj_t *a = build("button", en, ad);
      lv_obj_t *x = build("button", en, rm);
      pg->sent.push_back({id, a, x});
    }
  }
  fw::listen(root, [](lv_obj_t *from, const char *name, const char *detail, void *user) {
    auto *pg = (Page *) user;
    if (!strcmp(name, "tap"))
      for (Sent &s : pg->sent) {
        if (s.add && within(from, s.add)) {
          Doc v;
          v.set(true);
          fw::spec::deck_change(s.add, s.id, "on", v.as<ArduinoJson::JsonVariantConst>());
          return;
        }
        if (!s.remove || !within(from, s.remove)) continue;
        if (!s.armed) {  // confirm first: the button asks again
          s.armed = true;
          Doc cf;
          cf["kind"] = "primary", cf["label"] = "Confirm";
          fw::update_child(fw::find("button"), s.remove, cf.as<fw::Props>());
          return;
        }
        const std::string id = s.id;
        lv_obj_t *em = s.remove;
        Doc out;
        fw::spec::revert(id, out.to<ArduinoJson::JsonObject>());
        fw::emit(em, "spec", ("revert " + id).c_str());
        return;
      }
    if (!strcmp(name, fw::st::SWITCH_EVENT))
      for (const Row &r : pg->rows) {
        if (r.toggle != from) continue;
        const bool on = !strcmp(detail, "on");
        Doc v;
        v.set(on);
        fw::spec::deck_change(from, r.id, "on", v.as<ArduinoJson::JsonVariantConst>());
        return;
      }
    if (lv_obj_t *up = lv_obj_get_parent(pg->root)) fw::notify(up, name, detail);  // the rest goes on up (navigate, back)
  }, pg);
}
const int reg = fw::add({"screens", "Screens", "screens", META, build_screens});
}  // namespace

FW_UPDATE(screens) { return lv_obj_get_child_count(root) > 0; }
