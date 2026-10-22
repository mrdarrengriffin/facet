// Delta chip: the Facet delta chip (docs: Components > Charts > Delta chip, @facet/ui charts.ts deltaChip()): a change
// against a reference as a Status chip with an arrow: "▲ 12% vs yday". Built as the Status chip component itself
// (fw::find("status_chip")): tone ok when the change goes the good way, warn when it goes the bad way, idle when it is
// flat (under 0.5 % either way: "="); "--" with no number. Arrows ▲ ▼ are in the text glyph set (type.json).
// Static. Update path: the Status chip's own update (tone + text in place). Contrast pairs: colour.json "status_chip".
#include "fw.h"

namespace {
void chip_props(fw::Props p, ArduinoJson::JsonDocument &cp) {
  const char *ref = p["ref"] | "vs yday";
  const bool good_up = p["up_is_good"] | false;
  char b[64];
  const char *tone = "idle";
  if (!p["pct"].is<float>() && !p["pct"].is<int>()) snprintf(b, sizeof b, "-- %s", ref);
  else {
    const double pct = p["pct"].as<double>();
    const bool up = pct >= 0.5, down = pct <= -0.5;
    tone = !up && !down ? "idle" : up == good_up ? "ok" : "warn";
    snprintf(b, sizeof b, "%s %d%% %s", up ? "▲" : down ? "▼" : "=", (int) std::lround(std::fabs(pct)), ref);
  }
  cp["tone"] = tone;
  cp["text"] = b;
}
}  // namespace

FW_COMPONENT(delta_chip, "Delta chip", "components", R"({
  "about": "A change against a reference, as a chip with an arrow.",
  "frame": [256, 64],
  "props": [
    {"name": "pct", "type": "range", "min": -60, "max": 60, "step": 1, "default": -12},
    {"name": "ref", "type": "text", "default": "vs yday"},
    {"name": "up_is_good", "type": "bool", "default": false}
  ],
  "variants": [
    {"label": "down, down is good", "props": {}},
    {"label": "up, up is bad", "props": {"pct": 18}},
    {"label": "up, up is good", "props": {"pct": 18, "up_is_good": true}},
    {"label": "flat", "props": {"pct": 0}}
  ]
})") {
  const fw::Component *c = fw::find("status_chip");
  if (!c) return;
  ArduinoJson::JsonDocument cp;
  chip_props(p, cp);
  c->build(parent, cp.as<fw::Props>());
}

FW_UPDATE(delta_chip) {
  ArduinoJson::JsonDocument cp;
  chip_props(p, cp);
  return fw::update_child(fw::find("status_chip"), root, cp.as<fw::Props>());
}
