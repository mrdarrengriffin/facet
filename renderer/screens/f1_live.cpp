// F1 live (facet-157, WIP): one f1_race_feed on one screen, every part a real component in its OWN region (a holder
// box placed by the layout tokens; the component sits at 0,0 inside, so a refill never moves it).
//   header   where and how far: "Austin · Lap 34 / 56"
//   720      left: F1 standings, the whole field in one compact column (22 x sport-row-compact, gap + interval);
//            right: the F1 circuit with the cars, the full content height
//   480      a Segmented toggle Map | Order (the field does not fit beside a readable track at 480): Map = the
//            full-width circuit; Order = the field in two compact columns (1-11, 12-22)
// Darren 2026-10-22: positions and the track only; flags / race status / race control are parked (off this screen).
// The screen runs one simulator (sample data) or takes p["feed"] from an adapter and hands the same feed to every
// part through its update path (silent).
#include "fw.h"
#include "sport.h"

namespace {
using namespace fw::sport;
struct Screen {
  lv_obj_t *header = nullptr, *chip = nullptr, *board[2] = {nullptr, nullptr}, *map = nullptr;
  lv_obj_t *map_box = nullptr, *order_box = nullptr;
  int rows[2] = {22, 0}, from[2] = {1, 12}, sw = 0, mw = 0, mh = 0;  // dp
  bool small = false;
  std::string marker, labels, view;
};
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
void board_props(const Screen &s, int i, ArduinoJson::JsonDocument &d) {
  d["sim"] = false, d["rows"] = s.rows[i], d["from"] = s.from[i], d["width"] = s.sw, d["density"] = "compact", d["stat"] = "interval";
}
void map_props(const Screen &s, ArduinoJson::JsonDocument &d) {
  d["sim"] = false, d["marker"] = s.marker, d["labels"] = s.labels, d["width"] = s.mw, d["height"] = s.mh, d["race_control"] = false;
}
void push(lv_obj_t *root, ArduinoJson::JsonVariantConst f) {
  Screen *s = of(root);
  if (!s) return;
  for (int i = 0; i < 2; i++) {
    if (!s->board[i]) continue;
    ArduinoJson::JsonDocument d;
    board_props(*s, i, d);
    d["feed"] = f;
    fw::update_child(fw::find("f1_standings"), s->board[i], d.as<fw::Props>());
  }
  ArduinoJson::JsonDocument m;
  map_props(*s, m);
  m["feed"] = f;
  fw::update_child(fw::find("f1_circuit"), s->map, m.as<fw::Props>());
  // header: place and lap
  const Circuit *c = circuit(f["track"] | "");
  char t[64];
  const int lap = f["session"]["lap"] | 0, laps = f["session"]["laps"] | 0;
  if (lap > 0) snprintf(t, sizeof t, "%s · Lap %d / %d", c ? c->place : "", lap, laps);
  else snprintf(t, sizeof t, "%s · %d laps", c ? c->place : "F1", laps);
  ArduinoJson::JsonDocument hd;
  hd["label"] = t;
  fw::update_child(fw::find("header"), s->header, hd.as<fw::Props>());
}
void show_view(Screen &s) {
  if (!s.small) return;
  lv_obj_set_flag(s.map_box, LV_OBJ_FLAG_HIDDEN, s.view != "map");
  lv_obj_set_flag(s.order_box, LV_OBJ_FLAG_HIDDEN, s.view == "map");
}
void on_event(lv_obj_t *from, const char *name, const char *detail, void *user) {
  (void) from;
  Screen *s = (Screen *) user;
  if (strcmp(name, "change") || !detail) return;
  s->view = !strcmp(detail, "order") ? "order" : "map";
  show_view(*s);
}
const char *META = R"({
  "about": "A live race: the running order and the cars on the circuit.",
  "motion": "the simulated race",
  "frame": [480, 480],
  "props": [
    {"name": "track", "type": "select", "options": ["au-1953", "cn-2004", "jp-1962", "us-2022", "ca-1978", "mc-1929", "es-1991", "at-1969", "gb-1948", "be-1925", "hu-1986", "nl-1948", "it-1922", "es-2026", "az-2016", "my-1999", "sg-2008", "us-2012", "mx-1962", "br-1940", "us-2023", "qa-2004", "ae-2009"], "default": "us-2012"},
    {"name": "view", "type": "select", "options": ["map", "order"], "default": "map"},
    {"name": "marker", "type": "select", "options": ["dot", "code", "number"], "default": "dot"},
    {"name": "labels", "type": "select", "options": ["none", "leader", "top 3", "battles", "all"], "default": "battles"},
    {"name": "speed", "type": "select", "options": ["1x", "10x", "60x"], "default": "10x"},
    {"name": "jump", "type": "select", "options": ["start", "overtake", "pit stop", "fastest lap", "retirement", "final lap", "chequered"], "default": "start"},
    {"name": "seed", "type": "range", "min": 1, "max": 20, "step": 1, "default": 7},
    {"name": "feed", "type": "text", "default": ""}
  ],
  "opening": "Sample data: the race simulator (seeded) on the 2026 grid.",
  "variants": [
    {"label": "Austin · start", "props": {}},
    {"label": "Monaco · order", "props": {"track": "mc-1929", "view": "order", "jump": "overtake"}},
    {"label": "Spa · pit stops", "props": {"track": "be-1925", "jump": "pit stop"}},
    {"label": "Abu Dhabi · chequered", "props": {"track": "ae-2009", "jump": "chequered"}}
  ]
})";
std::string sig_of(fw::Props p) { return std::string(p["marker"] | "dot") + "|" + (p["labels"] | "battles"); }
void fill(lv_obj_t *root, fw::Props p) {
  const fw::Theme &t = *fw::T;
  lv_obj_remove_style_all(root);
  lv_obj_remove_flag(root, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_size(root, fw::lay::screen_w, fw::lay::screen_h);
  lv_obj_set_style_bg_color(root, t.bg, 0);
  lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
  Screen *s = new Screen;
  s->marker = p["marker"] | "dot", s->labels = p["labels"] | "battles", s->view = p["view"] | "map";
  s->small = fw::lay::screen_w < 600;
  lv_obj_t *h = box(root, 0, 0, 0, 0);
  lv_obj_set_user_data(h, s);
  lv_obj_add_event_cb(h, [](lv_event_t *e) { delete (Screen *) lv_event_get_user_data(e); }, LV_EVENT_DELETE, s);
  if (const fw::Component *hc = fw::find("header")) {
    ArduinoJson::JsonDocument hd;
    hd["label"] = "F1";
    hc->build(root, hd.as<fw::Props>());
    s->header = lv_obj_get_child(root, -1);
    lv_obj_set_pos(s->header, fw::lay::inset, fw::lay::inset);
  }
  const int X = fw::lay::inset, Y = fw::lay::top, W = fw::lay::content_w, H = fw::lay::screen_h - fw::lay::top - fw::lay::inset;
  const int gap = fw::sp::s4;
  // a region holds one component at 0,0 and takes its size (so nothing in it is cut, and the screen check sees it)
  auto build = [&](const char *id, lv_obj_t *holder, ArduinoJson::JsonDocument &d) {
    fw::find(id)->build(holder, d.as<fw::Props>());
    lv_obj_t *c = lv_obj_get_child(holder, -1);
    lv_obj_add_flag(holder, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    return c;
  };
  ArduinoJson::JsonDocument none;
  none["cars"].to<ArduinoJson::JsonArray>();
  int y = Y, rx = X, rwid = W;
  if (s->small) {
    // the toggle row
    lv_obj_t *seg_box = box(root, X, y, W, fw::lay::chip_h + fw::sp::s4);
    ArduinoJson::JsonDocument sd;
    sd["options"] = "Map, Order", sd["selected"] = s->view;
    build("segmented", seg_box, sd);
    lv_obj_update_layout(seg_box);
    const int sh = (lv_obj_get_height(lv_obj_get_child(seg_box, 0)) + 3) / 4 * 4;
    lv_obj_set_height(seg_box, sh);
    y += sh + gap;
  } else {
    // the whole field on the left
    const int sw = (W * 36 / 100) / 4 * 4;
    s->rows[0] = std::min(22, H / fw::lay::sport_row_compact), s->rows[1] = 0;
    s->sw = dp(sw);
    lv_obj_t *bb = box(root, X, Y, sw, s->rows[0] * fw::lay::sport_row_compact);
    ArduinoJson::JsonDocument d;
    board_props(*s, 0, d);
    d["feed"] = none;
    s->board[0] = build("f1_standings", bb, d);
    rx = X + sw + fw::lay::gutter, rwid = W - sw - fw::lay::gutter;
  }
  // the map region: the rest of the content box
  const int mh = (Y + H - y) / 4 * 4;
  s->map_box = box(root, rx, y, rwid, mh);
  s->mw = dp(rwid), s->mh = dp(mh);
  ArduinoJson::JsonDocument md;
  map_props(*s, md);
  md["feed"] = none, md["track"] = p["track"] | "us-2012";
  s->map = build("f1_circuit", s->map_box, md);
  if (s->small) {  // the order region: two compact columns in the same place
    s->order_box = box(root, rx, y, rwid, mh);
    const int cw = (rwid - fw::lay::gutter) / 2 / 4 * 4;
    s->sw = dp(cw);
    s->rows[0] = s->rows[1] = std::min(11, mh / fw::lay::sport_row_compact);
    s->from[1] = 1 + s->rows[0];
    for (int i = 0; i < 2; i++) {
      lv_obj_t *col = box(s->order_box, i ? cw + fw::lay::gutter : 0, 0, cw, mh);
      ArduinoJson::JsonDocument d;
      board_props(*s, i, d);
      d["feed"] = none;
      s->board[i] = build("f1_standings", col, d);
    }
    show_view(*s);
    fw::listen(root, on_event, s);
  }
  ArduinoJson::JsonDocument doc;
  ArduinoJson::JsonVariantConst f = f1_race_feed(root, p, doc, push);
  if (!f.isNull()) {
    fw::Quiet q;
    push(root, f);
  }
  fw::shape(root, sig_of(p));
}
void build_f1(lv_obj_t *parent, fw::Props p) { fill(lv_obj_create(parent), p); }
bool update_f1(lv_obj_t *root, fw::Props p) {
  Screen *s = of(root);
  if (!s || !fw::shape(root, sig_of(p))) {
    fw::refill(root);
    fill(root, p);
    return true;
  }
  const std::string v = p["view"] | "map";
  if (v != s->view) s->view = v, show_view(*s);
  ArduinoJson::JsonDocument doc;
  ArduinoJson::JsonVariantConst f = f1_race_feed(root, p, doc, push);
  if (!f.isNull()) push(root, f);
  return true;
}
const int reg = fw::add({"f1_live", "F1 live", "screens", META, build_f1});
const int upd = fw::set_update("f1_live", update_f1);
}  // namespace
