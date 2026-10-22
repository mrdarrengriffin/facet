// Foundations > Smooth strokes (lab, board facet-145; WIP experiment, nothing else uses it yet): the SAME curves drawn
// four ways side by side, so Darren can pick how Facet draws curves (Motion easing plots, mascot lines, the vacuum
// floorplan of facet-144). The old Facet docs drew these as browser SVG (float coordinates, AA joins), so they were
// smooth there; on LVGL they were 33 integer points through lv_line and look stepped.
//   current  33 points snapped to whole pixels, lv_line with round caps (Motion today)
//   dense    one point per pixel of length, still snapped, lv_line round caps
//   atom     fw::stroke_fn / fw::stroke_path (core/atoms.h): analytic AA stroke in float, cached A8 mask, recoloured
//   alt      the circle: lv_arc (LVGL's own AA arc, no buffer); the other curves: dense with square caps (the joins
//            without the overlapping round-cap fringes)
// Each sample is labelled with its draw cost (the stroke object alone redrawn into an off-screen RGB565 layer over its
// plot, best of 2 averaged rounds, measured during the build so the labels are final), the atom's one-off raster time,
// and memory (points or mask bytes). WASM numbers are this machine's Chromium; on a panel the page measures itself
// (esp_timer). The console gets one line per sample (+ "ink" = pixels the stroke changed, a check that it drew).
// Playground: curve (all = every curve at four across; one = that curve at two across, bigger) and stroke (token).
// Not a doc page (doc pages have no playground): preview + playground + one variant per curve.
#include <cstdio>
#include "fw.h"
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
static double now_ms() { return emscripten_get_now(); }
#elif defined(ESP_PLATFORM)
#include "esp_timer.h"
static double now_ms() { return esp_timer_get_time() / 1000.0; }
#else
static double now_ms() { return lv_tick_get(); }
#endif

namespace {
using namespace fw;

constexpr float W = 480;
constexpr int REPS = 40;  // max redraws averaged per measurement round

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
lv_obj_t *wrap_row(lv_obj_t *parent, int gap) {
  lv_obj_t *r = box(parent);
  lv_obj_set_size(r, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_style_pad_column(r, gap, 0);
  lv_obj_set_style_pad_row(r, gap, 0);
  return r;
}
lv_obj_t *entry(lv_obj_t *list, int gap) {
  lv_obj_t *e = column(list, gap);
  lv_obj_set_style_pad_ver(e, sp::s5, 0);
  lv_obj_set_style_border_side(e, LV_BORDER_SIDE_BOTTOM, 0);
  lv_obj_set_style_border_width(e, stroke::hair, 0);
  lv_obj_set_style_border_color(e, T->line, 0);
  return e;
}
int inner_w() { return px(W) - 2 * sp::s7; }
void free_on_delete(lv_obj_t *o, void *mem) {
  lv_obj_add_event_cb(o, [](lv_event_t *e) { delete (std::vector<lv_point_precise_t> *) lv_event_get_user_data(e); }, LV_EVENT_DELETE, mem);
}

// ---------------------------------------------------------------- the curves, in a unit square (y down)
enum Kind { EASE_OUT, EASE_IN_OUT, ARC, WAVE, PLAN, NK };
const char *const IDS[NK] = {"ease-out", "ease-in-out", "arc", "wave", "floorplan"};
const char *const ABOUT[NK] = {
    "mo::ease_out from the LVGL path callback (Motion's easing plot).",
    "mo::ease_in_out from the LVGL path callback.",
    "A 270 degree circle arc (dial track, mascot arches).",
    "A gentle wave: a mascot-like hand-drawn line.",
    "A room outline turned 6 degrees: near-flat walls + corners (vacuum floorplan, facet-144)."};
const StrokePt PLAN_PTS[] = {{.08f, .2f}, {.55f, .2f}, {.55f, .08f}, {.92f, .08f}, {.92f, .7f}, {.72f, .92f},
                             {.36f, .92f}, {.36f, .78f}, {.08f, .78f}, {.08f, .2f}};
constexpr int N_PLAN = sizeof(PLAN_PTS) / sizeof(PLAN_PTS[0]);
int easing_index(const char *id) {
  for (int i = 0; i < mo::N_EASINGS; i++)
    if (!strcmp(mo::EASINGS[i].id, id)) return i;
  return 0;
}
float eased(int e, float t) {  // the very callback LVGL runs, 0..1 (its own resolution: 1/1024)
  lv_anim_t a;
  lv_anim_init(&a);
  a.act_time = (int32_t) std::lround(t * 1024);
  a.duration = 1024;
  a.start_value = 0;
  a.end_value = 1024;
  return mo::EASINGS[e].path(&a) / 1024.f;
}
StrokePt rot(StrokePt p) {
  const float a = 6.f * 3.14159265f / 180.f, c = std::cos(a), s = std::sin(a);
  return {.5f + (p.x - .5f) * c - (p.y - .5f) * s, .5f + (p.x - .5f) * s + (p.y - .5f) * c};
}
StrokePt unit(Kind k, float t) {
  switch (k) {
    case EASE_OUT: return {t, 1 - eased(easing_index("ease-out"), t)};
    case EASE_IN_OUT: return {t, 1 - eased(easing_index("ease-in-out"), t)};
    case ARC: {
      const float a = (135.f + 270.f * t) * 3.14159265f / 180.f;
      return {.5f + .5f * std::cos(a), .5f + .5f * std::sin(a)};
    }
    case WAVE: return {t, .5f - .3f * std::sin(2 * 3.14159265f * 1.5f * t)};
    default: {  // floorplan: along the polyline by segment index
      const float f = t * (N_PLAN - 1);
      const int i = std::min(N_PLAN - 2, (int) f);
      const float u = f - i;
      return rot({PLAN_PTS[i].x + (PLAN_PTS[i + 1].x - PLAN_PTS[i].x) * u, PLAN_PTS[i].y + (PLAN_PTS[i + 1].y - PLAN_PTS[i].y) * u});
    }
  }
}
struct Map { Kind k; float m, s; };  // unit square -> plot px: inset m (caps stay inside), side s
StrokePt mapped(float t, void *u) {
  const Map *mp = (const Map *) u;
  const StrokePt p = unit(mp->k, t);
  return {mp->m + p.x * mp->s, mp->m + p.y * mp->s};
}
float length(const Map &mp) {
  float len = 0;
  StrokePt q = mapped(0, (void *) &mp);
  for (int i = 1; i <= 256; i++) {
    const StrokePt c = mapped(i / 256.f, (void *) &mp);
    len += std::sqrt((c.x - q.x) * (c.x - q.x) + (c.y - q.y) * (c.y - q.y));
    q = c;
  }
  return len;
}

// ---------------------------------------------------------------- samples
enum Approach { CURRENT, DENSE, ATOM, ALT, NA };
struct Sample { lv_obj_t *plot = nullptr, *stroke = nullptr, *nums = nullptr; Approach a; Kind k; Map mp; int in = 0, width = 0, bytes = 0; double raster = -1; };
struct Page { std::vector<Sample> s; };

lv_obj_t *line(lv_obj_t *plot, const Map &mp, int n, bool round, int width, int *bytes) {
  auto *pts = new std::vector<lv_point_precise_t>(n);
  if (mp.k == PLAN && n == N_PLAN) {  // the polyline's own vertices
    for (int i = 0; i < n; i++) {
      const StrokePt r = rot(PLAN_PTS[i]);
      (*pts)[i] = {(lv_value_precise_t) std::lround(mp.m + r.x * mp.s), (lv_value_precise_t) std::lround(mp.m + r.y * mp.s)};
    }
  } else {
    for (int i = 0; i < n; i++) {
      const StrokePt p = mapped((float) i / (n - 1), (void *) &mp);
      (*pts)[i] = {(lv_value_precise_t) std::lround(p.x), (lv_value_precise_t) std::lround(p.y)};
    }
  }
  lv_obj_t *ln = lv_line_create(plot);
  free_on_delete(ln, pts);
  lv_line_set_points(ln, pts->data(), n);
  lv_obj_set_style_line_width(ln, width, 0);
  lv_obj_set_style_line_color(ln, T->accent, 0);
  lv_obj_set_style_line_rounded(ln, round, 0);
  *bytes = n * (int) sizeof(lv_point_precise_t);
  return ln;
}
lv_obj_t *arc(lv_obj_t *plot, const Map &mp, int width) {
  lv_obj_t *a = lv_arc_create(plot);
  lv_obj_remove_style_all(a);
  lv_obj_remove_flag(a, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  const int d = (int) std::lround(mp.s) + width;  // lv_arc draws inward from its box: centre line on the same circle
  lv_obj_set_size(a, d, d);
  lv_obj_set_pos(a, (int) std::lround(mp.m) - width / 2, (int) std::lround(mp.m) - width / 2);
  lv_arc_set_rotation(a, 0);
  lv_arc_set_bg_angles(a, 135, 45);
  lv_obj_set_style_arc_width(a, width, 0);
  lv_obj_set_style_arc_color(a, T->accent, 0);
  lv_obj_set_style_arc_rounded(a, true, 0);
  lv_obj_set_style_arc_opa(a, LV_OPA_COVER, 0);
  lv_obj_set_style_arc_opa(a, LV_OPA_TRANSP, LV_PART_INDICATOR);
  return a;
}
const char *name(Approach a, Kind k) {
  switch (a) {
    case CURRENT: return k == PLAN ? "Current: lv_line" : "Current: 33 pts";
    case DENSE: return "Dense: 1 pt / px";
    case ATOM: return "Atom: AA mask";
    case ALT: return k == ARC ? "Alt: lv_arc" : "Alt: square caps";
    default: return "";
  }
}
void kb(char *b, size_t n, int bytes) {
  if (bytes >= 1024) snprintf(b, n, "%.1f KB", bytes / 1024.0);
  else snprintf(b, n, "%d B", bytes);
}

lv_obj_t *atom(lv_obj_t *plot, const Sample &s) {
  if (s.k == PLAN) {  // the polyline's own float vertices: exact corners
    StrokePt pts[N_PLAN];
    for (int i = 0; i < N_PLAN; i++) {
      const StrokePt r = rot(PLAN_PTS[i]);
      pts[i] = {s.mp.m + r.x * s.mp.s, s.mp.m + r.y * s.mp.s};
    }
    return stroke_path(plot, s.in, s.in, pts, N_PLAN, s.width, T->accent);
  }
  return stroke_fn(plot, s.in, s.in, mapped, (void *) &s.mp, s.width, T->accent);
}

void sample(Page *pg, lv_obj_t *parent, Kind k, Approach a, int P, int width) {
  lv_obj_t *cell = column(parent, sp::s2, P);
  lv_obj_t *plot = box(cell);
  lv_obj_set_size(plot, P, P);
  lv_obj_set_style_bg_color(plot, T->surface, 0);
  lv_obj_set_style_bg_opa(plot, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(plot, stroke::hair, 0);
  lv_obj_set_style_border_color(plot, T->line, 0);
  const int in = P - 2 * stroke::hair;
  const float m = width + sp::s2;
  const Map mp{k, m, in - 2 * m};
  Sample s;
  s.plot = plot;
  s.a = a;
  s.k = k;
  const int dense = (int) std::ceil(length(mp)) + 1;
  switch (a) {
    case CURRENT: s.stroke = line(plot, mp, k == PLAN ? N_PLAN : 33, true, width, &s.bytes); break;
    case DENSE: s.stroke = line(plot, mp, dense, true, width, &s.bytes); break;
    case ALT:
      if (k == ARC) s.stroke = arc(plot, mp, width);
      else s.stroke = line(plot, mp, k == PLAN ? N_PLAN : dense, false, width, &s.bytes);
      break;
    default:
      s.mp = mp;
      s.in = in;
      s.width = width;
      s.stroke = atom(plot, s);
      s.bytes = stroke_bytes(s.stroke);
  }
  text(cell, ty::c_caps_sm, name(a, k), T->text, P);
  s.nums = text(cell, ty::c_meta_sm, "measuring", T->text_dim, P);
  pg->s.push_back(s);
}

// draw cost of the stroke alone: lv_obj_redraw of the stroke object into an off-screen RGB565 layer over the plot's
// area (what a refresh of that area spends on it: lv_line segments / lv_arc / the A8 image blend), averaged over REPS
double redraw(lv_obj_t *cv, lv_obj_t *stroke, lv_obj_t *plot) {
  lv_area_t a;
  lv_obj_get_coords(plot, &a);
  double best = 1e9;
  for (int round = 0; round < 2; round++) {  // best of 2 rounds, each up to REPS redraws or ~6 ms
    const double t0 = now_ms();
    int i = 0;
    for (; i < REPS && (i < 3 || now_ms() - t0 < 6); i++) {
      lv_layer_t layer;
      lv_canvas_init_layer(cv, &layer);
      lv_area_t buf = layer.buf_area;  // canvas-sized at (0,0): move it onto the plot's screen area
      buf.x1 += a.x1, buf.x2 += a.x1, buf.y1 += a.y1, buf.y2 += a.y1;
      layer.buf_area = layer._clip_area = layer.phy_clip_area = buf;
      lv_obj_redraw(&layer, stroke);
      lv_canvas_finish_layer(cv, &layer);
    }
    best = std::min(best, (now_ms() - t0) / i);
  }
  return best;
}
// measured during the build (after layout), so the labels are final before the workbench sizes the stage
void measure(Page *pg) {
  if (pg->s.empty()) return;
  const int P = lv_obj_get_width(pg->s[0].plot);
  lv_draw_buf_t *db = lv_draw_buf_create(P, P, LV_COLOR_FORMAT_RGB565, LV_STRIDE_AUTO);
  lv_obj_t *cv = lv_canvas_create(lv_obj_get_parent(pg->s[0].plot));
  lv_obj_add_flag(cv, (lv_obj_flag_t) (LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_IGNORE_LAYOUT));
  lv_canvas_set_draw_buf(cv, db);
  lv_canvas_fill_bg(cv, T->surface, LV_OPA_COVER);
  for (Sample &s : pg->s) redraw(cv, s.stroke, s.plot);  // warm-up pass (first draws pay for caches / JIT)
  for (Sample &s : pg->s) {
    lv_canvas_fill_bg(cv, T->surface, LV_OPA_COVER);
    const double draw = redraw(cv, s.stroke, s.plot);
    int ink = 0;  // console check that the stroke really drew: pixels that are no longer the surface colour
    const uint16_t bg = lv_color_to_u16(T->surface);
    for (uint32_t y = 0; y < db->header.h; y++)
      for (uint32_t x = 0; x < db->header.w; x++) ink += ((uint16_t *) (db->data + y * db->header.stride))[x] != bg;
    if (s.a == ATOM) {  // the one-off raster (sampling + mask + image object), averaged over a few rebuilds
      const double t0 = now_ms();
      for (int i = 0; i < 8; i++) lv_obj_delete(atom(cv, s));
      s.raster = (now_ms() - t0) / 8;
    }
    char m[32], b[160];
    kb(m, sizeof m, s.bytes);
    if (s.a == ATOM) {
      char argb[32];
      kb(argb, sizeof argb, s.bytes * 4);
      snprintf(b, sizeof b, "raster %.2f ms once, draw %.3f ms, A8 mask %s (ARGB8888 %s)", s.raster, draw, m, argb);
    } else if (s.a == ALT && s.k == ARC) {
      snprintf(b, sizeof b, "draw %.3f ms, no buffer", draw);
    } else {
      snprintf(b, sizeof b, "draw %.3f ms, %d pts = %s", draw, s.bytes / (int) sizeof(lv_point_precise_t), m);
    }
    lv_label_set_text(s.nums, b);
    printf("strokes %s | %s | %s | ink %d px\n", IDS[s.k], name(s.a, s.k), b, ink);  // the browser console / panel log reads the numbers too
  }
  lv_obj_delete(cv);
  lv_draw_buf_destroy(db);
}

void curve(Page *pg, lv_obj_t *l, Kind k, bool big, int width) {
  lv_obj_t *e = entry(l, sp::s3);
  text(e, ty::c_caps_sm, IDS[k], T->text);
  text(e, ty::c_meta, ABOUT[k], T->text_dim, inner_w());
  const int cols = big ? 2 : 4;
  const int P = (inner_w() - (cols - 1) * sp::s4) / cols / 4 * 4;
  lv_obj_t *r = wrap_row(e, sp::s4);
  for (Approach a : {CURRENT, DENSE, ATOM, ALT})
    if (!(k == PLAN && a == DENSE)) sample(pg, r, k, a, P, width);
}

int stroke_of(const char *s) {
  if (!strcmp(s, "arc")) return stroke::arc;
  if (!strcmp(s, "indicator")) return stroke::indicator;
  if (!strcmp(s, "hair")) return stroke::hair;
  return stroke::edge;
}
}  // namespace

FW_COMPONENT(strokes, "Smooth strokes (lab)", "foundations", R"({
  "about": "Experiment (facet-145): the same curves drawn four ways, to pick how Facet draws smooth lines.",
  "frame": [480, 0],
  "props": [
    {"name": "curve", "type": "select", "options": ["all", "ease-out", "ease-in-out", "arc", "wave", "floorplan"], "default": "all"},
    {"name": "stroke", "type": "select", "options": ["edge", "arc", "indicator", "hair"], "default": "edge"}
  ],
  "variants": [
    {"label": "ease-out · edge", "props": {"curve": "ease-out"}},
    {"label": "ease-in-out · edge", "props": {"curve": "ease-in-out"}},
    {"label": "arc · arc", "props": {"curve": "arc", "stroke": "arc"}},
    {"label": "wave · indicator", "props": {"curve": "wave", "stroke": "indicator"}},
    {"label": "floorplan · edge", "props": {"curve": "floorplan"}}
  ]
})") {
  const char *c = p["curve"] | "all";
  const int width = stroke_of(p["stroke"] | "edge");
  lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_t *l = column(parent, 0, px(W));
  lv_obj_set_style_pad_hor(l, sp::s7, 0);
  auto *pg = new Page();
  lv_obj_add_event_cb(l, [](lv_event_t *ev) {
    auto *pg = (Page *) lv_event_get_user_data(ev);
    delete pg;
  }, LV_EVENT_DELETE, pg);
  lv_obj_t *e = entry(l, sp::s2);
  text(e, ty::c_caps_sm, "Smooth strokes: lab", T->text);
  char b[320];
  snprintf(b, sizeof b, "Stroke %d px. The old docs drew curves as browser SVG (float, AA joins). LVGL's lv_line snaps each point to "
           "whole pixels, even with LV_USE_FLOAT, and draws each segment on its own. Draw = the stroke alone redrawn "
           "into an off-screen layer, measured here (WASM on the ai box; on a panel with esp_timer).", width);
  text(e, ty::c_meta, b, T->text_dim, inner_w());
  bool any = false;
  for (int k = 0; k < NK; k++)
    if (!strcmp(c, "all") || !strcmp(c, IDS[k])) { curve(pg, l, (Kind) k, strcmp(c, "all") != 0, width); any = true; }
  if (!any) curve(pg, l, EASE_OUT, false, width);
  lv_obj_update_layout(parent);
  measure(pg);
}
