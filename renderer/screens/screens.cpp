// Screens (Settings > Screens, 2026-10-23; the panel lab's Screens screen in WASM): which screens this panel has and
// where each gets its data. Composed only of real components:
//   header   the Header (back + "Screens" + the screen chrome) at (inset, inset)
//   content  from lay::top, one scrolling column: the label "on this panel" (screen type label, text-dim, as the
//            Settings group labels), section-gap, then one List row per deck screen (not Home / Settings): its name,
//            the source as the sub line (core/sources.h: the deck entry's source, else the screen's first; "Built in"
//            when it has none), and a real Toggle appended as its trailing part (on = the screen is on: a Home card)
//   taps     the Toggle = on / off: kept in this panel's deck (fw::spec::deck_set "on", stored like a sent deck, Home
//            follows the next time it shows), event "deck" <id> on=true|false; the rest of the row opens the screen's
//            page (navigate "screen_set?{"id": <id>}")
// Off screens stay in the deck (their picks kept), so switching one back on brings it back as it was.
// Update path: none needed (the device builds it each time it opens; props: none). Pairs: the components'.
#include "fw.h"
#include "spec.h"
#include "sources.h"

namespace {
using Doc = ArduinoJson::JsonDocument;
lv_obj_t *box(lv_obj_t *parent) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  return o;
}
lv_obj_t *build(const char *id, lv_obj_t *parent, Doc &props) {
  const fw::Component *c = fw::find(id);
  if (!c) return nullptr;
  const uint32_t before = lv_obj_get_child_count(parent);
  c->build(parent, props.as<fw::Props>());
  return lv_obj_get_child_count(parent) > before ? lv_obj_get_child(parent, -1) : nullptr;
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
struct Page { lv_obj_t *root = nullptr; std::vector<Row> rows; };

const char *META = R"({
  "about": "Settings > Screens: every screen of this panel with where its data comes from and an on / off toggle; a screen off leaves Home. Tap a screen for its source and options.",
  "frame": [480, 480],
  "icons": ["back@glyph"],
  "props": []
})";

void build_screens(lv_obj_t *parent, fw::Props) {
  const fw::Theme &t = *fw::T;
  auto *pg = new Page;
  lv_obj_t *root = pg->root = box(parent);
  lv_obj_set_size(root, fw::lay::screen_w, fw::lay::screen_h);
  lv_obj_set_style_bg_color(root, t.bg, 0);
  lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
  lv_obj_add_event_cb(root, [](lv_event_t *e) { delete (Page *) lv_event_get_user_data(e); }, LV_EVENT_DELETE, pg);
  Doc hd;
  hd["label"] = "Screens";
  if (lv_obj_t *h = build("header", root, hd)) lv_obj_set_pos(h, fw::lay::inset, fw::lay::inset);

  lv_obj_t *c = box(root);
  lv_obj_add_flag(c, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(c, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(c, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_pos(c, fw::lay::inset, fw::lay::top);
  lv_obj_set_size(c, fw::lay::content_w, fw::lay::screen_h - fw::lay::top);
  lv_obj_set_style_pad_bottom(c, fw::lay::inset, 0);
  lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
  lv_obj_t *gl = fw::text(c, fw::ty::label, "on this panel", t.text_dim);
  lv_obj_remove_flag(gl, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_height(gl, fw::style(fw::ty::label).box);
  lv_obj_t *gap = box(c);
  lv_obj_set_size(gap, 1, fw::lay::section_gap);

  Doc deck, cat;
  deserializeJson(deck, fw::spec::deck_json());
  fw::src::load(cat);
  for (ArduinoJson::JsonVariantConst x : deck["screens"].as<ArduinoJson::JsonArrayConst>()) {
    const std::string id = x["id"] | "";
    if (id.empty() || id == "settings" || id == "home") continue;
    Doc rd;
    rd["title"] = name_of(id).c_str();
    rd["icon"] = "none";
    rd["sub"] = fw::src::line(cat, id, x).c_str();
    rd["trailing"] = "none";
    lv_obj_t *r = build("list_row", c, rd);
    if (!r) continue;
    Doc tp;
    tp["on"] = fw::spec::deck_on(x);
    lv_obj_t *tg = build("toggle", r, tp);  // the real Toggle as the row's trailing part (Settings' inline controls)
    pg->rows.push_back({id, tg});
    char *nav = strdup(("screen_set?{\"id\":\"" + id + "\"}").c_str());  // freed with the row
    lv_obj_add_event_cb(r, [](lv_event_t *e) { free(lv_event_get_user_data(e)); }, LV_EVENT_DELETE, nav);
    lv_obj_add_event_cb(r, [](lv_event_t *e) {  // after the List row's own tap (event tap + sound)
      auto *o = (lv_obj_t *) lv_event_get_current_target(e);
      if (lv_obj_has_state(o, fw::st::OFF)) return;
      fw::emit(o, "navigate", (const char *) lv_event_get_user_data(e));
    }, LV_EVENT_CLICKED, nav);
  }
  fw::listen(root, [](lv_obj_t *from, const char *name, const char *detail, void *user) {
    auto *pg = (Page *) user;
    if (!strcmp(name, fw::st::SWITCH_EVENT))
      for (const Row &r : pg->rows) {
        if (r.toggle != from) continue;
        const bool on = !strcmp(detail, "on");
        Doc v;
        v.set(on);
        fw::spec::deck_set(r.id, "on", v.as<ArduinoJson::JsonVariantConst>());
        fw::emit(from, "deck", (r.id + (on ? " on=true" : " on=false")).c_str());
        return;
      }
    if (lv_obj_t *up = lv_obj_get_parent(pg->root)) fw::notify(up, name, detail);  // the rest goes on up (navigate, back)
  }, pg);
}
const int reg = fw::add({"screens", "Screens", "screens", META, build_screens});
}  // namespace

FW_UPDATE(screens) { return lv_obj_get_child_count(root) > 0; }
