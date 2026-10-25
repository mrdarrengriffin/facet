// Event row: one thing that happened, in an activity list (the panel lab's Door lock "last 24 h" rows, lockEventRow3:
// "09:03  Locked · Alex"). lay::event_row_h tall, as wide as its parent (a Split column), a line hairline on top;
// flex row centred, gap sp::s5: the time (type label, text-dim, lay::event_time_w; the lab's text-mute is for
// disabled only, contrast rule) and what happened (type body, text-dim, one line, ellipsis): the state as a word +
// who did it. A jammed lock's row is in the error colour.
//   event    "09:03|locked|Alex" (time | the state | by whom, empty = nobody): a spec feeds it per item of a list
//            parameter ("each")
// Static: no press, no events, no sound. Update path: texts and the tone in place (silent).
// Contrast: section "event_row".
#include "fw.h"

namespace {
struct ER { lv_obj_t *time, *what; std::string ev; };
std::string field(const std::string &s, int n) {
  size_t i = 0;
  for (int k = 0; k < n; k++) { i = s.find('|', i); if (i == std::string::npos) return ""; i++; }
  const size_t j = s.find('|', i);
  return s.substr(i, j == std::string::npos ? std::string::npos : j - i);
}
void take(ER *r, fw::Props p) {
  r->ev = p["event"] | "";
  const std::string t = field(r->ev, 0), st = field(r->ev, 1), by = field(r->ev, 2);
  std::string w = st;
  if (!w.empty()) w[0] = (char) toupper((unsigned char) w[0]);
  if (!by.empty()) w += (w.empty() ? "" : " \xC2\xB7 ") + by;
  if (fw::cased(fw::ty::label, t.c_str()) != lv_label_get_text(r->time)) fw::set_text(r->time, fw::ty::label, t.c_str());
  if (fw::cased(fw::ty::body, w.c_str()) != lv_label_get_text(r->what)) fw::set_text(r->what, fw::ty::body, w.c_str());
  const lv_color_t c = st == "jammed" ? fw::T->error : fw::T->text_dim;
  if (!lv_color_eq(lv_obj_get_style_text_color(r->what, LV_PART_MAIN), c)) lv_obj_set_style_text_color(r->what, c, 0);  // only on a change
}
}  // namespace

FW_COMPONENT(event_row, "Event row", "components", R"({
  "about": "One thing that happened, in an activity list: the time and what happened, who did it (a jammed lock in the error colour).",
  "frame": [320, 0],
  "props": [
    {"name": "event", "about": "What happened, as time, action and who did it separated by bars, like 09:03|locked|Alex. A jammed lock shows in the error colour.", "type": "text", "default": "09:03|locked|Alex"}
  ],
  "variants": [
    {"label": "locked", "props": {}},
    {"label": "unlocked by the keypad", "props": {"event": "09:01|unlocked|keypad"}},
    {"label": "jammed", "props": {"event": "14:20|jammed|"}}
  ]
})") {
  const fw::Theme &t = *fw::T;
  auto *r = new ER;
  lv_obj_t *root = lv_obj_create(parent);
  lv_obj_remove_style_all(root);
  lv_obj_remove_flag(root, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_user_data(root, r);
  lv_obj_add_event_cb(root, [](lv_event_t *e) { delete (ER *) lv_event_get_user_data(e); }, LV_EVENT_DELETE, r);
  lv_obj_set_size(root, LV_PCT(100), fw::lay::event_row_h);
  lv_obj_set_style_border_side(root, LV_BORDER_SIDE_TOP, 0);
  lv_obj_set_style_border_width(root, fw::stroke::hair, 0);
  lv_obj_set_style_border_color(root, t.line, 0);
  lv_obj_set_flex_flow(root, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(root, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(root, fw::sp::s5, 0);
  r->time = fw::text(root, fw::ty::label, "", t.text_dim);
  lv_obj_set_size(r->time, fw::lay::event_time_w, fw::style(fw::ty::label).box);
  r->what = fw::text(root, fw::ty::body, "", t.text_dim);
  lv_obj_set_height(r->what, fw::style(fw::ty::body).box);
  lv_obj_set_flex_grow(r->what, 1);
  lv_label_set_long_mode(r->what, LV_LABEL_LONG_DOT);
  take(r, p);
}

FW_UPDATE(event_row) {
  auto *r = (ER *) lv_obj_get_user_data(root);
  if (!r) return false;
  take(r, p);
  return true;
}
