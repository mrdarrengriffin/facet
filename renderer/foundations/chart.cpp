// Foundations > Charts: tokens/chart.json (via the generated chart_gen.h) = the shared groundwork of every chart
// component (line / history graph, sparkline, bars, energy, gauges). Matches the approved Astro Facet charts
// (packages/ui/src/charts.ts): solid token fills, the series line + end dot, rounded data end of bars, dotted now
// marker, axis row, legend swatches, donut / gauge arcs; snapped to the 4 px grid.
// How it draws (LVGL, fast on both panels): the series line and the area under it are A8 masks (the smooth stroke atom,
// core/atoms.h stroke_segments, plus an area raster here) shown as recoloured lv_images: a line through float points
// is the one place lv_line visibly steps. Grid lines, bars, dots, cursor and labels are plain LVGL objects.
// Motion (chart.json "motion", shown on the Motion page section "Charts"): draw-in = a clip that grows (only the newly
// revealed strip repaints), new value = the masks slide one step left (one raster per value), morph = re-raster per
// frame, bars = heights, gauge = the arc atom's set_arc, ping = one small ring. Lite (480 default) + reduced per item.
// Sections = page variants (FW_CHART_SECTIONS); playground loop = one chart animating over and over, full screen (the
// frame-rate bench on the panels: GET /fw/perf while it runs). Status (WIP/Done) = this file + tokens/chart.json.
#include <cmath>
#include <cstdio>
#include "fw.h"
#include "fx.h"
#include "chart_gen.h"
#include "chart.h"  // the library this page shows (core/chart.h): Line, Bars, Gauge, legend, axis, big number
extern "C" void lv_image_cache_drop(const void *src);  // LVGL 9 (misc/cache/instance/lv_image_cache.h, not in lvgl.h)

namespace fw::chart {
void motion_section(lv_obj_t *list, bool reduced, int lite);  // foundations/motion.cpp, section "charts"
}

namespace {
using namespace fw;

constexpr float W = 480;  // doc page width in dp (= the screen width)
int g_lite = -1;           // lite motion: -1 = auto (the 480 build), 0 off, 1 on (Motion page playground)
bool lite() { return g_lite < 0 ?
#ifdef FW_1X
  true
#else
  false
#endif
  : g_lite == 1; }

lv_obj_t *box(lv_obj_t *parent) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, (lv_obj_flag_t) (LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE));
  return o;
}
lv_obj_t *column(lv_obj_t *parent, int gap, int w = LV_PCT(100)) {
  lv_obj_t *c = box(parent);
  lv_obj_set_size(c, w, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(c, gap, 0);
  return c;
}
lv_obj_t *row(lv_obj_t *parent, int gap, int w = LV_PCT(100)) {
  lv_obj_t *r = box(parent);
  lv_obj_set_size(r, w, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(r, gap, 0);
  return r;
}
void fill(lv_obj_t *o, lv_color_t c, lv_opa_t opa = LV_OPA_COVER) {
  lv_obj_set_style_bg_color(o, c, 0);
  lv_obj_set_style_bg_opa(o, opa, 0);
}
void outline(lv_obj_t *o, lv_color_t c, int w) {
  lv_obj_set_style_border_width(o, w, 0);
  lv_obj_set_style_border_color(o, c, 0);
}
lv_obj_t *rect(lv_obj_t *parent, int x, int y, int w, int h, lv_color_t c) {
  lv_obj_t *o = box(parent);
  lv_obj_set_pos(o, x, y);
  lv_obj_set_size(o, w, h);
  fill(o, c);
  return o;
}
int inner_w() { return px(W) - 2 * sp::s7; }
lv_obj_t *entry(lv_obj_t *list, int gap = sp::s3) {
  lv_obj_t *e = column(list, gap);
  lv_obj_set_style_pad_ver(e, sp::s5, 0);
  lv_obj_set_style_border_side(e, LV_BORDER_SIDE_BOTTOM, 0);
  outline(e, T->line, stroke::hair);
  return e;
}
// a card: surface + 1 px line, padding s5 (the border sits inside it, so content stays on the grid)
lv_obj_t *card(lv_obj_t *parent, const Theme &t, int w, int gap = sp::s3) {
  lv_obj_t *c = column(parent, gap, w);
  fill(c, t.surface);
  outline(c, t.line, stroke::hair);
  lv_obj_set_style_pad_all(c, sp::s5 - stroke::hair, 0);
  return c;
}
int card_in(int w) { return w - 2 * sp::s5; }  // content width of a card
lv_obj_t *page(lv_obj_t *parent) {
  lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_t *l = column(parent, 0, px(W));
  lv_obj_set_style_pad_hor(l, sp::s7, 0);
  return l;
}
void on_tap(lv_obj_t *o, lv_event_cb_t cb, void *user) {
  lv_obj_add_flag(o, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(o, cb, LV_EVENT_CLICKED, user);
}
lv_obj_t *hint(lv_obj_t *parent, const Theme &t, const char *s) { return text(parent, ty::c_meta_sm, s, t.text_dim); }

// one chart animation: the chart.json motion's duration + easing; reduced / lite collapse it (0 ms: exec(to), done)
bool collapsed(lv_obj_t *o, ch::MId m) {
  const ch::Motion &k = ch::MOTION[m];
  if (mo::reduced(o) && k.reduced != mo::R_keep) return true;
  return lite() && k.lite == ch::L_instant;
}
void run(lv_obj_t *var, ch::MId m, lv_anim_exec_xcb_t exec, int32_t from, int32_t to, lv_anim_completed_cb_t done,
         void *user, uint32_t ms = 0) {
  lv_anim_delete(var, exec);
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, var);
  lv_anim_set_exec_cb(&a, exec);
  lv_anim_set_values(&a, from, to);
  lv_anim_set_duration(&a, ms ? ms : ch::MOTION[m].ms);
  lv_anim_set_path_cb(&a, ch::path(m));
  lv_anim_set_user_data(&a, user);
  if (done) lv_anim_set_completed_cb(&a, done);
  if (collapsed(var, m)) {
    exec(var, to);
    if (done) done(&a);
    return;
  }
  lv_anim_start(&a);
}
int32_t ease(ch::MId m, int32_t t) {  // eased 0..1024 at t of 0..1024 (the same callback LVGL runs)
  lv_anim_t a;
  lv_anim_init(&a);
  a.act_time = t, a.duration = 1024, a.start_value = 0, a.end_value = 1024;
  return ch::path(m)(&a);
}
void set_opa(void *o, int32_t v) { lv_obj_set_style_opa((lv_obj_t *) o, (lv_opa_t) v, 0); }

// contrast of two colours (WCAG, on the RGB565 values the panel shows), for the colour table
float lum(lv_color_t c) {
  const uint16_t v = lv_color_to_u16(c);
  const int r5 = v >> 11, g6 = (v >> 5) & 63, b5 = v & 31;
  const float ch3[3] = {((r5 << 3) | (r5 >> 2)) / 255.f, ((g6 << 2) | (g6 >> 4)) / 255.f, ((b5 << 3) | (b5 >> 2)) / 255.f};
  float l[3];
  for (int i = 0; i < 3; i++) l[i] = ch3[i] <= 0.04045f ? ch3[i] / 12.92f : std::pow((ch3[i] + 0.055f) / 1.055f, 2.4f);
  return 0.2126f * l[0] + 0.7152f * l[1] + 0.0722f * l[2];
}
float ratio(lv_color_t a, lv_color_t b) {
  const float x = lum(a), y = lum(b);
  return (std::max(x, y) + 0.05f) / (std::min(x, y) + 0.05f);
}

// ---------------------------------------------------------------- demo data (nothing house-specific: shapes only)
float temp_at(int i) { return 16.5f + 3.6f * std::sin((i - 9) * 0.27f) + 0.9f * std::sin(i * 1.1f); }  // a day, hourly
float power_at(int i) { return std::max(0.f, 0.35f + 0.9f * std::sin((i - 6) * 0.26f) * (i > 5 && i < 22) + 0.25f * std::sin(i * 1.7f) * (i % 3 == 0)); }
std::vector<float> day(int n, int seed = 0) {
  std::vector<float> v(n);
  for (int i = 0; i < n; i++) v[i] = temp_at(i + seed);
  return v;
}

}  // namespace

namespace fw::chart {
// ---------------------------------------------------------------- the line chart (struct Line: core/chart.h)
int gt(const Line *L) { return std::min(fx::size("chart:plot-top"), L->h / 4); }
int gb(const Line *L) { return L->h - std::min(fx::size("chart:plot-bottom"), L->h / 8); }
int plot_top(const Line *L) { return gt(L); }
int plot_bottom(const Line *L) { return gb(L); }
float pad_r(const Line *L) { return L->dot_on ? fx::size("chart:dot") / 2.f + 1 : L->weight / 2.f + 1; }
float slot(const Line *L, int n) { return n > 1 ? (L->w - pad_r(L)) / (n - 1) : 0; }
float X(const Line *L, int i, int n) { return i * slot(L, n); }
float Y(const Line *L, float v) { return gt(L) + (1 - (v - L->lo) / (L->hi - L->lo)) * (gb(L) - gt(L)); }
int last_ok(const std::vector<float> &v) { for (int i = (int) v.size() - 1; i >= 0; i--) if (std::isfinite(v[i])) return i; return -1; }

lv_draw_buf_t *empty_mask(int w, int h) {
  lv_draw_buf_t *b = lv_draw_buf_create(std::max(1, w), std::max(1, h), LV_COLOR_FORMAT_A8, LV_STRIDE_AUTO);
  if (b) memset(b->data, 0, b->data_size);
  return b;
}
// the series line as one A8 mask: segments between finite neighbours (a gap = NAN breaks it), dashed = compare
lv_draw_buf_t *raster_line(const Line *L, const float *v, int n) {
  std::vector<StrokePt> e;
  auto seg = [&](StrokePt a, StrokePt b) { e.push_back(a), e.push_back(b); };
  float run = 0;  // dash phase along the length
  const float on = fx::size("chart:compare-dash"), off = fx::size("chart:compare-gap");
  for (int i = 0; i < n; i++) {
    if (!std::isfinite(v[i])) continue;
    const StrokePt p = {X(L, i, L->v.size()), Y(L, v[i])};
    const bool prev = i > 0 && std::isfinite(v[i - 1]), next = i + 1 < n && std::isfinite(v[i + 1]);
    if (!prev && !next) seg(p, p);  // a lone reading = a dot of the line's width
    if (!next) continue;
    const StrokePt q = {X(L, i + 1, L->v.size()), Y(L, v[i + 1])};
    if (!L->dashed) { seg(p, q); continue; }
    const float len = std::hypot(q.x - p.x, q.y - p.y);
    for (float s = 0; s < len;) {  // walk the dash pattern along this segment
      const float ph = std::fmod(run + s, on + off), step = ph < on ? std::min(on - ph, len - s) : std::min(on + off - ph, len - s);
      if (ph < on) seg({p.x + (q.x - p.x) * s / len, p.y + (q.y - p.y) * s / len}, {p.x + (q.x - p.x) * (s + step) / len, p.y + (q.y - p.y) * (s + step) / len});
      s += std::max(step, 0.01f);
    }
    run += len;
  }
  if (e.empty()) return empty_mask(L->tw, L->h);
  return stroke_segments(L->tw, L->h, e.data(), (int) e.size() / 2, L->weight, Cap::round);
}
// the area under the line down to the baseline (A8, top edge anti-aliased by coverage)
lv_draw_buf_t *raster_area(const Line *L, const float *v, int n) {
  lv_draw_buf_t *b = empty_mask(L->tw, L->h);
  if (!b || n < 2) return b;
  const float s = slot(L, L->v.size());
  const int bot = gb(L);
  for (int x = 0; x < L->tw; x++) {
    const float xc = x + 0.5f;
    const int i = (int) std::floor(xc / s);
    if (i < 0 || i + 1 >= n || !std::isfinite(v[i]) || !std::isfinite(v[i + 1])) continue;
    const float y = Y(L, v[i]) + (Y(L, v[i + 1]) - Y(L, v[i])) * (xc - i * s) / s;
    for (int r = std::max(0, (int) std::floor(y)); r < bot; r++) {
      const float c = std::min(1.f, r + 1 - y);
      ((uint8_t *) b->data)[r * b->header.stride + x] = (uint8_t) std::lround(255 * c);
    }
  }
  return b;
}
lv_obj_t *mask_image(lv_obj_t *parent, lv_color_t c, lv_opa_t opa = LV_OPA_COVER) {
  lv_obj_t *im = lv_image_create(parent);
  lv_obj_remove_flag(im, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_style_image_recolor(im, c, 0);
  lv_obj_set_style_image_recolor_opa(im, LV_OPA_COVER, 0);
  lv_obj_set_style_image_opa(im, opa, 0);
  return im;
}
void set_mask(lv_obj_t *im, lv_draw_buf_t *&slotb, lv_draw_buf_t *nb) {
  lv_image_set_src(im, nullptr);
  if (slotb) { lv_image_cache_drop(slotb); lv_draw_buf_destroy(slotb); }
  slotb = nb;
  if (nb) lv_image_set_src(im, nb);
}
void raster(Line *L, const float *v, int n) {
  set_mask(L->stroke, L->lb, raster_line(L, v, n));
  if (L->area) set_mask(L->area, L->ab, raster_area(L, v, n));
}
lv_color_t ink(const Line *L) { return L->unavailable ? ch::ink::unavailable(*L->t) : L->has_colour ? L->colour_override : ch::series(*L->t, L->series); }
void place_dot(Line *L, float x, float y) {
  if (!L->dot) return;
  lv_obj_set_pos(L->dot, (int) std::lround(x - fx::size("chart:dot") / 2.f), (int) std::lround(y - fx::size("chart:dot") / 2.f));
}
void dot_rest(Line *L) {
  const int i = last_ok(L->v);
  if (!L->dot) return;
  if (i < 0) { lv_obj_add_flag(L->dot, LV_OBJ_FLAG_HIDDEN); return; }
  lv_obj_remove_flag(L->dot, LV_OBJ_FLAG_HIDDEN);
  place_dot(L, X(L, i, L->v.size()), Y(L, L->v[i]));
}
float y_at(const Line *L, float x) {  // the line's y at x (for the pen tip during the draw-in)
  const int n = L->v.size();
  const float s = slot(L, n);
  int i = std::clamp((int) std::floor(x / s), 0, n - 1);
  while (i > 0 && !std::isfinite(L->v[i])) i--;
  const int j = std::min(n - 1, i + 1);
  if (!std::isfinite(L->v[i])) return Y(L, L->lo);
  if (!std::isfinite(L->v[j]) || j == i) return Y(L, L->v[i]);
  return Y(L, L->v[i]) + (Y(L, L->v[j]) - Y(L, L->v[i])) * std::clamp((x - i * s) / s, 0.f, 1.f);
}

// ping: one ring out of the end dot (small: only the ring's box repaints)
void ping_exec(void *o, int32_t p) {
  auto *L = (Line *) lv_obj_get_user_data((lv_obj_t *) o);
  const int d = fx::size("chart:dot") + (fx::size("chart:ping") - fx::size("chart:dot")) * p / 1024;
  lv_obj_set_size(L->ping, d, d);
  lv_obj_set_pos(L->ping, lv_obj_get_x(L->dot) + fx::size("chart:dot") / 2 - d / 2, lv_obj_get_y(L->dot) + fx::size("chart:dot") / 2 - d / 2);
  lv_obj_set_style_opa(L->ping, (lv_opa_t) (255 - 255 * p / 1024), 0);
}
void ping(Line *L) {
  if (!L->dot || !L->ping || L->unavailable || lv_obj_has_flag(L->dot, LV_OBJ_FLAG_HIDDEN)) return;
  if (collapsed(L->ping, ch::M_ping)) return;  // reduced: no ping at all
  lv_obj_remove_flag(L->ping, LV_OBJ_FLAG_HIDDEN);
  run(L->ping, ch::M_ping, ping_exec, 0, 1024, [](lv_anim_t *a) {
    auto *L = (Line *) lv_anim_get_user_data(a);
    lv_obj_add_flag(L->ping, LV_OBJ_FLAG_HIDDEN);
  }, L);
}

// draw-in: the track's children are clipped at `reveal` px (set at the end of the track's own draw, so LVGL clips its
// children there), and only the strip between the old and new reveal is invalidated each frame
void reveal_strip(Line *L, int a, int b) {
  lv_area_t c, r;
  lv_obj_get_coords(L->root, &c);
  r = {c.x1 + std::min(a, b) - L->weight - 1, c.y1, c.x1 + std::max(a, b) + L->weight + 1, c.y2};
  lv_obj_invalidate_area(L->root, &r);
}
void draw_exec(void *o, int32_t r) {
  auto *L = (Line *) lv_obj_get_user_data((lv_obj_t *) o);
  const int old = L->reveal < 0 ? 0 : L->reveal;
  L->reveal = r;
  reveal_strip(L, old, r);
  const int i = last_ok(L->v);
  if (L->dot && i >= 0) {
    const float x = std::min((float) r, X(L, i, L->v.size()));
    lv_obj_remove_flag(L->dot, LV_OBJ_FLAG_HIDDEN);
    place_dot(L, x, y_at(L, x));
  }
}
void draw_in(Line *L) {
  if (L->v.empty()) return;
  lv_anim_delete(L->root, draw_exec);
  const bool reduced = mo::reduced(L->root) && ch::MOTION[ch::M_draw].reduced != mo::R_keep;
  if (reduced) {  // reduced: a short fade of the whole line (opacity only, nothing moves)
    L->reveal = -1;
    dot_rest(L);
    lv_obj_set_style_opa(L->track, 0, 0);
    fw::Anim a{set_opa, 0, 255, mo::OPA};
    fw::animate(L->track, mo::fade, a);
    return;
  }
  L->reveal = 0;
  lv_obj_invalidate(L->root);
  run(L->root, ch::M_draw, draw_exec, 0, L->w, [](lv_anim_t *a) {
    auto *L = (Line *) lv_anim_get_user_data(a);
    L->reveal = -1;
    lv_obj_invalidate(L->root);
    dot_rest(L);
    ping(L);
  }, L);
}

// new value: raster old + new once (the new point one step past the right edge), slide the track left by one step;
// the end dot stays at the right edge and moves up / down to the new value on the same eased progress, so it looks
// like it draws the line as it goes; where it lands, it pings (Darren 2026-10-22: before, on the 480 the line jumped
// left at once while only the dot glided; the slide is now kept on the 480 too). Reduced: they jump, no ping.
void shift_exec(void *o, int32_t p) {
  auto *L = (Line *) lv_obj_get_user_data((lv_obj_t *) o);
  lv_obj_set_style_translate_x(L->track, -(int32_t) std::lround(slot(L, L->v.size()) * p / 1024.f), 0);
  if (L->dot) place_dot(L, X(L, L->v.size() - 1, L->v.size()), L->y0 + (L->y1 - L->y0) * p / 1024.f);
}
void shift_land(Line *L) {  // the slide's end state: the new set drawn in place, the track back at 0
  L->shifting = false;
  L->v.erase(L->v.begin());
  L->v.push_back(L->to.back());
  raster(L, L->v.data(), L->v.size());
  lv_obj_set_style_translate_x(L->track, 0, 0);
  dot_rest(L);
}
void push(Line *L, float nv) {
  if (L->v.size() < 2) return;
  lv_anim_delete(L->root, shift_exec);
  if (L->shifting) shift_land(L);  // a reading during the slide: land the last one first (no ping), then slide on
  const int n = L->v.size();
  L->y0 = std::isfinite(L->v[n - 1]) ? Y(L, L->v[n - 1]) : Y(L, nv);
  L->y1 = Y(L, nv);
  L->to = L->v;
  L->to.push_back(nv);
  L->reveal = -1;
  L->shifting = true;
  if (collapsed(L->root, ch::M_shift)) {
    shift_land(L);  // reduced (or a lite "instant"): line and dot jump together; ping() stays quiet with reduced motion
    ping(L);
    return;
  }
  raster(L, L->to.data(), n + 1);
  run(L->root, ch::M_shift, shift_exec, 0, 1024, [](lv_anim_t *a) {
    auto *L = (Line *) lv_anim_get_user_data(a);
    shift_land(L);
    ping(L);
  }, L);
}
// morph: the whole set changes; every point moves (re-raster each frame). Lite / reduced: jump.
void morph_exec(void *o, int32_t p) {
  auto *L = (Line *) lv_obj_get_user_data((lv_obj_t *) o);
  for (size_t i = 0; i < L->v.size(); i++)
    L->v[i] = std::isfinite(L->from[i]) && std::isfinite(L->to[i]) ? L->from[i] + (L->to[i] - L->from[i]) * p / 1024.f : (p < 512 ? L->from[i] : L->to[i]);
  raster(L, L->v.data(), L->v.size());
  dot_rest(L);
}
void morph(Line *L, const std::vector<float> &nv) {
  if (nv.size() != L->v.size()) return;
  L->from = L->v, L->to = nv;
  run(L->root, ch::M_morph, morph_exec, 0, 1024, [](lv_anim_t *a) { ping((Line *) lv_anim_get_user_data(a)); }, L);
}

// scrub: press and drag across the chart; the cursor, a dot with a surface ring and a readout follow point by point
// (a tick per point crossed, the slider rule); finger up = they fade out
// the scrub tick's pitch follows the line (Darren 2026-10-22): the line's height at that step, bottom to top of the plot,
// mapped into chart.json sound.scrub-pitch (a share of sound.json pitch); a gap takes the nearest reading's height
float scrub_pitch(void *u, int step) {
  const Line *L = (const Line *) u;
  const int n = L->v.size();
  float v = NAN;
  for (int d = 0; d < n && !std::isfinite(v); d++) {
    if (step - d >= 0 && std::isfinite(L->v[step - d])) v = L->v[step - d];
    else if (step + d < n && std::isfinite(L->v[step + d])) v = L->v[step + d];
  }
  const float h = std::isfinite(v) ? std::clamp((gb(L) - Y(L, v)) / std::max(1.f, (float) (gb(L) - gt(L))), 0.f, 1.f) : 0.5f;
  return ch::SCRUB_PITCH_LOW + (ch::SCRUB_PITCH_HIGH - ch::SCRUB_PITCH_LOW) * h;
}
void scrub_show(Line *L, int i) {
  const int n = L->v.size();
  if (i < 0 || i >= n || !std::isfinite(L->v[i])) return;
  const int x = (int) std::lround(X(L, i, n)), y = (int) std::lround(Y(L, L->v[i]));
  lv_obj_set_x(L->cur, x);
  const int sd = lv_obj_get_width(L->sdot);
  lv_obj_set_pos(L->sdot, x - sd / 2, y - sd / 2);
  char b[48];
  const int hr = (L->first_hour + i) % 24;
  snprintf(b, sizeof b, "%.1f%s  %02d:00", L->v[i], L->unit, hr);
  lv_label_set_text(L->read, b);
  lv_obj_update_layout(L->read);
  const int rw = lv_obj_get_width(L->read);
  lv_obj_set_x(L->read, std::clamp(x - rw / 2, 0, L->w - rw));
  if (L->scrub >= 0 && L->scrub != i && !L->quiet_ticks) step_ticks(L->root, L->scrub, i, n - 1, scrub_pitch, L);
  L->scrub = i;
}
void scrub_vis(Line *L, bool in) {
  lv_obj_t *parts[3] = {L->cur, L->sdot, L->read};
  for (lv_obj_t *o : parts) {
    if (in) lv_obj_remove_flag(o, LV_OBJ_FLAG_HIDDEN);
    fw::Anim a{set_opa, (int32_t) lv_obj_get_style_opa(o, LV_PART_MAIN), in ? 255 : 0, mo::OPA};
    fw::animate(o, in ? mo::press : mo::fade, a);
  }
}
int nearest(Line *L) {
  lv_point_t p;
  lv_indev_get_point(lv_indev_active(), &p);
  lv_area_t c;
  lv_obj_get_coords(L->root, &c);
  const int n = L->v.size();
  int i = std::clamp((int) std::lround((p.x - c.x1) / std::max(0.01f, slot(L, n))), 0, n - 1);
  for (int d = 0; d < n; d++) {  // nearest reading (skip gaps)
    if (i - d >= 0 && std::isfinite(L->v[i - d])) return i - d;
    if (i + d < n && std::isfinite(L->v[i + d])) return i + d;
  }
  return -1;
}
void scrub_cb(lv_event_t *e) {
  auto *L = (Line *) lv_event_get_user_data(e);
  const lv_event_code_t c = lv_event_get_code(e);
  if (c == LV_EVENT_PRESSED) { L->scrub = -1; scrub_show(L, nearest(L)); scrub_vis(L, true); }
  else if (c == LV_EVENT_PRESSING) scrub_show(L, nearest(L));
  else if (c == LV_EVENT_RELEASED || c == LV_EVENT_PRESS_LOST) {
    if (L->scrub >= 0) {
      char b[48];
      snprintf(b, sizeof b, "%d %.1f", L->scrub, L->v[L->scrub]);
      emit(L->root, "scrub", b);
    }
    scrub_vis(L, false);
  }
}

void dotted_v(lv_obj_t *parent, const Theme &t, int x, int y0, int y1) {  // the dotted now marker (vertical: crisp)
  for (int y = y0; y < y1; y += fx::size("chart:dash") + fx::size("chart:dash-gap")) rect(parent, x, y, fx::size("chart:grid"), std::min(fx::size("chart:dash"), y1 - y), ch::ink::cursor(t));
}
void grid_lines(lv_obj_t *root, const Theme &t, int w, int top, int bot) {
  for (int k = 0; k < ch::GRID_LINES; k++) {
    const int y = top + (bot - top) * k / (ch::GRID_LINES - 1);
    rect(root, 0, y, w, fx::size("chart:grid"), k == ch::GRID_LINES - 1 ? ch::ink::baseline(t) : ch::ink::grid(t));
  }
}
Line *line_chart(lv_obj_t *parent, const Theme &t, const LineOpts &o) {
  auto *L = new Line();
  L->t = &t, L->w = o.w, L->h = o.h, L->v = o.v, L->lo = o.lo, L->hi = o.hi, L->series = o.series, L->weight = o.weight;
  L->area_on = o.area, L->dot_on = o.dot, L->dashed = o.dashed, L->unavailable = o.unavailable, L->first_hour = o.first_hour;
  L->has_colour = o.has_colour, L->colour_override = o.colour;
  L->root = box(parent);
  lv_obj_set_size(L->root, o.w, o.h);
  lv_obj_set_user_data(L->root, L);
  lv_obj_add_event_cb(L->root, [](lv_event_t *e) {
    auto *L = (Line *) lv_event_get_user_data(e);
    if (L->lb) { lv_image_cache_drop(L->lb); lv_draw_buf_destroy(L->lb); }
    if (L->ab) { lv_image_cache_drop(L->ab); lv_draw_buf_destroy(L->ab); }
    delete L;
  }, LV_EVENT_DELETE, L);
  if (o.grid) grid_lines(L->root, t, o.w, gt(L), gb(L));
  if (o.baseline_dots)  // the docs sparkline: a dotted baseline on the box's bottom edge (docs y = h - 0.5), under the
    // lowest point, no grid (2026-10-22 reference check: it sat on the plot bottom and the line ran through its dots)
    if (o.ink) {  // token inks: the dashes through the Pen, stroke:hair thick in fg:line (a theme / tune repaints them)
      lv_obj_t *b = fx::strip(L->root, [](const fx::Pen &p) {
        const int t = fx::size("stroke:hair"), L0 = -(p.w / 2), y = p.h - p.h / 2 - t;
        for (int x = 0; x < p.w; x += fx::size("chart:dash") + fx::size("chart:dash-gap")) p.rect(L0 + x, y, std::min(fx::size("chart:dash"), p.w - x), t, "fg:line");
      }, "free");
      lv_obj_set_size(b, o.w, o.h);
    } else
      for (int x = 0; x < o.w; x += fx::size("chart:dash") + fx::size("chart:dash-gap")) rect(L->root, x, o.h - fx::size("chart:grid"), std::min(fx::size("chart:dash"), o.w - x), fx::size("chart:grid"), ch::ink::grid(t));
  const int n = std::max<int>(2, L->v.size());
  L->tw = L->v.size() < 2 ? o.w : o.w + (int) std::ceil(slot(L, n)) + L->weight + 2;  // room for one more point (shift)
  L->track = box(L->root);
  lv_obj_set_size(L->track, L->tw, o.h);
  fw::qa::clips(L->root, "the line's track runs one slot past the box: a new reading slides in from there");
  lv_obj_add_event_cb(L->track, [](lv_event_t *e) {
    auto *L = (Line *) lv_event_get_user_data(e);
    if (L->reveal < 0) return;
    lv_layer_t *ly = lv_event_get_layer(e);
    lv_area_t c;
    lv_obj_get_coords(L->root, &c);
    ly->_clip_area.x2 = std::min(ly->_clip_area.x2, c.x1 + L->reveal);
  }, LV_EVENT_DRAW_MAIN_END, L);
  if (o.area && !o.unavailable) {
    const lv_color_t sc = ink(L);
    L->area = mask_image(L->track, sc, (lv_opa_t) std::lround(ch::AREA_OPA * 255));
  }
  L->stroke = mask_image(L->track, ink(L));
  raster(L, L->v.data(), L->v.size());
  if (o.now >= 0) dotted_v(L->root, t, (int) std::lround(X(L, o.now, L->v.size())), 0, gb(L));
  if (o.dot && !o.unavailable) {
    L->ping = box(L->root);
    lv_obj_set_style_radius(L->ping, LV_RADIUS_CIRCLE, 0);
    outline(L->ping, ink(L), fx::size("stroke:edge"));
    lv_obj_add_flag(L->ping, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_user_data(L->ping, L);
    L->dot = box(L->root);
    fw::qa::free(L->dot, "the end dot sits on the latest value");
    fw::qa::free(L->ping, "the ping rings out from the end dot");
    lv_obj_set_size(L->dot, fx::size("chart:dot"), fx::size("chart:dot"));
    lv_obj_set_style_radius(L->dot, LV_RADIUS_CIRCLE, 0);
    fill(L->dot, ink(L));
    dot_rest(L);
  }
  if (o.scrub) {
    L->cur = rect(L->root, 0, 0, fx::size("chart:grid"), gb(L), ch::ink::cursor(t));
    const int sd = fx::size("chart:dot") + 2 * fx::size("chart:dot-ring");
    L->sdot = box(L->root);
    fw::qa::free(L->sdot, "the scrub dot follows the finger");
    fw::qa::free(L->cur, "the scrub cursor follows the finger");
    lv_obj_set_size(L->sdot, sd, sd);
    lv_obj_set_style_radius(L->sdot, LV_RADIUS_CIRCLE, 0);
    fill(L->sdot, ch::ink::select(t));
    outline(L->sdot, t.surface, fx::size("chart:dot-ring"));
    L->read = text(L->root, ty::c_meta, "", t.text);
    fill(L->read, t.surface);
    lv_obj_set_style_pad_hor(L->read, fx::size("space:1"), 0);
    lv_obj_t *parts[3] = {L->cur, L->sdot, L->read};
    for (lv_obj_t *p : parts) lv_obj_set_style_opa(p, 0, 0), lv_obj_add_flag(p, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(L->root, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(L->root, (lv_obj_flag_t) (LV_OBJ_FLAG_SCROLL_CHAIN_HOR | LV_OBJ_FLAG_SCROLL_CHAIN_VER));
    lv_obj_add_flag(L->root, LV_OBJ_FLAG_PRESS_LOCK);
    for (lv_event_code_t c : {LV_EVENT_PRESSED, LV_EVENT_PRESSING, LV_EVENT_RELEASED, LV_EVENT_PRESS_LOST})
      lv_obj_add_event_cb(L->root, scrub_cb, c, L);
  }
  if (o.ink) {  // token inks: shared styles (a local colour would outrank them)
    const std::string k = o.ink;
    lv_obj_remove_local_style_prop(L->stroke, LV_STYLE_IMAGE_RECOLOR, 0);
    fx::use(L->stroke, ("ink:" + k).c_str());
    if (L->dot) { lv_obj_remove_local_style_prop(L->dot, LV_STYLE_BG_COLOR, 0); fx::use(L->dot, ("bg:" + k).c_str()); }
    if (L->ping) { lv_obj_remove_local_style_prop(L->ping, LV_STYLE_BORDER_COLOR, 0); fx::use(L->ping, ("border:" + k).c_str()); }
  }
  return L;
}

Line *line_chart(lv_obj_t *parent, const LineOpts &o) { return line_chart(parent, *T, o); }

// ---------------------------------------------------------------- bars
int bar_h(const Bars *B, float v) {
  if (!std::isfinite(v) || v <= 0) return 0;
  return std::max(ch::bar_min, (int) std::lround(v / B->max * (B->bot - B->top)));
}
int bar_x(const Bars *B, int i) {
  const int g = std::max(1, B->group), gg = g > 1 ? B->group_gap : B->gap;  // between groups (no groups: the bar gap)
  const int sp = B->spread && B->n > 1 ? (int) ((int64_t) i * B->spread / (B->n - 1)) : 0;  // fill: the gaps share the rest
  return (i / g) * (g * (B->bw + B->gap) - B->gap + gg) + (i % g) * (B->bw + B->gap) + sp;
}
void bar_place(Bars *B, int i, int hp) {
  lv_obj_t *o = B->b[i];
  if (hp <= 0) { lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN); return; }
  lv_obj_remove_flag(o, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_pos(o, bar_x(B, i), B->bot - hp);
  lv_obj_set_size(o, B->bw, hp + ch::bar_radius);  // the radius hangs below the plot box: clipped = a square foot
}
void bar_colour(Bars *B, int i) {
  lv_obj_t *o = B->b[i];
  const Theme &t = *B->t;
  const lv_color_t c = i == B->sel ? ch::ink::select(t) : (size_t) i < B->col.size() ? B->col[i] : B->kind[i] == 1 ? ch::ink::highlight(t) : B->kind[i] == 2 ? ch::series(t, 3) : ch::series(t, B->series);
  if (B->kind[i] == 2 && i != B->sel) { fill(o, c, LV_OPA_TRANSP); outline(o, c, stroke::hair); }
  else { fill(o, c); outline(o, c, 0); }
}
void bars_exec(void *o, int32_t ms) {  // one animation for every bar: each starts a stagger later
  auto *B = (Bars *) lv_obj_get_user_data((lv_obj_t *) o);
  const ch::Motion &k = ch::MOTION[ch::M_bars];
  const bool together = B->from.size() == B->v.size() && !B->from.empty() && B->from[0] >= 0;
  // many bars: one invalidation of the plot instead of one per bar (past LVGL's 32 dirty areas a frame becomes a full
  // screen redraw: Moon by day's 55 bars repainted 62 % of the Guition per frame, 2026-10-22)
  // (the moves land in the layout pass, so that pass runs inside the window too; anything else waiting for layout goes
  // first, with its own invalidations, so nothing outside the plot is missed)
  lv_display_t *disp = lv_obj_get_display(B->root);
  const bool batch = B->n > 16;
  if (batch) {
    lv_obj_update_layout(B->root);
    lv_display_enable_invalidation(disp, false);
  }
  for (int i = 0; i < B->n; i++) {
    const int32_t local = together ? ms : std::clamp<int32_t>(ms - (int32_t) (i * k.stagger), 0, k.ms);
    const int32_t e = ease(ch::M_bars, local * 1024 / k.ms);
    const int h0 = together ? bar_h(B, B->from[i]) : 0, h1 = bar_h(B, B->v[i]);
    bar_place(B, i, h0 + (h1 - h0) * e / 1024);
  }
  if (batch) {
    lv_obj_update_layout(B->root);
    lv_display_enable_invalidation(disp, true);
    lv_obj_invalidate(B->plot);
  }
}
void bars_play(Bars *B, const std::vector<float> *nv) {
  const ch::Motion &k = ch::MOTION[ch::M_bars];
  if (nv) B->from = B->v, B->v = *nv;
  else B->from.assign(1, -1);  // grow from the baseline
  if (mo::reduced(B->root) && !nv) {  // reduced: the bars fade in at their heights
    bars_exec(B->root, k.ms + k.stagger * B->n);
    lv_obj_set_style_opa(B->plot, 0, 0);
    fw::Anim a{set_opa, 0, 255, mo::OPA};
    fw::animate(B->plot, mo::fade, a);
    return;
  }
  const uint32_t total = nv ? k.ms : k.ms + k.stagger * (B->n - 1);
  if (!nv) bars_exec(B->root, 0);
  // the anim's values are ms on a linear clock; each bar eases on its own (bars_exec)
  lv_anim_delete(B->root, bars_exec);
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, B->root);
  lv_anim_set_exec_cb(&a, bars_exec);
  lv_anim_set_values(&a, 0, nv ? k.ms : total);
  lv_anim_set_duration(&a, total);
  lv_anim_set_path_cb(&a, lv_anim_path_linear);
  if ((mo::reduced(B->root) && k.reduced != mo::R_keep) || (lite() && k.lite == ch::L_instant)) bars_exec(B->root, total);
  else lv_anim_start(&a);
}
void bars_read(Bars *B, int i) {
  char b[64];
  if (B->first_hour >= 0) snprintf(b, sizeof b, "%.*f %s  %02d:00", B->dec, B->v[i], B->unit, (B->first_hour + i) % 24);
  else snprintf(b, sizeof b, "%.*f %s  %s", B->dec, B->v[i], B->unit, (size_t) i < B->labels.size() ? B->labels[i].c_str() : "");
  lv_label_set_text(B->read, b);
  lv_obj_update_layout(B->read);
  const int rw = lv_obj_get_width(B->read), x = bar_x(B, i) + B->bw / 2;
  lv_obj_set_x(B->read, std::clamp<int32_t>(x - rw / 2, 0, lv_obj_get_width(B->root) - rw));
}
void bars_cb(lv_event_t *e) {
  auto *B = (Bars *) lv_event_get_user_data(e);
  const lv_event_code_t c = lv_event_get_code(e);
  if (c == LV_EVENT_RELEASED || c == LV_EVENT_PRESS_LOST) {
    const int s = B->sel;
    if (s >= 0) { char d[24]; snprintf(d, sizeof d, "%d %.*f", s, B->dec, B->v[s]); emit(B->root, "scrub", d); }
    B->sel = -1;
    if (s >= 0) bar_colour(B, s);
    if (B->read) { fw::Anim a{set_opa, 255, 0, mo::OPA}; fw::animate(B->read, mo::fade, a); }
    return;
  }
  lv_point_t p;
  lv_indev_get_point(lv_indev_active(), &p);
  lv_area_t r;
  lv_obj_get_coords(B->root, &r);
  int i = 0;  // the bar whose slot holds the finger (groups: the nearest bar)
  for (int k = 1; k < B->n; k++) if (p.x - r.x1 >= bar_x(B, k) - (B->gap + 1) / 2) i = k;
  if (i == B->sel || !std::isfinite(B->v[i])) return;
  const int old = B->sel;
  B->sel = i;
  if (old >= 0) { bar_colour(B, old); step_ticks(B->root, old, i, B->n - 1); }
  if (!B->read) return;
  bar_colour(B, i);
  bars_read(B, i);
  if (c == LV_EVENT_PRESSED && B->read) { fw::Anim a{set_opa, 0, 255, mo::OPA}; fw::animate(B->read, mo::press, a); }
}
// bars across `w`: the bar width snapped to the grid, the chart hugs them (groups: group_gap between groups), h tall
Bars *bars(lv_obj_t *parent, const Theme &t, const BarOpts &o) {
  auto *B = new Bars();
  B->t = &t, B->v = o.v, B->n = o.v.size(), B->h = o.h, B->col = o.col;
  B->kind = o.kind;
  B->kind.resize(B->n, 0);
  B->gap = o.gap < 0 ? ch::bar_gap : o.gap, B->group = std::max(1, o.group), B->group_gap = o.group > 1 ? o.group_gap : 0;
  B->unit = o.unit, B->dec = o.dec, B->first_hour = o.first_hour, B->labels = o.labels;
  float mx = o.max;
  if (mx <= 0) for (float x : o.v) if (std::isfinite(x)) mx = std::max(mx, x);
  B->max = mx > 0 ? mx : 1;
  const int ng = (B->n + B->group - 1) / B->group;
  const int avail = o.w - (ng - 1) * B->group_gap - (B->n - ng) * B->gap - (ng - 1) * B->gap * (B->group > 1 ? 0 : 1);
  B->bw = std::max(4, (avail / std::max(1, B->n)) / 4 * 4);
  if (o.fill && B->group == 1 && B->n > 1) B->spread = std::max(0, o.w - (B->n * B->bw + (B->n - 1) * B->gap));
  const int cw = B->n ? bar_x(B, B->n - 1) + B->bw : o.w;
  B->root = box(parent);
  lv_obj_set_size(B->root, cw, o.h);
  lv_obj_set_user_data(B->root, B);
  lv_obj_add_event_cb(B->root, [](lv_event_t *e) { delete (Bars *) lv_event_get_user_data(e); }, LV_EVENT_DELETE, B);
  B->top = ch::plot_top + (o.readout ? ty::STYLES[ty::c_meta].box : 0);  // room for the readout over the bars
  B->bot = o.h - ch::plot_bottom;
  B->plot = box(B->root);
  lv_obj_set_size(B->plot, cw, B->bot);
  fw::qa::clips(B->plot, "bars: the radius hangs below the baseline and is cut off there (a square foot)");
  for (int i = 0; i < B->n; i++) {
    lv_obj_t *b = box(B->plot);
    lv_obj_set_style_radius(b, ch::bar_radius, 0);
    fw::qa::free(b, "a bar's height is its value");
    B->b.push_back(b);
    bar_colour(B, i);
    bar_place(B, i, bar_h(B, B->v[i]));
  }
  if (o.baseline) rect(B->root, 0, B->bot, cw, ch::grid, ch::ink::baseline(t));
  if (o.now >= 0 && o.now < B->n) dotted_v(B->root, t, bar_x(B, o.now) + B->bw / 2, B->top, B->bot);
  if (o.readout) {
    B->read = text(B->root, ty::c_meta, "", t.text);
    lv_obj_set_style_opa(B->read, 0, 0);
  }
  if (o.scrub) {
    lv_obj_add_flag(B->root, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(B->root, LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_remove_flag(B->root, (lv_obj_flag_t) (LV_OBJ_FLAG_SCROLL_CHAIN_HOR | LV_OBJ_FLAG_SCROLL_CHAIN_VER));
    for (lv_event_code_t c : {LV_EVENT_PRESSED, LV_EVENT_PRESSING, LV_EVENT_RELEASED, LV_EVENT_PRESS_LOST})
      lv_obj_add_event_cb(B->root, bars_cb, c, B);
  }
  return B;
}
// the page's hourly bars (kWh readout): the general bars() with the foundation's settings
Bars *bar_chart(lv_obj_t *parent, const Theme &t, int w, int h, const std::vector<float> &v, const std::vector<uint8_t> &kind,
                float max, bool scrub, int now) {
  BarOpts o;
  o.w = w, o.h = h, o.v = v, o.kind = kind, o.max = max, o.scrub = scrub, o.now = now;
  o.unit = "kWh", o.dec = 2, o.first_hour = 0;
  return bars(parent, t, o);
}
std::vector<float> power_day(int seed = 0) {
  std::vector<float> v(24);
  for (int i = 0; i < 24; i++) v[i] = power_at(i + seed) + 0.05f;
  return v;
}

// ---------------------------------------------------------------- gauge (= a Dial without the knob)
constexpr float G0 = 135, GS = 270;
void gauge_exec(void *o, int32_t p) {
  auto *G = (Gauge *) lv_obj_get_user_data((lv_obj_t *) o);
  G->shown = G->start + (G->v - G->start) * p / 1024.f;
  const float f = std::clamp((G->shown - G->lo) / (G->hi - G->lo), 0.f, 1.f);
  stroke_set_arc(G->arc, G0, G0 + std::max(0.5f, GS * f));
  char b[16];
  snprintf(b, sizeof b, "%.0f", G->shown);
  set_text(G->num, ty::c_title, b);
}
void gauge_to(Gauge *G, float v) {
  G->start = G->shown, G->v = v;
  run(G->root, ch::M_gauge, gauge_exec, 0, 1024, nullptr, G);
}
Gauge *gauge(lv_obj_t *parent, const Theme &t, float v, const char *unit) {
  auto *G = new Gauge();
  const int d = ch::gauge;
  G->root = box(parent);
  lv_obj_set_size(G->root, d, d);
  lv_obj_set_user_data(G->root, G);
  lv_obj_add_event_cb(G->root, [](lv_event_t *e) { delete (Gauge *) lv_event_get_user_data(e); }, LV_EVENT_DELETE, G);
  const int inset = (ch::gauge_ind - ch::gauge_track) / 2;  // the track centred under the value arc (both inside the box)
  stroke_arc(G->root, inset, inset, d - 2 * inset, G0, G0 + GS, ch::gauge_track, ch::ink::grid(t));
  G->arc = stroke_arc(G->root, 0, 0, d, G0, G0 + 0.5f, ch::gauge_ind, ch::ink::highlight(t));
  lv_obj_t *c = column(G->root, 0, LV_SIZE_CONTENT);
  lv_obj_set_flex_align(c, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_center(c);
  G->num = text(c, ty::c_title, "0", t.text);
  text(c, ty::c_meta, unit, t.text_dim);
  G->shown = 0;
  gauge_to(G, v);
  return G;
}

// ---------------------------------------------------------------- labels
void legend(lv_obj_t *parent, const Theme &t, const int *series, const char *const *names, int n) {
  lv_obj_t *r = row(parent, sp::s5, LV_SIZE_CONTENT);
  for (int i = 0; i < n; i++) {
    lv_obj_t *it = row(r, sp::s2, LV_SIZE_CONTENT);
    lv_obj_t *sw = box(it);
    lv_obj_set_size(sw, ch::legend_sw, ch::legend_sw);
    lv_obj_set_style_radius(sw, rad::chip, 0);
    const lv_color_t c = series[i] < 0 ? ch::ink::highlight(t) : ch::series(t, series[i]);
    if (series[i] >= 0 && ch::SERIES[series[i]].dashed) outline(sw, c, stroke::hair);
    else fill(sw, c);
    text(it, ty::c_meta_sm, names[i], ch::ink::axis(t));
  }
}
lv_obj_t *legend_colours(lv_obj_t *parent, const Theme &t, const lv_color_t *cols, const bool *outl, const char *const *names, int n) {
  lv_obj_t *r = row(parent, sp::s5, LV_SIZE_CONTENT);
  for (int i = 0; i < n; i++) {
    lv_obj_t *it = row(r, sp::s2, LV_SIZE_CONTENT);
    lv_obj_t *sw = box(it);
    lv_obj_set_size(sw, ch::legend_sw, ch::legend_sw);
    lv_obj_set_style_radius(sw, rad::chip, 0);
    if (outl && outl[i]) outline(sw, cols[i], stroke::hair);
    else fill(sw, cols[i]);
    text(it, ty::c_meta_sm, names[i], ch::ink::axis(t));
  }
  return r;
}
// time axis under a chart: labels at fractions of w, the first left-aligned, the last right-aligned (docs axis())
void axis(lv_obj_t *parent, const Theme &t, int w, const char *const *labels, const float *at, int n) {
  lv_obj_t *a = box(parent);
  lv_obj_set_size(a, w, ty::STYLES[ty::c_meta_sm].box);
  for (int i = 0; i < n; i++) {
    lv_obj_t *l = text(a, ty::c_meta_sm, labels[i], ch::ink::axis(t));
    lv_obj_update_layout(l);
    const int lw = lv_obj_get_width(l);
    lv_obj_set_x(l, at[i] <= 0 ? 0 : at[i] >= 1 ? w - lw : (int) std::lround(at[i] * w - lw / 2.f));
  }
}
void y_labels(lv_obj_t *chart_root, const Theme &t, const Line *L, const char *const *labels) {  // above each grid line, left
  for (int k = 0; k < ch::GRID_LINES; k++) {
    const int y = gt(L) + (gb(L) - gt(L)) * k / (ch::GRID_LINES - 1);
    lv_obj_t *l = text(chart_root, ty::c_meta_sm, labels[k], ch::ink::axis(t));
    lv_obj_set_pos(l, 0, k == 0 ? y : y - ty::STYLES[ty::c_meta_sm].box);
  }
}
lv_obj_t *big_number(lv_obj_t *parent, const Theme &t, const char *label, const char *value, const char *unit) {
  lv_obj_t *c = column(parent, sp::s2, LV_SIZE_CONTENT);
  text(c, ty::c_caps_sm, label, t.text_dim);
  lv_obj_t *r = row(c, sp::s2, LV_SIZE_CONTENT);
  lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
  lv_obj_t *v = text(r, ty::c_number, value, t.text);
  text(r, ty::c_subtitle, unit, t.text_dim);
  return v;
}


// ---------------------------------------------------------------- token bars (the Bar chart on the contract)
// The same bars as bars() above (geometry, colours, motion, scrub), drawn through the Pen in one strip with colour
// token NAMES, so a theme / accent switch repaints them with no rebuild (C24, C44). The strip holds the plot (the
// readout row over the bars, the bars, the baseline, the dotted now marker) and, with ticks, the hour axis under it.
namespace {
struct TB {
  lv_obj_t *s = nullptr;
  std::vector<float> v, from;
  std::vector<std::string> ink;  // per bar ("fg:blue"); empty = by kind
  std::vector<uint8_t> kind;
  std::vector<int> shown;        // each bar's drawn height now (px)
  float max = 1;
  int n = 0, bw = 0, gap = 0, spread = 0, w = 0, h = 0, top = 0, bot = 0, now = -1, sel = -1, ticks = 0, dec = 2, first_hour = 0;
  bool scrub = true, fill = false, faded = false, busy = false;
  int w_dp = 0, h_dp = 80;
  std::string unit;
  lv_obj_t *axis = nullptr;  // the hour axis (ticks), placed from the bars
  int group = 1, ggap = 0, leg_h = 0;   // pairs: bars per group, the gap between groups; the legend row's height
  std::vector<std::string> labels, legend;
  std::vector<uint8_t> legend_kind;
  std::vector<lv_obj_t *> leg_words;    // the legend's words (real labels in the strip; the Pen draws their swatches)
  std::vector<uint8_t> marks;            // per bar: 0 none, 1 new moon, 2 full moon (TokenBars::marks)
  std::vector<std::string> names;        // per bar: the readout's word (TokenBars::names)
  int mk_h = 0;                          // the marks row (chart:mark + space 5), under the legend
};
int tb_groups(const TB *B) { return (B->n + B->group - 1) / B->group; }
void tb_free(lv_event_t *e) { delete (TB *) lv_event_get_user_data(e); }
TB *tb_of(lv_obj_t *s) {
  if (!s) return nullptr;
  for (uint32_t i = 0, n = lv_obj_get_event_count(s); i < n; i++) {
    lv_event_dsc_t *d = lv_obj_get_event_dsc(s, i);
    if (lv_event_dsc_get_cb(d) == tb_free) return (TB *) lv_event_dsc_get_user_data(d);
  }
  return nullptr;
}
int tb_h(const TB *B, float v) {
  if (!std::isfinite(v) || v <= 0) return 0;
  return std::max(fx::size("chart:bar-min"), (int) std::lround(v / B->max * (B->bot - B->top)));
}
int tb_x(const TB *B, int i) {  // group 1: every bar its own group (ggap = gap)
  const int g = i / B->group, k = i % B->group, ng = tb_groups(B);
  const int sp = B->spread && ng > 1 ? (int) ((int64_t) g * B->spread / (ng - 1)) : 0;
  return g * (B->group * B->bw + (B->group - 1) * B->gap + B->ggap) + k * (B->bw + B->gap) + sp;
}
int tb_centre(const TB *B, int g) {  // the middle of group g (an axis word, a label's place)
  const int first = g * B->group, last = std::min(B->n - 1, first + B->group - 1);
  return (tb_x(B, first) + tb_x(B, last) + B->bw) / 2;
}
// the legend row over the plot: per entry a legend-sw swatch, space 2, its word; space 5 between entries
int tb_sw_x(const TB *B, size_t k) {
  int x = 0;
  for (size_t j = 0; j < k && j < B->leg_words.size(); j++)
    x += fx::size("chart:legend-sw") + fx::size("space:2") + lv_obj_get_width(B->leg_words[j]) + fx::size("space:5");
  return x;
}
const char *tb_ink(const TB *B, int i) {
  if (i == B->sel) return "fg:text";  // ch::ink::select
  if ((size_t) i < B->ink.size() && !B->ink[i].empty()) return B->ink[i].c_str();
  return B->kind[i] == 1 ? "fg:accent" : B->kind[i] == 2 ? "fg:line-hi" : "fg:text-dim";  // highlight, series 3, series 2
}
void tb_draw(const fx::Pen &p) {
  const TB *B = tb_of(p.obj);
  if (!B) return;
  const int L = -(p.w / 2), T = -(p.h / 2);
  for (size_t k = 0; k < B->legend.size(); k++) {  // the legend's swatches, rounded like the bars (their words are labels)
    const int sw = fx::size("chart:legend-sw");
    const bool cmp = k < B->legend_kind.size() && B->legend_kind[k] == 2;
    p.rect(L + tb_sw_x(B, k), T + fx::size("space:2") + (std::max(sw, fx::type_box("type:c-meta-sm")) - sw) / 2, sw, sw, cmp ? "fg:line-hi" : "fg:text-dim", fx::size("chart:bar-radius"),
           cmp ? fx::size("stroke:hair") : 0);
  }
  if (B->mk_h) {  // the marks: a moon disc over its bar (full = moon-lit, new = moon-dark) in a moon-edge outline
    const int d = fx::size("chart:mark"), cw = B->n ? tb_x(B, B->n - 1) + B->bw : B->w, y = T + B->leg_h - B->mk_h + d / 2;
    for (int i = 0; i < B->n && i < (int) B->marks.size(); i++) {
      if (!B->marks[i]) continue;
      const int x = L + std::clamp(tb_x(B, i) + B->bw / 2, d / 2, std::max(d / 2, cw - d / 2));
      p.dot(x, y, "chart:mark", "fg:moon-edge");  // the outline, then the disc inside it
      p.dot(x, y, "chart:mark-fill", B->marks[i] == 2 ? "fg:moon-lit" : "fg:moon-dark");
    }
  }
  for (int i = 0; i < B->n; i++) {
    const int hp = B->shown[i];
    if (hp <= 0) continue;
    const bool outline = B->kind[i] == 2 && i != B->sel;
    p.rect(L + tb_x(B, i), T + B->bot - hp, B->bw, hp, tb_ink(B, i), fx::size("chart:bar-radius"), outline ? fx::size("stroke:hair") : 0, 1);
  }
  const int cw = B->n ? tb_x(B, B->n - 1) + B->bw : B->w;
  const int grid = fx::size("chart:grid"), dash = fx::size("chart:dash"), dash_gap = fx::size("chart:dash-gap");
  p.rect(L, T + B->bot, cw, grid, "fg:line-hi");  // the baseline
  if (B->now >= 0 && B->now < B->n)  // the dotted now marker (ch::ink::cursor)
    for (int y = B->top; y < B->bot; y += dash + dash_gap)
      p.rect(L + tb_x(B, B->now) + B->bw / 2, T + y, grid, std::min(dash, B->bot - y), "fg:text-dim");
  char b[64];
  if (B->sel >= 0) {  // the readout over the bars: "<value> <unit>  <hh>:00" (labels: "<value> <unit>  <word>"), inside the chart
    const int g = B->sel / B->group;
    if ((size_t) B->sel < B->names.size()) snprintf(b, sizeof b, "%.*f %s  %s", B->dec, B->v[B->sel], B->unit.c_str(), B->names[B->sel].c_str());
    else if (B->labels.empty()) snprintf(b, sizeof b, "%.*f %s  %02d:00", B->dec, B->v[B->sel], B->unit.c_str(), (B->first_hour + B->sel) % 24);
    else {
      std::string w = (size_t) g < B->labels.size() ? B->labels[g] : "";
      for (size_t k = 0; B->group > 1 && k < B->legend.size(); k++)  // which series: the legend word of this bar's kind
        if (k < B->legend_kind.size() && B->legend_kind[k] == B->kind[B->sel]) { w += " \xC2\xB7 " + B->legend[k]; break; }
      snprintf(b, sizeof b, "%.*f %s  %s", B->dec, B->v[B->sel], B->unit.c_str(), w.c_str());
    }
    const int rw = p.text_w(b, "type:c-meta"), x = std::clamp(tb_x(B, B->sel) + B->bw / 2 - rw / 2, 0, std::max(0, cw - rw));
    p.text(L + x + rw / 2, T + B->leg_h + fx::type_box("type:c-meta") / 2, b, "type:c-meta", "fg:text");
  }
}
void tb_redraw(TB *B) { fx::redraw(B->s, -(lv_obj_get_content_width(B->s) / 2), -(lv_obj_get_content_height(B->s) / 2), lv_obj_get_content_width(B->s), B->bot + fx::size("chart:grid") + 1); }
void tb_exec(void *o, int32_t ms) {  // one animation for every bar: each starts a stagger later
  TB *B = tb_of((lv_obj_t *) o);
  if (!B) return;
  const ch::Motion &k = ch::MOTION[ch::M_bars];
  const bool together = B->from.size() == B->v.size() && !B->from.empty() && B->from[0] >= 0;
  for (int i = 0; i < B->n; i++) {
    const int32_t local = together ? ms : std::clamp<int32_t>(ms - (int32_t) (i * k.stagger), 0, k.ms);
    const int32_t e = ease(ch::M_bars, local * 1024 / k.ms);
    const int h0 = together ? tb_h(B, B->from[i]) : 0, h1 = tb_h(B, B->v[i]);
    B->shown[i] = h0 + (h1 - h0) * e / 1024;
  }
  tb_redraw(B);  // the plot only (one area: many bars never pass LVGL's 32 dirty areas)
}
void tb_play(TB *B, const std::vector<float> *nv) {
  const ch::Motion &k = ch::MOTION[ch::M_bars];
  if (nv) B->from = B->v, B->v = *nv;
  else B->from.assign(1, -1);  // grow from the baseline
  const uint32_t total = nv ? k.ms : k.ms + k.stagger * (B->n - 1);
  lv_anim_delete(B->s, tb_exec);
  if (mo::reduced(B->s) && !nv) {  // reduced: the bars fade in at their heights
    tb_exec(B->s, total);
    lv_obj_set_style_opa(B->s, 0, 0);
    fw::Anim a{set_opa, 0, 255, mo::OPA};
    fw::animate(B->s, mo::fade, a);
    return;
  }
  if ((mo::reduced(B->s) && k.reduced != mo::R_keep) || (lite() && k.lite == ch::L_instant)) { tb_exec(B->s, total); return; }
  if (!nv) tb_exec(B->s, 0);
  lv_anim_t a;  // the anim's values are ms on a linear clock; each bar eases on its own (tb_exec)
  lv_anim_init(&a);
  lv_anim_set_var(&a, B->s);
  lv_anim_set_exec_cb(&a, tb_exec);
  lv_anim_set_values(&a, 0, nv ? k.ms : total);
  lv_anim_set_duration(&a, total);
  lv_anim_set_path_cb(&a, lv_anim_path_linear);
  lv_anim_start(&a);
}
void tb_cb(lv_event_t *e) {
  TB *B = (TB *) lv_event_get_user_data(e);
  const lv_event_code_t c = lv_event_get_code(e);
  if (c == LV_EVENT_RELEASED || c == LV_EVENT_PRESS_LOST) {
    const int s = B->sel;
    if (s >= 0) { char d[24]; snprintf(d, sizeof d, "%d %.*f", s, B->dec, B->v[s]); emit(B->s, "scrub", d); }
    B->sel = -1;
    if (s >= 0) tb_redraw(B);
    return;
  }
  lv_point_t p;
  lv_indev_get_point(lv_indev_active(), &p);
  lv_area_t r;
  lv_obj_get_content_coords(B->s, &r);
  int i = 0;  // the bar whose slot holds the finger
  for (int k = 1; k < B->n; k++) if (p.x - r.x1 >= tb_x(B, k) - (B->gap + 1) / 2) i = k;
  if (i == B->sel || !std::isfinite(B->v[i])) return;
  const int old = B->sel;
  B->sel = i;
  if (old >= 0) step_ticks(B->s, old, i, B->n - 1);
  tb_redraw(B);
}
// geometry from the opts and the room: the bar width snapped to the grid, the chart hugs them (fill: spans the parent)
void tb_size(TB *B, lv_obj_t *parent) {
  if (B->w_dp > 0) B->w = px((float) B->w_dp);
  else { if (!B->busy) lv_obj_update_layout(parent); B->w = lv_obj_get_content_width(parent); }
  B->fill = B->w_dp <= 0;
  B->h = px((float) B->h_dp);
  B->gap = fx::size("chart:bar-gap");
  B->ggap = B->group > 1 ? fx::size("chart:group-gap") : B->gap;
  const int ng = tb_groups(B), inner = ng * (B->group - 1) * B->gap + (ng - 1) * B->ggap;
  const int avail = B->w - inner;
  B->bw = std::max(4, (avail / std::max(1, B->n)) / 4 * 4);
  B->spread = B->fill && ng > 1 ? std::max(0, B->w - (B->n * B->bw + inner)) : 0;
  // the legend row: space 2 above it, space 9 under it to the plot (the old WASM week: 4 + 20 at 1x, 8 + 32 at 720)
  B->leg_h = B->legend.empty() ? 0 : fx::size("space:2") + std::max(fx::size("chart:legend-sw"), fx::type_box("type:c-meta-sm")) + fx::size("space:9");
  bool marked = false;
  for (uint8_t m : B->marks) marked = marked || m;
  B->mk_h = marked ? fx::size("chart:mark") + fx::size("space:5") : 0;  // docs: the marks row 24, 16 above the chart
  B->leg_h += B->mk_h;
  for (size_t k = 0; k < B->leg_words.size(); k++) {  // the words beside their swatches, in the legend row
    lv_obj_update_layout(B->leg_words[k]);
    lv_obj_set_pos(B->leg_words[k], tb_sw_x(B, k) + fx::size("chart:legend-sw") + fx::size("space:2"), fx::size("space:2"));
  }
  B->top = B->leg_h + fx::size("chart:plot-top") + fx::type_box("type:c-meta");  // the legend, room for the readout over the bars
  B->bot = B->leg_h + B->h - fx::size("chart:plot-bottom");
  const int cw = B->n ? tb_x(B, B->n - 1) + B->bw : B->w;
  lv_obj_set_size(B->s, cw, B->leg_h + B->h);
  B->shown.assign(B->n, 0);
}
void tb_place_axis(TB *B) {  // the labels under their bar's centre, kept inside the chart's width
  if (!B->axis) return;
  const int cw = B->n ? tb_x(B, B->n - 1) + B->bw : B->w;
  lv_obj_set_size(B->axis, cw, fx::type_box("type:c-meta-sm"));
  // hour ticks sit space 1 under the plot; day words (labels) space 2, as the old WASM week's axis
  lv_obj_set_style_margin_top(B->axis, fx::size(B->labels.empty() ? "space:1" : "space:2"), 0);
  for (uint32_t k = 0; k < lv_obj_get_child_count(B->axis); k++) {
    lv_obj_t *l = lv_obj_get_child(B->axis, k);
    const int i = (int) (intptr_t) lv_obj_get_user_data(l);
    lv_obj_update_layout(l);
    const int lw = lv_obj_get_width(l), mid = B->labels.empty() ? tb_x(B, i) + B->bw / 2 : tb_centre(B, i);
    lv_obj_set_x(l, std::clamp(mid - lw / 2, 0, std::max(0, cw - lw)));
  }
}
// a token tuned or a theme switched (fx::tune -> every object's STYLE_CHANGED): the bars, gaps and axis re-measured
// from the live chart:* / space:* tokens, the bars at their values (no replay)
void tb_retune(lv_event_t *e) {
  TB *B = (TB *) lv_event_get_user_data(e);
  if (B->busy || lv_event_get_target(e) != B->s) return;
  B->busy = true;
  tb_size(B, lv_obj_get_parent(B->s));
  tb_place_axis(B);
  tb_exec(B->s, ch::MOTION[ch::M_bars].ms + ch::MOTION[ch::M_bars].stagger * B->n);
  B->busy = false;
}
// the hour axis: "00" "06" ... under their bar's centre, space 1 below the chart, kept inside its width (Charts axis
// ink + type; real labels, so a spec's texts and the tests read them)
void tb_axis(TB *B, lv_obj_t *parent) {
  const int cw = B->n ? tb_x(B, B->n - 1) + B->bw : B->w;
  lv_obj_t *a = lv_obj_create(parent);
  lv_obj_remove_style_all(a);
  lv_obj_remove_flag(a, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  (void) cw;
  B->axis = a;
  if (!B->labels.empty()) {  // one word per group (the days of a week), under its centre
    for (int g = 0; g < tb_groups(B) && (size_t) g < B->labels.size(); g++)
      lv_obj_set_user_data(fx::text(a, B->labels[g].c_str(), "type:c-meta-sm", "fg:text-dim"), (void *) (intptr_t) g);
    tb_place_axis(B);
    return;
  }
  for (int i = 0; i < B->n; i++) {
    if ((B->first_hour + i) % B->ticks) continue;
    char hh[4];
    snprintf(hh, sizeof hh, "%02d", (B->first_hour + i) % 24);
    lv_obj_set_user_data(fx::text(a, hh, "type:c-meta-sm", "fg:text-dim"), (void *) (intptr_t) i);
  }
  tb_place_axis(B);
}
}  // namespace

lv_obj_t *token_bars(lv_obj_t *parent, const TokenBars &o, bool opening) {
  auto *B = new TB();
  B->v = o.v, B->n = (int) o.v.size(), B->ink = o.ink, B->kind = o.kind, B->now = o.now, B->ticks = o.ticks;
  B->kind.resize(B->n, 0);
  B->scrub = o.scrub, B->dec = o.dec, B->first_hour = o.first_hour, B->unit = o.unit, B->w_dp = o.w_dp, B->h_dp = o.h_dp;
  B->group = std::max(1, o.group), B->labels = o.labels, B->legend = o.legend, B->legend_kind = o.legend_kind, B->marks = o.marks, B->names = o.names;
  float mx = 0;
  for (float x : o.v) if (std::isfinite(x)) mx = std::max(mx, x);
  B->max = mx > 0 ? mx : 1;
  B->s = fx::strip(parent, tb_draw, "free");
  lv_obj_add_event_cb(B->s, tb_free, LV_EVENT_DELETE, B);
  qa::free(B->s, "bars: sized by their values (the chart hugs its bars)");
  for (const std::string &w : B->legend) B->leg_words.push_back(fx::text(B->s, w.c_str(), "type:c-meta-sm", "fg:text-dim"));
  tb_size(B, parent);
  if (B->scrub) {
    lv_obj_add_flag(B->s, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_PRESS_LOCK));
    lv_obj_remove_flag(B->s, (lv_obj_flag_t) (LV_OBJ_FLAG_SCROLL_CHAIN_HOR | LV_OBJ_FLAG_SCROLL_CHAIN_VER));
    for (lv_event_code_t c : {LV_EVENT_PRESSED, LV_EVENT_PRESSING, LV_EVENT_RELEASED, LV_EVENT_PRESS_LOST})
      lv_obj_add_event_cb(B->s, tb_cb, c, B);
  }
  if (B->ticks > 0 || !B->labels.empty()) tb_axis(B, parent);
  lv_obj_add_event_cb(B->s, tb_retune, LV_EVENT_STYLE_CHANGED, B);
  if (opening) tb_play(B, nullptr);
  else tb_exec(B->s, ch::MOTION[ch::M_bars].ms + ch::MOTION[ch::M_bars].stagger * B->n);
  return B->s;
}
bool token_bars_move(lv_obj_t *s, const std::vector<float> &nv) {
  TB *B = tb_of(s);
  if (!B || (int) nv.size() != B->n) return false;
  bool same = true;
  for (size_t i = 0; i < nv.size(); i++) same = same && ((std::isnan(nv[i]) && std::isnan(B->v[i])) || nv[i] == B->v[i]);
  if (same) return true;
  // a new largest value rescales the chart: keep the old bars' heights where they are (their values scaled into the
  // new max), then every height moves together (chart motion bars)
  float mx = 0;
  for (float x : nv) if (std::isfinite(x)) mx = std::max(mx, x);
  mx = mx > 0 ? mx : 1;
  if (mx != B->max) { for (float &x : B->v) x *= mx / B->max; B->max = mx; }
  tb_play(B, &nv);
  return true;
}


// ---- token strip (the Rate strip on the contract): price bands as runs (equal slots, one rounded run per stretch of
// the same band, stroke:edge between runs), a stroke:edge notch in the canvas colour at the middle of the now slot,
// and optionally an hour axis under it ("00" ... "24" every `ticks` hours, the first left-, the last right-aligned).
// All Pen-drawn in one strip, inks by token name: a theme switch repaints it, a tuned token re-sizes it.
namespace {
struct TS {
  lv_obj_t *s = nullptr, *parent = nullptr;
  std::vector<std::string> bands;
  int now = -1, ticks = 0, w_dp = 0;
  bool busy = false;
};
void ts_free(lv_event_t *e) { delete (TS *) lv_event_get_user_data(e); }
TS *ts_of(lv_obj_t *s) {
  if (!s) return nullptr;
  for (uint32_t i = 0, n = lv_obj_get_event_count(s); i < n; i++) {
    lv_event_dsc_t *d = lv_obj_get_event_dsc(s, i);
    if (lv_event_dsc_get_cb(d) == ts_free) return (TS *) lv_event_dsc_get_user_data(d);
  }
  return nullptr;
}
const char *ts_ink(const std::string &b) { return b == "peak" ? "fg:accent" : b == "cheap" ? "fg:blue" : "fg:band-normal"; }
void ts_draw(const fx::Pen &p) {
  const TS *S = ts_of(p.obj);
  if (!S) return;
  const int L = -(p.w / 2), T = -(p.h / 2), W = p.w, H = fx::size("chart:strip-h"), g = fx::size("stroke:edge");
  const int n = std::max<int>(1, S->bands.size());
  const float slot = W / (float) n;
  for (int i = 0; i < (int) S->bands.size();) {
    int j = i;
    while (j + 1 < (int) S->bands.size() && S->bands[j + 1] == S->bands[i]) j++;
    const int x0 = (int) std::lround(i * slot) + (i ? g / 2 : 0);
    const int x1 = (int) std::lround((j + 1) * slot) - (j < (int) S->bands.size() - 1 ? g - g / 2 : 0);
    p.rect(L + x0, T, x1 - x0, H, ts_ink(S->bands[i]), fx::size("chart:bar-radius"));
    i = j + 1;
  }
  if (S->now >= 0 && S->now < n) p.rect(L + (int) std::lround((S->now + 0.5f) * slot) - g / 2, T, g, H, "fg:bg");
  if (S->ticks <= 0) return;
  const int y = T + H + fx::size("space:1") + fx::type_box("type:c-meta-sm") / 2;
  for (int h = 0; h <= 24; h += S->ticks) {
    char hh[4];
    snprintf(hh, sizeof hh, "%02d", h);
    const int tw = p.text_w(hh, "type:c-meta-sm");
    const int x = std::clamp((int) std::lround(W * h / 24.0f), tw / 2, W - (tw + 1) / 2);
    p.text(L + x, y, hh, "type:c-meta-sm", "fg:text-dim");
  }
}
void ts_size(TS *S) {
  int w;
  if (S->w_dp > 0) w = fw::px((float) S->w_dp);
  else { if (!S->busy) lv_obj_update_layout(S->parent); w = lv_obj_get_content_width(S->parent); }
  const int h = fx::size("chart:strip-h") + (S->ticks > 0 ? fx::size("space:1") + fx::type_box("type:c-meta-sm") : 0);
  lv_obj_set_size(S->s, w, h);
}
void ts_retune(lv_event_t *e) {
  TS *S = (TS *) lv_event_get_user_data(e);
  if (S->busy || lv_event_get_target(e) != S->s) return;
  S->busy = true;
  ts_size(S);
  lv_obj_invalidate(S->s);
  S->busy = false;
}
}  // namespace

lv_obj_t *token_strip(lv_obj_t *parent, const std::vector<std::string> &bands, int now, int w_dp, int ticks) {
  auto *S = new TS();
  S->parent = parent, S->bands = bands, S->now = now, S->w_dp = w_dp, S->ticks = ticks;
  S->s = fx::strip(parent, ts_draw, "free");
  lv_obj_add_event_cb(S->s, ts_free, LV_EVENT_DELETE, S);
  ts_size(S);
  lv_obj_add_event_cb(S->s, ts_retune, LV_EVENT_STYLE_CHANGED, S);
  return S->s;
}
bool token_strip_set(lv_obj_t *s, const std::vector<std::string> &bands, int now, int w_dp, int ticks) {
  TS *S = ts_of(s);
  if (!S) return false;
  const bool resize = w_dp != S->w_dp || ticks != S->ticks;
  S->bands = bands, S->now = now, S->w_dp = w_dp, S->ticks = ticks;
  if (resize) ts_size(S);
  lv_obj_invalidate(s);
  return true;
}

}  // namespace fw::chart

namespace {
using namespace fw::chart;
// ---------------------------------------------------------------- page sections
void spec(lv_obj_t *e, const char *title, const char *about) {
  text(e, ty::c_caps_sm, title, T->text);
  if (about && *about) text(e, ty::c_meta, about, T->text_dim, inner_w());
}
// one theme's card: three series + compare + highlight, with the contrast of each against the card and the canvas
void colour_card(lv_obj_t *parent, const Theme &t, int w) {
  lv_obj_t *f = column(parent, sp::s3, w);  // the canvas of that theme around the card
  fill(f, t.bg);
  lv_obj_set_style_pad_all(f, sp::s3, 0);
  text(f, ty::c_caps_sm, t.light ? "light" : "dark", t.text_dim);
  lv_obj_t *c = card(f, t, w - 2 * sp::s3);
  const int cw = card_in(w - 2 * sp::s3);
  lv_obj_t *plot = box(c);
  lv_obj_set_size(plot, cw, ch::chart_h);
  for (int s = 3; s >= 0; s--) {  // compare at the back, series 1 on top
    LineOpts o{cw, ch::chart_h, day(24, s * 5)};
    for (float &x : o.v) x += (s - 1.5f) * 1.2f;
    o.series = s, o.weight = s == 0 ? ch::line : ch::line_thin, o.dot = s == 0, o.dashed = ch::SERIES[s].dashed, o.grid = s == 3;
    Line *L = line_chart(plot, t, o);
    lv_obj_add_flag(L->root, LV_OBJ_FLAG_IGNORE_LAYOUT);
  }
  // highlight: the "now" bar look on a strip of bars
  for (int s = -1; s < ch::N_SERIES; s++) {
    lv_obj_t *r = row(c, sp::s3);
    lv_obj_t *sw = box(r);
    lv_obj_set_size(sw, ch::legend_sw * 2, ch::legend_sw);
    lv_obj_set_style_radius(sw, rad::chip, 0);
    const lv_color_t col = s < 0 ? ch::ink::highlight(t) : ch::series(t, s);
    if (s >= 0 && ch::SERIES[s].dashed) outline(sw, col, stroke::hair);
    else fill(sw, col);
    char b[96];
    snprintf(b, sizeof b, "%s · %.1f:1 on the card, %.1f:1 on the canvas", s < 0 ? "Highlight" : s == 3 ? "Compare" : ch::SERIES[s].label,
             ratio(col, t.surface), ratio(col, t.bg));
    text(r, ty::c_meta_sm, b, t.text_dim);
  }
}
void colours(lv_obj_t *l) {
  lv_obj_t *e = entry(l);
  spec(e, "Series", "Three colours for up to three series and one compare style (last week, yesterday: dashed or outline only). The highlight is the accent: now, the latest bar, the value that matters. Charts need 3:1 against the card and the canvas; the numbers are the panel's 16-bit colours. Colour never works alone: a legend or a label always names the series.");
  const int w = inner_w();
  colour_card(e, *T, w);
  colour_card(e, *theme(!T->light, T->accent_set), w);
  lv_obj_t *q = entry(l);
  spec(q, "Everything else on a chart", "The rest of a chart takes its colours from the theme, so it changes with light and dark and with the accent.");
  for (int i = 0; i < ch::N_ROLES; i++) {
    lv_obj_t *r = row(q, sp::s3);
    lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_t *sw = box(r);
    lv_obj_set_size(sw, ch::legend_sw * 2, ty::STYLES[ty::c_meta].box);
    lv_obj_t *mark = rect(sw, 0, (ty::STYLES[ty::c_meta].box - ch::legend_sw) / 2, ch::legend_sw * 2, ch::legend_sw, ch::ink_of(*T, i));
    lv_obj_set_style_radius(mark, rad::chip, 0);
    char b[160];
    snprintf(b, sizeof b, "%c%s: %s", ch::ROLES[i].id[0] - 32, ch::ROLES[i].id + 1, ch::ROLES[i].about);
    text(r, ty::c_meta, b, T->text_dim, inner_w() - ch::legend_sw * 2 - sp::s3);
  }
}
void lines(lv_obj_t *l) {
  const int w = inner_w(), cw = card_in(w);
  {
    lv_obj_t *e = entry(l);
    char b[200];
    spec(e, "Line weight", "The main line is a little heavier on the bigger panel, so both read the same at arm's length; the dot marks the latest reading. Sparklines and extra series use the thin line, shown second.");
    for (int wt : {ch::line, ch::line_thin}) {
      lv_obj_t *c = card(e, *T, w);
      LineOpts o{cw, ch::chart_h, day(24)};
      o.weight = wt, o.area = true;
      line_chart(c, *T, o);
      hint(c, *T, wt == ch::line ? "main line" : "thin line");
    }
  }
  {
    lv_obj_t *e = entry(l);
    spec(e, "Area, grid and now", ch::AREA_ABOUT);
    lv_obj_t *c = card(e, *T, w);
    LineOpts o{cw, ch::chart_h, day(24)};
    o.area = true, o.now = 14;
    std::vector<float> v = o.v;
    for (int i = 15; i < 24; i++) o.v[i] = NAN;  // the rest of the day is still to come
    line_chart(c, *T, o);
    hint(c, *T, "the dotted line is now");
  }
  {
    lv_obj_t *e = entry(l);
    spec(e, "Sparkline", "Small and thin inside a card or row: no grid, the end dot only.");
    lv_obj_t *c = card(e, *T, w);
    lv_obj_t *r = row(c, sp::s5);
    big_number(r, *T, "outside", "17.4", "°C");
    LineOpts o{cw / 2, ch::spark_h, day(24)};
    o.weight = ch::line_thin, o.grid = false;
    line_chart(r, *T, o);
  }
}
void bars_section(lv_obj_t *l) {
  const int w = inner_w(), cw = card_in(w);
  {
    lv_obj_t *e = entry(l);
    spec(e, "Bars", "Hourly bars: the data end is rounded, the foot square on the baseline. The current hour is the highlight, later hours are outlines. Press and drag to read a bar.");
    lv_obj_t *c = card(e, *T, w);
    std::vector<float> v = power_day();
    std::vector<uint8_t> k(24, 0);
    k[14] = 1;
    for (int i = 15; i < 24; i++) k[i] = 2;
    bar_chart(c, *T, cw, ch::chart_h, v, k, 1.6f, true, 14);
    const int s[3] = {2, -1, 3};
    const char *n[3] = {"used", "now", "forecast"};
    legend(c, *T, s, n, 3);
  }
  {
    lv_obj_t *e = entry(l);
    spec(e, "Gauge", "A value arc in the highlight over a thin track (the Dial's look without the knob).");
    lv_obj_t *r = row(e, sp::s7);
    gauge(r, *T, 64, "%");
    gauge(r, *T, 18, "kW");
  }
}
void labels(lv_obj_t *l) {
  const int w = inner_w(), cw = card_in(w);
  lv_obj_t *e = entry(l);
  char sb[400];
  snprintf(sb, sizeof sb, "The big number with its unit beside it, small dim time labels under the chart, values above their grid line and a legend that names every line. Press and drag across the chart: the reading follows your finger point by point, with a tick for each. %s", ch::SCRUB_PITCH_ABOUT);
  spec(e, "Numbers, axis, legend, readout", sb);
  lv_obj_t *c = card(e, *T, w);
  big_number(c, *T, "outside", "17.4", "°C");
  LineOpts o{cw, ch::chart_h, day(24)};
  o.area = true, o.scrub = true;
  Line *L = line_chart(c, *T, o);
  const char *yl[3] = {"22°", "17°", "12°"};
  y_labels(L->root, *T, L, yl);
  const char *al[3] = {"00:00", "12:00", "now"};
  const float at[3] = {0, 0.5f, 1};
  axis(c, *T, cw, al, at, 3);
  const int s[2] = {0, 3};
  const char *n[2] = {"today", "yesterday"};
  legend(c, *T, s, n, 2);
}
void states(lv_obj_t *l) {
  const int w = inner_w(), cw = card_in(w);
  auto words = [&](lv_obj_t *root, const char *s) {
    lv_obj_t *t = text(root, ty::c_meta, s, ch::ink::axis(*T));
    lv_obj_center(t);
    return t;
  };
  {
    lv_obj_t *e = entry(l);
    spec(e, "No data yet", "Grid and baseline stay so the card keeps its shape; a short line of words in the middle.");
    lv_obj_t *c = card(e, *T, w);
    LineOpts o{cw, ch::chart_h, {}};
    Line *L = line_chart(c, *T, o);
    words(L->root, "No data yet");
  }
  {
    lv_obj_t *e = entry(l);
    spec(e, "Unavailable", "The sensor is gone: the last known line turns to the muted ink, no fill, no dot, and the word Unavailable where the dot was.");
    lv_obj_t *c = card(e, *T, w);
    LineOpts o{cw, ch::chart_h, day(24)};
    for (int i = 17; i < 24; i++) o.v[i] = NAN;
    o.unavailable = true;
    Line *L = line_chart(c, *T, o);
    lv_obj_t *t = text(L->root, ty::c_meta, "Unavailable", ch::ink::axis(*T));
    lv_obj_update_layout(t);
    lv_obj_set_pos(t, L->w - lv_obj_get_width(t), gt(L));
  }
  {
    lv_obj_t *e = entry(l);
    spec(e, "Loading", "While history loads: a flat placeholder line breathing (the Pulse pattern; still with reduced motion).");
    lv_obj_t *c = card(e, *T, w);
    LineOpts o{cw, ch::chart_h, {}};
    Line *L = line_chart(c, *T, o);
    lv_obj_t *p = rect(L->root, 0, (gt(L) + gb(L)) / 2, cw, ch::line, ch::ink::loading(*T));
    lv_obj_set_style_radius(p, LV_RADIUS_CIRCLE, 0);
    fw::Anim a{set_opa, 255, (int32_t) (mo::OPA_PULSE_MIN * 255), mo::OPA};
    a.loop = true;
    fw::animate(p, mo::pulse, a);
  }
  {
    lv_obj_t *e = entry(l);
    spec(e, "Gaps", "Missing readings break the line; nothing is drawn across a gap, a lone reading is a short dot.");
    lv_obj_t *c = card(e, *T, w);
    LineOpts o{cw, ch::chart_h, day(24)};
    for (int i : {5, 6, 7, 11, 13}) o.v[i] = NAN;
    o.area = true;
    line_chart(c, *T, o);
  }
}

// ---------------------------------------------------------------- Motion page section "Charts" (foundations/motion.cpp)
struct Demo { Line *L = nullptr; Bars *B = nullptr; Gauge *G = nullptr; lv_obj_t *num = nullptr, *card = nullptr; int k = 0; lv_timer_t *loop = nullptr; ch::MId m; bool away = false; };
// a tap replays the demo; it reports whether it ends at its preset (Darren 2026-10-22): a replay that ends where it
// started (the line draws in again, the same bars grow again) leaves it at its preset, so the hosts' demo reset is
// skipped (no reset step, the replay plays straight away); new readings, a morph or a new gauge value move it away,
// and the reset brings it back as before
void demo_play(Demo *d, bool report = true) {  // report = false: the first show at build (no interaction)
  d->k++;
  if (d->m == ch::M_bars) d->away = d->away || d->k % 2 == 0;  // odd = the same bars again, even = another day
  else d->away = d->m != ch::M_draw;
  if (report && d->card && !d->loop) fw::demo_away(d->card, d->away);
  switch (d->m) {
    case ch::M_draw: draw_in(d->L); break;
    case ch::M_shift:
    case ch::M_ping: {
      // readings step clearly up / down (about 3° from the last) so the dot's move reads in the demo (Darren 2026-10-22)
      const float nv = std::clamp(temp_at(23 + d->k) + (d->k % 2 ? 1.6f : -1.6f), 12.5f, 21.5f);
      push(d->L, nv);
      if (d->num) { char b[16]; snprintf(b, sizeof b, "%.1f", nv); set_text(d->num, ty::c_number, b); }
      break;
    }
    case ch::M_morph: morph(d->L, day(24, d->k * 7)); break;
    case ch::M_bars: if (d->k % 2) bars_play(d->B); else { std::vector<float> v = power_day(d->k * 5); bars_play(d->B, &v); } break;
    case ch::M_gauge: gauge_to(d->G, (float) ((d->k * 37) % 90 + 8)); break;
    default: break;
  }
}
const char *lite_text(ch::Lite m) { return ch::LITE_MODES[m]; }
lv_obj_t *demo(lv_obj_t *parent, ch::MId m, int w, int h, bool loop) {
  auto *d = new Demo();
  d->m = m;
  lv_obj_t *c = d->card = card(parent, *T, w);
  lv_obj_set_height(c, LV_SIZE_CONTENT);
  lv_obj_add_event_cb(c, [](lv_event_t *e) {
    auto *d = (Demo *) lv_event_get_user_data(e);
    if (d->loop) lv_timer_delete(d->loop);
    delete d;
  }, LV_EVENT_DELETE, d);
  const int cw = card_in(w);
  if (m == ch::M_bars) {
    std::vector<uint8_t> k(24, 0);
    k[23] = 1;
    d->B = bar_chart(c, *T, cw, h, power_day(), k, 1.6f, false);
  } else if (m == ch::M_gauge) {
    lv_obj_t *r = row(c, 0);
    lv_obj_set_flex_align(r, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    d->G = gauge(r, *T, 42, "%");
  } else {
    if (m == ch::M_ping) {
      lv_obj_t *r = row(c, sp::s5);
      lv_obj_t *b = column(r, sp::s2, LV_SIZE_CONTENT);
      text(b, ty::c_caps_sm, "outside", T->text_dim);
      lv_obj_t *nr = row(b, sp::s2, LV_SIZE_CONTENT);
      lv_obj_set_flex_align(nr, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
      d->num = text(nr, ty::c_number, "17.4", T->text);
      text(nr, ty::c_subtitle, "°C", T->text_dim);
      LineOpts o{cw / 2, ch::spark_h * 2, day(24)};
      o.weight = ch::line_thin, o.grid = false;
      d->L = line_chart(r, *T, o);
    } else {
      LineOpts o{cw, h, day(24)};
      o.area = true, o.scrub = m == ch::M_scrub;
      d->L = line_chart(c, *T, o);
    }
  }
  if (m == ch::M_scrub) hint(c, *T, "press and drag across the chart");
  else {
    hint(c, *T, loop ? "loops by itself" : "tap to play");
    on_tap(c, [](lv_event_t *e) { demo_play((Demo *) lv_event_get_user_data(e)); }, d);
    if (loop) d->loop = lv_timer_create([](lv_timer_t *t) { demo_play((Demo *) lv_timer_get_user_data(t), false); }, ch::MOTION[m].ms + ch::MOTION[m].stagger * 24 + 300, d);
    else if (m == ch::M_draw || m == ch::M_bars) demo_play(d, false);  // first show plays once
  }
  return c;
}
ch::MId motion_of(const char *s) {
  for (int i = 0; i < ch::N_MOTION; i++) if (!strcmp(ch::MOTION[i].id, s)) return (ch::MId) i;
  return ch::N_MOTION;
}
}  // namespace

namespace fw::chart {
void motion_section(lv_obj_t *l, bool reduced, int lite_mode) {
  g_lite = lite_mode;
  lv_obj_t *e0 = entry(l);
  char b[320];
  snprintf(b, sizeof b, "Charts move to show what changed, never for show: a line draws in once, new readings slide in, bars grow, gauges sweep. %s Lighter motion is %s on this panel.", ch::LITE_SAY, lite() ? "on" : "off");
  spec(e0, "Charts", b);
  for (int i = 0; i < ch::N_MOTION; i++) {
    const ch::Motion &k = ch::MOTION[i];
    lv_obj_t *e = entry(l, sp::s2);
    char m[160];
    int n = snprintf(m, sizeof m, "%u ms · %s", (unsigned) k.ms, mo::EASINGS[k.easing].id);
    if (k.stagger) snprintf(m + n, sizeof m - n, " · stagger %u ms", (unsigned) k.stagger);
    lv_obj_t *h = row(e, sp::s5);
    text(h, ty::c_caps_sm, k.label, T->text);
    text(h, ty::c_meta, m, T->text_dim);
    text(e, ty::c_meta, k.animates, T->text_dim, inner_w());
    snprintf(b, sizeof b, "With reduced motion%s: %s. On the 480 panel: %s.", reduced ? " (on now)" : "", mo::REDUCED_MODES[k.reduced], lite_text(k.lite));
    text(e, ty::c_meta, b, reduced ? T->accent_text : T->text_dim, inner_w());
    snprintf(b, sizeof b, "%s", k.cost);
    text(e, ty::c_meta, b, T->text_dim, inner_w());
    demo(e, (ch::MId) i, inner_w(), ch::chart_h, false);
  }
}
}  // namespace fw::chart

FW_COMPONENT(chart, "Charts", "foundations", R"({
  "about": "Series colours, line weights, areas, grid, dots, labels and the no-data states every chart shares. Chart motion is on the Motion page.",
  "layout": "doc",
  "frame": [480, 0],
  "props": [
    {"name": "section", "about": "Which part of the charts page to show, such as the series colours, the lines or the no-data states.", "type": "select", "options": )" FW_CHART_SECTION_IDS R"(, "default": "colours"},
    {"name": "loop", "about": "For previews only: plays one chart movement over and over, such as a line drawing in or bars growing. off keeps them still.", "type": "select", "options": ["off", "draw", "shift", "morph", "bars", "gauge", "ping"], "default": "off"},
    {"name": "lite", "about": "A lighter way of drawing charts for smaller panels. auto picks it on the smaller panel, on and off force it.", "type": "select", "options": ["auto", "on", "off"], "default": "auto"}
  ],
  "variants": )" FW_CHART_SECTIONS R"(
})") {
  const char *lt = p["lite"] | "auto";
  g_lite = !strcmp(lt, "on") ? 1 : !strcmp(lt, "off") ? 0 : -1;
  const ch::MId lp = motion_of(p["loop"] | "off");
  if (lp != ch::N_MOTION) {  // one chart motion, full screen width, over and over (frame-rate bench)
    lv_obj_t *l = page(parent);
    demo(l, lp, inner_w(), ch::chart_h, true);
    return;
  }
  const char *s = p["section"] | "colours";
  lv_obj_t *l = page(parent);
  if (!strcmp(s, "lines")) lines(l);
  else if (!strcmp(s, "bars")) bars_section(l);
  else if (!strcmp(s, "labels")) labels(l);
  else if (!strcmp(s, "states")) states(l);
  else colours(l);
}
