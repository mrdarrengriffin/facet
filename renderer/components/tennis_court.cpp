// Tennis court: the court from above on a playing surface from the Sports surfaces foundation (core/surface.h: ITF
// proportions, run-off, venue colours, white lines; one object), with what the score says about the next point: who
// serves, from which half (deuce or ad: the points played in the game) and into which service box (the diagonal
// one, highlighted). No ball or player positions: no public source has them (Hawk-Eye is private; sports-data.md).
// Ends are fixed (player 1 left): feeds do not say who is at which end.
//   surface  prop "surface" = a surface (hard, clay, grass, indoor, carpet) or a venue preset (wimbledon, ...);
//            the feed's "surface" word (Clay / Hard / Grass) wins when the prop is "auto"
//   serve    a court-line dot (lay::sport_marker) in the run-off behind the server's baseline, on the half they serve from
//   box      the target service box: sf::tennis_mark (court-line at court.mark under the lines)
//   names    caps label over each end (text); the server's in text, the receiver's in text-dim; only with a feed
// Motion: the serve dot and the box glide to the next point's place (Motion "value"); silent. Pairs: "sport", "surfaces".
// Data: prop "feed" = a tennis feed, or the tennis simulator.
#include "fw.h"
#include "sport.h"
#include "surface.h"

namespace {
using namespace fw::sport;
using fw::sf::tennis::length;
constexpr float L = length, W = fw::sf::tennis::width, SW = fw::sf::tennis::singles, SV = fw::sf::tennis::service;
struct View { lv_obj_t *dot = nullptr, *court = nullptr, *name[2] = {nullptr, nullptr}; int top = 0; bool placed = false, names = false; std::string surface; };
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
void set_x(void *o, int32_t v) { lv_obj_set_x((lv_obj_t *) o, v); }
void set_y(void *o, int32_t v) { lv_obj_set_y((lv_obj_t *) o, v); }
void move(lv_obj_t *o, int x, int y, bool glide) {
  lv_anim_delete(o, set_x), lv_anim_delete(o, set_y);
  if (!glide) { lv_obj_set_pos(o, x, y); return; }
  if (lv_obj_get_x(o) != x) fw::animate(o, fw::mo::value, {set_x, lv_obj_get_x(o), x});
  if (lv_obj_get_y(o) != y) fw::animate(o, fw::mo::value, {set_y, lv_obj_get_y(o), y});
}

const fw::sf::Surface &surface_of(fw::Props p, ArduinoJson::JsonVariantConst f) {
  const char *id = p["surface"] | "auto";
  if (strcmp(id, "auto")) return fw::sf::find(id);
  const fw::sf::Surface *s = fw::sf::from_data(f["surface"] | "");
  return s ? *s : fw::sf::find(nullptr);
}
void apply_feed(View &v, fw::Props p, ArduinoJson::JsonVariantConst f, bool glide) {
  const fw::Theme &t = *fw::T;
  fw::sf::tennis_surface(v.court, surface_of(p, f));
  const TennisNow n = tennis_now(f);
  for (int i = 0; i < 2 && v.names; i++) {
    const char *nm = f["players"][i]["short"] | (f["players"][i]["name"] | (i ? "P2" : "P1"));
    fw::set_text(v.name[i], fw::ty::c_caps_sm, nm);
    lv_obj_set_style_text_color(v.name[i], n.server == i ? t.text : t.text_dim, 0);
  }
  glide = glide && v.placed;
  if (n.server < 0 || n.winner >= 0) {
    lv_obj_add_flag(v.dot, LV_OBJ_FLAG_HIDDEN);
    fw::sf::tennis_mark(v.court, {}, false, false);
    return;
  }
  lv_obj_remove_flag(v.dot, LV_OBJ_FLAG_HIDDEN);
  // deuce side = the server's right: player 1 (left end, facing right) -> the lower half; player 2 -> the upper
  const int played = std::max(0, n.points[0]) + std::max(0, n.points[1]);
  const bool deuce = played % 2 == 0;
  const bool lower = (n.server == 0) == deuce;
  const fw::sf::TennisFit &g = fw::sf::tennis_geo(v.court);
  const int cx = lv_obj_get_x(v.court), cy = lv_obj_get_y(v.court);
  const int d = fw::lay::sport_marker, gap = fw::sp::s2;
  const int dx = n.server == 0 ? g.X(0) - gap - d : g.X(L) + gap;
  const float qy = lower ? (W / 2 + SW / 4) : (W / 2 - SW / 4);
  move(v.dot, cx + dx, cy + g.Y(qy) - d / 2, glide);
  // the target box: the receiver's side, diagonal (server lower -> receiver's upper half, and back)
  const float s0 = (W - SW) / 2;
  const float m0 = n.server == 0 ? L / 2 : L / 2 - SV, y0 = lower ? s0 : W / 2;
  fw::sf::Box b{g.X(m0), g.Y(y0), g.X(m0 + SV) - g.X(m0), g.Y(y0 + SW / 2) - g.Y(y0)};
  fw::sf::tennis_mark(v.court, b, true, glide);
  v.placed = true;
}
void sim_apply(lv_obj_t *root, ArduinoJson::JsonVariantConst feed) {
  if (View *v = view_of(root)) {
    ArduinoJson::JsonDocument d;
    d["surface"] = v->surface;
    apply_feed(*v, d.as<fw::Props>(), feed, true);
  }
}
std::string sig_of(fw::Props p) {
  char b[64];
  const char *feed = p["feed"].is<const char *>() ? p["feed"].as<const char *>() : "";
  snprintf(b, sizeof b, "%d|%d", (int) (p["width"] | 360), (int) ((p["sim"] | true) || *feed || p["feed"].is<ArduinoJson::JsonObjectConst>()));
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
  // names row (only with a feed or the simulator), then the court object with the serve dot in its run-off
  v->surface = p["surface"] | "auto";
  const char *ft = p["feed"].is<const char *>() ? p["feed"].as<const char *>() : "";
  v->names = (p["sim"] | true) || *ft || p["feed"].is<ArduinoJson::JsonObjectConst>();
  const int label_h = fw::style(fw::ty::c_caps_sm).box;
  v->top = v->names ? label_h + fw::sp::s3 : 0;
  v->court = fw::sf::court(root, w, surface_of(p, ArduinoJson::JsonVariantConst()));
  lv_obj_set_pos(v->court, 0, v->top);
  const fw::sf::TennisFit &g = fw::sf::tennis_geo(v->court);
  lv_obj_set_size(root, w, v->top + g.h);
  for (int i = 0; i < 2 && v->names; i++) {
    v->name[i] = fw::text(root, fw::ty::c_caps_sm, "", t.text_dim);
    lv_obj_set_width(v->name[i], g.X(L / 2) - g.X(0) - fw::sp::s3);
    lv_obj_set_style_text_align(v->name[i], i ? LV_TEXT_ALIGN_RIGHT : LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_pos(v->name[i], i ? g.X(L / 2) + fw::sp::s3 : g.X(0), 0);
  }
  v->dot = plain(root, 0, 0, fw::lay::sport_marker, fw::lay::sport_marker);
  lv_obj_set_style_radius(v->dot, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(v->dot, t.*fw::sf::line_ink(), 0);
  lv_obj_set_style_bg_opa(v->dot, LV_OPA_COVER, 0);
  lv_obj_add_flag(v->dot, LV_OBJ_FLAG_HIDDEN);
  fw::qa::free(v->dot, "behind the server's baseline");
  fw::qa::inside(v->dot, root);
  ArduinoJson::JsonDocument doc;
  ArduinoJson::JsonVariantConst feed = tennis_match_feed(root, p, doc, sim_apply);
  if (!feed.isNull()) apply_feed(*v, p, feed, false);
  fw::shape(root, sig_of(p));
}
}  // namespace

FW_COMPONENT(tennis_court, "Tennis court", "components", R"({
  "about": "The court from above on its real surface: who serves, from which side, into which box.",
  "motion": "the simulated match: the serve moves",
  "motion_off": "sim",
  "frame": [400, 256],
  "props": [
    {"name": "width", "type": "range", "min": 160, "max": 448, "step": 8, "default": 360},
    {"name": "surface", "type": "select", "options": ["auto", "hard", "clay", "grass", "indoor", "carpet", "wimbledon", "roland-garros", "us-open", "australian-open", "atp-finals"], "default": "auto"},
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
    {"label": "Wimbledon, player 1 serves", "props": {"sim": false, "surface": "wimbledon", "feed": "{\"state\":\"live\",\"players\":[{\"short\":\"LIN\"},{\"short\":\"MOR\"}],\"sets\":[[3,3]],\"points\":[0,0],\"server\":0}"}},
    {"label": "surface from the feed: clay", "props": {"sim": false, "feed": "{\"state\":\"live\",\"surface\":\"Clay\",\"players\":[{\"short\":\"LIN\"},{\"short\":\"MOR\"}],\"sets\":[[1,0]],\"points\":[3,2],\"server\":1}"}},
    {"label": "court only: US Open", "props": {"sim": false, "surface": "us-open", "width": 240}},
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
  v->surface = p["surface"] | "auto";
  ArduinoJson::JsonVariantConst feed = tennis_match_feed(root, p, doc, sim_apply);
  if (!feed.isNull()) apply_feed(*v, p, feed, true);
  else fw::sf::tennis_surface(v->court, surface_of(p, feed));
  return true;
}
