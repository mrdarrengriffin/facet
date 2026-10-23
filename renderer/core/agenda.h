// Agenda feed (docs @facet/ui screens/calendar.ts helpers + @facet/demo-data calendar()): the events the Calendar
// screen reads, with the docs wording ("in 25 min", "today 16:50", "16:50 – 17:50", "all day · 3 days"). Times are ms
// since the epoch in the device's time zone; an all-day event runs from local midnight to midnight. A panel fetches
// events from HA (calendar.get_events, a host job); sample() is the docs story until then. Header-only.
#pragma once
#include <cmath>
#include <cstdio>
#include <ctime>
#include <string>
#include <vector>

namespace fw::agenda {
constexpr double DAY = 86400000.0, HOUR = 3600000.0, SOON = 15 * 60000.0;
struct Cal { const char *id, *name, *colour; bool on; };  // colour = a colour.json token name
struct Event { std::string summary, location, calendar; double start, end; bool all_day; };
constexpr Cal CALENDARS[] = {{"family", "Family", "blue", true}, {"work", "Work", "ok", true}, {"school", "School", "warn", true},
                             {"birthdays", "Birthdays", "steel", true}, {"holidays", "Holidays", "text-dim", true}, {"bins", "Bins", "text", false}};
inline double midnight(double t) {
  const time_t s = (time_t) (t / 1000);
  struct tm tm;
  localtime_r(&s, &tm);
  tm.tm_hour = tm.tm_min = tm.tm_sec = 0;
  tm.tm_isdst = -1;
  return (double) mktime(&tm) * 1000;
}
inline double day_add(double t, int n) {  // local midnight n days on (DST-safe)
  const time_t s = (time_t) (midnight(t) / 1000);
  struct tm tm;
  localtime_r(&s, &tm);
  tm.tm_mday += n;
  tm.tm_hour = tm.tm_min = tm.tm_sec = 0;
  tm.tm_isdst = -1;
  return (double) mktime(&tm) * 1000;
}
inline long day_diff(double t, double now) { return std::lround((midnight(t) - midnight(now)) / DAY); }
inline std::string hm(double t) {
  const time_t s = (time_t) (t / 1000);
  struct tm tm;
  localtime_r(&s, &tm);
  char b[8];
  snprintf(b, sizeof b, "%02d:%02d", tm.tm_hour, tm.tm_min);
  return b;
}
inline std::string wd(double t) {
  static const char *W[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  const time_t s = (time_t) (t / 1000);
  struct tm tm;
  localtime_r(&s, &tm);
  return W[tm.tm_wday];
}
inline std::string dm(double t) {
  static const char *M[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  const time_t s = (time_t) (t / 1000);
  struct tm tm;
  localtime_r(&s, &tm);
  char b[16];
  snprintf(b, sizeof b, "%d %s", tm.tm_mday, M[tm.tm_mon]);
  return b;
}
inline std::string long_day(double t) { return wd(t) + " " + dm(t); }  // "Tue 20 Oct"
inline bool running(const Event &e, double now) { return e.start <= now && e.end > now; }
// the hero event: the running or next timed event, or an all-day one from tomorrow on
inline const Event *next(const std::vector<Event> &ev, double now) {
  for (const Event &e : ev)
    if (e.end > now && (!e.all_day || e.start > now)) return &e;
  return nullptr;
}
// "16:50 – 17:50", "all day", "all day · 3 days"
inline std::string time_text(const Event &e) {
  if (e.all_day) {
    const long n = std::lround((e.end - e.start) / DAY);
    if (n > 1) return "all day \xC2\xB7 " + std::to_string(n) + " days";
    return "all day";
  }
  return hm(e.start) + " \xE2\x80\x93 " + hm(e.end);
}
inline const char *colour_of(const std::string &cal) {
  for (const Cal &c : CALENDARS) if (cal == c.id) return c.colour;
  return "text-dim";
}
inline const char *cal_name(const std::string &cal) {
  for (const Cal &c : CALENDARS) if (cal == c.id) return c.name;
  return "";
}
// the docs story (two weeks from today; Bins hidden like a calendar you'd rather not see on a wall)
inline std::vector<Event> sample(double now) {
  const double d0 = midnight(now);
  auto at = [&](int n, double h, double len, const char *s, const char *cal, const char *loc = "") {
    const double st = day_add(d0, n) + h * HOUR;
    return Event{s, loc, cal, st, st + len * HOUR, false};
  };
  auto all = [&](int n, int days, const char *s, const char *cal) { return Event{s, "", cal, day_add(d0, n), day_add(d0, n + days), true}; };
  const double pot = d0 + 17.5 * HOUR > now + 20 * 60000 ? d0 + 17.5 * HOUR : now + 50 * 60000;
  std::vector<Event> ev = {
      at(0, 8.5, 0.5, "School run", "school"),
      Event{"Pottery class", "Larkfield studio", "family", pot, pot + 1.5 * HOUR, false},
      at(1, 10, 1, "Team planning", "work"),
      at(1, 18, 1, "Swimming lessons", "family", "Leisure centre"),
      all(2, 1, "Recycling out", "bins"),
      all(3, 1, "Sam's birthday", "birthdays"),
      at(3, 19.5, 1.5, "Book club", "family"),
      all(5, 2, "Weekend away", "family"),
      at(8, 18, 1, "Parents' evening", "school"),
      all(9, 1, "Recycling out", "bins"),
      at(11, 9, 1, "Boiler service", "family"),
      all(12, 5, "Half term", "holidays"),
  };
  std::vector<Event> on;
  for (Event &e : ev) {
    bool vis = true;
    for (const Cal &c : CALENDARS) if (e.calendar == c.id) vis = c.on;
    if (vis) on.push_back(e);
  }
  for (size_t i = 1; i < on.size(); i++)  // by start (all-day first on a tie)
    for (size_t j = i; j > 0 && (on[j].start < on[j - 1].start || (on[j].start == on[j - 1].start && on[j].all_day && !on[j - 1].all_day)); j--)
      std::swap(on[j], on[j - 1]);
  return on;
}
// the events by local day from today (an event over several days is on each), at most `max_days` days
struct DayGroup { double day; std::vector<const Event *> ev; };
inline std::vector<DayGroup> days(const std::vector<Event> &ev, double now, int max_days = 31) {
  std::vector<DayGroup> out;
  for (const Event &e : ev)
    for (double t = std::max(midnight(e.start), midnight(now)); t < e.end; t = day_add(t, 1)) {
      DayGroup *g = nullptr;
      for (DayGroup &x : out) if (x.day == t) g = &x;
      if (!g) { out.push_back({t, {}}); g = &out.back(); }
      g->ev.push_back(&e);
    }
  for (size_t i = 1; i < out.size(); i++)
    for (size_t j = i; j > 0 && out[j].day < out[j - 1].day; j--) std::swap(out[j], out[j - 1]);
  if ((int) out.size() > max_days) out.resize((size_t) max_days);
  return out;
}
}  // namespace fw::agenda
