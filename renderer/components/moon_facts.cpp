// Moon facts: what the moon maths works out with no API (docs: Screen parts > Moon facts, @facet/ui moon.ts moonData()
// + moonLine(), the Moon screen's details): the one line (the phase glyph at ic::feature + "Waxing gibbous · 78% ·
// full on Fri", type title), then six Stats in two columns (lit, age, moonrise, moonset, next full, next new), each a
// real Stat (fw::find("stat"), tone text). The maths = core/lunar.h (Meeus phase times, rise / set to a few minutes).
//   place   lat / lon props (a fictional default, the docs sample); south of the equator = the mirrored glyph
//   when    now + "ahead" days (the docs control); "now" prop = a fixed time (ms since the epoch, tests), 0 = live
// Static. Update path: the Stats' own (text in place) and the glyph redrawn in place. Contrast: "stat" + "moon".
#include <ctime>
#include "fw.h"
#include "lunar.h"

namespace {
using fw::lunar::DAY;
using fw::lunar::hm;
std::string day(double ms, double now) { return fw::lunar::day_word(ms, now); }
double now_of(fw::Props p) {
  const double fixed = p["now"] | 0.0;
  const double base = fixed > 0 ? fixed : (double) time(nullptr) * 1000;
  return base + (double) (p["ahead"] | 0) * DAY;
}
struct Facts { std::string line; std::string v[6]; float frac; bool south; };
const char *LABELS[6] = {"lit", "age", "moonrise", "moonset", "next full", "next new"};
Facts facts(fw::Props p) {
  const double now = now_of(p), lat = p["lat"] | 52.2, lon = p["lon"] | -1.2;
  const fw::lunar::Data m = fw::lunar::data(now, lat, lon);
  Facts f;
  f.frac = (float) m.frac;
  f.south = lat < 0;
  std::string name, rest;
  fw::lunar::line(m, now, &name, &rest);
  f.line = name + " \xC2\xB7 " + rest;
  char b[96];
  snprintf(b, sizeof b, "%ld%%", std::lround(m.illumination));
  f.v[0] = b;
  snprintf(b, sizeof b, "%.1f days", m.age);
  f.v[1] = b;
  f.v[2] = std::isnan(m.next_rise) ? "--" : day(m.next_rise, now) + " · " + hm(m.next_rise);
  f.v[3] = std::isnan(m.next_set) ? "--" : day(m.next_set, now) + " · " + hm(m.next_set);
  f.v[4] = day(m.next_full, now) + " · " + hm(m.next_full);
  f.v[5] = day(m.next_new, now) + " · " + hm(m.next_new);
  return f;
}
lv_obj_t *box(lv_obj_t *parent) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  return o;
}
void stat_props(const Facts &f, int i, ArduinoJson::JsonDocument &d) { d["label"] = LABELS[i]; d["value"] = f.v[i]; d["tone"] = "text"; }
void fill(lv_obj_t *root, fw::Props p) {
  const fw::Theme &t = *fw::T;
  const Facts f = facts(p);
  lv_obj_remove_style_all(root);
  lv_obj_remove_flag(root, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_size(root, fw::lay::content_w, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(root, fw::sp::s9, 0);
  lv_obj_t *top = box(root);
  lv_obj_set_size(top, LV_PCT(100), fw::ic::PX[fw::ic::feature]);
  lv_obj_set_flex_flow(top, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(top, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(top, fw::sp::s5, 0);
  fw::moon_icon(top, f.frac, fw::ic::feature, f.south);
  lv_obj_t *l = fw::text(top, fw::ty::title, f.line.c_str(), t.text);
  lv_obj_set_height(l, fw::style(fw::ty::title).box);
  lv_obj_t *grid = box(root);
  lv_obj_set_size(grid, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_style_pad_row(grid, fw::sp::s9, 0);
  lv_obj_set_style_pad_column(grid, fw::lay::gutter, 0);
  if (const fw::Component *sc = fw::find("stat"))
    for (int i = 0; i < 6; i++) {
      ArduinoJson::JsonDocument d;
      stat_props(f, i, d);
      lv_obj_t *cell = box(grid);  // Home columns: the contract Stat fills its parent (C5)
      lv_obj_set_size(cell, fw::lay::card_w, LV_SIZE_CONTENT);
      sc->build(cell, d.as<fw::Props>());
    }
}
}  // namespace

FW_COMPONENT(moon_facts, "Moon facts", "components", R"({
  "about": "What the moon maths works out with no API: the one line, how much is lit, its age, moonrise and moonset, the next full and new moon.",
  "frame": [464, 240],
  "props": [
    {"name": "ahead", "about": "For previews only: how many days ahead of today to work the moon out for. 0 is today.", "type": "range", "min": 0, "max": 29, "step": 1, "default": 0},
    {"name": "lat", "about": "Your latitude in degrees, which sets the moonrise and moonset times.", "type": "range", "min": -60, "max": 65, "step": 0.1, "default": 52.2},
    {"name": "lon", "about": "Your longitude in degrees, which sets the moonrise and moonset times.", "type": "range", "min": -180, "max": 180, "step": 0.1, "default": -1.2}
  ],
  "variants": [
    {"label": "tonight", "props": {"now": 1792944000000}},
    {"label": "a week on", "props": {"now": 1792944000000, "ahead": 7}},
    {"label": "southern hemisphere", "props": {"now": 1792944000000, "lat": -33.9, "lon": 151.2}}
  ]
})") {
  lv_obj_t *root = lv_obj_create(parent);
  fill(root, p);
}

FW_UPDATE(moon_facts) {
  const Facts f = facts(p);
  lv_obj_t *top = lv_obj_get_child(root, 0), *grid = lv_obj_get_child(root, 1);
  if (!top || !grid || lv_obj_get_child_count(grid) != 6) { fw::refill(root); fill(root, p); return true; }
  fw::moon_icon_set(lv_obj_get_child(top, 0), f.frac, f.south);
  fw::set_text(lv_obj_get_child(top, 1), fw::ty::title, f.line.c_str());
  for (int i = 0; i < 6; i++) {
    ArduinoJson::JsonDocument d;
    stat_props(f, i, d);
    fw::update_child(fw::find("stat"), lv_obj_get_child(lv_obj_get_child(grid, i), 0), d.as<fw::Props>());
  }
  return true;
}
