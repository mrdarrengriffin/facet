// Screen sources (Settings > Screens): where a screen's data can come from, from the source catalogue
// renderer/sources/sources.json (compiled in as fw::spec::SOURCES by tools/gen_specs.mjs). Parsed on demand (the
// Screens pages are rare; nothing stays on the heap). Header-only (core/*.h is bundled as is).
//   picked(doc, screen, entry)  the source a deck entry uses: its "source" when the screen has it, else the first
//   name(doc, screen, id)        a source's name ("Music Assistant"); "" = not a source of that screen
//   line(doc, screen, entry)     the Screens list's sub line: the source's name, "Built in" when it has none
//   adapter(src, param)          the spec "source" value (C51 adapter) a source sets: its adapter, else "ha" for a
//                                Home Assistant source, else its id; "" when the spec's source parameter has no such option
//   shown(doc, screen, entry, param, v)  the source a page really uses, given the spec's source parameter (param = its
//                                definition, v = its value): the picked source when it sets v or sets nothing, else the
//                                first source that sets v (of several, the one whose integration has the page's entity: fits());
//                                v "sample" and no such source = SAMPLE (named "Sample (previews)")
//   candidates(finds)            a Home Assistant source's own entity, found, not picked (sources.json "finds": {param,
//                                domain, platform?, match? ("a|b": a substring of the id), default?}): the registry's
//                                rows of that domain that fit, in its order (core/device.h ha_link::reg; read on first
//                                ask); none found (or no registry yet) = the default alone, if any
//   found(finds)                 the one it uses: the first candidate
#pragma once
#include <string>
#include "fw.h"
#include "specs_gen.h"
#include "device.h"

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
inline std::vector<std::string> candidates(ArduinoJson::JsonVariantConst f);
// several sources set the same adapter (Weather: one per integration, all "ha"): the one whose integration (sources.json
// finds.platform) has the page's entity, else (no entity set) one Home Assistant has an entity of; a source without
// a platform always fits; no registry read yet = fits (the old answer: the first)
inline bool fits(ArduinoJson::JsonVariantConst x, ArduinoJson::JsonVariantConst entry) {
  ArduinoJson::JsonVariantConst f = x["finds"];
  const std::string plat = f["platform"] | "", dom = f["domain"] | "";
  if (plat.empty() || dom.empty()) return true;
  const auto *r = dev::ha_link::reg(dom);
  if (!r) return true;
  const std::string ent = entry["params"][f["param"] | ""] | "";
  if (ent.empty()) return !candidates(f).empty();
  for (const auto &row : *r) if (row.id == ent) return row.platform == plat;
  return false;
}
inline std::string shown(const ArduinoJson::JsonDocument &d, const std::string &screen, ArduinoJson::JsonVariantConst entry,
                         ArduinoJson::JsonVariantConst param, const std::string &v) {
  const std::string p = picked(d, screen, entry);
  bool linked = false, p_sets = false;
  const bool saved = entry["source"].is<const char *>();  // the person picked it (not the first by default)
  std::string first, any;
  for (ArduinoJson::JsonVariantConst x : of(d, screen)) {
    const std::string a = adapter(x, param);
    if (a.empty()) continue;
    linked = true;
    if (p == (x["id"] | "")) p_sets = true;
    if (a != v) continue;
    if (p == (x["id"] | "") && (saved || !x["finds"]["platform"].is<const char *>() || fits(x, entry))) return p;  // the pick, unless it was only the first by default and another integration has the entity
    if (any.empty()) any = x["id"] | "";
    if (first.empty() && fits(x, entry)) first = x["id"] | "";
  }
  if (!p_sets) return p;
  if (!linked || v.empty()) return p;
  if (!first.empty()) return first;
  if (!any.empty()) return any;
  return v == "sample" ? SAMPLE : p;
}
inline std::string line(const ArduinoJson::JsonDocument &d, const std::string &screen, ArduinoJson::JsonVariantConst entry,
                        ArduinoJson::JsonVariantConst param = ArduinoJson::JsonVariantConst(), const std::string &v = "") {
  const std::string id = shown(d, screen, entry, param, v);
  if (id == SAMPLE) return SAMPLE_NAME;
  const std::string n = name(d, screen, id);
  return n.empty() ? "Built in" : n;
}
inline std::vector<std::string> candidates(ArduinoJson::JsonVariantConst f) {
  std::vector<std::string> out;
  const std::string dom = f["domain"] | "", plat = f["platform"] | "", match = f["match"] | "", def = f["default"] | "";
  if (!dom.empty()) {
    dev::ha_link::reg_fetch(dom);
    if (const auto *r = dev::ha_link::reg(dom))
      for (const auto &row : *r) {
        if (!plat.empty() && row.platform != plat) continue;
        bool hit = match.empty();
        for (size_t i = 0; !hit && i <= match.size();) {
          size_t j = match.find('|', i);
          if (j == std::string::npos) j = match.size();
          const std::string m = match.substr(i, j - i);
          hit = !m.empty() && row.id.find(m) != std::string::npos;
          i = j + 1;
        }
        if (hit && row.id.rfind(dom + ".", 0) == 0) out.push_back(row.id);
      }
  }
  if (out.empty() && !def.empty()) out.push_back(def);
  return out;
}
inline std::string found(ArduinoJson::JsonVariantConst f) {
  const std::vector<std::string> c = candidates(f);
  return c.empty() ? std::string() : c.front();
}
}  // namespace src
}  // namespace fw
