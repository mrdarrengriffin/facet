// Spec screen (core/spec.h, WIP): a screen built from its JSON spec, here for the workbench and the audit; the device
// runtime builds spec screens the same way when it opens one (core/device.cpp). Prop screen = one of this build's
// factory specs (renderer/specs/*.json); every other prop = one of the screen's parameters (empty = this panel's deck
// value, else the spec's default): the workbench makes a control per typed parameter (spec "params").
#include "fw.h"
#include "spec.h"
#include "device.h"

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
    {"name": "screen", "about": "Which built-in screen to draw from its spec, such as the Light or Weather screen.", "type": "select", "options": [)" + ids + R"(], "default": "living_room"},
    {"name": "entity", "about": "The entity the screen shows, by its Home Assistant id. Leave empty to use the screen's own.", "type": "text", "default": ""},
    {"name": "name", "about": "The name shown in the screen's header. Leave empty to use the screen's own.", "type": "text", "default": ""},
    {"name": "speakers", "about": "For the Media screen: the speakers you can switch between, as entity ids separated by commas.", "type": "text", "default": ""},
    {"name": "live", "about": "Runs the screen as on a panel: its taps open the screens it leads to, Back returns, and the screen itself shows no Back.", "type": "bool", "default": false}
  ],
  "variants": [
    )" + vars + R"(
  ]
})";
}
const std::string META = meta();
void build_spec(lv_obj_t *parent, fw::Props p) {
  ArduinoJson::JsonDocument params;
  for (ArduinoJson::JsonPairConst kv : p) {  // every other prop = a parameter (the workbench builds them from the
    const char *k = kv.key().c_str();      // spec's typed params); empty = the deck's / the spec's default
    if (!strcmp(k, "screen") || !strcmp(k, "opening") || !strcmp(k, "live")) continue;
    if (kv.value().is<bool>()) params[k] = kv.value().as<bool>() ? "true" : "false";
    else if (kv.value().is<const char *>() && *kv.value().as<const char *>()) params[k] = kv.value().as<const char *>();
  }
  const char *id = p["screen"] | "living_room";
  if (p["live"] | false) {  // the docs' screen demos: a device whose stack starts on this screen (core/device.h)
    std::string start = id, pj;
    serializeJson(params, pj);
    if (params.size()) start += "?" + pj;
    fw::dev::mount(parent, start.c_str());
    return;
  }
  fw::spec::build(parent, id, params.as<fw::Props>());
}
const int reg = fw::add({"spec", "Spec screen", "screens", META.c_str(), build_spec});
}  // namespace
