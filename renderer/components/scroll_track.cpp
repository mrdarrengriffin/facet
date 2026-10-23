// Scroll track: the Facet scroll track (docs: Screen parts > Scroll track, @facet/ui scrolltrack.ts): a 4 px bar at
// the right edge of every scrolling area, its height; the thumb shows how much is in view and where; nothing when
// nothing scrolls. This page shows the core atom fw::scroll_track (atoms.h, LVGL's own styled scrollbar + the track)
// on a scrolling column of real List rows (fw::find("list_row")), the way a long screen gets it.
//   rows    prop: how many List rows (few = nothing to scroll, so no track), the area lay::content_w x the frame
// Drag to scroll (LVGL's scroll, with its momentum). No events of its own. Update path: refills (snap).
// Contrast pairs: colour.json section "scroll_track".
#include "fw.h"

namespace {
const char *TITLES[] = {"Living room", "Kitchen", "Hallway", "Study", "Bedroom", "Bathroom", "Garden", "Garage", "Utility", "Landing", "Porch", "Loft"};
void fill(lv_obj_t *s, fw::Props p) {
  lv_obj_remove_style_all(s);
  lv_obj_add_flag(s, (lv_obj_flag_t) (LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE));
  lv_obj_set_size(s, fw::lay::content_w, fw::px((float) (p["height"] | 256)));
  lv_obj_set_flex_flow(s, LV_FLEX_FLOW_COLUMN);
  fw::scroll_track(s);
  const int n = std::max(1, std::min(12, (int) (p["rows"] | 8)));
  if (const fw::Component *rc = fw::find("list_row"))
    for (int i = 0; i < n; i++) {
      ArduinoJson::JsonDocument rp;
      rp["title"] = TITLES[i];
      rp["icon"] = "bulb";
      rp["trailing"] = "value";
      rp["value"] = i % 3 ? "Off" : "On";
      rc->build(s, rp.as<fw::Props>());
    }
}
}  // namespace

FW_COMPONENT(scroll_track, "Scroll track", "components", R"({
  "about": "A thin bar at the right edge of a long list that shows how much is in view and where. Nothing when nothing scrolls.",
  "frame": [464, 272],
  "icons": ["bulb@icon"],
  "props": [
    {"name": "rows", "type": "range", "min": 1, "max": 12, "step": 1, "default": 8},
    {"name": "height", "type": "range", "min": 128, "max": 256, "step": 8, "default": 256}
  ],
  "variants": [
    {"label": "eight rows · scrolls", "props": {}},
    {"label": "two rows · nothing to scroll", "props": {"rows": 2}}
  ]
})") {
  lv_obj_t *s = lv_obj_create(parent);
  fill(s, p);
}

FW_UPDATE(scroll_track) {
  fw::refill(root);
  fill(root, p);
  return true;
}
