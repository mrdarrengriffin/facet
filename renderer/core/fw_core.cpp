// Facet core shared by every host (README.md): the WASM workbench (core/main.cpp) and the device firmware
// (device/fw_device.h). Themes from the generated token tables, the component registry, and render() = build one
// component on a screen exactly the way the workbench preview does.
#include <string>
#include <vector>
#include "fw.h"

namespace fw {
// themes from the generated token tables (tokens/colour.json): [accent set][dark, light], built once
const Theme *theme(bool light, int set) {
  static Theme th[tokens::N_ACCENTS][2];
  static bool ready = false;
  if (!ready) {
    for (int a = 0; a < tokens::N_ACCENTS; a++)
      for (int l = 0; l < 2; l++) {
        Theme &t = th[a][l];
        t.light = l;
        t.accent_set = a;
        for (auto &c : tokens::COLOURS)
          *reinterpret_cast<lv_color_t *>(reinterpret_cast<char *>(&t) + c.off) = lv_color_hex(l ? c.light : c.dark);
        for (int r = 0; r < tokens::N_ROLES; r++)
          *reinterpret_cast<lv_color_t *>(reinterpret_cast<char *>(&t) + tokens::ROLE_OFF[r]) =
              lv_color_hex(l ? tokens::ACCENTS[a].light[r] : tokens::ACCENTS[a].dark[r]);
      }
    ready = true;
  }
  if (set < 0 || set >= tokens::N_ACCENTS) set = tokens::ACCENT_DEFAULT;
  return &th[set][light ? 1 : 0];
}
const Theme *T = nullptr;
std::vector<Component> &reg() {
  static std::vector<Component> r;
  return r;
}
int add(const Component &c) {
  reg().push_back(c);
  return (int) reg().size();
}

const Component *find(const char *id) {
  for (auto &r : reg())
    if (!strcmp(r.id, id)) return &r;
  return nullptr;
}

void meta_json(std::string &out) {
  ArduinoJson::JsonDocument doc;
  auto arr = doc.to<ArduinoJson::JsonArray>();
  for (auto &c : reg()) {
    auto o = arr.add<ArduinoJson::JsonObject>();
    o["id"] = c.id;
    o["name"] = c.name;
    o["section"] = c.section;
    o["meta"] = ArduinoJson::serialized(c.meta);
  }
  serializeJson(doc, out);
}

// clean `scr`, make it the Facet screen (bg token, flex column, centred) and build `comp` with `props` on it
const char *render(lv_obj_t *scr, const char *comp, const char *props, bool light, int accent) {
  const Component *c = find(comp);
  if (!c) return "no such component";
  ArduinoJson::JsonDocument doc;
  if (deserializeJson(doc, props && *props ? props : "{}")) return "props: bad JSON";
  T = theme(light, accent);
  lv_obj_clean(scr);
  lv_obj_remove_style_all(scr);
  lv_obj_set_style_bg_color(scr, T->bg, 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
  lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(scr, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
  c->build(scr, doc.as<ArduinoJson::JsonObjectConst>());
  lv_obj_update_layout(scr);
  lv_obj_invalidate(scr);
  return "";
}

}  // namespace fw
