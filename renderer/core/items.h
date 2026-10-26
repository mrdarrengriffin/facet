// Items: the one list model of every component that repeats per item (Grid items, List row stack, Picker row group,
// Scroll track; 2026-10-26). items = a comma list or a JSON list (usually bound to an entity attribute: {"e", "a"});
// "$item" in a prop = the item (an object item: its id, else its name), "$item.<key>" = a field of an object item.
#pragma once
#include <cctype>
#include <string>
#include <vector>
#include <cstring>
#include "ArduinoJson.h"

namespace fw::items {

inline std::vector<std::string> parse(const char *v) {
  std::vector<std::string> out;
  if (*v == '[') {
    ArduinoJson::JsonDocument d;
    if (!deserializeJson(d, v))
      for (ArduinoJson::JsonVariantConst x : d.as<ArduinoJson::JsonArrayConst>()) {
        std::string s;
        if (x.is<const char *>()) s = x.as<std::string>();
        else serializeJson(x, s);
        out.push_back(s);
      }
    return out;
  }
  for (const char *p = v; *p;) {
    const char *q = strchr(p, ',');
    std::string it(p, q ? (size_t) (q - p) : strlen(p));
    while (!it.empty() && it.front() == ' ') it.erase(0, 1);
    while (!it.empty() && it.back() == ' ') it.pop_back();
    if (!it.empty()) out.push_back(it);
    if (!q) break;
    p = q + 1;
  }
  return out;
}
// one field of an item ("" key = the item itself: an object's id, else its name); *num = the field is a JSON number
inline std::string field(const std::string &item, const std::string &key, bool *num = nullptr) {
  if (num) *num = false;
  if (item.empty() || item[0] != '{') return key.empty() ? item : "";
  ArduinoJson::JsonDocument d;
  if (deserializeJson(d, item)) return "";
  ArduinoJson::JsonVariantConst v = key.empty() ? (d["id"].isNull() ? d["name"] : d["id"]) : d[key.c_str()];
  if (v.isNull()) return "";
  if (v.is<const char *>()) return v.as<std::string>();
  if (num) *num = v.is<double>() || v.is<bool>();
  std::string s;
  serializeJson(v, s);
  return s;
}
inline void with_item(ArduinoJson::JsonVariant v, const std::string &item) {
  if (v.is<ArduinoJson::JsonObject>()) { for (ArduinoJson::JsonPair kv : v.as<ArduinoJson::JsonObject>()) with_item(kv.value(), item); return; }
  if (v.is<ArduinoJson::JsonArray>()) { for (ArduinoJson::JsonVariant x : v.as<ArduinoJson::JsonArray>()) with_item(x, item); return; }
  if (!v.is<const char *>()) return;
  const std::string s = v.as<std::string>();
  if (s.find("$item") == std::string::npos) return;
  std::string out;
  bool whole = false, num = false;
  for (size_t i = 0;;) {
    const size_t j = s.find("$item", i);
    if (j == std::string::npos) { out += s.substr(i); break; }
    out += s.substr(i, j - i);
    size_t k = j + 5;
    std::string key;
    if (k < s.size() && s[k] == '.') {
      size_t m = k + 1;
      while (m < s.size() && (isalnum((unsigned char) s[m]) || s[m] == '_')) m++;
      key = s.substr(k + 1, m - k - 1);
      k = m;
    }
    whole = j == 0 && k == s.size();
    out += field(item, key, &num);
    i = k;
  }
  if (whole && num) {  // "$item.count" alone, a number in the item: the prop gets the number
    ArduinoJson::JsonDocument d;
    deserializeJson(d, out);
    v.set(d.as<ArduinoJson::JsonVariantConst>());
  } else v.set(out);
}
// one row's props for item `item`: the node's props without `skip` keys; then each of `fields` the props do not fill
// with "$item..." comes from the item: an object item's field of that name (the first field also from its name, then
// its id), a plain-text item = the first field; last, every "$item" / "$item.key" is filled in
inline void row_props(ArduinoJson::JsonVariantConst p, const std::string &item, const char *const *fields, int nf,
                      const char *const *skip, int ns, ArduinoJson::JsonDocument &out) {
  out.set(p);
  if (out.isNull()) out.to<ArduinoJson::JsonObject>();
  for (int i = 0; i < ns; i++) out.remove(skip[i]);
  const bool obj = !item.empty() && item[0] == '{';
  ArduinoJson::JsonDocument d;
  if (obj) deserializeJson(d, item);
  for (int i = 0; i < nf; i++) {
    const char *f = fields[i];
    if (out[f].is<const char *>() && strstr(out[f] | "", "$item")) continue;
    if (!obj) { if (i == 0) out[f] = item; continue; }
    if (!d[f].isNull()) out[f] = d[f];
    else if (i == 0) out[f] = d["name"].isNull() ? d["id"] : d["name"];
  }
  with_item(out.as<ArduinoJson::JsonVariant>(), item);
}
}  // namespace fw::items
