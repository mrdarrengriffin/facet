// Football live (facet-152, WIP): one football_match_feed on one screen, in its own region (layout tokens).
//   header   the competition ("Sample League")
//   pitch    Football pitch at the widest that fits the content box: its scoreline + minute is the hero (value type,
//            the scores lift on a goal), possession bars, both line-ups in formation, the minute line of events,
//            the latest event in words and the state chip (Live pulses / Half time / Final)
// The screen runs one simulator (sample data) or takes p["feed"] from an adapter.
#include "fw.h"
#include "sport.h"

namespace {
using namespace fw::sport;
struct Screen { lv_obj_t *header = nullptr, *pitch = nullptr; int pw = 0; };
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
  if (!s) return;
  ArduinoJson::JsonDocument a;
  a["feed"] = f, a["sim"] = false, a["width"] = s->pw;
  fw::update_child(fw::find("football_pitch"), s->pitch, a.as<fw::Props>());
  ArduinoJson::JsonDocument hd;
  hd["label"] = f["comp"] | "Football";
  fw::update_child(fw::find("header"), s->header, hd.as<fw::Props>());
}
const char *META = R"({
  "about": "A live football match: the score and minute, both line-ups in formation, and the match's events.",
  "motion": "the simulated match",
  "frame": [480, 480],
  "props": [
    {"name": "speed", "type": "select", "options": ["1x", "10x", "60x"], "default": "60x"},
    {"name": "jump", "type": "select", "options": ["start", "kickoff", "goal", "card", "sub", "half time", "added time", "full time"], "default": "start"},
    {"name": "seed", "type": "range", "min": 1, "max": 20, "step": 1, "default": 7},
    {"name": "feed", "type": "text", "default": ""}
  ],
  "opening": "Sample data: the match simulator (seeded): made-up clubs and players.",
  "variants": [
    {"label": "the match · 60x", "props": {}},
    {"label": "first goal", "props": {"jump": "goal"}},
    {"label": "half time", "props": {"jump": "half time"}},
    {"label": "full time", "props": {"jump": "full time"}}
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
  lv_obj_add_event_cb(h, [](lv_event_t *e) { delete (Screen *) lv_event_get_user_data(e); }, LV_EVENT_DELETE, s);
  ArduinoJson::JsonDocument hd;
  hd["label"] = "Football";
  fw::find("header")->build(root, hd.as<fw::Props>());
  s->header = lv_obj_get_child(root, -1);
  lv_obj_set_pos(s->header, fw::lay::inset, fw::lay::inset);
  const int X = fw::lay::inset, Y = fw::lay::top, W = fw::lay::content_w, H = fw::lay::screen_h - fw::lay::top - fw::lay::inset;
  // the widest pitch whose whole component fits the content height (build, measure, shrink by 8 dp)
  lv_obj_t *pb = box(root, X, Y, W, H);
  int wdp = dp(W) / 8 * 8;
  for (;;) {
    ArduinoJson::JsonDocument pd;
    pd["sim"] = false, pd["feed"] = "{}", pd["width"] = wdp;
    fw::find("football_pitch")->build(pb, pd.as<fw::Props>());
    s->pitch = lv_obj_get_child(pb, -1);
    lv_obj_update_layout(pb);
    if (lv_obj_get_height(s->pitch) <= H || wdp <= 280) break;
    lv_obj_delete(s->pitch);
    wdp -= 8;
  }
  s->pw = wdp;
  const int pw = lv_obj_get_width(s->pitch);
  lv_obj_set_pos(pb, X + (W - pw) / 2 / 4 * 4, Y);
  lv_obj_set_size(pb, pw, H);
  ArduinoJson::JsonDocument doc;
  ArduinoJson::JsonVariantConst f = football_match_feed(root, p, doc, push);
  if (!f.isNull()) {
    fw::Quiet q;
    push(root, f);
  }
}
void build_f(lv_obj_t *parent, fw::Props p) { fill(lv_obj_create(parent), p); }
bool update_f(lv_obj_t *root, fw::Props p) {
  if (!of(root)) { fw::refill(root); fill(root, p); return true; }
  ArduinoJson::JsonDocument doc;
  ArduinoJson::JsonVariantConst f = football_match_feed(root, p, doc, push);
  if (!f.isNull()) push(root, f);
  return true;
}
const int reg = fw::add({"football_live", "Football live", "screens", META, build_f});
const int upd = fw::set_update("football_live", update_f);
}  // namespace
