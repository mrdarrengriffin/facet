// F1 race control: the race director's messages (OpenF1 race_control / F1 Sensor f1_race_control: category, flag,
// sector, driver, message), newest first, in plain case. One line (the latest, a ticker under the map) or a short
// list (the last few). A message that just arrived is accent-text for sport-moment and lifts in; older ones are
// text-dim. Each line: a dot in the message's colour (flag yellow / SC = warn, red / black = error, blue flag = blue,
// green / clear = ok, the rest text-dim), the race clock or lap, the words.
// Data: prop "feed" = an f1_race_feed (race_control[]), else the race simulator. Pairs: "sport".
#include "fw.h"
#include "sport.h"
#include <cctype>

namespace {
using namespace fw::sport;
struct View { std::vector<lv_obj_t *> rows; int n = 1; std::string newest; uint32_t until = 0; };
View *view_of(lv_obj_t *root) {
  lv_obj_t *h = lv_obj_get_child(root, 0);
  return h ? (View *) lv_obj_get_user_data(h) : nullptr;
}
void free_view(lv_event_t *e) { delete (View *) lv_event_get_user_data(e); }
lv_obj_t *plain(lv_obj_t *p) {
  lv_obj_t *o = lv_obj_create(p);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  return o;
}
std::string sentence(const char *m) {  // FIA caps -> sentence case, car codes kept
  std::string s = m ? m : "";
  bool first = true;
  for (size_t i = 0; i < s.size(); i++) {
    const bool code = i + 2 < s.size() && s[i] == '(' ;  // "(VER)" stays
    if (code) { i += 4; continue; }
    if (isalpha((unsigned char) s[i])) { s[i] = first ? s[i] : (char) tolower((unsigned char) s[i]); first = false; }
  }
  return s;
}
lv_color_t ink_of(ArduinoJson::JsonObjectConst m) {
  const fw::Theme &t = *fw::T;
  const std::string f = m["flag"] | "", c = m["category"] | "";
  if (f == "RED" || f == "BLACK") return t.error;
  if (f == "YELLOW" || f == "DOUBLE YELLOW" || c == "SafetyCar") return t.warn;
  if (f == "BLUE") return t.blue;
  if (f == "GREEN" || f == "CLEAR" || f == "CHEQUERED") return t.ok;
  return t.text_dim;
}
void apply_feed(lv_obj_t *root, View &v, ArduinoJson::JsonVariantConst f, bool glide) {
  const fw::Theme &t = *fw::T;
  ArduinoJson::JsonArrayConst all = f["race_control"];
  const int n = (int) all.size();
  const std::string newest = n ? (all[n - 1]["message"] | "") : "";
  const bool fresh = glide && newest != v.newest && !newest.empty();
  if (fresh) v.until = lv_tick_get() + fw::mo::ms::sport_moment;
  v.newest = newest;
  for (int i = 0; i < v.n; i++) {
    lv_obj_t *row = v.rows[i];
    const int k = n - 1 - i;  // newest first
    if (k < 0) { lv_obj_add_flag(row, LV_OBJ_FLAG_HIDDEN); continue; }
    lv_obj_remove_flag(row, LV_OBJ_FLAG_HIDDEN);
    ArduinoJson::JsonObjectConst m = all[k];
    lv_obj_t *dot = lv_obj_get_child(row, 0), *when = lv_obj_get_child(row, 1), *words = lv_obj_get_child(row, 2);
    lv_obj_set_style_bg_color(dot, ink_of(m), 0);
    char b[24];
    const int ts = (int) (float) (m["t"] | 0.0f);
    snprintf(b, sizeof b, "%d:%02d", ts / 60, ts % 60);
    fw::set_text(when, fw::ty::c_meta, b);
    fw::set_text(words, fw::ty::c_meta, sentence(m["message"] | "").c_str());
    lv_obj_set_style_text_color(words, i == 0 && lv_tick_get() < v.until ? t.accent_text : i == 0 ? t.text : t.text_dim, 0);
    if (i == 0 && fresh) lift_in(words);
  }
}
void sim_apply(lv_obj_t *root, ArduinoJson::JsonVariantConst feed) {
  if (View *v = view_of(root)) apply_feed(root, *v, feed, true);
}
std::string sig_of(fw::Props p) { return std::to_string((int) (p["width"] | 400)) + "|" + std::to_string((int) (p["lines"] | 1)) + ((p["sim"] | true) ? "s" : ""); }
void fill(lv_obj_t *root, fw::Props p) {
  const fw::Theme &t = *fw::T;
  lv_obj_remove_style_all(root);
  lv_obj_remove_flag(root, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  View *v = new View;
  lv_obj_t *h = plain(root);
  lv_obj_set_size(h, 0, 0);
  lv_obj_add_flag(h, LV_OBJ_FLAG_IGNORE_LAYOUT);
  lv_obj_set_user_data(h, v);
  lv_obj_add_event_cb(h, free_view, LV_EVENT_DELETE, v);
  v->n = std::max(1, std::min(4, (int) (p["lines"] | 1)));
  const int w = fw::px((float) (p["width"] | 400)), lh = fw::style(fw::ty::c_meta).box;
  lv_obj_set_size(root, w, v->n * lh + (v->n - 1) * fw::sp::s2 + fw::sp::s1);  // + room for the last descenders
  lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(root, fw::sp::s2, 0);
  for (int i = 0; i < v->n; i++) {
    lv_obj_t *row = plain(root);
    lv_obj_set_size(row, w, lh);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, fw::sp::s3, 0);
    lv_obj_add_flag(row, LV_OBJ_FLAG_OVERFLOW_VISIBLE);  // descenders reach below the text box (type rule)
    lv_obj_t *dot = plain(row);
    lv_obj_set_size(dot, fw::lay::dot * 2, fw::lay::dot * 2);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    fw::text(row, fw::ty::c_meta, " ", t.text_dim);
    lv_obj_t *words = fw::text(row, fw::ty::c_meta, " ", t.text_dim);
    lv_obj_set_flex_grow(words, 1);
    lv_label_set_long_mode(words, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_height(words, lh);
    fw::bleed(words, fw::sp::s1);  // its descenders hang below the text box: redraw them with it
    lv_obj_add_flag(row, LV_OBJ_FLAG_HIDDEN);
    v->rows.push_back(row);
  }
  ArduinoJson::JsonDocument doc;
  ArduinoJson::JsonVariantConst feed = f1_race_feed(root, p, doc, sim_apply);
  if (!feed.isNull()) apply_feed(root, *v, feed, false);
  fw::shape(root, sig_of(p));
}
}  // namespace

FW_COMPONENT(f1_race_control, "F1 race control", "components", R"({
  "about": "The race director's messages: flags, safety car, penalties and investigations, newest first.",
  "frame": [440, 120],
  "props": [
    {"name": "lines", "about": "How many of the latest messages are shown at once.", "type": "range", "min": 1, "max": 4, "step": 1, "default": 3},
    {"name": "width", "about": "How wide the message list is, in pixels.", "type": "range", "min": 240, "max": 448, "step": 8, "default": 400},
    {"name": "sim", "about": "For previews only: runs a made-up race so messages arrive.", "type": "bool", "default": true},
    {"name": "speed", "about": "For previews only: how fast the made-up race runs, real time or 10 or 60 times faster.", "type": "select", "options": ["1x", "10x", "60x"], "default": "10x"},
    {"name": "jump", "about": "For previews only: skips the made-up race to a moment such as a penalty or a red flag.", "type": "select", "options": ["start", "yellow", "penalty", "black and white", "vsc", "safety car", "red flag", "restart", "fastest lap", "chequered"], "default": "penalty"},
    {"name": "seed", "about": "For previews only: picks a different made-up race; the same number always gives the same race.", "type": "range", "min": 1, "max": 20, "step": 1, "default": 7},
    {"name": "feed", "about": "The live race data sent to the panel. Leave empty to use the made-up race.", "type": "text", "default": ""}
  ],
  "opening": "Sample data: the race simulator (seeded).",
  "variants": [
    {"label": "last three · penalty", "props": {}},
    {"label": "one line · safety car", "props": {"lines": 1, "jump": "safety car"}},
    {"label": "red flag", "props": {"jump": "red flag"}}
  ]
})") {
  fill(lv_obj_create(parent), p);
}

FW_UPDATE(f1_race_control) {
  View *v = view_of(root);
  if (!v || !fw::shape(root, sig_of(p))) {
    fw::refill(root);
    fill(root, p);
    return true;
  }
  ArduinoJson::JsonDocument doc;
  ArduinoJson::JsonVariantConst feed = f1_race_feed(root, p, doc, sim_apply);
  if (!feed.isNull()) apply_feed(root, *v, feed, true);
  return true;
}
