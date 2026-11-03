// The clock feed (CONTRACT.md C51): the reserved entity panel.clock (core/spec.h "local sources") in the HA store, the
// one shape the Clock screen's Home card binds to. On the Feed base (core/feed.h). Same file on the panels and in WASM.
//   state    ok (the device has the time) | loading (not yet: before the panel's clock is set)
//   time     required: "14:52" (24 h, the card's value; placeholder "--:--" while loading)
//   date     required: "MON 19 OCT" (the short date, the Time component's; placeholder "")
//   day      optional: "Mon 19 Oct" (the same date in sentence case: the Home card's line under the time)
// Adapters: one, the device clock (localtime, checked every second: an attribute changes only when its text does, so
// a bind updates once a minute). The Clock screen's own Time reads the device clock itself (core components/time.cpp).
// The Clock shows no weather and reads nothing from Home Assistant (like Moon and Timer). More adapters (a fixed time
// for previews) = more rows here, no screen change.
#include <cstdio>
#include <ctime>
#include "feed.h"
#include "fw.h"

namespace fw::dev::clock_source {
namespace {
class Clock : public feed::Feed {
 public:
  Clock() : Feed("panel.clock", 1000, "panel", {{"time", true, "--:--"}, {"date", true, ""}, {"day", false, ""}}) {}

 protected:
  void fill() override {
    const time_t t = time(nullptr);
    struct tm tm {};
    localtime_r(&t, &tm);
    if (tm.tm_year < 100) { state(feed::LOADING); return; }  // before 2000: the device has no time yet
    static const char *const D[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
    static const char *const M[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
    static const char *const Dd[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    static const char *const Mm[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    char b[24];
    snprintf(b, sizeof b, "%02d:%02d", tm.tm_hour, tm.tm_min);
    state(feed::OK);
    put("time", b);
    snprintf(b, sizeof b, "%s %d %s", D[tm.tm_wday], tm.tm_mday, M[tm.tm_mon]);
    put("date", b);
    snprintf(b, sizeof b, "%s %d %s", Dd[tm.tm_wday], tm.tm_mday, Mm[tm.tm_mon]);
    put("day", b);
  }
  void sample() override { fill(); }  // the device clock is its own sample (the Clock screen previews the time now)
};
Clock F;
}  // namespace
}  // namespace fw::dev::clock_source
