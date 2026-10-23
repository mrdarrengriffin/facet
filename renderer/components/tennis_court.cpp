// Tennis court: the court from above (ITF dimensions: 23.77 x 10.97 m, singles 8.23 m, service line 6.40 m from
// the net) in steel lines on the canvas, with what the score says about the next point: who serves, from which half
// (deuce or ad: the points played in the game) and into which service box (the diagonal one, a quiet steel-dim fill).
// No ball or player positions: no public source has them (Hawk-Eye is private; sports-data.md). Ends are fixed
// (player 1 left): feeds do not say who is at which end.
//   lines    stroke::edge, steel (3:1, section "sport"); the net = stroke::indicator in text-dim across the middle
//   serve    a text dot (lay::sport_marker) behind the server's baseline on the half they serve from
//   names    caps label over each end (text); the server's in text, the receiver's in text-dim
// Motion: the serve dot and the box glide to the next point's place (Motion "value"); silent. Pairs: "sport".
// Data: prop "feed" = a tennis feed, or the tennis simulator.
#include "fw.h"
#include "sport.h"

namespace {
using namespace fw::sport;
constexpr float L = 23.77f, W = 10.97f, SW = 8.23f, SV = 6.40f;
struct View { lv_obj_t *dot = nullptr, *box = nullptr, *name[2] = {nullptr, nullptr}; int ox = 0, oy = 0, cw = 0, ch = 0, top = 0; bool placed = false; };
View *view_of(lv_obj_t *root) {
  lv_obj_t *h = lv_obj_get_child(root, 0);
  return h ? (View *) lv_obj_get_user_data(h) : nullptr;
}
void free_view(lv_event_t *e) { delete (View *) lv_event_get_user_data(e); }
lv_obj_t *plain(lv_obj_t *parent, int x, int y, int w, int h) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_pos(o, x, y);
  lv_obj_set_size(o, w, h);
  return o;
}
lv_obj_t *rect(lv_obj_t *parent, int x, int y, int w, int h, lv_color_t c, int bw) {
  lv_obj_t *o = plain(parent, x, y, w, h);
  lv_obj_set_style_border_width(o, bw, 0);
  lv_obj_set_style_border_color(o, c, 0);
  fw::qa::free(o, "court lines at ITF proportions");
  return o;
}
lv_obj_t *bar(lv_obj_t *parent, int x, int y, int w, int h, lv_color_t c) {
  lv_obj_t *o = plain(parent, x, y, w, h);
  lv_obj_set_style_bg_color(o, c, 0);
  lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
  fw::qa::free(o, "court lines at ITF proportions");
  return o;
}
void set_x(void *o, int32_t v) { lv_obj_set_x((lv_obj_t *) o, v); }
void set_y(void *o, int32_t v) { lv_obj_set_y((lv_obj_t *) o, v); }
void move(lv_obj_t *o, int x, int y, bool glide) {
  lv_anim_delete(o, set_x), lv_anim_delete(o, set_y);
  if (!glide) { lv_obj_set_pos(o, x, y); return; }
  if (lv_obj_get_x(o) != x) fw::animate(o, fw::mo::value, {set_x, lv_obj_get_x(o), x});
  if (lv_obj_get_y(o) != y) fw::animate(o, fw::mo::value, {set_y, lv_obj_get_y(o), y});
}

void apply_feed(lv_obj_t *root, View &v, ArduinoJson::JsonVariantConst f, bool glide) {
  const fw::Theme &t = *fw::T;
  const TennisNow n = tennis_now(f);
  for (int i = 0; i < 2; i++) {
    const char *nm = f["players"][i]["short"] | (f["players"][i]["name"] | (i ? "P2" : "P1"));
    fw::set_text(v.name[i], fw::ty::c_caps_sm, nm);
    lv_obj_set_style_text_color(v.name[i], n.server == i ? t.text : t.text_dim, 0);
  }
  glide = glide && v.placed;
  if (n.server < 0 || n.winner >= 0) {
    lv_obj_add_flag(v.dot, LV_OBJ_FLAG_HIDDEN), lv_obj_add_flag(v.box, LV_OBJ_FLAG_HIDDEN);
    return;
  }
  lv_obj_remove_flag(v.dot, LV_OBJ_FLAG_HIDDEN), lv_obj_remove_flag(v.box, LV_OBJ_FLAG_HIDDEN);
  // deuce side = the server's right: player 1 (left end, facing right) -> the lower half; player 2 -> the upper
  const int played = std::max(0, n.points[0]) + std::max(0, n.points[1]);
  const bool deuce = played % 2 == 0;
  const bool lower = (n.server == 0) == deuce;
  const float sx = v.cw / L, sy = v.ch / W;
  const int d = fw::lay::sport_marker, gap = fw::sp::s2;
  const int dx = n.server == 0 ? v.ox - gap - d : v.ox + v.cw + gap;
  const float qy = lower ? (W / 2 + SW / 4) : (W / 2 - SW / 4);
  move(v.dot, dx, v.oy + (int) std::lround(qy * sy) - d / 2, glide);
  // the target box: the receiver's side, diagonal (server lower -> receiver's upper half, and back)
  const int bw = (int) std::lround(SV * sx), bh = (int) std::lround(SW / 2 * sy);
  const int bx = n.server == 0 ? v.ox + (int) std::lround(L / 2 * sx) : v.ox + (int) std::lround((L / 2 - SV) * sx);
  const int by = v.oy + (int) std::lround((W - SW) / 2 * sy) + (lower ? 0 : bh);
  move(v.box, bx, by, glide);
  v.placed = true;
}
void sim_apply(lv_obj_t *root, ArduinoJson::JsonVariantConst feed) {
  if (View *v = view_of(root)) apply_feed(root, *v, feed, true);
}
std::string sig_of(fw::Props p) {
  char b[64];
  snprintf(b, sizeof b, "%d|%d", (int) (p["width"] | 360), (int) (p["sim"] | true));
  return b;
}
void fill(lv_obj_t *root, fw::Props p) {
  const fw::Theme &t = *fw::T;
  lv_obj_remove_style_all(root);
  lv_obj_remove_flag(root, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  const int w = fw::px((float) (p["width"] | 360));
  View *v = new View;
  lv_obj_t *h = plain(root, 0, 0, 0, 0);
  lv_obj_set_user_data(h, v);
  lv_obj_add_event_cb(h, free_view, LV_EVENT_DELETE, v);
  // names row, then the court with room for the serve dot behind each baseline
  const int label_h = fw::style(fw::ty::c_caps_sm).box, side = fw::lay::sport_marker + fw::sp::s2 * 2;
  v->top = label_h + fw::sp::s3;
  v->cw = w - 2 * side, v->ch = (int) std::lround(v->cw * W / L);
  v->ox = side, v->oy = v->top;
  lv_obj_set_size(root, w, v->top + v->ch + fw::sp::s2);  // + the net past the posts
  const float sx = v->cw / L, sy = v->ch / W;
  auto X = [&](float m) { return v->ox + (int) std::lround(m * sx); };
  auto Y = [&](float m) { return v->oy + (int) std::lround(m * sy); };
  const int e = fw::stroke::edge;
  // the target service box first (under the lines)
  v->box = plain(root, 0, 0, 0, 0);
  lv_obj_set_size(v->box, (int) std::lround(SV * sx), (int) std::lround(SW / 2 * sy));
  lv_obj_set_style_bg_color(v->box, t.steel_dim, 0);
  lv_obj_set_style_bg_opa(v->box, LV_OPA_COVER, 0);
  fw::qa::free(v->box, "the service box the next serve goes into");
  rect(root, X(0), Y(0), X(L) - X(0), Y(W) - Y(0), t.steel, e);                             // doubles court
  const float s0 = (W - SW) / 2, s1 = W - s0;
  bar(root, X(0), Y(s0) - e / 2, X(L) - X(0), e, t.steel);                                   // singles sidelines
  bar(root, X(0), Y(s1) - e / 2, X(L) - X(0), e, t.steel);
  bar(root, X(L / 2 - SV) - e / 2, Y(s0), e, Y(s1) - Y(s0), t.steel);                         // service lines
  bar(root, X(L / 2 + SV) - e / 2, Y(s0), e, Y(s1) - Y(s0), t.steel);
  bar(root, X(L / 2 - SV), Y(W / 2) - e / 2, X(L / 2 + SV) - X(L / 2 - SV), e, t.steel);     // centre service line
  bar(root, X(0), Y(W / 2) - e / 2, fw::sp::s2, e, t.steel);                                 // centre marks
  bar(root, X(L) - fw::sp::s2, Y(W / 2) - e / 2, fw::sp::s2, e, t.steel);
  const int nw = fw::stroke::indicator;
  bar(root, X(L / 2) - nw / 2, Y(0) - fw::sp::s2, nw, Y(W) - Y(0) + 2 * fw::sp::s2, t.text_dim);  // the net, past the posts
  for (int i = 0; i < 2; i++) {
    v->name[i] = fw::text(root, fw::ty::c_caps_sm, "", t.text_dim);
    lv_obj_set_width(v->name[i], v->cw / 2 - fw::sp::s3);
    lv_obj_set_style_text_align(v->name[i], i ? LV_TEXT_ALIGN_RIGHT : LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_pos(v->name[i], i ? X(L / 2) + fw::sp::s3 : v->ox, 0);
  }
  v->dot = plain(root, 0, 0, fw::lay::sport_marker, fw::lay::sport_marker);
  lv_obj_set_style_radius(v->dot, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(v->dot, t.text, 0);
  lv_obj_set_style_bg_opa(v->dot, LV_OPA_COVER, 0);
  fw::qa::free(v->dot, "behind the server's baseline");
  fw::qa::inside(v->dot, root);
  ArduinoJson::JsonDocument doc;
  ArduinoJson::JsonVariantConst feed = tennis_match_feed(root, p, doc, sim_apply);
  if (!feed.isNull()) apply_feed(root, *v, feed, false);
  fw::shape(root, sig_of(p));
}
}  // namespace

FW_COMPONENT(tennis_court, "Tennis court", "components", R"({
  "about": "The court from above: who serves, from which side, into which box.",
  "motion": "the simulated match: the serve moves",
  "motion_off": "sim",
  "frame": [400, 240],
  "props": [
    {"name": "width", "type": "range", "min": 240, "max": 448, "step": 8, "default": 360},
    {"name": "sim", "type": "bool", "default": true},
    {"name": "speed", "type": "select", "options": ["1x", "10x", "60x"], "default": "10x"},
    {"name": "jump", "type": "select", "options": ["start", "break point", "break", "tiebreak", "set point", "match point"], "default": "start"},
    {"name": "seed", "type": "range", "min": 1, "max": 20, "step": 1, "default": 7},
    {"name": "best_of", "type": "select", "options": ["3", "5"], "default": "3"},
    {"name": "final_set", "type": "select", "options": ["tb10", "tb7", "adv"], "default": "tb10"},
    {"name": "feed", "type": "text", "default": ""}
  ],
  "opening": "Sample data: the tennis simulator (seeded): made-up players, real scoring.",
  "variants": [
    {"label": "player 1 serves, deuce side", "props": {"sim": false, "feed": "{\"state\":\"live\",\"players\":[{\"short\":\"LIN\"},{\"short\":\"MOR\"}],\"sets\":[[2,1]],\"points\":[1,1],\"server\":0}"}},
    {"label": "player 2 serves, ad side", "props": {"sim": false, "feed": "{\"state\":\"live\",\"players\":[{\"short\":\"LIN\"},{\"short\":\"MOR\"}],\"sets\":[[2,2]],\"points\":[2,1],\"server\":1}"}},
    {"label": "simulator", "props": {}}
  ]
})") {
  fill(lv_obj_create(parent), p);
}

FW_UPDATE(tennis_court) {
  View *v = view_of(root);
  if (!v || !fw::shape(root, sig_of(p))) {
    fw::refill(root);
    fill(root, p);
    return true;
  }
  ArduinoJson::JsonDocument doc;
  ArduinoJson::JsonVariantConst feed = tennis_match_feed(root, p, doc, sim_apply);
  if (!feed.isNull()) apply_feed(root, *v, feed, true);
  return true;
}
