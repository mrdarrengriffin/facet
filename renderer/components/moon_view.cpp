// Moon view: the Moon screen's summary body (docs screens/moon.ts moonParts nodes; the factory spec renderer/specs/
// moon.json adds the header and the "details" more row that opens Moon details): the moon itself as the hero, drawn
// from the phase (fw::moon_icon_px at lay::moon_hero; south of the equator = mirrored), and one line under it:
// "Waxing gibbous" (title, text) + " · 78% · full on Fri" (title, text-dim), sp::s9 under the moon, the two centred
// as one. The maths = core/lunar.h (no API).
//   box      content width x (content height - lay::more_row_h): the more row under it is the spec's; size
//            "weather" = the Weather screen's Moon tab (lay::moon_hero_tab, its own height)
//   place    lat / lon props (the docs sample), now = a fixed time (ms, tests), 0 = live; the view re-works the moon
//            every minute (clock-tick x 60) while live
// Update path: the glyph redraws in place (moon_icon_set), the line's texts in place. Static to touch. Contrast:
// "moon" (+ "text").
#include <ctime>
#include "fw.h"
#include "lunar.h"

namespace {
struct MV { lv_obj_t *hero = nullptr, *name = nullptr, *rest = nullptr; lv_timer_t *tick = nullptr; double fixed = 0, lat = 0, lon = 0; };
MV *of(lv_obj_t *o) { return (MV *) lv_obj_get_user_data(o); }
lv_obj_t *box(lv_obj_t *parent) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  return o;
}
void show(lv_obj_t *root) {
  MV *v = of(root);
  const double now = v->fixed > 0 ? v->fixed : (double) time(nullptr) * 1000;
  const fw::lunar::Data m = fw::lunar::data(now, v->lat, v->lon);
  fw::moon_icon_set(v->hero, (float) m.frac, v->lat < 0);
  std::string name, rest;
  fw::lunar::line(m, now, &name, &rest);
  fw::set_text(v->name, fw::ty::title, name.c_str());
  fw::set_text(v->rest, fw::ty::title, (" \xC2\xB7 " + rest).c_str());
}
void take(lv_obj_t *root, fw::Props p) {
  MV *v = of(root);
  v->fixed = p["now"] | 0.0;
  v->lat = p["lat"] | 52.2;
  v->lon = p["lon"] | -1.2;
  show(root);
}
}  // namespace

FW_COMPONENT(moon_view, "Moon view", "components", R"({
  "about": "The Moon screen's summary: the moon itself as the hero, drawn from tonight's phase, and one line under it.",
  "frame": [480, 360],
  "props": [
    {"name": "size", "about": "screen fills the Moon screen; weather fits it inside the Weather screen's moon view.", "type": "select", "options": ["screen", "weather"], "default": "screen"},
    {"name": "lat", "about": "Your latitude in degrees, used to work out tonight's moon.", "type": "range", "min": -60, "max": 65, "step": 0.1, "default": 52.2},
    {"name": "lon", "about": "Your longitude in degrees, used to work out tonight's moon.", "type": "range", "min": -180, "max": 180, "step": 0.1, "default": -1.2}
  ],
  "variants": [
    {"label": "tonight", "props": {"now": 1792944000000}},
    {"label": "new moon", "props": {"now": 1793714400000}},
    {"label": "southern hemisphere", "props": {"now": 1792944000000, "lat": -33.9, "lon": 151.2}},
    {"label": "the Weather screen's tab", "props": {"now": 1792944000000, "size": "weather"}}
  ]
})") {
  const fw::Theme &t = *fw::T;
  auto *v = new MV;
  lv_obj_t *root = lv_obj_create(parent);
  lv_obj_remove_style_all(root);
  lv_obj_remove_flag(root, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_user_data(root, v);
  lv_obj_add_event_cb(root, [](lv_event_t *e) {
    auto *v = (MV *) lv_event_get_user_data(e);
    if (v->tick) lv_timer_delete(v->tick);
    delete v;
  }, LV_EVENT_DELETE, v);
  const bool tab = !strcmp(p["size"] | "screen", "weather");  // the Weather screen's Moon tab: a smaller moon, its own height
  lv_obj_set_size(root, fw::lay::content_w, tab ? LV_SIZE_CONTENT : fw::lay::content_h - fw::lay::more_row_h);
  lv_obj_t *col = box(root);
  lv_obj_set_size(col, fw::lay::content_w, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(col, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(col, fw::sp::s9, 0);
  v->hero = fw::moon_icon_px(col, 0.25f, tab ? fw::lay::moon_hero_tab : fw::lay::moon_hero, false);
  lv_obj_t *line = box(col);
  lv_obj_set_size(line, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(line, LV_FLEX_FLOW_ROW);
  v->name = fw::text(line, fw::ty::title, "", t.text);
  v->rest = fw::text(line, fw::ty::title, "", t.text_dim);
  take(root, p);
  if (!v->fixed) v->tick = lv_timer_create([](lv_timer_t *tm) { show((lv_obj_t *) lv_timer_get_user_data(tm)); }, fw::mo::ms::clock_tick * 60, root);
}

FW_UPDATE(moon_view) {
  if (!of(root)) return false;
  take(root, p);
  return true;
}
