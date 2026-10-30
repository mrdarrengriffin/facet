// Screen sources (Settings > Screens, 2026-10-23): where a screen's data can come from, from the source catalogue
// renderer/sources/sources.json (compiled in as fw::spec::SOURCES by tools/gen_specs.mjs). Parsed on demand (the
// Screens pages are rare; nothing stays on the heap). Header-only (core/*.h is bundled as is).
//   picked(doc, screen, entry)  the source a deck entry uses: its "source" when the screen has it, else the first
//   name(doc, screen, id)        a source's name ("Music Assistant"); "" = not a source of that screen
//   line(doc, screen, entry)     the Screens list's sub line: the source's name, "Built in" when it has none
//   adapter(src, param)          the spec "source" value (C51 adapter) a source sets: its adapter, else "ha" for a
//                                Home Assistant source, else its id; "" when the spec's source parameter has no such option
//   shown(doc, screen, entry, param, v)  the source a page really uses, given the spec's source parameter (param = its
//                                definition, v = its value): the picked source when it sets v or sets nothing, else the
//                                first source that sets v; v "sample" and no such source = SAMPLE (named "Sample (previews)")
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
constexpr const char *SAMPLE = "~sample", *SAMPLE_NAME = "Sample (previews)";
inline std::string adapter(ArduinoJson::JsonVariantConst x, ArduinoJson::JsonVariantConst param) {
  std::string a = x["adapter"] | "";
  if (a.empty()) a = strcmp(x["kind"] | "", "ha") ? (x["id"] | "") : "ha";
  for (ArduinoJson::JsonVariantConst o : param["options"].as<ArduinoJson::JsonArrayConst>())
    if (a == (o | "")) return a;
  return "";
}
inline std::string shown(const ArduinoJson::JsonDocument &d, const std::string &screen, ArduinoJson::JsonVariantConst entry,
                         ArduinoJson::JsonVariantConst param, const std::string &v) {
  const std::string p = picked(d, screen, entry);
  bool linked = false, p_sets = false;
  std::string first;
  for (ArduinoJson::JsonVariantConst x : of(d, screen)) {
    const std::string a = adapter(x, param);
    if (a.empty()) continue;
    linked = true;
    if (p == (x["id"] | "")) p_sets = true;
    if (a != v) continue;
    if (p == (x["id"] | "")) return p;
    if (first.empty()) first = x["id"] | "";
  }
  if (!p_sets) return p;
  if (!linked || v.empty()) return p;
  if (!first.empty()) return first;
  return v == "sample" ? SAMPLE : p;
}
inline std::string line(const ArduinoJson::JsonDocument &d, const std::string &screen, ArduinoJson::JsonVariantConst entry,
                        ArduinoJson::JsonVariantConst param = ArduinoJson::JsonVariantConst(), const std::string &v = "") {
  const std::string id = shown(d, screen, entry, param, v);
  if (id == SAMPLE) return SAMPLE_NAME;
  const std::string n = name(d, screen, id);
  return n.empty() ? "Built in" : n;
}
}  // namespace src
}  // namespace fw
