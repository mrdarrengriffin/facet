// Moon phases: the next four principal phases (docs screens/moon.ts, Moon details "next phases" strip .fs-mn): four
// equal columns across the content width, each the phase's moon (fw::moon_icon at ic::feature), sp::s2 lower its name
// (type c-caps-sm, text-mute: New, First quarter, Full, Last quarter; docs label, too wide for a quarter of 440) and the day (body, text: "tonight", "Fri", "9 Nov").
// The maths = core/lunar.h phases_around (Meeus, no API). Props lat (south = mirrored glyphs), now (ms, tests; 0 =
// live). Static to touch; update path: glyphs and texts in place. Contrast: "moon" + "text".
#include <ctime>
#include "fw.h"
#include "lunar.h"

namespace {
constexpr float FRAC[4] = {0, 0.25f, 0.5f, 0.75f};
constexpr const char *NAME[4] = {"New", "First quarter", "Full", "Last quarter"};
struct Col { lv_obj_t *ic, *k, *d; };
struct MP { Col c[4]; };
MP *of(lv_obj_t *o) { return (MP *) lv_obj_get_user_data(o); }
lv_obj_t *box(lv_obj_t *parent) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  return o;
}
void show(lv_obj_t *root, fw::Props p) {
  MP *m = of(root);
  const double fixed = p["now"] | 0.0, now = fixed > 0 ? fixed : (double) time(nullptr) * 1000;
  const bool south = (double) (p["lat"] | 52.2) < 0;
  int n = 0;
  for (const fw::lunar::Phase &ph : fw::lunar::phases_around(now, 4)) {
    if (ph.t <= now || n >= 4) continue;
    Col &c = m->c[n++];
    fw::moon_icon_set(c.ic, FRAC[ph.kind], south);
    fw::set_text(c.k, fw::ty::c_caps_sm, NAME[ph.kind]);
    fw::set_text(c.d, fw::ty::body, fw::lunar::day_word(ph.t, now).c_str());
  }
}
}  // namespace

FW_COMPONENT(moon_phases, "Moon phases", "components", R"({
  "about": "The next four principal phases of the moon: the glyph, its name and the day, in four columns.",
  "frame": [480, 160],
  "props": [
    {"name": "lat", "type": "range", "min": -60, "max": 65, "step": 0.1, "default": 52.2}
  ],
  "variants": [
    {"label": "from tonight", "props": {"now": 1792944000000}},
    {"label": "southern hemisphere", "props": {"now": 1792944000000, "lat": -33.9}}
  ]
})") {
  const fw::Theme &t = *fw::T;
  auto *m = new MP;
  lv_obj_t *root = lv_obj_create(parent);
  lv_obj_remove_style_all(root);
  lv_obj_remove_flag(root, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_user_data(root, m);
  lv_obj_add_event_cb(root, [](lv_event_t *e) { delete (MP *) lv_event_get_user_data(e); }, LV_EVENT_DELETE, m);
  lv_obj_set_size(root, fw::lay::content_w, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(root, LV_FLEX_FLOW_ROW);
  const int W = fw::lay::content_w / 4 / 4 * 4;  // four equal columns on the grid
  for (Col &c : m->c) {
    lv_obj_t *col = box(root);
    lv_obj_set_size(col, W, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(col, fw::sp::s2, 0);
    c.ic = fw::moon_icon(col, 0, fw::ic::feature, false);
    lv_obj_set_style_margin_bottom(c.ic, fw::sp::s2, 0);  // docs: the glyph a step further from its text
    c.k = fw::text(col, fw::ty::c_caps_sm, "--", t.text_mute);  // caps-sm: "FIRST QUARTER" fits a quarter of 440
    c.d = fw::text(col, fw::ty::body, "--", t.text);
  }
  show(root, p);
}

FW_UPDATE(moon_phases) {
  if (!of(root)) return false;
  show(root, p);
  return true;
}
