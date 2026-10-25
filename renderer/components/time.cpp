// Time: the Facet time of day (docs: Components > Clock > Time, @facet/ui clocks.ts timeText() + digits()): the time in
// fixed-width digit cells so it never shifts as it changes, the colon in its own cell raised onto the figures, and in
// 12h a small AM / PM beside it on the baseline.
//   size    compact (a header: clock-compact), mid (Ring / context faces: clock-mid), hero (clock-hero): type styles
//           (tokens/type.json, glyph set "clock" = 0-9 : and the placeholder only, so the big fonts stay small)
//   font    mono (default, JetBrains Mono) or inter: the -mono / sans style of the same size
//   cells   type.json "cells" per style: each digit centred in a digit-wide cell (em x size), the colon cell and its
//           raise (docs digit / colon / colonY); the row is rounded up to the grid and centred in it
//   12h     AM / PM in clock-ampm(-mono), clock-ampm-sm beside the compact time, sp::s2 after the digits, text-dim
//   seconds prop (Darren 2026-10-22, off by default): ":SS" in the same cells (compact, mid), or beside the hero in
//           clock-seconds (56 / 36) on its baseline (HH:MM:SS at 180 px would not fit the screen); each second only the
//           seconds cell that changed redraws, the layout never shifts
//   time    prop "time" = a fixed "HH:MM[:SS]" that runs on from there (docs, tests, the audit); empty = live:
//           localtime, checked every second, a cell changes only when its digit does; "--:--" (the placeholder) until
//           the device has the time
// Static (no touch, no sound). Update path: a new time changes the changed cells in place; size / font / format refill.
// Contrast pairs: colour.json section "clock".
#include <ctime>
#include "fw.h"

namespace {
struct TimeT {
  fw::ty::Id st = fw::ty::clock_mid_mono, ap_st = fw::ty::clock_ampm_mono, sec_st = fw::ty::clock_mid_mono;
  bool h12 = false, secs = false;
  std::string fixed, shown;
  long base_s = -1;    // a fixed time: seconds of the day at t0 (it runs on from there)
  uint32_t t0 = 0;
  lv_obj_t *row = nullptr, *srow = nullptr, *ampm = nullptr;
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
// split UTF-8 into characters
std::vector<std::string> chars(const std::string &s) {
  std::vector<std::string> v;
  for (size_t i = 0; i < s.size();) {
    size_t n = (s[i] & 0x80) == 0 ? 1 : (s[i] & 0xE0) == 0xC0 ? 2 : (s[i] & 0xF0) == 0xE0 ? 3 : 4;
    v.push_back(s.substr(i, n));
    i += n;
  }
  return v;
}
void cell(lv_obj_t *row, fw::ty::Id st, const std::string &c, bool colon) {
  const fw::ty::Cells *k = fw::ty::cells(st);
  const int size = fw::style(st).size;
  lv_obj_t *o = lv_obj_create(row);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_size(o, (int) std::lround((colon ? k->colon : k->digit) * size), fw::style(st).box);
  lv_obj_t *l = fw::text(o, st, c.c_str(), fw::T->text);
  lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(l, LV_ALIGN_TOP_MID, 0, 0);
  if (colon) lv_obj_set_style_translate_y(l, lv_obj_get_style_translate_y(l, LV_PART_MAIN) + (int) std::lround(k->colon_y * size), 0);
}
// cells of `row` for text t in style st: a cell changes only when its character does (the seconds redraw one cell)
void cells(lv_obj_t *row, fw::ty::Id st, const std::string &t, const std::string &was_t) {
  const std::vector<std::string> now = chars(t), was = chars(was_t);
  bool same_shape = now.size() == was.size() && lv_obj_get_child_count(row) == now.size();
  for (size_t i = 0; same_shape && i < now.size(); i++) same_shape = (now[i] == ":") == (was[i] == ":");
  if (same_shape) {
    for (size_t i = 0; i < now.size(); i++)
      if (now[i] != was[i]) lv_label_set_text(lv_obj_get_child(lv_obj_get_child(row, i), 0), now[i].c_str());
  } else {
    lv_obj_clean(row);
    for (const auto &c : now) cell(row, st, c, c == ":");
  }
  // the row rounded up to the grid (cells are em widths), digits centred in it
  lv_obj_update_layout(row);
  int w = 0;
  for (uint32_t i = 0; i < lv_obj_get_child_count(row); i++) w += lv_obj_get_width(lv_obj_get_child(row, i));
  lv_obj_set_width(row, (w + fw::sp::GRID - 1) / fw::sp::GRID * fw::sp::GRID);
}
// where a style's figures end below its box top (cap height centred in the box: the text-box rule)
int cap_bottom(fw::ty::Id st, uint32_t ch) {
  const lv_font_t *f = fw::font(st);
  lv_font_glyph_dsc_t g;
  const int cap = lv_font_get_glyph_dsc(f, &g, ch, 0) ? g.box_h : f->line_height / 2;
  return (fw::style(st).box + cap) / 2;
}
void set(lv_obj_t *root) {
  auto *d = of(root);
  std::string t, ss, ap;
  parts(d, &t, &ss, &ap);
  const std::string key = t + "|" + ss + "|" + ap;
  if (key == d->shown) return;
  std::string wt, ws;
  if (!d->shown.empty()) { const size_t a = d->shown.find('|'), b = d->shown.find('|', a + 1); wt = d->shown.substr(0, a); ws = d->shown.substr(a + 1, b - a - 1); }
  if (d->srow) cells(d->row, d->st, t, wt), cells(d->srow, d->sec_st, ss, ws);
  else cells(d->row, d->st, t + ss, wt + ws);  // seconds in the same cells as the time
  if (d->ampm) {  // its box on the grid too (Label fit8), so the whole time is
    fw::set_text(d->ampm, d->ap_st, ap.c_str());
    lv_obj_set_width(d->ampm, LV_SIZE_CONTENT);
    lv_obj_update_layout(d->ampm);
    lv_obj_set_width(d->ampm, (lv_obj_get_width(d->ampm) + fw::sp::GRID - 1) / fw::sp::GRID * fw::sp::GRID);
  }
  d->shown = key;
}
void fill(lv_obj_t *root, fw::Props p) {
  auto *d = of(root);
  const char *size = p["size"] | "mid";
  const bool mono = strcmp(p["font"] | "mono", "inter");
  d->st = !strcmp(size, "hero") ? (mono ? fw::ty::clock_hero_mono : fw::ty::clock_hero)
        : !strcmp(size, "compact") ? (mono ? fw::ty::clock_compact_mono : fw::ty::clock_compact)
        : (mono ? fw::ty::clock_mid_mono : fw::ty::clock_mid);
  d->ap_st = !strcmp(size, "compact") ? fw::ty::clock_ampm_sm : mono ? fw::ty::clock_ampm_mono : fw::ty::clock_ampm;
  d->h12 = !strcmp(p["format"] | "24h", "12h");
  d->secs = p["seconds"] | false;
  // seconds: the same cells as the time, except beside the hero (HH:MM:SS at 180 px would not fit the screen)
  d->sec_st = !strcmp(size, "hero") ? (mono ? fw::ty::clock_seconds_mono : fw::ty::clock_seconds) : d->st;
  d->fixed = p["time"] | "";
  d->base_s = -1;
  if (!d->fixed.empty()) {
    int h = 0, m = 0, sec = 0;
    sscanf(d->fixed.c_str(), "%d:%d:%d", &h, &m, &sec);
    d->base_s = (h * 60L + m) * 60 + sec;
    d->t0 = lv_tick_get();
  }
  d->shown.clear();
  lv_obj_remove_style_all(root);
  lv_obj_remove_flag(root, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_size(root, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(root, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(root, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_set_style_pad_column(root, fw::sp::s2, 0);
  d->row = lv_obj_create(root);
  lv_obj_remove_style_all(d->row);
  lv_obj_remove_flag(d->row, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_size(d->row, LV_SIZE_CONTENT, fw::style(d->st).box);
  lv_obj_set_flex_flow(d->row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(d->row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  d->srow = nullptr;
  if (d->secs && d->sec_st != d->st) {  // smaller seconds, their figures on the time's baseline
    d->srow = lv_obj_create(root);
    lv_obj_remove_style_all(d->srow);
    lv_obj_remove_flag(d->srow, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
    lv_obj_set_size(d->srow, LV_SIZE_CONTENT, fw::style(d->sec_st).box);
    lv_obj_set_flex_flow(d->srow, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_margin_top(d->srow, cap_bottom(d->st, '0') - cap_bottom(d->sec_st, '0'), 0);
  }
  d->ampm = nullptr;
  if (d->h12) {
    d->ampm = fw::text(root, d->ap_st, "PM", fw::T->text_dim);
    lv_obj_set_height(d->ampm, fw::style(d->ap_st).box);
    lv_obj_set_style_margin_top(d->ampm, cap_bottom(d->st, '0') - cap_bottom(d->ap_st, 'M'), 0);  // on the baseline
  }
  set(root);
  if (!d->timer) d->timer = lv_timer_create([](lv_timer_t *t) { set((lv_obj_t *) lv_timer_get_user_data(t)); }, 1000, root);
  fw::shape(root, std::string(size) + "|" + (mono ? "m" : "i") + "|" + (d->h12 ? "12" : "24") + "|" + (d->secs ? "s" : ""));
}
}  // namespace

FW_COMPONENT(time, "Time", "components", R"({
  "about": "The time in fixed-width digit cells, so it never shifts as it changes.",
  "frame": [480, 160],
  "props": [
    {"name": "size", "about": "How big the digits are: compact for a header, mid for a card, hero for a screen's main figure.", "type": "select", "options": ["compact", "mid", "hero"], "default": "mid"},
    {"name": "font", "about": "The typeface of the digits: mono is the fixed-width face, inter the rounder everyday face.", "type": "select", "options": ["mono", "inter"], "default": "mono"},
    {"name": "format", "about": "24h shows 13:00; 12h shows 1:00 with AM or PM.", "type": "select", "options": ["24h", "12h"], "default": "24h"},
    {"name": "seconds", "about": "Shows the seconds as well as hours and minutes.", "type": "bool", "default": false},
    {"name": "time", "about": "For previews only: a fixed time to show, like 14:52:20. Leave empty for the real time.", "type": "text", "default": ""}
  ],
  "variants": [
    {"label": "mid · mono · 14:52", "props": {"time": "14:52"}},
    {"label": "mid · inter · 14:52", "props": {"time": "14:52", "font": "inter"}},
    {"label": "mid · 12h · 2:52 PM", "props": {"time": "14:52", "format": "12h"}},
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
  const char *size = p["size"] | "mid";
  const bool mono = strcmp(p["font"] | "mono", "inter");
  const bool h12 = !strcmp(p["format"] | "24h", "12h"), secs = p["seconds"] | false;
  if (!fw::shape(root, std::string(size) + "|" + (mono ? "m" : "i") + "|" + (h12 ? "12" : "24") + "|" + (secs ? "s" : ""))) {
    fw::refill(root);
    fill(root, p);
    return true;
  }
  const std::string fixed = p["time"] | "";
  if (fixed != d->fixed) {
    d->fixed = fixed;
    d->base_s = -1;
    if (!fixed.empty()) {
      int h = 0, m = 0, sec = 0;
      sscanf(fixed.c_str(), "%d:%d:%d", &h, &m, &sec);
      d->base_s = (h * 60L + m) * 60 + sec;
      d->t0 = lv_tick_get();
    }
  }
  set(root);
  return true;
}
