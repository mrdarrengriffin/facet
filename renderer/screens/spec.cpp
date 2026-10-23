// Spec screen (core/spec.h, WIP): a screen built from its JSON spec, here for the workbench and the audit; the device
// runtime builds spec screens the same way when it opens one (core/device.cpp). Prop screen = one of this build's
// factory specs (renderer/specs/*.json); entity / name / speakers = the screen's parameters (empty = the spec's own
// default; speakers = the Media screen's players, a comma list of media_player ids the host knows).
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
    {"name": "entity", "type": "text", "default": ""},
    {"name": "name", "type": "text", "default": ""},
    {"name": "speakers", "type": "text", "default": ""}
  ],
  "variants": [
    )" + vars + R"(
  ]
})";
}
const std::string META = meta();
void build_spec(lv_obj_t *parent, fw::Props p) {
  ArduinoJson::JsonDocument params;
  for (const char *k : {"entity", "name", "speakers"})  // given = the screen opened with it, else the spec's default
    if (*(p[k] | "")) params[k] = p[k] | "";
  fw::spec::build(parent, p["screen"] | "living_room", params.as<fw::Props>());
}
const int reg = fw::add({"spec", "Spec screen", "screens", META.c_str(), build_spec});
}  // namespace
