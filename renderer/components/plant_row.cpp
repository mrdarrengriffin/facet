// Plant row: one plant in a list (Plant monitor): the real List row (fw::find) with the plant's name (title), what it
// needs (sub: "needs water", "moist", "too wet", "offline") and its moisture (trailing value "18%"), and between the
// text and the value a small Level bar (lay::level_bar_w, fw::find) with the dry line as its mark: accent when it
// needs water, steel when fine, blue when too wet.
//   entity  the plant's sensor: with one, a tap opens the plant's own screen (event "open" "plants?{entity}", the
//           spec's nav.open); without (the sample), the row is a readout (no press, no sound)
// Feed: name, moisture (0-100), dry / wet (default 30 / 80). Update path: the row's texts through its own update,
// the bar glides. Contrast: list_row + level_bar (no colours of its own).
// audit deps: list_row, level_bar
#include "fw.h"

namespace {
struct PR { lv_obj_t *row = nullptr, *bar = nullptr; std::string entity; };
PR *of(lv_obj_t *o) { return (PR *) lv_obj_get_user_data(o); }
struct Read { int m; const char *sub, *tone; };
Read read(fw::Props p) {
  const bool known = p["moisture"].is<double>() || (p["moisture"].is<const char *>() && isdigit((unsigned char) *(p["moisture"] | "x")));
  const double v = p["moisture"].is<double>() ? p["moisture"].as<double>() : strtod(p["moisture"] | "", nullptr);
  const int m = known ? (int) std::lround(LV_CLAMP(0.0, v, 100.0)) : -1, dry = p["dry"] | 30, wet = p["wet"] | 80;
  if (m < 0) return {m, "offline", "off"};
  if (m < dry) return {m, "needs water", "accent"};
  if (m > wet) return {m, "too wet", "blue"};
  return {m, "moist", "steel"};
}
void row_props(fw::Props p, const Read &r, ArduinoJson::JsonDocument &d) {
  d["title"] = p["name"] | "Fern", d["sub"] = r.sub, d["icon"] = "none", d["trailing"] = "value";
  d["value"] = r.m < 0 ? "" : std::to_string(r.m) + "%";
}
void bar_props(fw::Props p, const Read &r, ArduinoJson::JsonDocument &d) {
  d["value"] = r.m, d["mark"] = p["dry"] | 30, d["tone"] = r.tone, d["width"] = (int) std::lround(fw::lay::level_bar_w / fw::D);
}
lv_obj_t *build(const char *id, lv_obj_t *parent, ArduinoJson::JsonDocument &d) {
  const uint32_t n = lv_obj_get_child_count(parent);
  if (const fw::Component *c = fw::find(id)) c->build(parent, d.as<fw::Props>());
  return lv_obj_get_child_count(parent) > n ? lv_obj_get_child(parent, -1) : nullptr;
}
void clickable(PR *c) {
  if (!c->row) return;
  if (c->entity.empty()) lv_obj_remove_flag(c->row, LV_OBJ_FLAG_CLICKABLE);
  else lv_obj_add_flag(c->row, LV_OBJ_FLAG_CLICKABLE);
}
}  // namespace

FW_COMPONENT(plant_row, "Plant row", "components", R"({
  "about": "One plant in a list: its name, what it needs, a small moisture bar with its dry line, and the percentage.",
  "frame": [480, 0],
  "props": [
    {"name": "name", "about": "The plant's name.", "type": "text", "default": "Fern"},
    {"name": "moisture", "about": "The soil moisture now, as a percentage. -1 means it cannot be read.", "type": "range", "min": -1, "max": 100, "step": 1, "default": 18},
    {"name": "dry", "about": "The moisture below which the plant needs water. The bar shows it as a line.", "type": "range", "min": 0, "max": 100, "step": 1, "default": 30},
    {"name": "wet", "about": "The moisture above which the soil is too wet.", "type": "range", "min": 0, "max": 100, "step": 1, "default": 80},
    {"name": "entity", "about": "The plant's moisture sensor, by its Home Assistant entity id.", "type": "text", "default": ""}
  ],
  "variants": [
    {"label": "needs water", "props": {}},
    {"label": "moist", "props": {"name": "Monstera", "moisture": 46}},
    {"label": "too wet", "props": {"name": "Basil", "moisture": 91}},
    {"label": "offline", "props": {"name": "Snake plant", "moisture": -1}}
  ]
})") {
  auto *c = new PR;
  lv_obj_t *root = lv_obj_create(parent);
  lv_obj_remove_style_all(root);
  lv_obj_remove_flag(root, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_size(root, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_user_data(root, c);
  lv_obj_add_event_cb(root, [](lv_event_t *e) { delete (PR *) lv_event_get_user_data(e); }, LV_EVENT_DELETE, c);
  c->entity = p["entity"] | "";
  const Read r = read(p);
  ArduinoJson::JsonDocument rp;
  row_props(p, r, rp);
  c->row = build("list_row", root, rp);
  if (!c->row) return;
  ArduinoJson::JsonDocument bp;
  bar_props(p, r, bp);
  c->bar = build("level_bar", c->row, bp);
  if (c->bar) {
    lv_obj_move_to_index(c->bar, (int32_t) lv_obj_get_child_count(c->row) - 2);  // before the value
  }
  clickable(c);
  lv_obj_add_event_cb(c->row, [](lv_event_t *e) {
    auto *root = (lv_obj_t *) lv_event_get_user_data(e);
    PR *c = of(root);
    if (c->entity.empty()) return;
    fw::emit(root, "open", ("plants?{\"entity\":\"" + c->entity + "\"}").c_str());
  }, LV_EVENT_CLICKED, root);
}

FW_UPDATE(plant_row) {
  PR *c = of(root);
  if (!c || !c->row) return false;
  c->entity = p["entity"] | "";
  const Read r = read(p);
  ArduinoJson::JsonDocument rp, bp;
  row_props(p, r, rp);
  bar_props(p, r, bp);
  if (!fw::update_child(fw::find("list_row"), c->row, rp.as<fw::Props>())) return false;
  bool kept = false;  // the row may have made its contents again (pointers compared, never followed)
  for (uint32_t i = 0; i < lv_obj_get_child_count(c->row); i++) kept |= lv_obj_get_child(c->row, (int32_t) i) == c->bar;
  if (!kept) c->bar = nullptr;
  if (c->bar) fw::update_child(fw::find("level_bar"), c->bar, bp.as<fw::Props>());
  else return false;
  clickable(c);
  return true;
}
