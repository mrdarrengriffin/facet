// Scroll track: the Facet scroll track (docs: Screen parts > Scroll track, @facet/ui scrolltrack.ts): a 4 px bar at
// the right edge of every scrolling area, its height; the thumb shows how much is in view and where; nothing when
// nothing scrolls. This page shows the core atom fw::scroll_track (atoms.h, LVGL's own styled scrollbar + the track)
// on a scrolling column of real List rows (fw::find("list_row"), its stack), the way a long screen gets it.
//   items   the rows (2026-10-26 real rows: the List row stack's items model, core/items.h: a comma list or a JSON
//           list, usually bound to an entity attribute; plain item = title, object item = title / icon / sub / value
//           / trailing / on / state fields); few = nothing to scroll, so no track; none = the empty words. Nothing
//           demo: the sample rooms live in the variants. The area lay::content_w x height.
// Drag to scroll (LVGL's scroll, with its momentum). No events of its own. Update path: refills (snap), keeps the scroll.
// Contrast pairs: colour.json section "scroll_track".
#include "fw.h"

namespace {
void fill(lv_obj_t *s, fw::Props p) {
  lv_obj_remove_style_all(s);
  lv_obj_add_flag(s, (lv_obj_flag_t) (LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE));
  lv_obj_set_size(s, fw::lay::content_w, fw::px((float) (p["height"] | 256)));
  lv_obj_set_flex_flow(s, LV_FLEX_FLOW_COLUMN);
  fw::scroll_track(s);
  if (const fw::Component *rc = fw::find("list_row")) {
    ArduinoJson::JsonDocument rp;  // one List row stack: its rows from the items, plain rows unless an item says more
    rp["list"] = "stack";
    rp["icon"] = "none";
    rp["trailing"] = "none";
    rp["items"] = p["items"] | "";
    if (!p["empty"].isNull()) rp["empty"] = p["empty"];
    rc->build(s, rp.as<fw::Props>());
  }
}
}  // namespace

FW_COMPONENT(scroll_track, "Scroll track", "components", R"({
  "about": "A thin bar at the right edge of a long list that shows how much is in view and where. Nothing when nothing scrolls.",
  "frame": [464, 272],
  "icons": ["bulb@icon"],
  "props": [
    {"name": "items", "about": "The rows of the scrolling list, a comma list or a JSON list, usually bound to an entity attribute. A plain item is a row's title; an object item's title (or name), icon, sub, value, trailing, on and state fields make the row. Up to 24 rows, the last a +N row for the rest. No items shows the empty words, never sample rows.", "type": "text", "default": ""},
    {"name": "empty", "about": "The words shown when items is empty.", "type": "text", "default": "Nothing here"},
    {"name": "height", "about": "How tall the list's visible area is, in pixels.", "type": "range", "min": 128, "max": 256, "step": 8, "default": 256}
  ],
  "variants": [
    {"label": "eight rows · scrolls", "props": {"items": "[{\"title\":\"Living room\",\"icon\":\"bulb\",\"trailing\":\"value\",\"value\":\"On\"},{\"title\":\"Kitchen\",\"icon\":\"bulb\",\"trailing\":\"value\",\"value\":\"Off\"},{\"title\":\"Hallway\",\"icon\":\"bulb\",\"trailing\":\"value\",\"value\":\"Off\"},{\"title\":\"Study\",\"icon\":\"bulb\",\"trailing\":\"value\",\"value\":\"On\"},{\"title\":\"Bedroom\",\"icon\":\"bulb\",\"trailing\":\"value\",\"value\":\"Off\"},{\"title\":\"Bathroom\",\"icon\":\"bulb\",\"trailing\":\"value\",\"value\":\"Off\"},{\"title\":\"Garden\",\"icon\":\"bulb\",\"trailing\":\"value\",\"value\":\"On\"},{\"title\":\"Garage\",\"icon\":\"bulb\",\"trailing\":\"value\",\"value\":\"Off\"}]"}},
    {"label": "two rows · nothing to scroll", "props": {"items": "Living room,Kitchen"}},
    {"label": "empty", "props": {"empty": "No rooms yet"}}
  ]
})") {
  lv_obj_t *s = lv_obj_create(parent);
  fill(s, p);
}

FW_UPDATE(scroll_track) {  // the rows again, where you were scrolled to (an update never jumps the list: BF-028)
  const int32_t y = lv_obj_get_scroll_y(root);
  fw::refill(root);
  fill(root, p);
  lv_obj_update_layout(root);
  lv_obj_scroll_to_y(root, std::max<int32_t>(0, std::min(y, lv_obj_get_scroll_y(root) + lv_obj_get_scroll_bottom(root))), LV_ANIM_OFF);
  return true;
}
