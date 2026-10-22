// Home (PLACEHOLDER, Darren 2026-10-22): the real Home will be customisable per room (not just a card grid); until
// then a simple grid of the screens and workbench pages that exist, so the panel and the workbench can get around.
// Composed only of real components:
//   header   the Header component (no back; "Home" + the screen chrome: the settings button (Home only) + live clock)
//            at (inset, inset)
//   content  from lay::top, one scrolling column of Cards, two per row (lay::card_w x lay::card_h, a gutter apart both
//            ways; 480: 2 x 184 + 16 = 384 of the 388 content height, the 4 px left over stays at the bottom), bottom
//            padding inset
//   cards    one per page in PAGES that this build has (fw::find): title only (the page name as the Card label; no
//            icon, value or line under it: Darren 2026-10-22, placeholder). Tap = the Card's own tap (event tap + sound open), then
//            event "navigate" (detail = the page id): the workbench opens that page, the panel host will open the screen
// Update path: nothing in it changes with props (the clock runs on its own), so an update keeps it as it is.
#include "fw.h"

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

// the pages Home lists, screens first. Title only on every card (Darren 2026-10-22: "just show the title for now"),
// no icon, so they all match
const char *const PAGES[] = {"settings", "colour", "type", "space", "layout", "icons", "motion", "states", "mascots", "chart"};

const char *META = R"({
  "about": "Placeholder Home: a grid of cards for the screens and workbench pages that exist; each card opens its page. The real Home will be laid out per room.",
  "frame": [480, 480],
  "icons": ["screen-settings@icon"],
  "props": [],
  "variants": [
    {"label": "default", "props": {}}
  ]
})";

void build_home(lv_obj_t *parent, fw::Props) {
  const fw::Theme &t = *fw::T;
  lv_obj_t *root = box(parent);
  lv_obj_set_size(root, fw::lay::screen_w, fw::lay::screen_h);
  lv_obj_set_style_bg_color(root, t.bg, 0);
  lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);

  Doc hd;  // the Header + its screen chrome (settings button, live clock)
  hd["label"] = "Home";
  hd["back"] = false;
  hd["settings"] = true;  // the settings button: Home only for now (Darren 2026-10-22)
  if (lv_obj_t *h = build("header", root, hd)) lv_obj_set_pos(h, fw::lay::inset, fw::lay::inset);

  lv_obj_t *c = box(root);
  lv_obj_add_flag(c, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(c, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(c, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_pos(c, fw::lay::inset, fw::lay::top);
  lv_obj_set_size(c, fw::lay::content_w, fw::lay::screen_h - fw::lay::top);
  lv_obj_set_style_pad_bottom(c, fw::lay::inset, 0);
  lv_obj_set_flex_flow(c, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_style_pad_column(c, fw::lay::gutter, 0);
  lv_obj_set_style_pad_row(c, fw::lay::gutter, 0);

  for (const char *id : PAGES) {
    const fw::Component *comp = fw::find(id);
    if (!comp) continue;
    Doc d;  // title only
    d["label"] = comp->name;
    d["icon"] = "none";
    d["preview"] = "none";
    d["mode"] = "value";
    d["value"] = "";
    d["sub"] = "";
    lv_obj_t *card = build("card", c, d);
    if (!card) continue;
    lv_obj_add_event_cb(card, [](lv_event_t *e) {  // after the Card's own tap (event tap + sound open)
      auto *o = (lv_obj_t *) lv_event_get_current_target(e);
      if (lv_obj_has_state(o, fw::st::OFF)) return;
      fw::emit(o, "navigate", (const char *) lv_event_get_user_data(e));
    }, LV_EVENT_CLICKED, (void *) id);
  }
}
const int reg = fw::add({"home", "Home", "screens", META, build_home});
}  // namespace

// Update path: no props change what Home shows (the clock keeps itself), so it stays as it is. Silent.
FW_UPDATE(home) { return lv_obj_get_child_count(root) > 0; }
