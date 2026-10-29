// The lightning feed (CONTRACT.md C51): the reserved entity panel.lightning (core/spec.h "local sources") in the HA
// store, the one shape the Lightning map binds to, whatever detects the strikes. Same file on the panels and in WASM.
//   state     storm (strikes in the last 15 minutes) | quiet | loading | error
//   n         the nearest strike of the last 15 minutes in km ("15"), "Quiet", or "--" (loading / error); role hero
//             for a number, value (64) for the word (docs .fs-lx-n.is-word); accent "true" only within 25 km (docs is-near)
//   where     under the number: "km W" / "nothing within 100 km" / "loading" / "offline"
//   count     "23 strikes" / "1 strike" in the last 15 minutes; count_tone text | dim
//   trend_label approaching | moving away | trend; trend "48 km/h" / "steady" / "--"; trend_tone warn | ok | dim
//   last      the newest strike: "just now" / "4 min ago" / "2 h ago" / "none"
//   strikes   the strikes, newest first, a JSON list of [km, bearing, ms] (the Lightning radar's strikes)
// Adapters (the feed's settings, the spec's "feeds"; core/ha.h feed_config): source
//   ha      a Home Assistant sensor in the panel lightning shape (setting sensor, such as sensor.panel_lightning: attribute
//           s = "J" + [[km, bearing, unix s], ...] newest first, trend approaching | receding | steady | none, kmh),
//           a live subscription like any bind; ages, counts and the nearest are worked out here from the strikes and
//           the clock (every minute too, so they age with no new strike)
//   sample  the docs storm (core/lightning.h sample), always passing: storm time runs 20x from the middle of its
//           hour and loops (a strike about every 2 s), shifted onto the real clock so ages read right; every 2 s
// More adapters (Blitzortung read by the panel itself, other networks) = more rows here, no screen change.
#include <cmath>
#include <cstdio>
#include <ctime>
#include "fw.h"
#include "ha.h"
#include "lightning.h"

namespace fw::dev::lightning_source {
namespace {
constexpr const char *E = "panel.lightning";
std::string cfg_seen, ent_now;
bool sampling = false;  // the sample adapter is on (its 2 s timer moves the storm)
lv_timer_t *tick = nullptr;

double now_ms() { return (double) time(nullptr) * 1000; }
std::string ago(double t, double now) {
  const double m = (now - t) / 60000;
  char b[24];
  if (m < 1) return "just now";
  if (m < 60) snprintf(b, sizeof b, "%d min ago", (int) m);
  else snprintf(b, sizeof b, "%d h ago", (int) (m / 60));
  return b;
}
void put_empty(bool err) {
  auto put = [](const char *a, const std::string &x) { ha::put(E, a, x); };
  put("n", "--"), put("role", "hero"), put("accent", "false"), put("where", err ? "offline" : "loading");
  put("count", "--"), put("count_tone", "dim");
  put("trend_label", "trend"), put("trend", "--"), put("trend_tone", "dim");
  put("last", "--"), put("strikes", "[]");
  put("", err ? "error" : "loading");
}
// trend: 1 approaching, -1 receding, 0 steady, 2 none (no word yet)
void put_data(const lx::Data &d, int trend, int kmh, double now) {
  auto put = [](const char *a, const std::string &x) { ha::put(E, a, x); };
  char b[48];
  if (d.has_nearest) {
    snprintf(b, sizeof b, "%d", (int) std::lround(d.nearest.d));
    put("n", b), put("role", "hero"), put("accent", d.nearest.d <= 25 ? "true" : "false");
    put("where", std::string("km ") + lx::compass(d.nearest.az));
  } else {
    put("n", "Quiet"), put("role", "value"), put("accent", "false");
    snprintf(b, sizeof b, "nothing within %d km", d.radius);
    put("where", b);
  }
  snprintf(b, sizeof b, "%d strike%s", d.m15, d.m15 == 1 ? "" : "s");
  put("count", b), put("count_tone", d.m15 ? "text" : "dim");
  if (trend == 1) { snprintf(b, sizeof b, "%d km/h", std::abs(kmh)); put("trend_label", "approaching"), put("trend", b), put("trend_tone", "warn"); }
  else if (trend == -1) { snprintf(b, sizeof b, "%d km/h", std::abs(kmh)); put("trend_label", "moving away"), put("trend", b), put("trend_tone", "ok"); }
  else if (trend == 0) put("trend_label", "trend"), put("trend", "steady"), put("trend_tone", "dim");
  else put("trend_label", "trend"), put("trend", "--"), put("trend_tone", "dim");
  put("last", d.strikes.empty() ? "none" : ago(d.strikes.front().t, now));
  std::string s = "[";
  for (const lx::Strike &x : d.strikes) {
    snprintf(b, sizeof b, "%s[%.1f,%.0f,%.0f]", s.size() > 1 ? "," : "", x.d, x.az, x.t);
    s += b;
  }
  put("strikes", s + "]");
  put("", d.has_nearest ? "storm" : "quiet");
}

// ---- adapter: ha (a sensor in the panel lightning shape)
void rebuild() {
  if (ent_now.empty()) return;
  const std::string *st = ha::get(ent_now), *s = ha::get(ent_now, "s");
  if (!st) { put_empty(false); return; }
  if (*st == "unavailable" || *st == "unknown") { put_empty(true); return; }
  lx::Data d;
  ArduinoJson::JsonDocument j;
  if (s && s->size() > 1 && !deserializeJson(j, s->c_str() + ((*s)[0] == 'J' ? 1 : 0)))
    for (ArduinoJson::JsonArrayConst a : j.as<ArduinoJson::JsonArrayConst>()) {
      double t = a[2] | 0.0;
      if (t < 1e12) t *= 1000;  // unix seconds
      d.strikes.push_back({0, a[0] | 0.0f, a[1] | 0.0f, t});
    }
  const double now = now_ms();
  lx::summarise(d, now);
  const std::string *tr = ha::get(ent_now, "trend"), *k = ha::get(ent_now, "kmh");
  const int trend = !tr ? 2 : *tr == "approaching" ? 1 : *tr == "receding" ? -1 : *tr == "steady" ? 0 : 2;
  put_data(d, trend, k ? atoi(k->c_str()) : 0, now);
}
void entity_put(const std::string &e, const std::string &a, const std::string &) {
  if (!ent_now.empty() && e == ent_now && (a.empty() || a == "s" || a == "trend" || a == "kmh")) rebuild();
}

void sample_now() {
  constexpr double H = 3600000.0, SPEED = 20, START = 0.2 * H, SPAN = 0.4 * H;  // the cell is over the area 0.2-0.6 h
  const double now = now_ms(), t = std::floor(now / H) * H + START + std::fmod(now * SPEED, SPAN);
  lx::Data d = lx::sample(t);
  for (lx::Strike &x : d.strikes) x.t += now - t;  // onto the real clock
  lx::summarise(d, now);
  put_data(d, d.trend, d.kmh, now);
}
void fresh() {
  if (!tick) tick = lv_timer_create([](lv_timer_t *) { fresh(); }, 60000, nullptr);  // ages move on with no new strike
  static lv_timer_t *fast = nullptr;  // the sample storm moves every 2 s
  const std::string &cfg = ha::feed_config(E);
  ArduinoJson::JsonDocument c;
  deserializeJson(c, cfg.empty() ? "{}" : cfg.c_str());
  const std::string src = c["source"] | "ha", ent = c["sensor"] | "";
  const bool changed = cfg != cfg_seen;
  cfg_seen = cfg;
  static const bool hooked = (ha::on_put(entity_put), true);
  (void) hooked;
  sampling = src != "ha";
  if (src == "ha" && ent.rfind("sensor.", 0) == 0) {
    if (changed || ent != ent_now) {
      ent_now = ent;
      for (const char *a : {"", "s", "trend", "kmh"}) ha::want(ent, a);
    }
    rebuild();
  } else if (src == "ha") { ent_now.clear(); put_empty(true); }  // no sensor picked
  else {
    ent_now.clear();
    sample_now();  // sampling is set
    if (!fast) fast = lv_timer_create([](lv_timer_t *) { if (sampling) sample_now(); }, 2000, nullptr);
  }
}
[[maybe_unused]] const bool registered = (ha::on_local(E, fresh), true);  // binds on panel.lightning never reach a host
}  // namespace
}  // namespace fw::dev::lightning_source
