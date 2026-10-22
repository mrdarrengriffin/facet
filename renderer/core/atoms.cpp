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

// ---------------------------------------------------------------- smooth stroke (facet-145, experiment)
extern "C" void lv_image_cache_drop(const void *src);  // LVGL 9 (misc/cache/instance/lv_image_cache.h, not in lvgl.h)
namespace {
void stroke_raster(lv_draw_buf_t *b, const StrokePt *p, int n, float width) {
  const int w = b->header.w, h = b->header.h, stride = b->header.stride;
  uint8_t *d = (uint8_t *) b->data;
  const float hw = width * 0.5f, r = hw + 0.5f;
  auto seg = [&](StrokePt a, StrokePt c) {
    const int x0 = std::max(0, (int) std::floor(std::min(a.x, c.x) - r)), x1 = std::min(w - 1, (int) std::ceil(std::max(a.x, c.x) + r));
    const int y0 = std::max(0, (int) std::floor(std::min(a.y, c.y) - r)), y1 = std::min(h - 1, (int) std::ceil(std::max(a.y, c.y) + r));
    const float dx = c.x - a.x, dy = c.y - a.y, ll = dx * dx + dy * dy;
    for (int y = y0; y <= y1; y++) {
      uint8_t *row = d + y * stride;
      for (int x = x0; x <= x1; x++) {
        const float px = x - a.x, py = y - a.y;
        float t = ll > 0 ? (px * dx + py * dy) / ll : 0;
        t = t < 0 ? 0 : t > 1 ? 1 : t;
        const float ex = px - t * dx, ey = py - t * dy;
        const float cov = r - std::sqrt(ex * ex + ey * ey);
        if (cov <= 0) continue;
        const uint8_t v = cov >= 1 ? 255 : (uint8_t) (cov * 255.f + 0.5f);
        if (v > row[x]) row[x] = v;
      }
    }
  };
  if (n == 1) seg(p[0], p[0]);
  for (int i = 1; i < n; i++) seg(p[i - 1], p[i]);
}
}  // namespace

lv_obj_t *stroke_path(lv_obj_t *parent, int w, int h, const StrokePt *pts, int n, int width, lv_color_t colour) {
  lv_draw_buf_t *b = lv_draw_buf_create(w, h, LV_COLOR_FORMAT_A8, LV_STRIDE_AUTO);
  lv_obj_t *im = lv_image_create(parent);
  if (!b) return im;
  std::memset(b->data, 0, b->data_size);
  stroke_raster(b, pts, n, (float) width);
  lv_image_set_src(im, b);
  lv_obj_set_style_image_recolor(im, colour, 0);
  lv_obj_set_style_image_recolor_opa(im, LV_OPA_COVER, 0);
  lv_obj_remove_flag(im, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_user_data(im, b);
  lv_obj_add_event_cb(im, [](lv_event_t *e) {
    auto *b = (lv_draw_buf_t *) lv_event_get_user_data(e);
    lv_image_cache_drop(b);
    lv_draw_buf_destroy(b);
  }, LV_EVENT_DELETE, b);
  return im;
}

lv_obj_t *stroke_fn(lv_obj_t *parent, int w, int h, StrokeFn f, void *user, int width, lv_color_t colour) {
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
  return stroke_path(parent, w, h, pts.data(), n, width, colour);
}

int stroke_bytes(lv_obj_t *s) {
  auto *b = (lv_draw_buf_t *) lv_obj_get_user_data(s);
  return b ? (int) b->data_size : 0;
}

}  // namespace fw
