// The clock feed (CONTRACT.md C51): the reserved entity panel.clock (core/spec.h "local sources") in the HA store, the
// one shape the Clock screen's Home card binds to. Same file on the panels and in WASM.
//   state    ok (the device has the time) | loading (not yet: before the panel's clock is set)
//   time     "14:52" (24 h, the card's value; "--:--" while loading)
//   date     "MON 19 OCT" (the short date, the Time component's)
// Adapters: one, the device clock (localtime, checked every second: an attribute changes only when its text does, so
// a bind updates once a minute). The Clock screen's own Time reads the device clock itself (core components/time.cpp);
// the weather line comes from the weather feed (panel.weather brief). More adapters (a fixed time for previews) = more
// rows here, no screen change.
#include <cstdio>
#include <ctime>
#include "fw.h"
#include "ha.h"

namespace fw::dev::clock_source {
namespace {
constexpr const char *E = "panel.clock";
lv_timer_t *tick = nullptr;
void fresh() {
  if (!tick) tick = lv_timer_create([](lv_timer_t *) { fresh(); }, 1000, nullptr);
  const time_t t = time(nullptr);
  struct tm tm {};
  localtime_r(&t, &tm);
  if (tm.tm_year < 100) {  // before 2000: the device has no time yet
    ha::put(E, "", "loading");
    ha::put(E, "time", "--:--");
    ha::put(E, "date", "");
    return;
  }
  static const char *const D[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
  static const char *const M[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
  char b[24];
  snprintf(b, sizeof b, "%02d:%02d", tm.tm_hour, tm.tm_min);
  ha::put(E, "", "ok");
  ha::put(E, "time", b);
  snprintf(b, sizeof b, "%s %d %s", D[tm.tm_wday], tm.tm_mday, M[tm.tm_mon]);
  ha::put(E, "date", b);
}
[[maybe_unused]] const bool registered = (ha::on_local(E, fresh), true);  // binds on panel.clock never reach a host
}  // namespace
}  // namespace fw::dev::clock_source
