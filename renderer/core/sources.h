// Screen sources (Settings > Screens, 2026-10-23): where a screen's data can come from, from the source catalogue
// renderer/sources/sources.json (compiled in as fw::spec::SOURCES by tools/gen_specs.mjs). Parsed on demand (the
// Screens pages are rare; nothing stays on the heap). Header-only (core/*.h is bundled as is).
//   picked(doc, screen, entry)  the source a deck entry uses: its "source" when the screen has it, else the first
//   name(doc, screen, id)        a source's name ("Music Assistant"); "" = not a source of that screen
//   line(doc, screen, entry)     the Screens list's sub line: the source's name, "Built in" when it has none
#pragma once
#include <string>
#include "fw.h"
#include "specs_gen.h"

namespace fw {
namespace src {
inline bool load(ArduinoJson::JsonDocument &d) { return !deserializeJson(d, spec::SOURCES); }
inline ArduinoJson::JsonArrayConst of(const ArduinoJson::JsonDocument &d, const std::string &screen) {
  return d["screens"][screen]["sources"].as<ArduinoJson::JsonArrayConst>();
}
inline std::string picked(const ArduinoJson::JsonDocument &d, const std::string &screen, ArduinoJson::JsonVariantConst entry) {
  const std::string want = entry["source"] | "";
  std::string first;
  for (ArduinoJson::JsonVariantConst x : of(d, screen)) {
    const std::string id = x["id"] | "";
    if (first.empty()) first = id;
    if (id == want) return id;
  }
  return first;
}
inline std::string name(const ArduinoJson::JsonDocument &d, const std::string &screen, const std::string &id) {
  for (ArduinoJson::JsonVariantConst x : of(d, screen))
    if (id == (x["id"] | "")) return x["name"] | "";
  return "";
}
inline std::string line(const ArduinoJson::JsonDocument &d, const std::string &screen, ArduinoJson::JsonVariantConst entry) {
  const std::string n = name(d, screen, picked(d, screen, entry));
  return n.empty() ? "Built in" : n;
}
}  // namespace src
}  // namespace fw
