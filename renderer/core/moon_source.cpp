// The moon feed (CONTRACT.md C51): the reserved entity panel.moon (core/spec.h "local sources") in the HA store, the
// one shape the Moon screen binds to. Worked out on the panel (core/lunar.h: Meeus phase times, the main lunar terms;
// no API, docs @facet/ui moon.ts moonData). Same file on the panels and in WASM.
//   state     the phase now: new_moon | waxing_crescent | first_quarter | waxing_gibbous | full_moon | waning_gibbous |
//             last_quarter | waning_crescent
//   phase     0..1 through the synodic month ("0.372"; the Moon phase's phase), south "true" | "false" (lat < 0)
//   name, rest  the one line (docs moonLine): "Waxing gibbous", "78% · full on Fri"
//   lit, age  "78%", "9.6 days"; rise, set, full, new "tonight · 18:04" / "Fri · 06:12" (rise / set "--" when none
//             in the next three days); rise_tone, set_tone text | dim
//   next      the next four principal phases, a JSON list of Moon phase columns: [{"phase": 0.5, "label": "Full",
//             "value": "Fri"}]
//   days      the next-days chart (docs moonRangeChart): one bar per evening (21:00) from DAYS_BACK days ago across
//             the content width (8 px a day: a 4 px bar + chart bar-gap), values = illumination %, bands full | past |
//             day, marks full | new | "" (the day the phase crosses 0.5 / 0), names "24 Oct" (the readout), labels the
//             month's short name under the 1st (none in the last 6 days: it would run off), now = DAYS_BACK;
//             title_now / title_details / title_days the header per view ("Moon", "Moon details", "Next 77 days"), days_more "next 77 days", day_name / day_rest the evening now ("Waxing
//             gibbous", "82% · tonight")
//   card_value, card_sub  the Home card: "78%", "Waxing gibbous"
// Adapters (the feed's settings, the spec's "feeds"; core/ha.h feed_config): source
//   panel   worked out on the panel for the place lat / lon (texts, degrees; default the docs sample place) from the
//           clock, again every minute
//   sample  the same maths at a fixed moment (25 Oct 2026 evening, the docs sample place): previews and tests
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>
#include "fw.h"
#include "fx.h"
#include "ha.h"
#include "lunar.h"

namespace fw::dev::moon_source {
namespace {
constexpr const char *E = "panel.moon";
constexpr double SAMPLE_NOW = 1792702800000.0;  // 22 Oct 2026 21:00 UTC: an evening, four days before full
constexpr int DAYS_BACK = 7;                     // docs MOON_RANGE_BACK
lv_timer_t *tick = nullptr;
const char *KEYS[8] = {"new_moon", "waxing_crescent", "first_quarter", "waxing_gibbous", "full_moon", "waning_gibbous", "last_quarter", "waning_crescent"};
const char *SHORT[4] = {"New", "First quarter", "Full", "Last quarter"};
const char *MONTHS[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

void put(const char *a, const std::string &v) { ha::put(E, a, v); }
std::string day_time(double t, double now) { return lunar::day_word(t, now) + " \xC2\xB7 " + lunar::hm(t); }
// the phase name of a fraction (docs moonPhaseName) or of the principal phase the day crosses
int phase_ix(double f) { return f < 0.25 ? 1 : f < 0.5 ? 3 : f < 0.75 ? 5 : 7; }

void put_days(double now) {
  const int slot = fx::size("space:1") + fx::size("chart:bar-gap");
  const int n = std::max(DAYS_BACK + 1, (fw::lay::content_w + fx::size("chart:bar-gap")) / std::max(1, slot));
  const double d0 = lunar::day_start(now - DAYS_BACK * lunar::DAY);
  std::string v, bands, marks, names, labels;
  char b[32];
  std::string day_name, day_rest;
  for (int i = 0; i < n; i++) {
    const double day = lunar::day_start(d0 + (i + 0.5) * lunar::DAY);  // local midnight (DST-safe)
    const double f0 = lunar::frac(day), f1 = lunar::frac(day + lunar::DAY), f = lunar::frac(day + 21 * 3600000.0);
    const auto across = [&](double x) { return f0 < x && f1 >= x; };
    const int ev = f1 < f0 ? 0 : across(0.5) ? 2 : across(0.25) ? 1 : across(0.75) ? 3 : -1;  // principal phase that day
    const long lit = std::lround(lunar::illumination(f) * 100);
    const char *sep = i ? "," : "";
    snprintf(b, sizeof b, "%s%ld", sep, lit), v += b;
    bands += sep, bands += ev == 2 ? "full" : i < DAYS_BACK ? "past" : "day";
    marks += sep, marks += ev == 2 ? "full" : ev == 0 ? "new" : "";
    const time_t tt = (time_t) (day / 1000);
    struct tm tm;
    localtime_r(&tt, &tm);
    snprintf(b, sizeof b, "%s%d %s", sep, tm.tm_mday, MONTHS[tm.tm_mon]), names += b;
    labels += sep;
    if (tm.tm_mday == 1 && i < n - 6) labels += MONTHS[tm.tm_mon];
    if (i == DAYS_BACK) {
      day_name = lunar::PHASE_TEXT[ev >= 0 ? ev * 2 : phase_ix(f)];
      snprintf(b, sizeof b, "%ld%% \xC2\xB7 tonight", lit), day_rest = b;
    }
  }
  put("values", v), put("bands", bands), put("marks", marks), put("names", names), put("labels", labels);
  snprintf(b, sizeof b, "%d", DAYS_BACK), put("now", b);
  snprintf(b, sizeof b, "Next %d days", n - DAYS_BACK), put("title_days", b);
  put("title_now", "Moon"), put("title_details", "Moon details");  // the header's title per view (the spec's $view)
  snprintf(b, sizeof b, "next %d days", n - DAYS_BACK), put("days_more", b);
  put("day_name", day_name), put("day_rest", day_rest);
}

void put_data(double now, double lat, double lon) {
  const lunar::Data m = lunar::data(now, lat, lon);
  char b[64];
  snprintf(b, sizeof b, "%.3f", m.frac), put("phase", b);
  put("south", lat < 0 ? "true" : "false");
  std::string name, rest;
  lunar::line(m, now, &name, &rest);
  put("name", name), put("rest", rest);
  snprintf(b, sizeof b, "%ld%%", std::lround(m.illumination)), put("lit", b);
  snprintf(b, sizeof b, "%.1f days", m.age), put("age", b);
  put("rise", std::isnan(m.next_rise) ? "--" : day_time(m.next_rise, now)), put("rise_tone", std::isnan(m.next_rise) ? "dim" : "text");
  put("set", std::isnan(m.next_set) ? "--" : day_time(m.next_set, now)), put("set_tone", std::isnan(m.next_set) ? "dim" : "text");
  put("full", day_time(m.next_full, now)), put("new", day_time(m.next_new, now));
  std::string js = "[";
  int got = 0;
  for (const lunar::Phase &p : lunar::phases_around(now, 4)) {
    if (p.t <= now || got == 4) continue;
    snprintf(b, sizeof b, "%s{\"phase\":%.2f,\"label\":\"%s\",\"value\":\"", got ? "," : "", p.kind * 0.25, SHORT[p.kind]);
    js += b + lunar::day_word(p.t, now) + "\"}";
    got++;
  }
  put("next", js + "]");
  put_days(now);
  snprintf(b, sizeof b, "%ld%%", std::lround(m.illumination)), put("card_value", b);
  put("card_sub", name);
  put("", KEYS[m.phase]);
}

void fresh() {
  if (!tick) tick = lv_timer_create([](lv_timer_t *) { fresh(); }, 60000, nullptr);  // the moon moves on with the clock
  const std::string &cfg = ha::feed_config(E);
  ArduinoJson::JsonDocument c;
  deserializeJson(c, cfg.empty() ? "{}" : cfg.c_str());
  const std::string src = c["source"] | "panel";
  const char *la = c["lat"] | "", *lo = c["lon"] | "";
  const double lat = *la ? strtod(la, nullptr) : 52.2, lon = *lo ? strtod(lo, nullptr) : -1.2;
  if (src == "sample") put_data(SAMPLE_NOW, 52.2, -1.2);
  else put_data((double) time(nullptr) * 1000, std::isfinite(lat) ? std::clamp(lat, -89.0, 89.0) : 52.2, std::isfinite(lon) ? lon : -1.2);
}
[[maybe_unused]] const bool registered = (ha::on_local(E, fresh), true);  // binds on panel.moon never reach a host
}  // namespace
}  // namespace fw::dev::moon_source
