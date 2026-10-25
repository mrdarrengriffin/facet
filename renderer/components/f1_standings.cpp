// F1 standings: the running order of a race feed as a column (facet-157: the left side of the F1 screen). One row per
// car: position, team bar, code, then the gap and one small stat. Read from across the room: the code and the order
// first, the gap second, the stat last (sports-ux.md "Hierarchy").
//   row      lay::sport_row (read-out, not a touch target), hairline between rows (line, decor)
//   pos      c-meta, text-dim, tabular; flashes accent-text for sport-moment after the car gains a place
//   bar      team colour, lay::dot wide, fitted to 3:1 on the background (sport::team_ink)
//   code     screen body, text (text-dim once retired)
//   gap      c-meta, text-dim, right: leader = "Leader", "+1.234", "+1 lap", "Pit", "Out"
//   badge    one race-control badge per row, the weightiest first: DSQ (black flag, error) > penalty +5s / +10s /
//            DT (warn) > INV (under investigation) > B/W (black and white flag) > BLUE (blue flag, blue) > FL
//            (fastest lap, sport-purple) > DRS (within a second: DRS to 2025, overtake mode 2026, ok); stat "tyre"
//            shows the compound letter instead, "interval" the gap to the car ahead
//   window   from + rows: a slice of the order (two columns at 480: 1-11 and 12-22); density compact =
//            lay::sport_row_compact with c-meta text (the whole field in one column at 720)
// Motion: a new order slides each row to its place (Motion "value", silent; reduced = jump); first build at rest.
// Data: prop "feed" = a race feed, or the race simulator (sim props as on the F1 circuit). Pairs: "sport".
#include "fw.h"
#include "sport.h"
#include <map>

namespace {
using namespace fw::sport;
// set a label's text only when it changed (a set always invalidates; 22 rows x 4 Hz)
void put(lv_obj_t *l, fw::ty::Id s, const char *txt) {
  if (strcmp(lv_label_get_text(l), fw::cased(s, txt).c_str())) fw::set_text(l, s, txt);
}
struct Row { lv_obj_t *box = nullptr, *pos = nullptr, *bar = nullptr, *code = nullptr, *gap = nullptr, *stat = nullptr; int at = -1; uint32_t gained = 0; };
struct View { std::map<std::string, Row> rows; int n = 10, from = 1, w = 0, rh = 0; bool compact = false; std::string stat = "badges"; lv_obj_t *list = nullptr; uint32_t tick = 0; };
// set a colour only when it changes (every style set invalidates the object: 22 rows x 4 a second x 3 colours
// overflowed LVGL's 32 dirty areas into a full redraw)
void text_colour(lv_obj_t *o, lv_color_t c) { if (!lv_color_eq(lv_obj_get_style_text_color(o, LV_PART_MAIN), c)) lv_obj_set_style_text_color(o, c, 0); }
void bg_colour(lv_obj_t *o, lv_color_t c) { if (!lv_color_eq(lv_obj_get_style_bg_color(o, LV_PART_MAIN), c)) lv_obj_set_style_bg_color(o, c, 0); }
View *view_of(lv_obj_t *root) {
  lv_obj_t *l = lv_obj_get_child(root, 0);
  return l ? (View *) lv_obj_get_user_data(l) : nullptr;
}
void free_view(lv_event_t *e) { delete (View *) lv_event_get_user_data(e); }
void set_y(void *o, int32_t y) { lv_obj_set_y((lv_obj_t *) o, y); }

int g_rh = 0;  // the row height being built (cell centring)
lv_obj_t *cell(lv_obj_t *row, fw::ty::Id s, lv_color_t c, int x, int w, lv_text_align_t al) {
  lv_obj_t *l = fw::text(row, s, "", c);
  lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_CLIP);
  lv_obj_set_width(l, w);
  lv_obj_set_height(l, fw::style(s).box);
  lv_obj_set_style_text_align(l, al, 0);
  lv_obj_set_pos(l, x, (g_rh - fw::style(s).box) / 2);
  return l;
}
// column x / widths from tokens: pos (2 digits), gap, bar, code, then gap + stat right-aligned
struct Cols { int pos_w, bar_x, code_x, code_w, stat_w, stat_x, gap_w, gap_x; };
Cols cols(int w) {
  Cols c;
  c.pos_w = fw::sp::s7 + fw::sp::s2;
  c.bar_x = c.pos_w + fw::sp::s2;
  c.code_x = c.bar_x + fw::lay::dot + fw::sp::s3;
  c.code_w = fw::sp::s10 + fw::sp::s7;
  c.stat_w = fw::sp::s10;
  c.stat_x = w - c.stat_w;
  c.gap_w = 2 * fw::sp::s10;
  c.gap_x = c.stat_x - fw::sp::s3 - c.gap_w;
  return c;
}
Row make_row(View &v, const char *code) {
  const fw::Theme &t = *fw::T;
  const Cols c = cols(v.w);
  Row r;
  r.box = lv_obj_create(v.list);
  lv_obj_remove_style_all(r.box);
  lv_obj_remove_flag(r.box, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  g_rh = v.rh;
  lv_obj_set_size(r.box, v.w, v.rh);
  lv_obj_set_style_border_side(r.box, LV_BORDER_SIDE_BOTTOM, 0);
  lv_obj_set_style_border_width(r.box, fw::stroke::hair, 0);
  lv_obj_set_style_border_color(r.box, t.line, 0);
  fw::qa::free(r.box, "a row slides to its place in the order");
  r.pos = cell(r.box, fw::ty::c_meta, t.text_dim, 0, c.pos_w, LV_TEXT_ALIGN_RIGHT);
  r.bar = lv_obj_create(r.box);
  lv_obj_remove_style_all(r.bar);
  const int bi = v.compact ? fw::sp::s2 : fw::sp::s3;
  lv_obj_set_size(r.bar, fw::lay::dot, v.rh - 2 * bi);
  lv_obj_set_pos(r.bar, c.bar_x, bi);
  lv_obj_set_style_radius(r.bar, fw::lay::dot / 2, 0);
  lv_obj_set_style_bg_opa(r.bar, LV_OPA_COVER, 0);
  const fw::ty::Id cs = v.compact ? fw::ty::c_meta : fw::ty::body;
  r.code = cell(r.box, cs, t.text, c.code_x, c.code_w, LV_TEXT_ALIGN_LEFT);
  r.gap = cell(r.box, fw::ty::c_meta, t.text_dim, c.gap_x, c.gap_w, LV_TEXT_ALIGN_RIGHT);
  r.stat = cell(r.box, fw::ty::c_meta, t.text_dim, c.stat_x, c.stat_w, LV_TEXT_ALIGN_RIGHT);
  fw::set_text(r.code, cs, code);
  return r;
}
void gap_text(char *b, size_t n, ArduinoJson::JsonObjectConst x, int pos) {
  if (x["out"] | false) snprintf(b, n, "Out");
  else if (x["pit"] | false) snprintf(b, n, "Pit");
  else if (pos == 1) snprintf(b, n, "Leader");
  else if ((x["laps_down"] | 0) > 0) snprintf(b, n, "+%d lap%s", (int) (x["laps_down"] | 0), (x["laps_down"] | 0) > 1 ? "s" : "");
  else snprintf(b, n, "+%.3f", (float) (x["gap"] | 0.0f));
}
void apply_feed(lv_obj_t *root, View &v, ArduinoJson::JsonVariantConst feed, bool glide) {
  const fw::Theme &t = *fw::T;
  const uint32_t now = lv_tick_get();
  v.tick++;
  std::map<std::string, bool> seen;  // rows not in this feed hide at the end (hiding all first redrew every row)
  for (ArduinoJson::JsonObjectConst x : feed["cars"].as<ArduinoJson::JsonArrayConst>()) {
    const char *code = x["code"] | "";
    const int pos = x["pos"] | 99;
    if (!*code || pos < v.from || pos >= v.from + v.n) continue;
    auto it = v.rows.find(code);
    if (it == v.rows.end()) it = v.rows.emplace(code, make_row(v, code)).first;
    Row &r = it->second;
    seen[code] = true;
    if (lv_obj_has_flag(r.box, LV_OBJ_FLAG_HIDDEN)) lv_obj_remove_flag(r.box, LV_OBJ_FLAG_HIDDEN);
    // the gap and the stat refresh a quarter of the rows each update (each row once a second at 4 Hz), or at once
    // when the row moves: the order is what changes the picture, a gap's last digit can wait
    const bool fresh_text = !glide || r.at != pos || (pos + (int) v.tick) % 4 == 0;
    char b[24];
    snprintf(b, sizeof b, "%d", pos);
    put(r.pos, fw::ty::c_meta, b);
    bg_colour(r.bar, team_ink(hex(x["colour"] | "", t.text_dim), t.bg, t.text));
    const bool out = x["out"] | false;
    text_colour(r.code, out ? t.text_dim : t.text);
    gap_text(b, sizeof b, x, pos);
    if (fresh_text) put(r.gap, fw::ty::c_meta, b);
    lv_color_t sc = t.text_dim;
    if (v.stat == "tyre") snprintf(b, sizeof b, "%s", x["tyre"] | "");
    else if (v.stat == "interval") {
      if (pos == 1 || out || (x["laps_down"] | 0)) snprintf(b, sizeof b, " ");
      else snprintf(b, sizeof b, "%.1f", (float) (x["int"] | 0.0f));
    } else {  // the weightiest race-control badge
      bool blue = false, bw = false, black = false;
      for (const char *f : x["flags"].as<ArduinoJson::JsonArrayConst>()) blue |= !strcmp(f, "blue"), bw |= !strcmp(f, "bw"), black |= !strcmp(f, "black");
      const char *pen = x["penalty"] | "";
      if ((x["dsq"] | false) || black) snprintf(b, sizeof b, "DSQ"), sc = t.error;
      else if (*pen) snprintf(b, sizeof b, "%s", pen), sc = t.warn;
      else if (x["inv"] | false) snprintf(b, sizeof b, "INV"), sc = t.text;
      else if (bw) snprintf(b, sizeof b, "B/W"), sc = t.text;
      else if (blue) snprintf(b, sizeof b, "BLUE"), sc = t.blue;
      else if (x["fl"] | false) snprintf(b, sizeof b, "FL"), sc = t.sport_purple;
      else if (x["assist"] | false) snprintf(b, sizeof b, "DRS"), sc = t.ok;
      else snprintf(b, sizeof b, " ");
    }
    if (fresh_text) put(r.stat, fw::ty::c_meta, b), text_colour(r.stat, sc);
    // place: slide from the old row to the new one; a gain marks the position for a moment
    const int y = (pos - v.from) * v.rh;
    if (glide && r.at > 0 && pos < r.at && !out) r.gained = now + fw::mo::ms::sport_moment;
    text_colour(r.pos, now < r.gained ? t.accent_text : t.text_dim);
    if (glide && r.at > 0 && r.at != pos) {
      lv_anim_delete(r.box, set_y);
      fw::animate(r.box, fw::mo::value, {set_y, lv_obj_get_y(r.box), y});
    } else if (r.at != pos || !glide) {
      lv_anim_delete(r.box, set_y);
      lv_obj_set_y(r.box, y);
    }
    r.at = pos;
  }
  for (auto &[code, r] : v.rows)
    if (!seen.count(code) && !lv_obj_has_flag(r.box, LV_OBJ_FLAG_HIDDEN)) lv_obj_add_flag(r.box, LV_OBJ_FLAG_HIDDEN), r.at = -1;
}
void sim_apply(lv_obj_t *root, ArduinoJson::JsonVariantConst feed) {
  if (View *v = view_of(root)) apply_feed(root, *v, feed, true);
}
std::string sig_of(fw::Props p) {
  char b[96];
  snprintf(b, sizeof b, "%d|%d|%d|%s|%s|%d", (int) (p["rows"] | 10), (int) (p["from"] | 1), (int) (p["width"] | 208), p["stat"] | "badges",
           p["density"] | "normal", (int) (p["sim"] | true));
  return b;
}
void fill(lv_obj_t *root, fw::Props p) {
  lv_obj_remove_style_all(root);
  lv_obj_remove_flag(root, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  View *v = new View;
  v->n = p["rows"] | 10, v->from = p["from"] | 1, v->w = fw::px((float) (p["width"] | 208)), v->stat = p["stat"] | "badges";
  v->compact = !strcmp(p["density"] | "normal", "compact");
  v->rh = v->compact ? fw::lay::sport_row_compact : fw::lay::sport_row;
  lv_obj_set_size(root, v->w, v->n * v->rh);
  v->list = lv_obj_create(root);
  lv_obj_remove_style_all(v->list);
  lv_obj_remove_flag(v->list, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_size(v->list, v->w, v->n * v->rh);
  lv_obj_set_user_data(v->list, v);
  lv_obj_add_event_cb(v->list, free_view, LV_EVENT_DELETE, v);
  ArduinoJson::JsonDocument doc;
  ArduinoJson::JsonVariantConst feed = f1_race_feed(root, p, doc, sim_apply);
  if (!feed.isNull()) apply_feed(root, *v, feed, false);
  fw::shape(root, sig_of(p));
}
}  // namespace

FW_COMPONENT(f1_standings, "F1 standings", "components", R"({
  "about": "The running order of a race: position, team, driver, gap and one small stat per row.",
  "motion": "the simulated race: rows slide, gaps change",
  "motion_off": "sim",
  "frame": [240, 0],
  "props": [
    {"name": "rows", "about": "How many drivers are listed.", "type": "range", "min": 3, "max": 22, "step": 1, "default": 10},
    {"name": "from", "about": "The position the list starts at, so you can show the middle or the back of the field.", "type": "range", "min": 1, "max": 20, "step": 1, "default": 1},
    {"name": "density", "about": "normal gives each driver a comfortable row; compact fits more rows in the same height.", "type": "select", "options": ["normal", "compact"], "default": "normal"},
    {"name": "stat", "about": "The small fact at the end of each row: badges for fastest lap and pit stops, interval for the gap to the car ahead, or tyre for the tyre in use.", "type": "select", "options": ["badges", "interval", "tyre"], "default": "badges"},
    {"name": "width", "about": "How wide the list is, in pixels.", "type": "range", "min": 176, "max": 320, "step": 8, "default": 208},
    {"name": "sim", "about": "For previews only: runs a made-up race so the order changes.", "type": "bool", "default": true},
    {"name": "speed", "about": "For previews only: how fast the made-up race runs, real time or 10 or 60 times faster.", "type": "select", "options": ["1x", "10x", "60x"], "default": "10x"},
    {"name": "jump", "about": "For previews only: skips the made-up race to a moment such as an overtake or a pit stop.", "type": "select", "options": ["start", "penalty", "black and white", "blue flag", "overtake", "pit stop", "safety car", "red flag", "fastest lap", "retirement", "final lap", "chequered"], "default": "start"},
    {"name": "seed", "about": "For previews only: picks a different made-up race; the same number always gives the same race.", "type": "range", "min": 1, "max": 20, "step": 1, "default": 7},
    {"name": "track", "about": "For previews only: which circuit the made-up race is run on.", "type": "select", "options": ["us-2012", "mc-1929", "gb-1948", "be-1925", "it-1922"], "default": "us-2012"},
    {"name": "feed", "about": "The live race data sent to the panel. Leave empty to use the made-up race.", "type": "text", "default": ""}
  ],
  "opening": "Sample data: the race simulator (seeded).",
  "variants": [
    {"label": "top 10 · badges", "props": {}},
    {"label": "penalty", "props": {"jump": "penalty"}},
    {"label": "blue flags · 12-22", "props": {"jump": "blue flag", "from": 12, "rows": 11}},
    {"label": "whole field · compact", "props": {"rows": 22, "density": "compact"}},
    {"label": "tyre", "props": {"stat": "tyre", "jump": "pit stop"}},
    {"label": "safety car", "props": {"jump": "safety car"}},
    {"label": "chequered", "props": {"jump": "chequered", "rows": 6}}
  ]
})") {
  fill(lv_obj_create(parent), p);
}

FW_UPDATE(f1_standings) {
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
