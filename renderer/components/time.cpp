// Time: the Facet time of day (docs: Components > Clock > Time, @facet/ui clocks.ts timeText() + digits()): the time in
// fixed-width digit cells so it never shifts as it changes, the colon in its own cell raised onto the figures, and in
// 12h a small AM / PM beside it on the baseline. CONTRACT v2 (Header round 3, 2026-10-27): token names only (core/fx.h).
//   size    header (the time at the end of every Header: type:label, fg:text-dim, one label: the approved header
//           clock, docs .fs-hclock = fst-label; label is mono, so it never shifts), compact (clock-compact), mid
//           (Ring / context faces: clock-mid), hero (clock-hero): type styles (tokens/type.json, glyph set "clock")
//   font    mono (default, JetBrains Mono) or inter: the -mono / sans style of the same size
//   cells   digit:<type> / colon:<type> (type.json "cells": em x size, a derived style per clock type) and
//           colon-raise:<type> (the colon onto the figures); the row is fitted up to the 4 px grid (fx::fit_grid)
//   12h     AM / PM in clock-ampm(-mono), clock-ampm-sm beside the compact time, gap:2 after the digits, fg:text-dim,
//           on the time's baseline (baseline:<time type>:<ampm type>); header: "9:42 PM" in the one label
//   seconds prop (off by default): ":SS" in the same cells (compact, mid), or beside the hero in clock-seconds on its
//           baseline; each second only the seconds cell that changed redraws, the layout never shifts
//   time    prop "time" = a fixed "HH:MM[:SS]" that runs on from there (docs, tests); empty = live: localtime, checked
//           every second, a cell changes only when its digit does; the placeholder until the device has the time
// Steps: none (it hugs its digits; C7 note: a time is as wide as its figures). Bleed: none. States: n/a (static: no
// touch, no sound; the placeholder is its "unknown"). Update path: a new time changes the changed cells in place;
// size / font / format / seconds = the contents again. Contrast pairs: colour.json sections "clock", "header".
#include <ctime>
#include "fw.h"
#include "fx.h"

namespace {
struct TimeT {
  std::string type, ap_type, sec_type;  // "type:clock-mid-mono" ... ; header: type = "type:label", no cells
  bool h12 = false, secs = false, header = false;
  std::string fixed, shown;
  long base_s = -1;    // a fixed time: seconds of the day at t0 (it runs on from there)
  uint32_t t0 = 0;
  lv_obj_t *row = nullptr, *srow = nullptr, *ampm = nullptr, *label = nullptr;
  lv_timer_t *timer = nullptr;
};
TimeT *of(lv_obj_t *o) { return (TimeT *) lv_obj_get_user_data(o); }

void parts(const TimeT *d, std::string *hhmm, std::string *ss, std::string *ap) {
  int h = -1, m = 0, s = 0;
  if (d->base_s >= 0) {
    const long t = (d->base_s + (long) (lv_tick_elaps(d->t0) / 1000)) % 86400;
    h = (int) (t / 3600), m = (int) (t / 60 % 60), s = (int) (t % 60);
  } else {
    const time_t now = time(nullptr);
    struct tm tm;
    localtime_r(&now, &tm);
    if (tm.tm_year + 1900 >= 2020) h = tm.tm_hour, m = tm.tm_min, s = tm.tm_sec;
  }
  const std::string P = fw::st::PLACEHOLDER;
  if (h < 0) { *hhmm = P + P + ":" + P + P; *ss = d->secs ? ":" + P + P : ""; *ap = ""; return; }
  char b[8];
  if (d->h12) snprintf(b, sizeof b, "%d:%02d", h % 12 ? h % 12 : 12, m), *ap = h < 12 ? "AM" : "PM";
  else snprintf(b, sizeof b, "%02d:%02d", h, m), *ap = "";
  *hhmm = b;
  snprintf(b, sizeof b, ":%02d", s);
  *ss = d->secs ? b : "";
}
std::vector<std::string> chars(const std::string &s) {  // UTF-8 -> characters
  std::vector<std::string> v;
  for (size_t i = 0; i < s.size();) {
    size_t n = (s[i] & 0x80) == 0 ? 1 : (s[i] & 0xE0) == 0xC0 ? 2 : (s[i] & 0xF0) == 0xE0 ? 3 : 4;
    v.push_back(s.substr(i, n));
    i += n;
  }
  return v;
}
std::string strip(const std::string &type) { return type.substr(5); }  // "type:clock-mid" -> "clock-mid"
void cell(lv_obj_t *row, const std::string &type, const std::string &c, bool colon) {
  lv_obj_t *o = fx::box(row, "free");
  fx::use(o, ((colon ? "colon:" : "digit:") + strip(type)).c_str());
  lv_obj_t *l = fx::text(o, c.c_str(), type.c_str(), "fg:text");
  fx::use(l, "cell-text");  // the cell's width, the figure centred (a colon's advance is wider than its cell)
  lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_CLIP);
  if (colon) fx::use(l, ("colon-raise:" + strip(type)).c_str());
}
// cells of `row` for text t: a cell changes only when its character does (the seconds redraw one cell)
void cells(lv_obj_t *row, const std::string &type, const std::string &t, const std::string &was_t) {
  const std::vector<std::string> now = chars(t), was = chars(was_t);
  bool same_shape = now.size() == was.size() && lv_obj_get_child_count(row) == now.size();
  for (size_t i = 0; same_shape && i < now.size(); i++) same_shape = (now[i] == ":") == (was[i] == ":");
  if (same_shape) {
    for (size_t i = 0; i < now.size(); i++)
      if (now[i] != was[i]) fx::set_text(lv_obj_get_child(lv_obj_get_child(row, i), 0), now[i].c_str(), type.c_str());
  } else {
    lv_obj_clean(row);
    for (const auto &c : now) cell(row, type, c, c == ":");
    fx::fit_grid(row);  // the row up to the grid (cells are em widths), digits centred in it
  }
}
void set(lv_obj_t *root) {
  auto *d = of(root);
  std::string t, ss, ap;
  parts(d, &t, &ss, &ap);
  const std::string key = t + "|" + ss + "|" + ap;
  if (key == d->shown) return;
  if (d->header) {
    fx::set_text(d->label, (t + ss + (ap.empty() ? "" : " " + ap)).c_str(), "type:label");
    fx::fit_grid(d->label);  // on the 4 px grid (what follows it lands on the grid)
    d->shown = key;
    return;
  }
  std::string wt, ws;
  if (!d->shown.empty()) { const size_t a = d->shown.find('|'), b = d->shown.find('|', a + 1); wt = d->shown.substr(0, a); ws = d->shown.substr(a + 1, b - a - 1); }
  if (d->srow) cells(d->row, d->type, t, wt), cells(d->srow, d->sec_type, ss, ws);
  else cells(d->row, d->type, t + ss, wt + ws);  // seconds in the same cells as the time
  if (d->ampm) {  // its box on the grid too (Label fit8), so the whole time is
    fx::set_text(d->ampm, ap.c_str(), d->ap_type.c_str());
    fx::fit_grid(d->ampm);
  }
  d->shown = key;
}
void fixed_from(TimeT *d, const std::string &f) {
  d->fixed = f;
  d->base_s = -1;
  if (f.empty()) return;
  int h = 0, m = 0, sec = 0;
  sscanf(f.c_str(), "%d:%d:%d", &h, &m, &sec);
  d->base_s = (h * 60L + m) * 60 + sec;
  d->t0 = lv_tick_get();
}
std::string sig(fw::Props p) {
  return std::string(p["size"] | "mid") + "|" + (strcmp(p["font"] | "mono", "inter") ? "m" : "i") + "|" +
         (strcmp(p["format"] | "24h", "12h") ? "24" : "12") + "|" + ((p["seconds"] | false) ? "s" : "");
}
void fill(lv_obj_t *root, fw::Props p) {
  auto *d = of(root);
  const std::string size = p["size"] | "mid";
  const bool mono = strcmp(p["font"] | "mono", "inter");
  const std::string f = mono ? "-mono" : "";
  d->header = size == "header";
  d->type = d->header ? "type:label" : "type:clock-" + (size == "hero" || size == "compact" ? size : std::string("mid")) + f;
  d->ap_type = size == "compact" ? "type:clock-ampm-sm" : "type:clock-ampm" + f;
  d->h12 = !strcmp(p["format"] | "24h", "12h");
  d->secs = p["seconds"] | false;
  d->sec_type = size == "hero" ? "type:clock-seconds" + f : d->type;  // beside the hero: HH:MM:SS at 180 px would not fit
  fixed_from(d, p["time"] | "");
  d->shown.clear();
  lv_obj_remove_style_all(root);
  lv_obj_remove_flag(root, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_add_flag(root, LV_OBJ_FLAG_EVENT_BUBBLE);
  fw::shape(root, sig(p));
  for (const char *t : {"box", "row-top", "hug", "gap:2"}) fx::use(root, t);
  d->row = d->srow = d->ampm = d->label = nullptr;
  if (d->header) {
    fx::use(root, "label-row");  // the label's box tall (its text-box translate never shortens the root)
    d->label = fx::text(root, "", "type:label", "fg:text-dim");
  } else {
    d->row = fx::box(root, "cells");  // a row, its cells centred in it (fit_grid rounds it up to the grid)
    fx::use(d->row, "hug");
    if (d->secs && d->sec_type != d->type) {  // smaller seconds, their figures on the time's baseline
      d->srow = fx::box(root, "row-top");
      fx::use(d->srow, "hug");
      fx::use(d->srow, ("baseline:" + strip(d->type) + ":" + strip(d->sec_type)).c_str());
    }
    if (d->h12) {
      d->ampm = fx::text(root, "PM", d->ap_type.c_str(), "fg:text-dim");
      fx::use(d->ampm, ("baseline:" + strip(d->type) + ":" + strip(d->ap_type)).c_str());
    }
  }
  set(root);
  if (!d->timer) d->timer = lv_timer_create([](lv_timer_t *t) { set((lv_obj_t *) lv_timer_get_user_data(t)); }, 1000, root);
}
}  // namespace

FW_COMPONENT(time, "Time", "components", R"({
  "about": "The time in fixed-width digit cells, so it never shifts as it changes.",
  "contract": 2,
  "parts": [],
  "min": ["layout:dot", "layout:dot"],
  "frame": [480, 160],
  "props": [
    {"name": "size", "about": "How big the digits are: header for the time at the end of every screen's header (small caps, dim), compact for a small clock, mid for a card, hero for a screen's main figure.", "type": "select", "options": ["header", "compact", "mid", "hero"], "default": "mid"},
    {"name": "font", "about": "The typeface of the digits: mono is the fixed-width face, inter the rounder everyday face.", "type": "select", "options": ["mono", "inter"], "default": "mono"},
    {"name": "format", "about": "24h shows 13:00; 12h shows 1:00 with AM or PM.", "type": "select", "options": ["24h", "12h"], "default": "24h"},
    {"name": "seconds", "about": "Shows the seconds as well as hours and minutes.", "type": "bool", "default": false},
    {"name": "time", "about": "For previews only: a fixed time to show, like 14:52:20. Leave empty for the real time.", "type": "text", "default": ""}
  ],
  "variants": [
    {"label": "mid · mono · 14:52", "props": {"time": "14:52"}},
    {"label": "mid · inter · 14:52", "props": {"time": "14:52", "font": "inter"}},
    {"label": "mid · 12h · 2:52 PM", "props": {"time": "14:52", "format": "12h"}},
    {"label": "header · 21:42", "props": {"time": "21:42", "size": "header"}},
    {"label": "header · 12h", "props": {"time": "21:42", "size": "header", "format": "12h"}},
    {"label": "compact · 09:05", "props": {"time": "09:05", "size": "compact"}},
    {"label": "compact · 12h", "props": {"time": "09:05", "size": "compact", "format": "12h"}},
    {"label": "hero · 10:08", "props": {"time": "10:08", "size": "hero"}},
    {"label": "mid · seconds · mono", "props": {"time": "14:52:07", "seconds": true}},
    {"label": "mid · seconds · inter · 12h", "props": {"time": "14:52:07", "seconds": true, "font": "inter", "format": "12h"}},
    {"label": "compact · seconds", "props": {"time": "09:05:30", "size": "compact", "seconds": true}},
    {"label": "hero · seconds", "props": {"time": "10:08:30", "size": "hero", "seconds": true}},
    {"label": "hero · seconds · inter · 12h", "props": {"time": "22:08:30", "size": "hero", "seconds": true, "font": "inter", "format": "12h"}}
  ]
})") {
  auto *d = new TimeT;
  lv_obj_t *root = lv_obj_create(parent);
  fx::name(root, "time");
  lv_obj_set_user_data(root, d);
  lv_obj_add_event_cb(root, [](lv_event_t *e) {
    auto *d = (TimeT *) lv_event_get_user_data(e);
    if (d->timer) lv_timer_delete(d->timer);
    delete d;
  }, LV_EVENT_DELETE, d);
  fill(root, p);
}

FW_UPDATE(time) {
  auto *d = of(root);
  if (!d) return false;
  if (!fw::shape(root, sig(p))) {
    fw::refill(root);
    fill(root, p);
    return true;
  }
  const std::string fixed = p["time"] | "";
  if (fixed != d->fixed) fixed_from(d, fixed);
  set(root);
  return true;
}
