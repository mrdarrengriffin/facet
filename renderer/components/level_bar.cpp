// Level bar: how full something is, as a straight bar: a track (line) sp::s2 thick, rad::round, the level from the
// left in its tone, and an optional mark (a threshold: water below here) = a stroke::tick wide notch in text-dim,
// three tracks tall (the box), the track in its middle third so every edge is on the grid. Value-placed parts (the
// level, the mark) say qa::free.
//   value  0..100 (-1 = unknown: no level)
//   mark   0..100, -1 = none
//   tone   accent (needs you) | steel (fine) | blue (too much) | off (text-mute)
//   width  dp, 0 = its parent's width
// Static, not a control. Update path: the level glides (Motion value, silent), tone fades. Contrast: "level_bar".
#include "fw.h"

namespace {
struct LB { lv_obj_t *track, *level, *mark; int w; std::string tone; int to = -2; };
LB *of(lv_obj_t *o) { return (LB *) lv_obj_get_user_data(o); }
lv_color_t tone(const char *t) {
  const fw::Theme &th = *fw::T;
  return !strcmp(t, "accent") ? th.accent : !strcmp(t, "blue") ? th.blue : !strcmp(t, "off") ? th.text_mute : th.steel;
}
void level_w(void *o, int32_t v) { lv_obj_set_width((lv_obj_t *) o, v); }
void place(lv_obj_t *root, LB *b, fw::Props p, bool animate) {
  const int v = (int) lround(p["value"] | -1.0), m = (int) lround(p["mark"] | -1.0), H = fw::sp::s2;  // binds give doubles
  const int to = v < 0 ? 0 : std::max(H, (int) std::lround(b->w * LV_CLAMP(0, v, 100) / 100.0));
  const int from = lv_obj_get_width(b->level);
  if (to != b->to) {  // only a new level moves (the same props redraw nothing)
    b->to = to;
    if (animate && from != to) fw::animate(b->level, fw::mo::value, {level_w, from, to, fw::mo::VALUE});
    else lv_obj_set_width(b->level, to);
  }
  if ((m < 0) != lv_obj_has_flag(b->mark, LV_OBJ_FLAG_HIDDEN)) lv_obj_set_flag(b->mark, LV_OBJ_FLAG_HIDDEN, m < 0);
  if (m >= 0) {
    const int x = (int) std::lround(b->w * LV_CLAMP(0, m, 100) / 100.0) - fw::stroke::tick / 2;
    if (lv_obj_get_style_x(b->mark, LV_PART_MAIN) != x) lv_obj_set_x(b->mark, x);
  }
  const std::string t = p["tone"] | "steel";
  if (t != b->tone) {
    b->tone = t;
    fw::st::restyle(root, [&] { lv_obj_set_style_bg_color(b->level, tone(t.c_str()), 0); }, animate);
  }
}
}  // namespace

FW_COMPONENT(level_bar, "Level bar", "components", R"({
  "about": "How full something is as a straight bar, with an optional threshold mark: soil moisture, a tank, a battery.",
  "frame": [320, 0],
  "props": [
    {"name": "value", "about": "How full the bar is, from 0 to 100. -1 means the level is not known and shows an empty bar.", "type": "range", "min": -1, "max": 100, "step": 1, "default": 44},
    {"name": "mark", "about": "Where the threshold line sits, from 0 to 100, such as the point a plant needs water. -1 hides it.", "type": "range", "min": -1, "max": 100, "step": 1, "default": 30},
    {"name": "tone", "about": "The fill colour: accent, steel, blue, or off for a grey fill.", "type": "select", "options": ["accent", "steel", "blue", "off"], "default": "steel"},
    {"name": "width", "about": "How wide the bar is, in pixels. 0 fills the space it is in.", "type": "range", "min": 0, "max": 480, "step": 4, "default": 0}
  ],
  "variants": [
    {"label": "fine", "props": {}},
    {"label": "needs you", "props": {"value": 18, "tone": "accent"}},
    {"label": "too much", "props": {"value": 92, "mark": -1, "tone": "blue"}},
    {"label": "unknown", "props": {"value": -1, "tone": "off"}}
  ]
})") {
  const fw::Theme &t = *fw::T;
  auto *b = new LB;
  lv_obj_t *root = lv_obj_create(parent);
  lv_obj_remove_style_all(root);
  lv_obj_remove_flag(root, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_user_data(root, b);
  lv_obj_add_event_cb(root, [](lv_event_t *e) { delete (LB *) lv_event_get_user_data(e); }, LV_EVENT_DELETE, b);
  const int wdp = p["width"] | 0;
  if (wdp > 0) b->w = fw::px((float) wdp);
  else { lv_obj_update_layout(parent); b->w = lv_obj_get_content_width(parent); }
  const int T = fw::sp::s2, H = 3 * T;  // the track sp::s2 thick in the middle third, the mark the full height
  lv_obj_set_size(root, b->w, H);
  b->track = lv_obj_create(root);
  lv_obj_remove_style_all(b->track);
  lv_obj_remove_flag(b->track, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_size(b->track, b->w, T);
  lv_obj_set_pos(b->track, 0, T);
  lv_obj_set_style_radius(b->track, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(b->track, t.line, 0);
  lv_obj_set_style_bg_opa(b->track, LV_OPA_COVER, 0);
  lv_obj_set_style_clip_corner(b->track, true, 0);
  b->level = lv_obj_create(b->track);
  lv_obj_remove_style_all(b->level);
  lv_obj_remove_flag(b->level, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_size(b->level, 0, T);
  lv_obj_set_style_radius(b->level, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(b->level, LV_OPA_COVER, 0);
  fw::qa::free(b->level, "the level: its width is the value");
  b->mark = lv_obj_create(root);
  lv_obj_remove_style_all(b->mark);
  lv_obj_remove_flag(b->mark, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_size(b->mark, fw::stroke::tick, H);
  lv_obj_set_style_bg_color(b->mark, t.text_dim, 0);
  lv_obj_set_style_bg_opa(b->mark, LV_OPA_COVER, 0);
  fw::qa::free(b->mark, "the mark: placed at its value");
  place(root, b, p, false);
}

FW_UPDATE(level_bar) {
  LB *b = of(root);
  if (!b) return false;
  place(root, b, p, true);
  return true;
}
