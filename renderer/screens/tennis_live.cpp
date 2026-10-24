// Tennis live (facet-152, WIP): one tennis_match_feed on one screen, every part a real component in its own region.
//   header   the event ("Sample Open · Final")
//   hero     Tennis score, hero layout: the point in display type (what changes most, read from across the room),
//            the players with the serve dot, finished sets and the set in play, the moment line + state chip
//   court    Tennis court under it: who serves, from which half, into which box; sized to the room left
// The screen runs one simulator (sample data) or takes p["feed"] from an adapter and hands it to both parts.
#include "fw.h"
#include "surface.h"
#include "sport.h"
#include "sport_pick.h"

namespace {
using namespace fw::sport;
struct Screen { lv_obj_t *header = nullptr, *score = nullptr, *court = nullptr; int sw = 0, cw = 0; Picker *pick = nullptr; };
Screen *of(lv_obj_t *root) {
  lv_obj_t *h = lv_obj_get_child(root, 0);
  return h ? (Screen *) lv_obj_get_user_data(h) : nullptr;
}
inline int dp(int px) { return px * 480 / fw::lay::screen_w; }
lv_obj_t *box(lv_obj_t *parent, int x, int y, int w, int h) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_pos(o, x, y);
  lv_obj_set_size(o, w, h);
  return o;
}
void push(lv_obj_t *root, ArduinoJson::JsonVariantConst f) {
  Screen *s = of(root);
  if (!s || picker_paused(s->pick)) return;  // TV sync paused: the screen holds what it shows
  ArduinoJson::JsonDocument a;
  a["feed"] = f, a["sim"] = false, a["layout"] = "hero", a["width"] = s->sw;
  fw::update_child(fw::find("tennis_score"), s->score, a.as<fw::Props>());
  ArduinoJson::JsonDocument b;
  b["feed"] = f, b["sim"] = false, b["width"] = s->cw;
  fw::update_child(fw::find("tennis_court"), s->court, b.as<fw::Props>());
  ArduinoJson::JsonDocument hd;
  hd["label"] = f["event"] | "Tennis";
  fw::update_child(fw::find("header"), s->header, hd.as<fw::Props>());
  const char *st = f["state"] | "";
  picker_live(s->pick, !strcmp(st, "live") || !strcmp(st, "break"));
}
const char *META = R"({
  "about": "A live tennis match: the point as the hero, the sets and who serves, the court with the next serve.",
  "motion": "the simulated match",
  "frame": [480, 480],
  "props": [
    {"name": "speed", "type": "select", "options": ["1x", "10x", "60x"], "default": "10x"},
    {"name": "jump", "type": "select", "options": ["start", "break point", "break", "tiebreak", "set point", "match point"], "default": "start"},
    {"name": "seed", "type": "range", "min": 1, "max": 20, "step": 1, "default": 7},
    {"name": "best_of", "type": "select", "options": ["3", "5"], "default": "3"},
    {"name": "final_set", "type": "select", "options": ["tb10", "tb7", "adv"], "default": "tb10"},
    {"name": "feed", "type": "text", "default": ""},
    {"name": "match", "type": "text", "default": ""},
    {"name": "pick", "type": "text", "default": ""},
    {"name": "event", "type": "text", "default": ""},
    {"name": "picker", "type": "text", "default": ""},
    {"name": "delay", "type": "range", "min": 0, "max": 300, "step": 5, "default": 0}
  ],
  "opening": "Sample data: the tennis simulator (seeded): made-up players, real scoring.",
  "variants": [
    {"label": "the match", "props": {}},
    {"label": "break point", "props": {"jump": "break point"}},
    {"label": "match point · best of 5", "props": {"jump": "match point", "best_of": "5"}}
  ]
})";
void fill(lv_obj_t *root, fw::Props p) {
  const fw::Theme &t = *fw::T;
  lv_obj_remove_style_all(root);
  lv_obj_remove_flag(root, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_size(root, fw::lay::screen_w, fw::lay::screen_h);
  lv_obj_set_style_bg_color(root, t.bg, 0);
  lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
  Screen *s = new Screen;
  lv_obj_t *h = box(root, 0, 0, 0, 0);
  lv_obj_set_user_data(h, s);
  lv_obj_add_event_cb(h, [](lv_event_t *e) { auto *s = (Screen *) lv_event_get_user_data(e); picker_free(s->pick); delete s; }, LV_EVENT_DELETE, s);
  ArduinoJson::JsonDocument hd;
  hd["label"] = "Tennis";
  fw::find("header")->build(root, hd.as<fw::Props>());
  s->header = lv_obj_get_child(root, -1);
  lv_obj_set_pos(s->header, fw::lay::inset, fw::lay::inset);
  const int X = fw::lay::inset, Y = fw::lay::top, W = fw::lay::content_w, H = fw::lay::screen_h - fw::lay::top - fw::lay::inset;
  // the score: full width, its own height
  lv_obj_t *sb = box(root, X, Y, W, 0);
  lv_obj_set_height(sb, LV_SIZE_CONTENT);
  s->sw = dp(W);
  ArduinoJson::JsonDocument sd;
  sd["sim"] = false, sd["feed"] = "{\"players\":[{},{}],\"sets\":[[0,0]],\"points\":[0,0],\"server\":0}", sd["layout"] = "hero", sd["width"] = s->sw;
  fw::find("tennis_score")->build(sb, sd.as<fw::Props>());
  s->score = lv_obj_get_child(sb, -1);
  lv_obj_add_flag(sb, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
  lv_obj_update_layout(sb);
  const int sh = (lv_obj_get_height(s->score) + 3) / 4 * 4;
  lv_obj_set_height(sb, sh);
  // the court: as wide as fits the room left (its height = label row + the surface: Sports surfaces sf::tennis_fit)
  const int room = H - sh - fw::lay::group;
  const int label = fw::style(fw::ty::c_caps_sm).box + fw::sp::s3;
  int cw = W;
  while (cw > 160 && label + fw::sf::tennis_fit(cw).h > room) cw -= 8;
  cw = cw / 8 * 8;
  lv_obj_t *cb = box(root, X + (W - cw) / 2 / 4 * 4, Y + sh + fw::lay::group, cw, room);
  s->cw = dp(cw);
  ArduinoJson::JsonDocument cd;
  cd["sim"] = false, cd["feed"] = "{}", cd["width"] = s->cw;
  fw::find("tennis_court")->build(cb, cd.as<fw::Props>());
  s->court = lv_obj_get_child(cb, -1);
  lv_obj_add_flag(cb, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
  // the match picker: the header button + its sheet over the whole screen (core/sport_pick.h)
  s->pick = picker_attach(root, s->header, 't', p);
  picker_hero(s->pick, sb);
  ArduinoJson::JsonDocument doc;
  ArduinoJson::JsonVariantConst f = tennis_match_feed(root, p, doc, push);
  if (!f.isNull()) {
    fw::Quiet q;
    push(root, f);
  }
}
void build_t(lv_obj_t *parent, fw::Props p) { fill(lv_obj_create(parent), p); }
bool update_t(lv_obj_t *root, fw::Props p) {
  if (!of(root)) { fw::refill(root); fill(root, p); return true; }
  picker_update(of(root)->pick, p);
  ArduinoJson::JsonDocument doc;
  ArduinoJson::JsonVariantConst f = tennis_match_feed(root, p, doc, push);
  if (!f.isNull()) push(root, f);
  return true;
}
const int reg = fw::add({"tennis_live", "Tennis live", "screens", META, build_t});
const int upd = fw::set_update("tennis_live", update_t);
}  // namespace
