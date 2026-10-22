// Atoms: focus ring + corner ticks (atoms.h). Pure moves of the code Button / Icon button drew inline (frame-identical,
// hashed before and after at both densities, both themes, every accent set).
#include "fw.h"

namespace fw {

void focus_ring(lv_obj_t *o, lv_style_selector_t show, int offset) {
  lv_obj_set_style_outline_color(o, st::focus_ring(*T), 0);
  lv_obj_set_style_outline_width(o, 0, 0);
  lv_obj_set_style_outline_width(o, st::FOCUS_W, show);
  lv_obj_set_style_outline_pad(o, offset - st::FOCUS_W, show);
  lv_obj_set_style_outline_width(o, 0, st::OFF);  // disabled / unavailable hide it, even when active
  bleed(o, offset);                                // the ring sits outside the box: parents must not clip it
}

namespace {
lv_obj_t *tick(lv_obj_t *o, int len, int thick, lv_border_side_t sides, lv_style_selector_t show) {
  lv_obj_t *t = lv_obj_create(o);
  lv_obj_remove_style_all(t);
  lv_obj_add_flag(t, LV_OBJ_FLAG_IGNORE_LAYOUT);
  lv_obj_remove_flag(t, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_size(t, len, len);
  lv_obj_set_style_border_side(t, sides, 0);
  lv_obj_set_style_border_width(t, thick, 0);
  lv_obj_set_style_border_color(t, st::on_mark(*T), 0);
  lv_obj_set_style_border_opa(t, show ? LV_OPA_TRANSP : LV_OPA_COVER, 0);
  if (show) lv_obj_set_style_border_opa(t, LV_OPA_COVER, show);
  // pressed (the owner trickles it): accent-text, so the ticks keep 3:1 on the pressed grey (states.json on.mark-pressed)
  lv_obj_set_style_border_color(t, st::on_mark_pressed(*T), LV_STATE_PRESSED);
  return t;
}
}  // namespace

void corner_ticks(lv_obj_t *o, Corners c, lv_style_selector_t show, int len, int thick) {
  // children align inside padding + border: step back by both so the ticks sit on the border box
  const int sl = lv_obj_get_style_space_left(o, LV_PART_MAIN), st = lv_obj_get_style_space_top(o, LV_PART_MAIN);
  const int sr = lv_obj_get_style_space_right(o, LV_PART_MAIN), sb = lv_obj_get_style_space_bottom(o, LV_PART_MAIN);
  const auto side = [](int a, int b) { return (lv_border_side_t) (a | b); };
  const bool all = c == Corners::all;
  if (all) lv_obj_align(tick(o, len, thick, side(LV_BORDER_SIDE_TOP, LV_BORDER_SIDE_LEFT), show), LV_ALIGN_TOP_LEFT, -sl, -st);
  lv_obj_align(tick(o, len, thick, side(LV_BORDER_SIDE_TOP, LV_BORDER_SIDE_RIGHT), show), LV_ALIGN_TOP_RIGHT, sr, -st);
  lv_obj_align(tick(o, len, thick, side(LV_BORDER_SIDE_BOTTOM, LV_BORDER_SIDE_LEFT), show), LV_ALIGN_BOTTOM_LEFT, -sl, sb);
  if (all) lv_obj_align(tick(o, len, thick, side(LV_BORDER_SIDE_BOTTOM, LV_BORDER_SIDE_RIGHT), show), LV_ALIGN_BOTTOM_RIGHT, sr, sb);
}

// ---------------------------------------------------------------- smooth stroke (facet-145: atom for everything)
extern "C" void lv_image_cache_drop(const void *src);  // LVGL 9 (misc/cache/instance/lv_image_cache.h, not in lvgl.h)
namespace {
constexpr float PI_F = 3.14159265f;
struct Stroke {  // a stroke image's state (its user data)
  lv_draw_buf_t *b;
  int width;
  Cap cap;
  bool arc = false;
  float cx = 0, cy = 0, r = 0, a0 = 0, a1 = 0;  // arcs: centre + centre-line radius in mask px, angles
};
Stroke *stroke_of(lv_obj_t *o) { return o ? (Stroke *) lv_obj_get_user_data(o) : nullptr; }

// one segment a -> c into the mask (max-combined); ca / cc = the cap at a / at c (Cap::round for inner joins)
void seg(lv_draw_buf_t *b, StrokePt a, StrokePt c, float width, Cap ca, Cap cc, const lv_area_t *clip) {
  const int w = b->header.w, h = b->header.h, stride = b->header.stride;
  uint8_t *d = (uint8_t *) b->data;
  const float r = width * 0.5f + 0.5f;
  int x0 = std::max(0, (int) std::floor(std::min(a.x, c.x) - r)), x1 = std::min(w - 1, (int) std::ceil(std::max(a.x, c.x) + r));
  int y0 = std::max(0, (int) std::floor(std::min(a.y, c.y) - r)), y1 = std::min(h - 1, (int) std::ceil(std::max(a.y, c.y) + r));
  if (clip) {
    x0 = std::max(x0, (int) clip->x1), x1 = std::min(x1, (int) clip->x2);
    y0 = std::max(y0, (int) clip->y1), y1 = std::min(y1, (int) clip->y2);
  }
  const float dx = c.x - a.x, dy = c.y - a.y, ll = dx * dx + dy * dy, L = std::sqrt(ll);
  const bool ba = ca == Cap::butt && ll > 0, bc = cc == Cap::butt && ll > 0;
  for (int y = y0; y <= y1; y++) {
    uint8_t *row = d + y * stride;
    for (int x = x0; x <= x1; x++) {
      const float px = x - a.x, py = y - a.y;
      const float tt = ll > 0 ? (px * dx + py * dy) / ll : 0;
      float t = tt;  // a round end clamps to the end point (distance to it = the cap); a butt end keeps the line's
      if (!ba && t < 0) t = 0;
      if (!bc && t > 1) t = 1;
      const float ex = px - t * dx, ey = py - t * dy;
      float cov = r - std::sqrt(ex * ex + ey * ey);
      if (ba) cov = std::min(cov, 0.5f + tt * L);        // cut square at a (AA over the last half pixel)
      if (bc) cov = std::min(cov, 0.5f + (1 - tt) * L);  // and at c
      if (cov <= 0) continue;
      const uint8_t v = cov >= 1 ? 255 : (uint8_t) (cov * 255.f + 0.5f);
      if (v > row[x]) row[x] = v;
    }
  }
}
void path_raster(lv_draw_buf_t *b, const StrokePt *p, int n, float width, Cap cap, const lv_area_t *clip = nullptr) {
  if (n == 1) seg(b, p[0], p[0], width, Cap::round, Cap::round, clip);
  for (int i = 1; i < n; i++) seg(b, p[i - 1], p[i], width, i == 1 ? cap : Cap::round, i == n - 1 ? cap : Cap::round, clip);
}
void clear(lv_draw_buf_t *b, const lv_area_t *a) {
  if (!a) { std::memset(b->data, 0, b->data_size); return; }
  for (int y = a->y1; y <= a->y2; y++) std::memset((uint8_t *) b->data + y * b->header.stride + a->x1, 0, a->x2 - a->x1 + 1);
}
lv_draw_buf_t *mask(int w, int h) {
  lv_draw_buf_t *b = lv_draw_buf_create(std::max(1, w), std::max(1, h), LV_COLOR_FORMAT_A8, LV_STRIDE_AUTO);
  if (b) std::memset(b->data, 0, b->data_size);
  return b;
}
lv_obj_t *image_of(lv_obj_t *parent, Stroke *s, lv_color_t colour) {
  lv_obj_t *im = lv_image_create(parent);
  lv_obj_remove_flag(im, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_add_flag(im, LV_OBJ_FLAG_EVENT_BUBBLE);
  lv_obj_set_style_image_recolor(im, colour, 0);
  lv_obj_set_style_image_recolor_opa(im, LV_OPA_COVER, 0);
  lv_obj_set_user_data(im, s);
  if (s->b) lv_image_set_src(im, s->b);
  lv_obj_add_event_cb(im, [](lv_event_t *e) {
    auto *s = (Stroke *) lv_event_get_user_data(e);
    if (s->b) { lv_image_cache_drop(s->b); lv_draw_buf_destroy(s->b); }
    delete s;
  }, LV_EVENT_DELETE, s);
  return im;
}
// the arc's centre line a0 -> a1 (degrees) as points, <= 0.75 px apart
std::vector<StrokePt> arc_points(const Stroke *s, float a0, float a1) {
  const float span = a1 - a0;
  const int n = std::max(2, (int) std::ceil(s->r * span * PI_F / 180.f / 0.75f) + 1);
  std::vector<StrokePt> p(n);
  for (int i = 0; i < n; i++) {
    const float a = (a0 + span * i / (n - 1)) * PI_F / 180.f;
    p[i] = {s->cx + s->r * std::cos(a), s->cy + s->r * std::sin(a)};
  }
  return p;
}
void arc_raster(Stroke *s, const lv_area_t *clip) {
  if (s->a1 - s->a0 <= 0.001f) return;
  const std::vector<StrokePt> p = arc_points(s, s->a0, s->a1);
  path_raster(s->b, p.data(), (int) p.size(), (float) s->width, s->cap, clip);
}
void refresh(lv_obj_t *o, Stroke *s, const lv_area_t *dirty) {  // the mask changed: drop the cached image, redraw
  lv_image_cache_drop(s->b);
  if (!dirty) { lv_obj_invalidate(o); return; }
  lv_area_t c, a;
  lv_obj_get_coords(o, &c);
  a = {c.x1 + dirty->x1, c.y1 + dirty->y1, c.x1 + dirty->x2, c.y1 + dirty->y2};
  lv_obj_invalidate_area(o, &a);
}
}  // namespace

lv_obj_t *stroke_path(lv_obj_t *parent, int w, int h, const StrokePt *pts, int n, int width, lv_color_t colour, Cap cap) {
  auto *s = new Stroke{mask(w, h), width, cap};
  if (s->b) path_raster(s->b, pts, n, (float) width, cap);
  return image_of(parent, s, colour);
}

std::vector<StrokePt> stroke_sample(StrokeFn f, void *user) {
  float len = 0;  // length from 64 chords, then one sample per 0.75 px of it
  StrokePt q = f(0, user);
  for (int i = 1; i <= 64; i++) {
    const StrokePt c = f(i / 64.f, user);
    len += std::sqrt((c.x - q.x) * (c.x - q.x) + (c.y - q.y) * (c.y - q.y));
    q = c;
  }
  const int n = std::min(8192, std::max(16, (int) std::ceil(len / 0.75f))) + 1;
  std::vector<StrokePt> pts(n);
  for (int i = 0; i < n; i++) pts[i] = f((float) i / (n - 1), user);
  return pts;
}

lv_obj_t *stroke_fn(lv_obj_t *parent, int w, int h, StrokeFn f, void *user, int width, lv_color_t colour, Cap cap) {
  const std::vector<StrokePt> pts = stroke_sample(f, user);
  return stroke_path(parent, w, h, pts.data(), (int) pts.size(), width, colour, cap);
}

lv_obj_t *stroke_at(lv_obj_t *parent, const StrokePt *pts, int n, int width, lv_color_t colour, Cap cap) {
  float x0 = pts[0].x, y0 = pts[0].y, x1 = x0, y1 = y0;
  for (int i = 1; i < n; i++) x0 = std::min(x0, pts[i].x), x1 = std::max(x1, pts[i].x), y0 = std::min(y0, pts[i].y), y1 = std::max(y1, pts[i].y);
  const float m = width * 0.5f + 1;
  const int ox = (int) std::floor(x0 - m), oy = (int) std::floor(y0 - m);
  std::vector<StrokePt> p(pts, pts + n);
  for (auto &q : p) q.x -= ox, q.y -= oy;
  lv_obj_t *im = stroke_path(parent, (int) std::ceil(x1 + m) - ox + 1, (int) std::ceil(y1 + m) - oy + 1, p.data(), n, width, colour, cap);
  lv_obj_set_pos(im, ox, oy);
  return im;
}

void stroke_redraw(lv_obj_t *o, const StrokePt *pts, int n, const lv_area_t *dirty) {
  Stroke *s = stroke_of(o);
  if (!s || !s->b) return;
  clear(s->b, dirty);
  path_raster(s->b, pts, n, (float) s->width, s->cap, dirty);
  refresh(o, s, dirty);
}

lv_obj_t *stroke_arc(lv_obj_t *parent, int x, int y, int d, float a0, float a1, int width, lv_color_t colour, Cap cap) {
  auto *s = new Stroke{mask(d + 2, d + 2), width, cap};
  s->arc = true;
  s->cx = s->cy = d / 2.f + 0.5f;  // the box centre (edge coords d / 2) in mask pixel-centre coords, + the 1 px fringe
  s->r = d / 2.f - width / 2.f;
  s->a0 = a0;
  s->a1 = a1 < a0 ? a1 + 360 : a1;
  if (s->b) arc_raster(s, nullptr);
  lv_obj_t *im = image_of(parent, s, colour);
  lv_obj_set_pos(im, x - 1, y - 1);
  return im;
}

void stroke_set_arc(lv_obj_t *o, float a0, float a1) {
  Stroke *s = stroke_of(o);
  if (!s || !s->b || !s->arc) return;
  if (a1 < a0) a1 += 360;
  if (a0 == s->a0 && a1 == s->a1) return;
  lv_area_t dirty = {0, 0, (int32_t) s->b->header.w - 1, (int32_t) s->b->header.h - 1};
  if (a0 == s->a0) {  // only the end moved: the swept part (+ the cap) is all that changes
    const std::vector<StrokePt> p = arc_points(s, std::min(s->a1, a1), std::max(s->a1, a1));
    float x0 = p[0].x, y0 = p[0].y, x1 = x0, y1 = y0;
    for (auto &q : p) x0 = std::min(x0, q.x), x1 = std::max(x1, q.x), y0 = std::min(y0, q.y), y1 = std::max(y1, q.y);
    const float m = s->width * 0.5f + 1;
    dirty.x1 = std::max<int32_t>(dirty.x1, (int32_t) std::floor(x0 - m));
    dirty.y1 = std::max<int32_t>(dirty.y1, (int32_t) std::floor(y0 - m));
    dirty.x2 = std::min<int32_t>(dirty.x2, (int32_t) std::ceil(x1 + m));
    dirty.y2 = std::min<int32_t>(dirty.y2, (int32_t) std::ceil(y1 + m));
  }
  s->a0 = a0;
  s->a1 = a1;
  clear(s->b, &dirty);
  arc_raster(s, &dirty);
  refresh(o, s, &dirty);
}

int stroke_bytes(lv_obj_t *o) {
  Stroke *s = stroke_of(o);
  return s && s->b ? (int) s->b->data_size : 0;
}

lv_draw_buf_t *stroke_segments(int w, int h, const StrokePt *ends, int n_segments, int width, Cap cap) {
  lv_draw_buf_t *b = mask(w, h);
  if (b)
    for (int i = 0; i < n_segments; i++) seg(b, ends[2 * i], ends[2 * i + 1], (float) width, cap, cap, nullptr);
  return b;
}

void stroke_draw(lv_layer_t *layer, lv_draw_buf_t *m, int x, int y, lv_color_t colour) {
  if (!m) return;
  lv_draw_image_dsc_t d;
  lv_draw_image_dsc_init(&d);
  d.src = m;
  d.recolor = colour;
  d.recolor_opa = LV_OPA_COVER;
  lv_area_t a = {x, y, x + (int32_t) m->header.w - 1, y + (int32_t) m->header.h - 1};
  lv_draw_image(layer, &d, &a);
}

}  // namespace fw
