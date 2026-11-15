// History (#200, the Home Sparkline card): a sensor's last 24 hours as numbers, for a Sparkline. The reserved local
// entity panel.history (core/ha.h on_local) holds one attribute per sensor a card shows: its hourly means, oldest
// first, a comma list ("20.1,20.4,21.0"). Home names the sensors (track(); screens/home.cpp, one per Sparkline card);
// while something shows panel.history, one Home Assistant read (recorder/statistics_during_period, period hour, type
// mean: the long-term statistics HA keeps for a sensor with a state class) fetches every tracked sensor's last 24
// hours, again every 15 minutes and when a new sensor is tracked. A sensor without statistics, or a failed read,
// leaves its attribute empty: the Sparkline draws nothing and the card shows the value alone. Same file on the panels
// and in WASM (the read goes through the platform's ha_link ws(), core/device.h).
#include <algorithm>
#include <cstdio>
#include <ctime>
#include <string>
#include <vector>
#include "fw.h"
#include "ha.h"
#include "device.h"
#include "history.h"

namespace fw::dev::history {
namespace {
constexpr const char *E = "panel.history";
constexpr size_t MAX_SENSORS = 12, MAX_REPLY = 64 * 1024;
constexpr uint32_t EVERY_MS = 15 * 60000u;
std::vector<std::string> tracked;
uint32_t fetched_at = 0;
bool fetched = false, busy = false;
std::string reply;

std::string iso(time_t t) {
  struct tm tm;
  gmtime_r(&t, &tm);
  char b[32];
  strftime(b, sizeof b, "%Y-%m-%dT%H:%M:%SZ", &tm);
  return b;
}
// the reply: {sensor: [{start, end, mean}, ...]} (as the result itself, or inside a result message)
void take() {
  ArduinoJson::JsonDocument d;
  if (deserializeJson(d, reply)) return;
  ArduinoJson::JsonObjectConst r = d["result"].is<ArduinoJson::JsonObjectConst>() ? d["result"].as<ArduinoJson::JsonObjectConst>() : d.as<ArduinoJson::JsonObjectConst>();
  for (const std::string &e : tracked) {
    std::string vs;
    for (ArduinoJson::JsonVariantConst x : r[e].as<ArduinoJson::JsonArrayConst>()) {
      if (!x["mean"].is<double>() && !x["mean"].is<long>()) continue;
      char b[24];
      snprintf(b, sizeof b, "%.4g", x["mean"].as<double>());
      vs += (vs.empty() ? "" : ",") + std::string(b);
    }
    ha::put(E, e, vs);
  }
}
void fetch() {
  namespace hl = fw::dev::ha_link;
  if (busy || tracked.empty() || !hl::ready()) return;
  ArduinoJson::JsonDocument c;
  c["type"] = "recorder/statistics_during_period", c["start_time"] = iso(time(nullptr) - 24 * 3600), c["period"] = "hour";
  for (const std::string &e : tracked) c["statistic_ids"].add(e);
  c["types"].add("mean");
  std::string cmd;
  serializeJson(c, cmd);
  reply.clear();
  busy = true, fetched = true, fetched_at = lv_tick_get();
  const uint32_t id = hl::ws(cmd, [](const char *p, size_t n) { if (reply.size() + n <= MAX_REPLY) reply.append(p, n); },
                             [](bool ok, const std::string &) { busy = false; if (ok) take(); reply.clear(); reply.shrink_to_fit(); });
  if (!id) busy = false, fetched = false;  // no link yet: the next look
}
void fresh() {
  static lv_timer_t *t = nullptr;
  if (!t) t = lv_timer_create([](lv_timer_t *) { if (ha::watched(E)) fresh(); }, 60000, nullptr);
  if (!fetched || lv_tick_elaps(fetched_at) >= EVERY_MS) fetch();
}
[[maybe_unused]] const bool registered = (ha::on_local(E, fresh), true);
}  // namespace

void track(const std::string &e) {
  if (e.empty() || std::find(tracked.begin(), tracked.end(), e) != tracked.end() || tracked.size() >= MAX_SENSORS) return;
  tracked.push_back(e);
  fetched = false;  // the new one on the next look
  if (ha::watched(E)) fetch();
}
}  // namespace fw::dev::history
