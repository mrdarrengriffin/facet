// Placeholder: a part that isn't built yet, shown as a plain labelled outline (Facet rule: never a look-alike of a
// part to come). The content width wide, a hairline line-hi outline (rad::card), the name centred (type label,
// text-dim, caps) over one quiet line (type body, text-dim). First use: the robot vacuum's map area, waiting for the
// Floor plan foundation (Darren 2026-10-23: floor plans get their own drawing rules first).
//   height   card (lay::card_h, the default) | tile (lay::tile_h) | row (lay::list_row_h)
// Static, not a control. Update path: the texts in place. Contrast: section "placeholder".
#include "fw.h"

namespace {
// a text only when it changes (an update with the same props redraws nothing)
void put(lv_obj_t *l, fw::ty::Id ty, const char *t) {
  if (fw::cased(ty, t) != lv_label_get_text(l)) fw::set_text(l, ty, t);
}
struct PH { lv_obj_t *name, *note; };
int height_of(const char *h) { return !strcmp(h, "tile") ? fw::lay::tile_h : !strcmp(h, "row") ? fw::lay::list_row_h : fw::lay::card_h; }
}  // namespace

FW_COMPONENT(placeholder, "Placeholder", "components", R"({
  "about": "A part that isn't built yet: a plain outline with its name and one line, never a look-alike.",
  "frame": [480, 0],
  "props": [
    {"name": "label", "about": "The name of the part that is not built yet.", "type": "text", "default": "map"},
    {"name": "note", "about": "One line saying what is coming.", "type": "text", "default": "Floor plans are coming"},
    {"name": "height", "about": "How tall the outline is, matching the part it stands in for: a card, a tile or a row.", "type": "select", "options": ["card", "tile", "row"], "default": "card"}
  ],
  "variants": [
    {"label": "card", "props": {}},
    {"label": "row", "props": {"label": "sheet", "note": "Component coming", "height": "row"}}
  ]
})") {
  const fw::Theme &t = *fw::T;
  lv_obj_t *b = lv_obj_create(parent);
  lv_obj_remove_style_all(b);
  lv_obj_remove_flag(b, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_size(b, fw::lay::content_w, height_of(p["height"] | "card"));
  lv_obj_set_style_radius(b, fw::rad::card, 0);
  lv_obj_set_style_border_width(b, fw::stroke::hair, 0);
  lv_obj_set_style_border_color(b, t.line_hi, 0);
  lv_obj_set_flex_flow(b, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(b, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(b, fw::sp::s2, 0);
  auto *ph = new PH;
  lv_obj_set_user_data(b, ph);
  lv_obj_add_event_cb(b, [](lv_event_t *e) { delete (PH *) lv_event_get_user_data(e); }, LV_EVENT_DELETE, ph);
  ph->name = fw::text(b, fw::ty::label, p["label"] | "map", t.text_dim);
  ph->note = fw::text(b, fw::ty::body, p["note"] | "", t.text_dim);
}

FW_UPDATE(placeholder) {
  auto *ph = (PH *) lv_obj_get_user_data(root);
  if (!ph || lv_obj_get_height(root) != height_of(p["height"] | "card")) return false;
  put(ph->name, fw::ty::label, p["label"] | "map");
  put(ph->note, fw::ty::body, p["note"] | "");
  return true;
}
