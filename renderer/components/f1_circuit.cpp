// F1 circuit: an F1 circuit from the Sport foundation's data (the 23 rounds of 2026, bacinger/f1-circuits, MIT,
// calibrated: start / finish, race direction, marshal sectors) drawn in Facet style, with the cars of an f1_race_feed
// gliding along it and the race-control state on the track itself. claude-assistant notes/research/sports-ux.md.
//   track    one stroke-atom line (stroke::track) through the surveyed points (Catmull-Rom, every point stays on the
//            line), north-up, fitted INSIDE the component's box (aspect kept, inset by the stroke and the largest
//            mark); start / finish = a short text bar across the track at lap 0. No pit lane (not in the data).
//   status   the line's colour IS the track status: steel = green, warn = Safety Car / Virtual Safety Car (and
//            their "ending" lap), error = red flag; a yellow / double yellow marshal sector is overdrawn in warn
//            (double = a wider line). Colour changes fade (Motion fade), silent.
//   cars     Sport marks (drawn by the cars layer with sport::draw_mark, the look of the mark objects): dot / code /
//            number; the leader carries a text ring, a car in the pits goes hollow, a retired or disqualified car
//            leaves the track. The Safety Car = an "SC" mark in warn just ahead of the leader (feed sc_at). Each frame
//            invalidates one box per moved car (old united with new, tag included): 22 boxes, under LVGL's 32-area
//            buffer (22 objects made 44 and the overflow redrew the whole screen every frame: 3 fps on the Guition).
//   labels   none / leader / top 3 / battles (within a second) / all, then COLLISION: tags are placed in priority
//            order (leader, top 3, battles, moments, the rest) and a tag that would cover another tag or another car
//            is hidden (variant "number": no tags, the number inside the mark).
//   orient   broadcast (default: turned like the TV / timing map, calibrated tracks) or north (the survey as it lies)
//   motion   lap fractions; each update glides the field along the track for the time since the last one (at most
//            sport-glide-max), one anim; reduced motion = jump. First build = at rest. The draw order is redone only
//            when the order changed. Lite (auto = the 480 build): tags are re-laid (collisions) only when a feed update
//            arrives, not every frame, so a frame between updates only moves the marks.
//   moments  an overtake on track tags both cars for sport-moment.
//   data     prop "feed" = an f1_race_feed (sports-data.md), else the race simulator (seed, speed, jump).
// A read-out: no touch, no sound. Contrast pairs: colour.json section "sport".
#include "fw.h"
#include "sport.h"
#include <cmath>
#include <map>

namespace {
using namespace fw::sport;
using fw::StrokePt;
// One car = its look + where it is. The cars are NOT objects: the layer draws them all in its draw event
// (sport::draw_mark, the same look as the mark objects) and each frame invalidates one box per car (old position
// united with the new, tag included). 22 objects invalidated 2 areas each, overflowing LVGL's 32-area buffer into a
// full-screen redraw every frame (the 3 fps Darren saw on the Guition, 2026-10-23).
struct Car {
  std::string code;
  MarkLook l;
  MarkInk k;
  float from = 0, to = 0, cx = -1, cy = -1;
  int pos = 99, prio = 9;
  bool want = false, tag = false, shown = false;  // a tag wanted / drawn; drawn at all (placed once)
  uint32_t tag_until = 0;
};
struct View {
  Track track;
  Mark mark = Mark::dot;
  bool rc = true;  // show race control on the track (status colour, yellow sectors, the SC)
  bool lite = false;  // 480 build: tags re-laid only on a feed update, the glide stepped (sport-glide-step)
  std::string labels = "leader", moments = "tags";
  std::map<std::string, Car> cars;
  Car sc;  // the safety car (sc.shown false = off track)
  bool sc_on = false;
  std::vector<Car *> order;  // draw order: back of the field first, the leader last (on top)
  lv_obj_t *layer = nullptr, *line = nullptr, *yellow = nullptr;
  std::string status = "green", yellow_sig;
  uint32_t last_ms = 0, dur = 0;
  int w = 0, h = 0, last_step = -1;
  lv_color_t behind;
  const Circuit *c = nullptr;
  std::string tag_text;  // storage for the tag strings (MarkLook::code points into Car::code)
};
View *view_of(lv_obj_t *root) {
  lv_obj_t *layer = lv_obj_get_child(root, -1);
  return layer ? (View *) lv_obj_get_user_data(layer) : nullptr;
}
void free_view(lv_event_t *e) { delete (View *) lv_event_get_user_data(e); }

float lerp_frac(float a, float b, float v) {  // forward along the lap (b can be past the line)
  if (b < a - 0.5f) b += 1;
  if (b > a + 0.5f) b -= 1;  // a backwards jump (sim jump / rewind) takes the short way
  return a + (b - a) * v;
}
bool overlap(const lv_area_t &a, const lv_area_t &b) { return a.x1 <= b.x2 && b.x1 <= a.x2 && a.y1 <= b.y2 && b.y1 <= a.y2; }
// invalidate a box in the layer's px (to screen px)
void dirty(View &v, lv_area_t a) {
  lv_area_t o;
  lv_obj_get_coords(v.layer, &o);
  lv_area_move(&a, o.x1, o.y1);
  lv_obj_invalidate_area(v.layer, &a);
}
void each(View &v, void (*f)(View &, Car &)) {
  for (Car *c : v.order) f(v, *c);
  if (v.sc_on) f(v, v.sc);
}
// show the wanted tags in priority order, hiding any that would cover a shown tag or another car's mark
void declutter(View &v) {
  std::vector<Car *> all(v.order.begin(), v.order.end());
  if (v.sc_on) all.push_back(&v.sc);
  std::sort(all.begin(), all.end(), [](Car *a, Car *b) { return a->prio != b->prio ? a->prio < b->prio : a->pos < b->pos; });
  std::vector<lv_area_t> discs(all.size()), tags;
  for (size_t i = 0; i < all.size(); i++) { lv_area_t t; mark_boxes(all[i]->l, all[i]->k, all[i]->cx, all[i]->cy, v.w, &discs[i], &t); }
  for (size_t i = 0; i < all.size(); i++) {
    Car &c = *all[i];
    bool show = c.want && c.k.tag && !c.l.out;
    if (show) {
      lv_area_t d, t;
      mark_boxes(c.l, c.k, c.cx, c.cy, v.w, &d, &t);
      for (auto &x : tags) show = show && !overlap(t, x);
      for (size_t j = 0; j < discs.size() && show; j++) show = j == i || !overlap(t, discs[j]);
      if (show) tags.push_back(t);
    }
    if (show != c.tag) {  // the tag comes or goes: its box is redrawn
      dirty(v, mark_extent(c.l, c.k, c.cx, c.cy, v.w, true));
      c.tag = show;
    }
  }
}
// move every car to fraction k of its glide; each car invalidates the union of where it was and where it is now
void place_all(View &v, float k, bool update = true) {
  auto move = [&](Car &c) {
    if (c.l.out) {
      if (c.shown) dirty(v, mark_extent(c.l, c.k, c.cx, c.cy, v.w, true)), c.shown = false;
      return;
    }
    const StrokePt pt = point_at(v.track, lerp_frac(c.from, c.to, k));
    const float nx = std::round(pt.x), ny = std::round(pt.y);
    if (c.shown && nx == c.cx && ny == c.cy) return;
    lv_area_t a = mark_extent(c.l, c.k, nx, ny, v.w, true);
    if (c.shown) {
      const lv_area_t o = mark_extent(c.l, c.k, c.cx, c.cy, v.w, true);
      a = {std::min(a.x1, o.x1), std::min(a.y1, o.y1), std::max(a.x2, o.x2), std::max(a.y2, o.y2)};
    }
    c.cx = nx, c.cy = ny, c.shown = true;
    dirty(v, a);
  };
  for (Car *c : v.order) move(*c);
  if (v.sc_on) move(v.sc);
  if (update || !v.lite) declutter(v);
}
void anim_cb(void *var, int32_t x) {
  View &v = *(View *) lv_obj_get_user_data((lv_obj_t *) var);
  if (v.lite && v.dur > 0) {  // stepped glide: positions change every sport-glide-step, not every frame
    const int steps = std::max(1, (int) (v.dur * 1 / fw::mo::ms::sport_glide_step));
    const int s = (int) (x * steps / 1024);
    if (s == v.last_step && x < 1024) return;
    v.last_step = s;
    x = x >= 1024 ? 1024 : s * 1024 / steps;
  }
  place_all(v, x / 1024.0f, false);
}
// the layer's draw: every car where it is, back of the field first (the leader on top), tags after all marks
void draw_cb(lv_event_t *e) {
  View &v = *(View *) lv_event_get_user_data(e);
  lv_layer_t *layer = lv_event_get_layer(e);
  lv_area_t o;
  lv_obj_get_coords(v.layer, &o);
  const lv_point_t at = {o.x1, o.y1};
  for (int part = 1; part <= 2; part++) {
    for (Car *c : v.order)
      if (c->shown && !c->l.out && (part == 1 || c->tag)) draw_mark(layer, c->l, c->k, c->cx, c->cy, v.w, part, at);
    if (v.sc_on && v.sc.shown && (part == 1 || v.sc.tag)) draw_mark(layer, v.sc.l, v.sc.k, v.sc.cx, v.sc.cy, v.w, part, at);
  }
}
// the track's colour for a status (green / yellow flags keep it steel: yellows are marked per sector)
lv_color_t status_ink(const std::string &s) {
  const fw::Theme &t = *fw::T;
  if (s == "red") return t.error;
  if (!s.compare(0, 2, "sc") || !s.compare(0, 3, "vsc")) return t.warn;
  return t.steel;
}
struct Fade { lv_obj_t *img; lv_color_t from, to; };
void fade_cb(lv_anim_t *a, int32_t x) {
  Fade *f = (Fade *) lv_anim_get_user_data(a);
  lv_obj_set_style_image_recolor(f->img, lv_color_mix(f->to, f->from, (uint8_t) x), 0);
}
void fade_done(lv_anim_t *a) { delete (Fade *) lv_anim_get_user_data(a); }
void recolour(lv_obj_t *img, lv_color_t to, bool glide) {
  const lv_color_t from = lv_obj_get_style_image_recolor(img, LV_PART_MAIN);
  lv_anim_delete(img, nullptr);
  if (!glide || fw::mo::reduced(img)) { lv_obj_set_style_image_recolor(img, to, 0); return; }
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, img);
  lv_anim_set_user_data(&a, new Fade{img, from, to});
  lv_anim_set_custom_exec_cb(&a, fade_cb);
  lv_anim_set_deleted_cb(&a, fade_done);  // also on completion
  lv_anim_set_values(&a, 0, 255);
  lv_anim_set_duration(&a, fw::mo::ms::fade);
  lv_anim_start(&a);
}
// yellow sectors: one stroke over the track for every marshal sector under yellow (double = wider)
void yellows(lv_obj_t *root, View &v, ArduinoJson::JsonVariantConst feed, bool glide) {
  std::string sig;
  for (ArduinoJson::JsonObjectConst s : feed["sectors"].as<ArduinoJson::JsonArrayConst>())
    sig += std::to_string((int) (s["n"] | 0)) + (s["flag"] | "") + ",";
  if (sig == v.yellow_sig) return;
  v.yellow_sig = sig;
  if (v.yellow) lv_obj_delete(v.yellow), v.yellow = nullptr;
  if (sig.empty() || !v.c || !v.c->n_sectors) return;
  v.yellow = lv_obj_create(root);
  lv_obj_remove_style_all(v.yellow);
  lv_obj_remove_flag(v.yellow, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_size(v.yellow, v.w, v.h);
  lv_obj_move_to_index(v.yellow, lv_obj_get_index(v.layer));  // under the cars
  for (ArduinoJson::JsonObjectConst s : feed["sectors"].as<ArduinoJson::JsonArrayConst>()) {
    const int n = (s["n"] | 0) - 1;
    if (n < 0 || n >= v.c->n_sectors) continue;
    const float a = v.c->sectors[n] / 65535.0f, b = n + 1 < v.c->n_sectors ? v.c->sectors[n + 1] / 65535.0f : 1.0f + v.c->sectors[0] / 65535.0f;
    std::vector<StrokePt> pts;
    const int steps = std::max(2, (int) ((b - a) * v.track.len / 3));
    for (int i = 0; i <= steps; i++) pts.push_back(point_at(v.track, a + (b - a) * i / steps));
    const bool dbl = !strcmp(s["flag"] | "", "double_yellow");
    lv_obj_t *seg = fw::stroke_path(v.yellow, v.w, v.h, pts.data(), (int) pts.size(), dbl ? fw::stroke::track + 2 * fw::stroke::edge : fw::stroke::track, fw::T->warn);
    if (glide) fw::appear(seg, fw::mo::fade, true, 0);
  }
}

bool labelled(const View &v, int pos, bool battle) {
  return v.labels == "all" || ((v.labels == "top 3") && pos <= 3) || (v.labels == "leader" && pos == 1)
      || (v.labels == "battles" && (pos == 1 || battle));
}
// apply a race feed to the cars (create / restyle / move); `glide` = animate from where they are
void apply_feed(lv_obj_t *root, View &v, ArduinoJson::JsonVariantConst feed, bool glide) {
  const fw::Theme &t = *fw::T;
  const uint32_t now = lv_tick_get();
  uint32_t dur = v.last_ms ? now - v.last_ms : 0;
  dur = std::min<uint32_t>(fw::mo::ms::sport_glide_max, dur);
  v.last_ms = now, v.dur = dur, v.last_step = -1;
  lv_anim_delete(v.layer, anim_cb);
  for (auto &[code, c] : v.cars) (void) code, c.from = c.to;
  v.sc.from = v.sc.to;
  // track status
  const std::string st = v.rc ? (feed["session"]["flag"] | "green") : "green";
  if (st != v.status) {
    v.status = st;
    recolour(v.line, status_ink(st), glide);
  }
  if (v.rc) yellows(root, v, feed, glide);
  std::map<std::string, int> before;
  for (auto &[code, c] : v.cars) before[code] = c.pos;
  std::map<int, float> ints;  // battles: within a second of the car ahead (and the car ahead)
  for (ArduinoJson::JsonObjectConst x : feed["cars"].as<ArduinoJson::JsonArrayConst>())
    if (x["int"].is<float>() && (float) (x["int"] | 9.0f) < 1.0f) ints[x["pos"] | 99] = 1, ints[(x["pos"] | 99) - 1] = 1;
  for (ArduinoJson::JsonObjectConst x : feed["cars"].as<ArduinoJson::JsonArrayConst>()) {
    const char *code = x["code"] | "";
    if (!*code) continue;
    auto it = v.cars.find(code);
    const bool fresh = it == v.cars.end();
    Car &c = fresh ? v.cars.emplace(code, Car()).first->second : it->second;
    if (fresh) c.code = code;
    const int pos = x["pos"] | 99;
    MarkLook l;
    l.mark = v.mark, l.team = hex(x["colour"] | "", t.text_dim), l.behind = v.behind, l.code = c.code.c_str();
    l.num = x["num"] | 0, l.lead = pos == 1, l.pit = x["pit"] | false, l.out = (x["out"] | false) || (x["dsq"] | false);
    const bool moment = v.moments == "tags" && glide && now < c.tag_until;
    const bool battle = ints.count(pos) > 0;
    c.want = v.mark != Mark::number && (v.mark == Mark::code || labelled(v, pos, battle) || moment);
    c.prio = pos == 1 ? 0 : pos <= 3 ? 1 : battle ? 2 : moment ? 3 : 4;
    l.label = c.want;
    const bool look = fresh || l.lead != c.l.lead || l.pit != c.l.pit || l.out != c.l.out || l.label != c.l.label || l.num != c.l.num
                   || !lv_color_eq(l.team, c.l.team);
    if (look) {
      if (c.shown) dirty(v, mark_extent(c.l, c.k, c.cx, c.cy, v.w, true));  // the old look goes
      c.l = l, c.k = mark_ink(l);
      if (c.shown) dirty(v, mark_extent(c.l, c.k, c.cx, c.cy, v.w, true));
    }
    const float at = x["at"] | 0.0f;
    if (fresh) c.from = c.to = at;
    else c.from = glide ? c.to : at, c.to = at;
    c.pos = pos;
  }
  // the safety car
  const bool sc = v.rc && feed["sc_at"].is<float>();
  if (sc) {
    const float at = feed["sc_at"] | 0.0f;
    if (!v.sc_on) {
      v.sc = Car();
      v.sc.code = "SC";
      v.sc.l.mark = Mark::code, v.sc.l.team = t.warn, v.sc.l.behind = v.behind, v.sc.l.code = v.sc.code.c_str();
      v.sc.k = mark_ink(v.sc.l);
      v.sc.from = v.sc.to = at, v.sc.want = true, v.sc.prio = 0, v.sc.pos = 0;
      v.sc_on = true;
    } else {
      v.sc.from = glide ? v.sc.to : at, v.sc.to = at;
    }
  } else if (v.sc_on) {
    if (v.sc.shown) dirty(v, mark_extent(v.sc.l, v.sc.k, v.sc.cx, v.sc.cy, v.w, true));
    v.sc_on = false, v.sc.shown = false;
  }
  // an overtake on track: a car gained a place on one it was behind, neither in the pits -> both show their tags
  if (glide && v.moments == "tags") {
    for (auto &[code, c] : v.cars) {
      auto b = before.find(code);
      if (b == before.end() || b->second >= 99 || c.pos >= b->second || c.l.out) continue;
      for (auto &[code2, d] : v.cars)
        if (&d != &c && before.count(code2) && before[code2] < b->second && before[code2] >= c.pos && d.pos > c.pos && !d.l.out)
          c.tag_until = d.tag_until = now + fw::mo::ms::sport_moment;
    }
  }
  // draw order: back of the field first, the leader last (on top). Data only: no objects move, nothing is redrawn
  // for it except where cars actually are
  v.order.clear();
  for (auto &[code, c] : v.cars) (void) code, v.order.push_back(&c);
  std::sort(v.order.begin(), v.order.end(), [](Car *a, Car *b) { return a->pos > b->pos; });
  const bool still = !glide || fw::mo::reduced(root) || dur == 0;
  if (still) { place_all(v, 1); return; }
  place_all(v, 0);
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, v.layer);
  lv_anim_set_exec_cb(&a, anim_cb);
  lv_anim_set_values(&a, 0, 1024);
  lv_anim_set_duration(&a, dur);
  lv_anim_set_path_cb(&a, lv_anim_path_linear);
  lv_anim_start(&a);
}

void sim_apply(lv_obj_t *root, ArduinoJson::JsonVariantConst feed) {
  if (View *v = view_of(root)) apply_feed(root, *v, feed, true);
}
std::string sig_of(fw::Props p, const char *track) {
  char b[160];
  snprintf(b, sizeof b, "%s|%d|%d|%s|%s|%s|%d|%d|%s|%s", track, (int) (p["width"] | 384), (int) (p["height"] | 304),
           p["marker"] | "dot", p["labels"] | "battles", p["moments"] | "tags", (int) (p["sim"] | true), (int) (p["race_control"] | true),
           p["orientation"] | "broadcast", p["lite"] | "auto");
  return b;
}

void fill(lv_obj_t *root, fw::Props p) {
  const fw::Theme &t = *fw::T;
  lv_obj_remove_style_all(root);
  lv_obj_remove_flag(root, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  const int w = fw::px((float) (p["width"] | 384)), h = fw::px((float) (p["height"] | 304));
  lv_obj_set_size(root, w, h);
  ArduinoJson::JsonDocument doc;
  ArduinoJson::JsonVariantConst feed = f1_race_feed(root, p, doc, sim_apply);
  const char *tid = feed["track"] | (p["track"] | "us-2012");
  const Circuit *c = circuit(tid);
  if (!c) c = &CIRCUITS[0];
  View *v = new View;
  v->c = c;
  v->w = w, v->h = h, v->mark = mark_of(p["marker"] | "dot"), v->labels = p["labels"] | "battles";
  v->moments = p["moments"] | "tags", v->behind = t.bg, v->rc = p["race_control"] | true;
  const char *lite = p["lite"] | "auto";
#ifdef FW_1X
  v->lite = strcmp(lite, "off") != 0;
#else
  v->lite = !strcmp(lite, "on");
#endif
  // the track fits inside the box: room for the largest mark (and the lead ring) on every side
  const int pad = mark_size(Mark::number) / 2 + fw::stroke::edge + fw::sp::s1;
  v->track = fit(*c, w, h, pad, strcmp(p["orientation"] | "broadcast", "north") != 0);
  v->line = fw::stroke_path(root, w, h, v->track.pts.data(), (int) v->track.pts.size(), fw::stroke::track, t.steel);
  // start / finish: a bar across the track at lap 0
  const StrokePt s0 = point_at(v->track, 0);
  const float hd = heading_at(v->track, 0) + 1.5707963f, half = fw::lay::sport_sf / 2.0f;
  const StrokePt sf[2] = {{s0.x - std::cos(hd) * half, s0.y - std::sin(hd) * half}, {s0.x + std::cos(hd) * half, s0.y + std::sin(hd) * half}};
  fw::stroke_at(root, sf, 2, fw::stroke::edge * 2, t.text, fw::Cap::butt);
  // the cars: one layer over the track (holds the view)
  lv_obj_t *layer = lv_obj_create(root);
  lv_obj_remove_style_all(layer);
  lv_obj_remove_flag(layer, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_size(layer, w, h);
  v->layer = layer;
  lv_obj_set_user_data(layer, v);
  lv_obj_add_event_cb(layer, free_view, LV_EVENT_DELETE, v);
  lv_obj_add_event_cb(layer, draw_cb, LV_EVENT_DRAW_MAIN, v);  // the cars (drawn, not objects)
  if (!feed.isNull()) apply_feed(root, *v, feed, false);
  fw::shape(root, sig_of(p, c->id));
}
}  // namespace

FW_COMPONENT(f1_circuit, "F1 circuit", "components", R"({
  "about": "An F1 circuit drawn from survey data, with the cars of a race moving along it and the flags on the track.",
  "motion": "the simulated race: cars glide round the track",
  "motion_off": "sim",
  "frame": [400, 320],
  "props": [
    {"name": "track", "type": "select", "options": ["au-1953", "cn-2004", "jp-1962", "us-2022", "ca-1978", "mc-1929", "es-1991", "at-1969", "gb-1948", "be-1925", "hu-1986", "nl-1948", "it-1922", "es-2026", "az-2016", "my-1999", "sg-2008", "us-2012", "mx-1962", "br-1940", "us-2023", "qa-2004", "ae-2009"], "default": "us-2012"},
    {"name": "marker", "type": "select", "options": ["dot", "code", "number"], "default": "dot"},
    {"name": "labels", "type": "select", "options": ["none", "leader", "top 3", "battles", "all"], "default": "battles"},
    {"name": "moments", "type": "select", "options": ["tags", "off"], "default": "tags"},
    {"name": "race_control", "type": "bool", "default": true},
    {"name": "orientation", "type": "select", "options": ["broadcast", "north"], "default": "broadcast"},
    {"name": "lite", "type": "select", "options": ["auto", "on", "off"], "default": "auto"},
    {"name": "sim", "type": "bool", "default": true},
    {"name": "speed", "type": "select", "options": ["1x", "10x", "60x"], "default": "1x"},
    {"name": "jump", "type": "select", "options": ["start", "yellow", "double yellow", "vsc", "safety car", "red flag", "restart", "overtake", "pit stop", "blue flag", "fastest lap", "retirement", "final lap", "chequered"], "default": "start"},
    {"name": "seed", "type": "range", "min": 1, "max": 20, "step": 1, "default": 7},
    {"name": "cars", "type": "range", "min": 2, "max": 22, "step": 1, "default": 22},
    {"name": "width", "type": "range", "min": 240, "max": 448, "step": 8, "default": 384},
    {"name": "height", "type": "range", "min": 200, "max": 448, "step": 8, "default": 304},
    {"name": "feed", "type": "text", "default": ""}
  ],
  "opening": "Sample data: the race simulator (seeded) runs the 2026 grid round the chosen circuit.",
  "variants": [
    {"label": "dot · battles tagged", "props": {}},
    {"label": "code · every tag that fits", "props": {"marker": "code", "labels": "all"}},
    {"label": "number in the mark", "props": {"marker": "number", "labels": "none"}},
    {"label": "Silverstone · north up", "props": {"track": "gb-1948", "orientation": "north"}},
    {"label": "Monaco · 20 cars", "props": {"track": "mc-1929", "cars": 20}},
    {"label": "Spa · yellow sector", "props": {"track": "be-1925", "jump": "yellow"}},
    {"label": "Silverstone · safety car", "props": {"track": "gb-1948", "jump": "safety car"}},
    {"label": "Monza · red flag", "props": {"track": "it-1922", "jump": "red flag"}},
    {"label": "Suzuka · chequered", "props": {"track": "jp-1962", "jump": "chequered"}}
  ]
})") {
  fill(lv_obj_create(parent), p);
}

// Update path: a new feed (or sim speed / jump) glides the cars and fades the status in place, silent; another
// track, size, mark, labels or data source refills (snaps).
FW_UPDATE(f1_circuit) {
  View *v = view_of(root);
  ArduinoJson::JsonDocument doc;
  const char *tid = (p["feed"]["track"] | (p["track"] | "us-2012"));
  if (!v || !fw::shape(root, sig_of(p, tid))) {
    fw::refill(root);
    fill(root, p);
    return true;
  }
  ArduinoJson::JsonVariantConst feed = f1_race_feed(root, p, doc, sim_apply);
  if (!feed.isNull()) apply_feed(root, *v, feed, true);
  return true;
}
