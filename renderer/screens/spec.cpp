// Spec screen (core/spec.h, WIP): a screen built from its JSON spec, here for the workbench and the audit; the device
// runtime builds spec screens the same way when it opens one (core/device.cpp). Prop screen = one of this build's
// factory specs (renderer/specs/*.json); entity / name = the Light spec's parameters.
#include "fw.h"
#include "spec.h"

namespace {
std::string meta() {
  std::string ids, vars;
  for (const auto &f : fw::spec::FACTORY) {
    ids += std::string(ids.empty() ? "" : ", ") + "\"" + f.id + "\"";
    vars += std::string(vars.empty() ? "" : ",\n    ") + "{\"label\": \"" + f.id + "\", \"props\": {\"screen\": \"" + f.id + "\"}}";
  }
  return R"({
  "about": "A screen built from its JSON spec (rooms, the Light screen): the same builder the panel uses; the deck puts them on Home.",
  "frame": [480, 480],
  "icons": ["back@glyph", "bulb@icon"],
  "props": [
    {"name": "screen", "type": "select", "options": [)" + ids + R"(], "default": "living_room"},
    {"name": "entity", "type": "text", "default": "light.living_room_dimmer_light"},
    {"name": "name", "type": "text", "default": "Ceiling"}
  ],
  "variants": [
    )" + vars + R"(
  ]
})";
}
const std::string META = meta();
void build_spec(lv_obj_t *parent, fw::Props p) {
  ArduinoJson::JsonDocument params;
  params["entity"] = p["entity"] | "light.living_room_dimmer_light";
  params["name"] = p["name"] | "Ceiling";
  fw::spec::build(parent, p["screen"] | "living_room", params.as<fw::Props>());
}
const int reg = fw::add({"spec", "Spec screen", "screens", META.c_str(), build_spec});
}  // namespace
