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


// ---- Dials and rings (atoms.h): moved from the Dial (dial.cpp TickRing, round 2), extended for the other round parts
void nice_intervals(double span, double *major, double *minor, int max_labels) {
  const double raw = span / (max_labels - 1), e = std::pow(10.0, std::floor(std::log10(raw)));
  double m = 10 * e;
  for (double f : {1.0, 2.0, 5.0}) if (f * e >= raw - 1e-9) { m = f * e; break; }
  const double lead = m / std::pow(10.0, std::floor(std::log10(m) + 1e-9));
  *major = m;
  *minor = m / (std::lround(lead) == 2 ? 4 : 5);
}
int decimals_of(double v) {
  for (int d = 0; d < 2; d++) if (std::fabs(v * std::pow(10.0, d) - std::lround(v * std::pow(10.0, d))) < 1e-6) return d;
  return 2;
}

namespace {
void ring_draw(lv_event_t *e) {
  auto *o = (lv_obj_t *) lv_event_get_current_target(e);
  auto *r = (const TickRing *) lv_event_get_user_data(e);
  lv_layer_t *layer = lv_event_get_layer(e);
  lv_area_t a;
  lv_obj_get_coords(o, &a);
  const float cx = (a.x1 + a.x2 + 1) / 2.0f, cy = (a.y1 + a.y2 + 1) / 2.0f, R = lv_area_get_width(&a) / 2.0f;
  const lv_font_t *f = font(r->num_style);
  const ty::Style &st = style(r->num_style);
  // visual centre of a digit below the line top (cap height centred, the text-box rule)
  lv_font_glyph_dsc_t g;
  float dig_mid = f->line_height / 2.0f;
  if (lv_font_get_glyph_dsc(f, &g, '0', 0)) dig_mid = (f->line_height - f->base_line) - (g.ofs_y + g.box_h / 2.0f);
  const int sub = r->major > 0 ? (int) std::lround(r->major / r->minor) : 0;
  const long k0 = (long) std::ceil(r->lo / r->minor - 1e-6), k1 = (long) std::floor(r->hi / r->minor + 1e-6);
  const long m0 = sub > 0 ? (long) std::ceil(k0 / (double) sub - 1e-9) : 0;  // index of the first major (labels)
  for (long k = k0; k <= k1; k++) {
    const double v = k * r->minor;
    const bool maj = sub > 0 && ((k % sub) + sub) % sub == 0;
    const float off = (float) ((v - r->lo) / (r->hi - r->lo)) * r->range;
    if (r->range >= 360.0f && off >= 360.0f - 1e-3f) continue;  // a full turn: the last tick IS the first
    const float deg = r->start + off;
    const float rad = deg * 3.14159265f / 180.0f, c = std::cos(rad), s = std::sin(rad);
    const int len = maj ? r->major_len : r->minor_len;
    const bool lit = !std::isnan(r->lit) && v <= r->lit + 1e-9;
    const lv_color_t col = lit ? r->lit_c : maj ? r->major_c : r->minor_c;
    if (r->dots) {  // a round dot of `width` touching the edge
      const float dr = R - r->width / 2.0f;
      lv_draw_rect_dsc_t dd;
      lv_draw_rect_dsc_init(&dd);
      dd.bg_color = col;
      dd.radius = LV_RADIUS_CIRCLE;
      const int x = (int) std::lround(cx + c * dr - r->width / 2.0f), y = (int) std::lround(cy + s * dr - r->width / 2.0f);
      lv_area_t da = {x, y, x + r->width - 1, y + r->width - 1};
      lv_draw_rect(layer, &dd, &da);
    } else {
      // LVGL line points are whole numbers (no LV_USE_FLOAT) on the pixel EDGE grid: a horizontal line from x 10 to
      // 20 covers pixels 10..19, a width-2 line at y 240 covers rows 239..240 (measured, 2026-10-22). The audit bug:
      // the float ends were assigned to ints, which TRUNCATES (cos 90 deg = -4e-8 put the 6 o'clock tick a pixel off,
      // and the 3 o'clock tick a pixel short). Now every end is rounded from the edge-grid position, so the four axes
      // mirror exactly.
      auto pix = [](float v) { return (int32_t) std::lround(v); };
      lv_draw_line_dsc_t ld;
      lv_draw_line_dsc_init(&ld);
      ld.color = col;
      ld.width = r->width;
      // a slanted butt end's corners (and their anti-aliased fringe) reach past its end point: pull the outer end in
      // so they stay on the circle (the 18 % dial tick put a pixel outside the scale's box, cut off: audit 2026-10-22)
      const float Ro = std::sqrt(std::max(0.0f, (R - 0.5f) * (R - 0.5f) - r->width * r->width / 4.0f));
      ld.p1.x = pix(cx + c * Ro); ld.p1.y = pix(cy + s * Ro);
      ld.p2.x = pix(cx + c * (R - len)); ld.p2.y = pix(cy + s * (R - len));
      lv_draw_line(layer, &ld);
    }
    if (!maj || !r->numbers) continue;
    char b[16];
    const long mi = k / sub - m0;
    if (r->labels) {
      if (mi < 0 || mi >= r->n_labels || !r->labels[mi] || !*r->labels[mi]) continue;
      snprintf(b, sizeof b, "%s", r->labels[mi]);
    } else {
      snprintf(b, sizeof b, "%.*f", r->num_dec, std::fabs(v) < 1e-9 ? 0.0 : v);
    }
    lv_point_t sz;
    lv_text_get_size(&sz, b, f, st.letter_space, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    const float nr = R - r->major_len - r->num_gap - st.box / 2.0f;  // the number's box sits num_gap inside the tick
    const int x = (int) std::lround(cx + c * nr - sz.x / 2.0f), y = (int) std::lround(cy + s * nr - dig_mid);
    lv_draw_label_dsc_t td;
    lv_draw_label_dsc_init(&td);
    td.font = f;
    td.color = r->num_c;
    td.letter_space = st.letter_space;
    td.text = b;
    td.text_local = 1;
    lv_area_t ta = {x, y, x + sz.x, y + f->line_height};
    lv_draw_label(layer, &td, &ta);
  }
}
}  // namespace

lv_obj_t *tick_ring(lv_obj_t *parent, int size, const TickRing &spec) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_size(o, size, size);
  auto *r = new TickRing(spec);
  lv_obj_set_user_data(o, r);
  // audit (core/audit.h): which of 12 / 3 / 6 / 9 o'clock carry a tick, so their symmetry is checked on the pixels
  uint8_t axes = 0;
  const float AX[4] = {270, 0, 90, 180};
  for (int k = 0; k < 4; k++) {
    const float rel = std::fmod(AX[k] - spec.start + 720.0f, 360.0f);
    if (rel > spec.range + 1e-3f) continue;
    const double v = spec.lo + rel / spec.range * (spec.hi - spec.lo), q = v / spec.minor;
    if (std::fabs(q - std::lround(q)) < 1e-4) axes |= 1 << k;
  }
  qa::radial(o, axes, std::max(spec.minor_len, spec.major_len) ? std::max(spec.minor_len, spec.major_len) : spec.width);
  lv_obj_add_event_cb(o, ring_draw, LV_EVENT_DRAW_MAIN, r);
  lv_obj_add_event_cb(o, [](lv_event_t *e) { delete (TickRing *) lv_event_get_user_data(e); }, LV_EVENT_DELETE, r);
  return o;
}
TickRing *tick_ring_spec(lv_obj_t *ring) { return ring ? (TickRing *) lv_obj_get_user_data(ring) : nullptr; }
void tick_ring_changed(lv_obj_t *ring) { if (ring) lv_obj_invalidate(ring); }

lv_obj_t *value_arc(lv_obj_t *parent, int d, const ArcLook &k) {
  lv_obj_t *a = lv_arc_create(parent);
  lv_obj_remove_flag(a, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_size(a, d, d);
  lv_arc_set_rotation(a, 0);
  const float end = k.start + k.range;
  lv_arc_set_bg_angles(a, (lv_value_precise_t) k.start, (lv_value_precise_t) (end >= 360.0f ? end - 360.0f : end));
  lv_arc_set_range(a, 0, k.steps);
  lv_arc_set_value(a, 0);
  lv_obj_set_style_arc_width(a, k.track_w, LV_PART_MAIN);
  lv_obj_set_style_arc_color(a, k.track_c, LV_PART_MAIN);
  lv_obj_set_style_arc_opa(a, k.track_w ? LV_OPA_COVER : LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_arc_rounded(a, false, LV_PART_MAIN);
  lv_obj_set_style_arc_width(a, k.ind_w, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(a, k.ind_c, LV_PART_INDICATOR);
  lv_obj_set_style_arc_rounded(a, k.round, LV_PART_INDICATOR);
  if (k.knob) {
    lv_obj_set_style_bg_opa(a, LV_OPA_COVER, LV_PART_KNOB);
    lv_obj_set_style_bg_color(a, k.knob_c, LV_PART_KNOB);
    lv_obj_set_style_radius(a, LV_RADIUS_CIRCLE, LV_PART_KNOB);
    lv_obj_set_style_border_width(a, k.knob_ring, LV_PART_KNOB);
    lv_obj_set_style_border_color(a, k.knob_ring_c, LV_PART_KNOB);
    lv_obj_set_style_pad_all(a, (k.knob - k.ind_w) / 2, LV_PART_KNOB);
  } else {
    lv_obj_set_style_bg_opa(a, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_pad_all(a, 0, LV_PART_KNOB);
  }
  return a;
}

float ring_frac(lv_obj_t *o, lv_point_t pt, float start, float range, float *dist, float held) {
  lv_area_t a;
  lv_obj_get_coords(o, &a);
  const float dx = pt.x - (a.x1 + a.x2 + 1) / 2.0f, dy = pt.y - (a.y1 + a.y2 + 1) / 2.0f;
  if (dist) *dist = std::sqrt(dx * dx + dy * dy);
  const float rel = std::fmod(std::atan2(dy, dx) * 180.0f / 3.14159265f - start + 720.0f, 360.0f);
  float f;
  if (rel > range) f = held >= 0 ? (held > 0.5f ? 1.0f : 0.0f) : (rel > range + (360.0f - range) / 2 ? 0.0f : 1.0f);
  else f = rel / range;
  if (held >= 0 && range < 360.0f && std::fabs(f - held) > 0.5f) f = held > 0.5f ? 1.0f : 0.0f;  // never across the gap
  return f;
}

}  // namespace fw
