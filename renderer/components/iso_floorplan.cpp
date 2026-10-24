// Iso floor plan: a floor plan in true isometric (Floor plans foundation, tokens/floorplan.json -> fw::fp::; the
// panel lab's Vacuum Map view, apps/hub panel/vacuum/iso.ts + vacuum.css, board facet-52): rooms as raised tiles
// with a low back wall, the dock, the robot and its run's path. The first Floor plans component.
//   plan     rooms as DATA (a spec parameter, never code): "Name|segment|x,z x,z ...|lx,lz;..." in metres (plan x
//            right, z down, origin = the inside rear-left corner, polygons clockwise); segment = the robot's room id
//            (empty = it can't clean it: canvas top, no label); lx,lz = the label point (empty = no label)
//   dock     "x0,z0,x1,z1,height" metres: a steel box (the sample: against the back wall)
//   robot    the robot's position: "x,y[,mode]" in the source's units, through `transform` ("a,b,c,d,e,f": metres
//            x = a x + b y + c, z = d x + e y + f; empty = metres already). Shown while a run is on (state cleaning,
//            returning, paused); docked = on the dock whatever it says; otherwise hidden (the lab's rule)
//   trail    the run's path "x,y x,y ..." (same units as robot): text-mute at its opacity, blue while it cleans
//   picked   the screen's picked rooms (spec "$rooms": "Kitchen:3,1": the id after a colon) = accent-dim top + accent
//            edge (the label stays text: accent-text is under 4.5:1 on accent-dim); here = the room it's cleaning now (name or id) = accent edge
//   state    the vacuum's state: cleaning = the LiDAR in accent + the live trail; while a run is on a tap is blocked
//   show     plan | rules (the foundation's rules in words, for the workbench page)
// Drawing (fw::fp:: tokens): scale = the largest that fits the whole house in content-w x lay::plan_h, plan centred
// on the grid. ONE RGB565 image, drawn here with 4-sample anti-aliasing (scanline coverage), in painter's order: back
// + left walls, rooms back to front (front + right sides, top, hairline edge), the dock after the room it stands in,
// the trail. Labels = real labels (type label) over it. The robot = a small ARGB sprite (cylinder + LiDAR) moved with
// lv_anim (fp::GLIDE_MS, linear: the position sensor's interval; reduced motion = a jump): a move invalidates two
// sprite-sized boxes, never the plan (lag test). A pick or a new current room redraws only that room's box.
// Tap: a room the robot cleans = event "pick" (detail "Name:id", the spec's screen.toggle) + sound tap, picked at once;
// during a run = blocked + error. Update path: robot glides; picked / here / trail / state redraw their boxes at once
// (one picture, audit snaps); plan / dock / transform / theme = built again. Contrast: section "iso_floorplan".
#include <algorithm>
#include <cmath>
#include <cstring>
#include <set>
#include <vector>
#include "fw.h"
#include "floorplan_gen.h"

namespace {
constexpr float C30 = 0.8660254f, S30 = 0.5f;
struct P { float x, z; };
struct Room { std::string name, seg; std::vector<P> poly; bool has_label = false; P lab{0, 0}; };
struct Rect { int x0, y0, x1, y1; };  // inclusive-exclusive pixel box

struct FP {
  std::vector<Room> rooms;
  float dock[5] = {0, 0.4f, 0.14f, 0.7f, 0.36f};
  float tf[6] = {1, 0, 0, 0, 1, 0};
  std::string key_plan, picked_s, here, state, trail_s, robot_s;
  std::set<std::string> picked;
  std::vector<P> trail;
  float W = 1, D = 1, k = 1, ox = 0, oy = 0;
  int iw = 0, ih = 0, offx = 0, offy = 0;  // image size + its place in the root
  lv_draw_buf_t *buf = nullptr, *bot_buf = nullptr;
  lv_obj_t *img = nullptr, *bot = nullptr;
  std::vector<lv_obj_t *> labels;  // one per labelled room, in rooms order (nullptr = none)
  int bcx = 0, bcy = 0;            // the sprite's floor point inside it
  bool has_bot = false, lidar_run = false;
  float bx = 0, by = 0;            // the sprite's floor point now (root px)
  float fx = 0, fy = 0, tx = 0, ty = 0;  // a glide: from, to
  const fw::Theme *theme = nullptr;
};
FP *of(lv_obj_t *o) { return (FP *) lv_obj_get_user_data(o); }

// ---- parsing
std::vector<std::string> split(const std::string &s, char d) {
  std::vector<std::string> o;
  size_t i = 0;
  while (i <= s.size()) {
    size_t j = s.find(d, i);
    if (j == std::string::npos) j = s.size();
    o.push_back(s.substr(i, j - i));
    i = j + 1;
  }
  return o;
}
std::vector<P> points(const std::string &s) {  // "x,z x,z ..."
  std::vector<P> o;
  for (const std::string &t : split(s, ' ')) {
    float x, z;
    if (sscanf(t.c_str(), "%f,%f", &x, &z) == 2) o.push_back({x, z});
  }
  return o;
}
void parse_plan(FP *f, const std::string &plan) {
  f->rooms.clear();
  f->W = f->D = 0.5f;
  for (const std::string &r : split(plan, ';')) {
    std::vector<std::string> fl = split(r, '|');
    if (fl.size() < 3) continue;
    Room rm;
    rm.name = fl[0], rm.seg = fl[1], rm.poly = points(fl[2]);
    if (rm.poly.size() < 3) continue;
    if (fl.size() > 3 && sscanf(fl[3].c_str(), "%f,%f", &rm.lab.x, &rm.lab.z) == 2) rm.has_label = !rm.seg.empty();
    for (const P &p : rm.poly) f->W = std::max(f->W, p.x), f->D = std::max(f->D, p.z);
    f->rooms.push_back(rm);
  }
}
std::string id_of(const std::string &item) {  // "Kitchen:3" -> "3"
  const size_t c = item.rfind(':');
  std::string v = c == std::string::npos ? item : item.substr(c + 1);
  while (!v.empty() && v.front() == ' ') v.erase(0, 1);
  return v;
}
P source_pt(const FP *f, float x, float y) { return {f->tf[0] * x + f->tf[1] * y + f->tf[2], f->tf[3] * x + f->tf[4] * y + f->tf[5]}; }
bool running(const std::string &s) { return s == "cleaning" || s == "returning" || s == "paused"; }

// ---- projection (image px)
struct V { float x, y; };
V pt(const FP *f, float x, float z, float y = 0) { return {f->ox + (x - z) * C30 * f->k, f->oy + (x + z) * S30 * f->k - y * f->k}; }

// ---- raster (RGB565, 4 samples)
struct Canvas { uint16_t *px; int w, h, stride; Rect clip; };
inline void blend(Canvas &c, int x, int y, lv_color_t col, float a) {
  if (a <= 0) return;
  uint16_t &d = c.px[y * c.stride + x];
  if (a >= 1) { d = lv_color_to_u16(col); return; }
  const int r = (d >> 11) & 31, g = (d >> 5) & 63, b = d & 31;
  const float R = r * 255.f / 31, G = g * 255.f / 63, B = b * 255.f / 31;
  const lv_color_t m = lv_color_make((uint8_t) lroundf(R + (col.red - R) * a), (uint8_t) lroundf(G + (col.green - G) * a), (uint8_t) lroundf(B + (col.blue - B) * a));
  d = lv_color_to_u16(m);
}
Rect bbox(const std::vector<V> &v, float pad) {
  float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
  for (const V &p : v) x0 = std::min(x0, p.x), y0 = std::min(y0, p.y), x1 = std::max(x1, p.x), y1 = std::max(y1, p.y);
  return {(int) floorf(x0 - pad), (int) floorf(y0 - pad), (int) ceilf(x1 + pad) + 1, (int) ceilf(y1 + pad) + 1};
}
Rect meet(Rect a, Rect b) { return {std::max(a.x0, b.x0), std::max(a.y0, b.y0), std::min(a.x1, b.x1), std::min(a.y1, b.y1)}; }
// polygon fill (even-odd), coverage from 2 x 2 samples per pixel, opacity a
void fill(Canvas &c, const std::vector<V> &v, lv_color_t col, float a = 1) {
  const Rect r = meet(bbox(v, 1), c.clip);
  if (r.x0 >= r.x1 || r.y0 >= r.y1) return;
  std::vector<uint8_t> cnt(r.x1 - r.x0);
  std::vector<float> xs;
  for (int y = r.y0; y < r.y1; y++) {
    std::fill(cnt.begin(), cnt.end(), 0);
    for (float sy : {y + 0.25f, y + 0.75f}) {
      xs.clear();
      for (size_t i = 0; i < v.size(); i++) {
        const V &p = v[i], &q = v[(i + 1) % v.size()];
        if ((p.y <= sy) != (q.y <= sy)) xs.push_back(p.x + (sy - p.y) / (q.y - p.y) * (q.x - p.x));
      }
      std::sort(xs.begin(), xs.end());
      for (size_t i = 0; i + 1 < xs.size(); i += 2)
        for (float sx0 : {0.25f, 0.75f}) {  // the samples at x + sx0 inside [xs[i], xs[i+1])
          const int a0 = std::max(r.x0, (int) ceilf(xs[i] - sx0)), a1 = std::min(r.x1 - 1, (int) ceilf(xs[i + 1] - sx0) - 1);
          for (int x = a0; x <= a1; x++) cnt[x - r.x0]++;
        }
    }
    for (int x = r.x0; x < r.x1; x++)
      if (cnt[x - r.x0]) blend(c, x, y, col, a * cnt[x - r.x0] / 4.f);
  }
}
float seg_dist(float px, float py, V a, V b) {
  const float dx = b.x - a.x, dy = b.y - a.y, l2 = dx * dx + dy * dy;
  float t = l2 > 0 ? ((px - a.x) * dx + (py - a.y) * dy) / l2 : 0;
  t = std::max(0.f, std::min(1.f, t));
  const float ex = a.x + t * dx - px, ey = a.y + t * dy - py;
  return sqrtf(ex * ex + ey * ey);
}
// a polyline (closed = a polygon's edge), width w, round joins: per pixel the nearest segment, blended once
void stroke(Canvas &c, const std::vector<V> &v, bool closed, float w, lv_color_t col, float a = 1) {
  if (v.size() < 2) return;
  const Rect r = meet(bbox(v, w), c.clip);
  if (r.x0 >= r.x1 || r.y0 >= r.y1) return;
  const size_t n = closed ? v.size() : v.size() - 1;
  for (int y = r.y0; y < r.y1; y++)
    for (int x = r.x0; x < r.x1; x++) {
      float d = 1e9f;
      for (size_t i = 0; i < n; i++) d = std::min(d, seg_dist(x + 0.5f, y + 0.5f, v[i], v[(i + 1) % v.size()]));
      const float cov = std::max(0.f, std::min(1.f, w * 0.5f + 0.5f - d));
      if (cov > 0) blend(c, x, y, col, a * cov);
    }
}
lv_color_t ink(fw::fp::Ink i) { return fw::T->*i; }
lv_color_t mixed(fw::fp::Ink i, int opa, fw::fp::Ink under) { return lv_color_mix(ink(i), ink(under), (uint8_t) (opa * 255 / 100)); }

bool contains(const std::vector<P> &poly, float x, float z) {
  bool in = false;
  for (size_t i = 0, j = poly.size() - 1; i < poly.size(); j = i++)
    if ((poly[i].z > z) != (poly[j].z > z) && x < (poly[j].x - poly[i].x) * (z - poly[i].z) / (poly[j].z - poly[i].z) + poly[i].x) in = !in;
  return in;
}
float depth(const Room &r) { float d = 1e9f; for (const P &p : r.poly) d = std::min(d, p.x + p.z); return d; }
bool is_picked(const FP *f, const Room &r) { return !r.seg.empty() && f->picked.count(r.seg); }
bool is_here(const FP *f, const Room &r) { return running(f->state) && !f->here.empty() && (f->here == r.name || f->here == r.seg); }
std::vector<V> top_of(const FP *f, const Room &r) { std::vector<V> v; for (const P &p : r.poly) v.push_back(pt(f, p.x, p.z)); return v; }
// a room's box in the image: its top + the slab under it + a mark stroke
Rect room_box(const FP *f, const Room &r) {
  std::vector<V> v = top_of(f, r);
  for (const P &p : r.poly) v.push_back(pt(f, p.x, p.z, -fw::fp::SLAB));
  return bbox(v, fw::fp::W_MARK + 1);
}

// the plan into the image, only inside `clip`
void render(FP *f, Rect clip) {
  const fw::Theme &t = *fw::T;
  Canvas c{(uint16_t *) f->buf->data, f->iw, f->ih, (int) (f->buf->header.stride / 2), meet(clip, {0, 0, f->iw, f->ih})};
  if (c.clip.x0 >= c.clip.x1 || c.clip.y0 >= c.clip.y1) return;
  const uint16_t bg = lv_color_to_u16(t.bg);
  for (int y = c.clip.y0; y < c.clip.y1; y++)
    for (int x = c.clip.x0; x < c.clip.x1; x++) c.px[y * c.stride + x] = bg;
  using namespace fw::fp;
  // the low walls along the back (z = 0) and the left (x = 0): the dolls-house cutaway
  fill(c, {pt(f, 0, 0), pt(f, f->W, 0), pt(f, f->W, 0, WALL), pt(f, 0, 0, WALL)}, ink(fw::fp::INK_WALL));
  fill(c, {pt(f, 0, 0), pt(f, 0, f->D), pt(f, 0, f->D, WALL), pt(f, 0, 0, WALL)}, mixed(INK_WALL_LEFT, OPA_WALL_LEFT, fw::fp::INK_TOP_OFF));
  std::vector<const Room *> order;
  for (const Room &r : f->rooms) order.push_back(&r);
  std::stable_sort(order.begin(), order.end(), [](const Room *a, const Room *b) { return depth(*a) < depth(*b); });
  const float dcx = (f->dock[0] + f->dock[2]) / 2, dcz = (f->dock[1] + f->dock[3]) / 2;
  bool dock_done = false;
  auto draw_dock = [&] {
    const float x0 = f->dock[0], z0 = f->dock[1], x1 = f->dock[2], z1 = f->dock[3], hh = f->dock[4];
    fill(c, {pt(f, x1, z0), pt(f, x1, z1), pt(f, x1, z1, hh), pt(f, x1, z0, hh)}, ink(INK_SIDE_RIGHT));
    fill(c, {pt(f, x0, z1), pt(f, x1, z1), pt(f, x1, z1, hh), pt(f, x0, z1, hh)}, ink(INK_SIDE_FRONT));
    fill(c, {pt(f, x0, z0, hh), pt(f, x1, z0, hh), pt(f, x1, z1, hh), pt(f, x0, z1, hh)}, ink(INK_DOCK));
    dock_done = true;
  };
  for (const Room *r : order) {
    const std::vector<P> &q = r->poly;
    for (size_t i = 0; i < q.size(); i++) {  // clockwise: an edge going -x faces the viewer (+z), one going +z faces right
      const P &a = q[i], &b = q[(i + 1) % q.size()];
      const bool front = b.x < a.x, right = !front && b.z > a.z;
      if (front || right) fill(c, {pt(f, a.x, a.z), pt(f, b.x, b.z), pt(f, b.x, b.z, -SLAB), pt(f, a.x, a.z, -SLAB)}, ink(front ? INK_SIDE_FRONT : INK_SIDE_RIGHT));
    }
    const std::vector<V> top = top_of(f, *r);
    const bool can = !r->seg.empty(), pk = is_picked(f, *r), hr = is_here(f, *r);
    fill(c, top, ink(pk ? INK_PICKED : can ? INK_TOP : INK_TOP_OFF));
    stroke(c, top, true, pk || hr ? W_MARK : W_EDGE, ink(pk ? INK_PICKED_EDGE : hr ? INK_HERE_EDGE : can ? INK_EDGE : INK_EDGE_OFF));
    if (!dock_done && contains(q, dcx, dcz)) draw_dock();
  }
  if (!dock_done) draw_dock();
  if (f->trail.size() > 1) {
    std::vector<V> tv;
    for (const P &p : f->trail) tv.push_back(pt(f, p.x, p.z));
    const bool live = f->state == "cleaning";
    stroke(c, tv, false, W_TRAIL, ink(live ? INK_TRAIL_LIVE : INK_TRAIL), (live ? OPA_TRAIL_LIVE : OPA_TRAIL) / 100.f);
  }
}
void invalidate(FP *f, Rect r) {
  lv_area_t a;
  lv_obj_get_coords(f->img, &a);
  lv_area_t b{a.x1 + std::max(0, r.x0), a.y1 + std::max(0, r.y0), a.x1 + std::min(f->iw, r.x1) - 1, a.y1 + std::min(f->ih, r.y1) - 1};
  if (b.x2 >= b.x1 && b.y2 >= b.y1) lv_obj_invalidate_area(f->img, &b);
}

// ---- the robot sprite (ARGB8888, 4 x 4 samples): side band, top, LiDAR
void render_bot(FP *f) {
  using namespace fw::fp;
  const float r = ROBOT_R * f->k, rx = r * (float) M_SQRT2 * C30, ry = r * (float) M_SQRT2 * S30, hh = ROBOT_H * f->k;
  const int w = (int) ceilf(rx * 2 + 4), h = (int) ceilf(ry * 2 + hh + 4);
  const float cx = w / 2.f, cy = h - ry - 2;
  if (!f->bot_buf || (int) f->bot_buf->header.w != w || (int) f->bot_buf->header.h != h) {
    if (f->bot_buf) lv_draw_buf_destroy(f->bot_buf);
    f->bot_buf = lv_draw_buf_create(w, h, LV_COLOR_FORMAT_ARGB8888, LV_STRIDE_AUTO);
  }
  f->bcx = (int) lroundf(cx), f->bcy = (int) lroundf(cy);
  const lv_color_t side = ink(INK_ROBOT_SIDE), top = ink(INK_ROBOT_TOP), lid = ink(f->lidar_run ? INK_LIDAR_RUN : fw::fp::INK_LIDAR);
  auto inside = [](float x, float y, float ex, float ey, float a, float b) { const float u = (x - ex) / a, v = (y - ey) / b; return u * u + v * v <= 1; };
  for (int y = 0; y < h; y++) {
    auto *row = (uint8_t *) f->bot_buf->data + y * f->bot_buf->header.stride;
    for (int x = 0; x < w; x++) {
      int n = 0, R = 0, G = 0, B = 0;
      for (int sy = 0; sy < 4; sy++)
        for (int sx = 0; sx < 4; sx++) {
          const float px = x + (sx + 0.5f) / 4, py = y + (sy + 0.5f) / 4;
          const lv_color_t *col = nullptr;
          if (inside(px, py, cx, cy - hh - 1, rx * fw::fp::LIDAR, ry * fw::fp::LIDAR)) col = &lid;
          else if (inside(px, py, cx, cy - hh, rx, ry)) col = &top;
          else if (inside(px, py, cx, cy, rx, ry) || (fabsf(px - cx) <= rx && py >= cy - hh && py <= cy)) col = &side;
          if (col) n++, R += col->red, G += col->green, B += col->blue;
        }
      uint8_t *p = row + x * 4;
      p[0] = n ? B / n : 0, p[1] = n ? G / n : 0, p[2] = n ? R / n : 0, p[3] = (uint8_t) (n * 255 / 16);
    }
  }
  lv_image_set_src(f->bot, f->bot_buf);
  lv_obj_set_size(f->bot, w, h);
  lv_obj_invalidate(f->bot);
}
void place_bot(FP *f, float x, float y) {
  f->bx = x, f->by = y;
  lv_obj_set_pos(f->bot, (int32_t) lroundf(x) - f->bcx, (int32_t) lroundf(y) - f->bcy);
}
void glide_cb(void *o, int32_t v) {
  FP *f = of((lv_obj_t *) o);
  if (f) place_bot(f, f->fx + (f->tx - f->fx) * v / 1000.f, f->fy + (f->ty - f->fy) * v / 1000.f);
}
// the robot's floor point in root px from the props (false = hidden)
bool bot_target(FP *f, float &x, float &y) {
  P m;
  if (f->state == "docked") m = {f->dock[2] + fw::fp::ROBOT_REAL_R, (f->dock[1] + f->dock[3]) / 2};  // parked in front of it
  else if (running(f->state)) {
    float a, b;
    if (sscanf(f->robot_s.c_str(), "%f,%f", &a, &b) != 2) return false;
    m = source_pt(f, a, b);
  } else return false;
  const V v = pt(f, m.x, m.z);
  x = f->offx + v.x, y = f->offy + v.y;
  return true;
}
void move_bot(lv_obj_t *root, FP *f, bool animate) {
  float x, y;
  const bool show = bot_target(f, x, y);
  if (show != !lv_obj_has_flag(f->bot, LV_OBJ_FLAG_HIDDEN)) lv_obj_set_flag(f->bot, LV_OBJ_FLAG_HIDDEN, !show);
  if (!show) { lv_anim_delete(root, glide_cb); return; }
  if (fabsf(x - f->tx) < 0.5f && fabsf(y - f->ty) < 0.5f && f->has_bot) return;  // already going there
  lv_anim_delete(root, glide_cb);
  if (!animate || !f->has_bot || fw::mo::reduced(root)) { f->tx = x, f->ty = y; place_bot(f, x, y); f->has_bot = true; return; }
  f->fx = f->bx, f->fy = f->by, f->tx = x, f->ty = y;
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, root);
  lv_anim_set_exec_cb(&a, glide_cb);
  lv_anim_set_values(&a, 0, 1000);
  lv_anim_set_duration(&a, fw::fp::GLIDE_MS);
  lv_anim_set_path_cb(&a, lv_anim_path_linear);
  lv_anim_start(&a);
}

// ---- labels
void paint_labels(FP *f) {
  const fw::Theme &t = *fw::T;
  for (size_t i = 0; i < f->rooms.size(); i++) {
    lv_obj_t *l = f->labels[i];
    if (!l) continue;
    const lv_color_t c = is_picked(f, f->rooms[i]) ? ink(fw::fp::INK_LABEL_PICKED) : ink(fw::fp::INK_LABEL);
    if (!lv_color_eq(lv_obj_get_style_text_color(l, LV_PART_MAIN), c)) lv_obj_set_style_text_color(l, c, 0);  // only on a change
    (void) t;
  }
}
void take(FP *f, fw::Props p) {
  f->picked_s = p["picked"] | "";
  f->picked.clear();
  for (const std::string &it : split(f->picked_s, ',')) if (!id_of(it).empty()) f->picked.insert(id_of(it));
  f->here = p["here"] | "";
  f->state = p["state"] | "docked";
  f->lidar_run = f->state == "cleaning";
  f->robot_s = p["robot"] | "";
  f->trail_s = p["trail"] | "";
  f->trail.clear();
  for (const std::string &t : split(f->trail_s, ' ')) {
    float a, b;
    if (sscanf(t.c_str(), "%f,%f", &a, &b) == 2) f->trail.push_back(source_pt(f, a, b));
  }
}
std::string build_key(fw::Props p) {
  return std::string(p["plan"] | "") + "#" + (p["dock"] | "") + "#" + (p["transform"] | "") + "#" + (p["show"] | "plan");
}

#define FP_PLAN "Dining|2|0,0 3.6,0 3.6,3 0,3|1.8,1.5;Utility||3.7,0 6.4,0 6.4,1.4 3.7,1.4|;Study||3.7,1.5 6.4,1.5 6.4,3 3.7,3|;Lounge|1|0,3.1 3.9,3.1 3.9,7.2 0,7.2|1.95,5.15;Hall|3|4,3.1 6.4,3.1 6.4,5.6 4,5.6|5.2,4.35;Cloakroom||4,5.7 6.4,5.7 6.4,7.2 4,7.2|"
#define FP_TRAIL "0.6,0.7 3.1,0.7 3.1,1.3 0.6,1.3 0.6,1.9 3.1,1.9 3.1,2.5 0.6,2.5"

void rules_page(lv_obj_t *root) {
  const fw::Theme &t = *fw::T;
  lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(root, fw::sp::s4, 0);
  for (const fw::fp::Rule &r : fw::fp::RULES) {
    fw::text(root, fw::ty::label, r.title, t.text_dim, fw::lay::content_w);
    fw::text(root, fw::ty::body, r.about, t.text, fw::lay::content_w);
  }
  lv_obj_set_height(root, LV_SIZE_CONTENT);
}
}  // namespace

FW_COMPONENT(iso_floorplan, "Iso floor plan", "components", R"({
  "about": "A floor plan in isometric: the rooms as raised tiles with a low back wall, the dock, and the robot moving on it with its path. Rooms are data (polygons in metres); tap a room to pick it. Drawn once as one picture per theme; only the robot moves. Rules: Floor plans tokens (tokens/floorplan.json): tile 0.3 m, wall 0.5 m, tops surface + line-hi hairline, sides line-hi / line, picked accent-dim + accent edge, labels in the caps label type for rooms it cleans.",
  "frame": [480, 0],
  "audit": {"snaps": {"picked": "one pre-rendered picture (the lab's rule: no live drawing per frame): a pick redraws that room once", "here": "the same picture: the current room's edge is redrawn once", "state": "the LiDAR colour and the trail ink are part of the pictures", "trail": "the path is drawn into the picture when a new point arrives, every few seconds"}},
  "props": [
    {"name": "plan", "type": "text", "default": "Dining|2|0,0 3.6,0 3.6,3 0,3|1.8,1.5;Utility||3.7,0 6.4,0 6.4,1.4 3.7,1.4|;Study||3.7,1.5 6.4,1.5 6.4,3 3.7,3|;Lounge|1|0,3.1 3.9,3.1 3.9,7.2 0,7.2|1.95,5.15;Hall|3|4,3.1 6.4,3.1 6.4,5.6 4,5.6|5.2,4.35;Cloakroom||4,5.7 6.4,5.7 6.4,7.2 4,7.2|"},
    {"name": "dock", "type": "text", "default": "0,0.5,0.14,0.8,0.36"},
    {"name": "robot", "type": "text", "default": "1.4,4.8"},
    {"name": "transform", "type": "text", "default": ""},
    {"name": "trail", "type": "text", "default": "0.6,0.7 3.1,0.7 3.1,1.3 0.6,1.3 0.6,1.9 3.1,1.9 3.1,2.5 0.6,2.5"},
    {"name": "picked", "type": "text", "default": ""},
    {"name": "here", "type": "text", "default": ""},
    {"name": "state", "type": "select", "options": ["docked", "cleaning", "returning", "paused", "idle", "error", "unavailable"], "default": "docked"},
    {"name": "show", "type": "select", "options": ["plan", "rules"], "default": "plan"}
  ],
  "variants": [
    {"label": "docked · last run", "about": "On the dock; the last run's path in text-mute. Rooms it cleans have names; the pantry, stairs and WC stay quiet.", "props": {}},
    {"label": "two rooms picked", "about": "Picked rooms: accent-dim top and an accent edge; the name in text (tap a room to pick it).", "props": {"picked": "Dining:2,Hall:3"}},
    {"label": "cleaning", "about": "A run on: the robot on the plan with its LiDAR in the accent, the room it's in edged in the accent, the live path in blue.", "props": {"state": "cleaning", "here": "Lounge", "robot": "1.4,4.8", "trail": "0.5,3.6 3.4,3.6 3.4,4.2 0.5,4.2 0.5,4.8 1.4,4.8"}},
    {"label": "rules", "about": "The Floor plans rules, from tokens/floorplan.json.", "props": {"show": "rules"}}
  ]
})") {
  auto *f = new FP;
  lv_obj_t *root = lv_obj_create(parent);
  lv_obj_remove_style_all(root);
  lv_obj_remove_flag(root, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_user_data(root, f);
  lv_obj_add_event_cb(root, [](lv_event_t *e) {
    auto *f = (FP *) lv_event_get_user_data(e);
    lv_anim_delete(lv_event_get_current_target(e), glide_cb);
    if (f->buf) lv_draw_buf_destroy(f->buf);
    if (f->bot_buf) lv_draw_buf_destroy(f->bot_buf);
    delete f;
  }, LV_EVENT_DELETE, f);
  f->key_plan = build_key(p);
  lv_obj_set_width(root, fw::lay::content_w);
  if (!strcmp(p["show"] | "plan", "rules")) { rules_page(root); return; }
  lv_obj_set_height(root, fw::lay::plan_h);
  f->theme = fw::T;
  parse_plan(f, p["plan"] | FP_PLAN);
  sscanf(p["dock"] | "", "%f,%f,%f,%f,%f", &f->dock[0], &f->dock[1], &f->dock[2], &f->dock[3], &f->dock[4]);
  const std::string tf = p["transform"] | "";
  if (!tf.empty()) sscanf(tf.c_str(), "%f,%f,%f,%f,%f,%f", &f->tf[0], &f->tf[1], &f->tf[2], &f->tf[3], &f->tf[4], &f->tf[5]);
  take(f, p);
  // the largest scale that fits the whole house (W + D across, half that down + the slab and wall)
  const float span = f->W + f->D, BW = (float) fw::lay::content_w, BH = (float) fw::lay::plan_h;
  f->k = std::min(BW / (span * C30), BH / (span * S30 + fw::fp::SLAB + fw::fp::WALL));
  f->ox = f->D * C30 * f->k, f->oy = fw::fp::WALL * f->k;
  f->iw = (int) ceilf(span * C30 * f->k), f->ih = (int) ceilf(span * S30 * f->k + (fw::fp::SLAB + fw::fp::WALL) * f->k);
  f->offx = (fw::lay::content_w - f->iw) / 2 / fw::sp::GRID * fw::sp::GRID, f->offy = (fw::lay::plan_h - f->ih) / 2 / fw::sp::GRID * fw::sp::GRID;
  f->buf = lv_draw_buf_create(f->iw, f->ih, LV_COLOR_FORMAT_RGB565, LV_STRIDE_AUTO);
  f->img = lv_image_create(root);
  lv_obj_set_pos(f->img, f->offx, f->offy);
  lv_obj_set_size(f->img, f->iw, f->ih);
  fw::qa::free(f->img, "drawn to scale: the plan's size follows the house, centred on the grid");
  render(f, {0, 0, f->iw, f->ih});
  lv_image_set_src(f->img, f->buf);
  lv_obj_add_flag(f->img, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(f->img, LV_OBJ_FLAG_PRESS_LOCK);
  // room names (rooms it cleans, at their label points)
  const fw::Theme &t = *fw::T;
  const lv_font_t *font = fw::font(fw::ty::label);
  for (const Room &r : f->rooms) {
    if (!r.has_label) { f->labels.push_back(nullptr); continue; }
    lv_obj_t *l = fw::text(root, fw::ty::label, r.name.c_str(), t.text_dim);
    lv_point_t sz;
    lv_text_get_size(&sz, lv_label_get_text(l), font, lv_obj_get_style_text_letter_space(l, LV_PART_MAIN), 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    const V v = pt(f, r.lab.x, r.lab.z);
    lv_obj_set_pos(l, f->offx + (int32_t) lroundf(v.x - sz.x / 2.f), f->offy + (int32_t) lroundf(v.y) - fw::style(fw::ty::label).box / 2);
    lv_obj_set_height(l, fw::style(fw::ty::label).box);
    fw::qa::free(l, "placed at the room's label point (to scale)");
    f->labels.push_back(l);
  }
  paint_labels(f);
  // the robot
  f->bot = lv_image_create(root);
  lv_obj_remove_flag(f->bot, LV_OBJ_FLAG_CLICKABLE);
  fw::qa::free(f->bot, "placed at the robot's position (to scale)");
  render_bot(f);
  move_bot(root, f, false);
  // tap: the room under the finger (the floor, y = 0)
  lv_obj_add_event_cb(f->img, [](lv_event_t *e) {
    auto *root = (lv_obj_t *) lv_event_get_user_data(e);
    FP *f = of(root);
    lv_point_t pnt;
    lv_indev_t *in = lv_indev_active();
    if (!in) return;
    lv_indev_get_point(in, &pnt);
    lv_area_t a;
    lv_obj_get_coords(f->img, &a);
    const float sx = (pnt.x - a.x1 - f->ox) / (C30 * f->k), sy = (pnt.y - a.y1 - f->oy) / (S30 * f->k);
    const float x = (sx + sy) / 2, z = (sy - sx) / 2;
    for (Room &r : f->rooms) {
      if (r.seg.empty() || !contains(r.poly, x, z)) continue;
      if (running(f->state)) { fw::st::blocked(f->img, r.name.c_str()); return; }  // rooms are for the next run
      const bool on = !f->picked.count(r.seg);
      if (on) f->picked.insert(r.seg); else f->picked.erase(r.seg);
      const std::string item = r.name + ":" + r.seg;
      fw::emit(f->img, "pick", item.c_str());
      fw::sound(f->img, "tap");
      const Rect b = room_box(f, r);
      render(f, b);
      invalidate(f, b);
      paint_labels(f);
      return;
    }
  }, LV_EVENT_CLICKED, root);
}

FW_UPDATE(iso_floorplan) {
  FP *f = of(root);
  if (!f || f->key_plan != build_key(p) || f->theme != fw::T) return false;  // another house / theme: built again
  if (!f->buf) return true;                                                   // the rules page
  const std::set<std::string> was_picked = f->picked;
  const std::string was_here = f->here, was_state = f->state, was_trail = f->trail_s;
  std::vector<bool> was_here_room;
  for (const Room &r : f->rooms) was_here_room.push_back(is_here(f, r));
  take(f, p);
  if (f->trail_s != was_trail || (running(was_state) != running(f->state)) || ((was_state == "cleaning") != (f->state == "cleaning") && f->trail.size() > 1)) {
    render(f, {0, 0, f->iw, f->ih});  // a new path / run: the whole picture once
    lv_obj_invalidate(f->img);
  } else {
    for (size_t i = 0; i < f->rooms.size(); i++) {  // only the rooms whose look changed
      const Room &r = f->rooms[i];
      if (was_picked.count(r.seg) == f->picked.count(r.seg) && was_here_room[i] == is_here(f, r)) continue;
      const Rect b = room_box(f, r);
      render(f, b);
      invalidate(f, b);
    }
  }
  paint_labels(f);
  if ((was_state == "cleaning") != f->lidar_run) render_bot(f);
  move_bot(root, f, true);
  return true;
}
