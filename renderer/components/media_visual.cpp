// Media visuals: the Media screen's picture and its Home card preview (2026-10-30: absorbs the Media disc, visual
// "disc": draw_disc below, the old media_disc's parts and tokens; size "card" = lay::mv_card tall, a square crop
// showing the right end as the screen does). The Facet media visuals (docs: Components > Controls > Media visuals, @facet/ui media-visuals/, CSS
// .f-mv-*): a cassette, a CD, or a 12" vinyl LP with its tonearm for the Media screen's hero
// slot, drawn from the real-world dimensions (docs specs.ts: IEC 60094-7 cassette, ECMA-130 CD, RIAA E4 LP, Technics
// SL-1200 arm), turning at the real tape and disc speeds while the track plays (CD slowed 16 x: docs; the docs'
// reel-to-reel was removed, Darren 2026-10-22), the tape packs (area-conserving) and the tonearm following the track's progress.
//   size     every visual is scaled so its height = lay::mv_h (256 / 168, the disc's slot; docs layout mv-h), shown
//            whole in its own box (docs mediaVisual(); on the Media screen it runs off the left edge, a screen job)
//   colours  the mv-* colour tokens (both themes), disc / groove; the cassette label stripes take the docs fallback
//            palette (accent, blue, warn, steel: cover-art colours need an image pipeline, later)
//   drawing  one object, one draw event, the docs SVG's layers in order with LVGL primitives (filled circles, arcs as
//            rings / annuli / sectors, rounded rects, triangles, lines). mm strokes under 1 px are drawn as 1 px.
// Motion: one frame clock (lv_anim, the weather icons' pattern) runs only while something moves: the spin eases up
// / down (docs: platter start 0.7 s, brake 1 s; tape ~0.25 s), the arm eases to its groove (docs tau 0.45 s). Each
// frame invalidates only what moved: the two cassette hubs, the CD's print mark, the arm's box. Reduced
// motion = nothing turns; packs and the arm jump to the track's place (docs: they still follow).
// Docs differences (UI pass later): no cover art (an image pipeline + 565 dithering is a separate job), so the CD's
// print band shows the mirror and the vinyl label is plain paper (its turn is invisible, so it is not redrawn); no
// iridescent / gloss sheens and no window glare (conic gradients and 7 % tints: not in the LVGL-safe subset / lost in
// 565); vinyl grooves drawn every sp::s1 (4 px) instead of the real 0.92 mm pitch (sub-pixel at this scale); the
// cassette label text uses the sans type styles (no mono in the type scale).
// A read-out (docs: no touch): no events, no sound. Update path: progress / playing / title move in place (packs,
// arm, spin ease); another visual refills (snap). Contrast pairs: tokens/colour.json section "media_visual".
#include "fw.h"

namespace {
constexpr float PI = 3.14159265f;
enum Kind { CASSETTE, CD, VINYL, DISC };
const char *KIND_IDS[] = {"cassette", "cd", "vinyl", "disc"};

// ---- the specs (mm), docs media-visuals/specs.ts
namespace cs {  // Compact Cassette
constexpr float w = 100.4f, h = 63.8f, corner = 3, hubPitch = 42.5f, hubY = 28.9f, hubR = 8.5f, boreR = 6.2f;
constexpr int teeth = 6;
constexpr float toothLen = 1.6f, toothW = 1.4f;
constexpr float labX = 5.2f, labY = 6.0f, labW = 90, labH = 40.4f, labR = 2;
constexpr float winX = 20.2f, winY = 20.4f, winW = 60, winH = 17, winR = 2.2f;
constexpr float screws[5][2] = {{4.2f, 3.9f}, {96.2f, 3.9f}, {3.2f, 49.9f}, {97.2f, 49.9f}, {50.2f, 48.9f}};
constexpr float screwR = 1.7f;
constexpr float head[4][2] = {{12.7f, 63.8f}, {19.2f, 53.4f}, {81.2f, 53.4f}, {87.7f, 63.8f}};
constexpr float openings[3][2] = {{30.7f, 37.7f}, {44.2f, 56.2f}, {62.7f, 69.7f}};
constexpr float openingDepth = 3.6f;
constexpr float capstan[2][2] = {{34.2f, 57.9f}, {66.2f, 57.9f}};
constexpr float capstanR = 1.8f;
constexpr float pins[2][2] = {{24.2f, 58.4f}, {76.2f, 58.4f}};
constexpr float pinSide = 2.6f;
constexpr float speed = 47.625f, tapeLength = 90000, tapeThickness = 0.016f;
}  // namespace cs
namespace cd {  // Compact Disc, diameters
constexpr float d = 120, hole = 15, clampIn = 26, clampOut = 33, stackRing = 34, infoIn = 44, infoOut = 117;
constexpr float printIn = 46, program = 50, programMax = 116, scan = 1300;
}  // namespace cd
namespace lp {  // 12" LP, diameters
constexpr float d = 301.6f, hole = 7.26f, outerGroove = 297.6f, music = 292.1f, lastMusic = 120.6f, lockGroove = 106.4f;
constexpr float label = 100, rpm = 100.0f / 3;
}  // namespace lp
namespace arm {  // Technics SL-1200 MK2
constexpr float effective = 230, offset = 22, baseR = 20, bearingR = 9, tubeW = 7.5f, weightFrom = 22, weightTo = 50;
constexpr float weightW = 30, restR = 168, pivot = 215;
}  // namespace arm

float full_radius(float core, float length, float thick) { return std::sqrt(core * core + length * thick / PI); }
void packs(float core, float full, float p, float *supply, float *takeup) {
  const float q = LV_CLAMP(0.0f, p, 1.0f), A = full * full - core * core;
  *supply = std::sqrt(core * core + (1 - q) * A);
  *takeup = std::sqrt(core * core + q * A);
}
float deg_per_sec(float v, float r) { return v / r * (180.0f / PI); }
float arm_angle(float Px, float Py, float L, float r, int side) {
  const float d = std::hypot(Px, Py);
  const float cosA = LV_CLAMP(-1.0f, (L * L + d * d - r * r) / (2 * L * d), 1.0f);
  return (std::atan2(-Py, -Px) + side * std::acos(cosA)) * (180.0f / PI);
}
float ease(float cur, float target, float dt, float tau) { return cur + (target - cur) * (1 - std::exp(-dt / std::max(0.001f, tau))); }

struct MV {
  Kind kind = CASSETTE;
  bool card = false;              // size card: the Home card preview (lay::mv_card tall, a square crop)
  float shown = 0, mark = 0;     // disc: the progress drawn (eases to progress), the marker angle (degrees)
  float k = 1, ox = 0, oy = 0;  // px per mm; the box's top-left in mm
  int W = 0, H = 0;
  float progress = 0;            // 0..1
  bool playing = false;
  float m = 0;                   // spin 0..1 (eases)
  float ang[2] = {0, 0};         // degrees
  float armA = 0, armT = 0;      // tonearm angle, target
  float Px = 0, Py = 0;          // vinyl pivot (mm)
  float sup = 0, tak = 0;        // pack radii (mm)
  std::string title, artist;
  uint32_t last = 0;
  bool running = false, starting = false;
};
MV *of(lv_obj_t *o) { return (MV *) lv_obj_get_user_data(o); }

// ---- drawing helpers: mm -> px, a manual cull against the layer's clip (a frame redraws small areas)
struct Ctx {
  lv_layer_t *l;
  lv_area_t o;  // object coords
  const MV *d;
  float X(float mm) const { return o.x1 + (mm - d->ox) * d->k; }
  float Y(float mm) const { return o.y1 + (mm - d->oy) * d->k; }
  float S(float mm) const { return mm * d->k; }
  bool visible(float x1, float y1, float x2, float y2) const {
    const lv_area_t &c = l->_clip_area;
    return x2 >= c.x1 - 1 && x1 <= c.x2 + 1 && y2 >= c.y1 - 1 && y1 <= c.y2 + 1;
  }
};
int px_w(const Ctx &c, float mm) { return std::max(1, (int) std::lround(c.S(mm))); }
void rrect(const Ctx &c, float x, float y, float w, float h, float r, lv_color_t col, lv_opa_t opa = LV_OPA_COVER, int bw = 0, lv_color_t bc = {}) {
  const float x1 = c.X(x), y1 = c.Y(y), x2 = c.X(x + w), y2 = c.Y(y + h);
  if (!c.visible(x1, y1, x2, y2)) return;
  lv_draw_rect_dsc_t dd;
  lv_draw_rect_dsc_init(&dd);
  dd.bg_color = col;
  dd.bg_opa = opa;
  dd.radius = (int32_t) std::lround(c.S(r));
  dd.border_width = bw;
  dd.border_color = bc;
  dd.border_opa = bw ? LV_OPA_COVER : LV_OPA_TRANSP;
  lv_area_t a = {(int32_t) std::lround(x1), (int32_t) std::lround(y1), (int32_t) std::lround(x2) - 1, (int32_t) std::lround(y2) - 1};
  lv_draw_rect(c.l, &dd, &a);
}
void outline_rr(const Ctx &c, float x, float y, float w, float h, float r, int bw, lv_color_t col, lv_opa_t opa = LV_OPA_COVER) {
  const float x1 = c.X(x), y1 = c.Y(y), x2 = c.X(x + w), y2 = c.Y(y + h);
  if (!c.visible(x1, y1, x2, y2)) return;
  lv_draw_rect_dsc_t dd;
  lv_draw_rect_dsc_init(&dd);
  dd.bg_opa = LV_OPA_TRANSP;
  dd.radius = (int32_t) std::lround(c.S(r));
  dd.border_width = bw;
  dd.border_color = col;
  dd.border_opa = opa;
  lv_area_t a = {(int32_t) std::lround(x1), (int32_t) std::lround(y1), (int32_t) std::lround(x2) - 1, (int32_t) std::lround(y2) - 1};
  lv_draw_rect(c.l, &dd, &a);
}
// a filled circle, or a ring: width px inward from the outer radius r (mm); a0 / a1 = a sector (LVGL degrees)
void arc(const Ctx &c, float cx, float cy, float r, int width, lv_color_t col, float a0 = 0, float a1 = 360, lv_opa_t opa = LV_OPA_COVER) {
  const float X = c.X(cx), Y = c.Y(cy), R = c.S(r);
  if (R < 0.5f || !c.visible(X - R, Y - R, X + R, Y + R)) return;
  lv_draw_arc_dsc_t ad;
  lv_draw_arc_dsc_init(&ad);
  ad.color = col;
  ad.opa = opa;
  ad.center = {(int32_t) std::lround(X), (int32_t) std::lround(Y)};
  ad.radius = (uint16_t) std::lround(R);
  ad.width = width <= 0 ? ad.radius : std::min<int32_t>(width, ad.radius);
  ad.start_angle = (lv_value_precise_t) a0;
  ad.end_angle = (lv_value_precise_t) a1;
  lv_draw_arc(c.l, &ad);
}
void disc(const Ctx &c, float cx, float cy, float r, lv_color_t col) { arc(c, cx, cy, r, 0, col); }
void ring(const Ctx &c, float cx, float cy, float r, float wmm, lv_color_t col, lv_opa_t opa = LV_OPA_COVER) {
  const int w = px_w(c, wmm);
  arc(c, cx, cy, r + wmm / 2, w, col, 0, 360, opa);  // centred on r like an SVG stroke
}
void annulus(const Ctx &c, float cx, float cy, float r1, float r2, lv_color_t col) {
  arc(c, cx, cy, r2, std::max(1, (int) std::lround(c.S(r2 - r1))), col);
}
void line(const Ctx &c, float x1, float y1, float x2, float y2, float wmm, lv_color_t col, bool round = true) {
  const float X1 = c.X(x1), Y1 = c.Y(y1), X2 = c.X(x2), Y2 = c.Y(y2), m = c.S(wmm);
  if (!c.visible(std::min(X1, X2) - m, std::min(Y1, Y2) - m, std::max(X1, X2) + m, std::max(Y1, Y2) + m)) return;
  lv_draw_line_dsc_t ld;
  lv_draw_line_dsc_init(&ld);
  ld.color = col;
  ld.width = px_w(c, wmm);
  ld.round_start = ld.round_end = round;
  ld.p1.x = (int32_t) std::lround(X1); ld.p1.y = (int32_t) std::lround(Y1);
  ld.p2.x = (int32_t) std::lround(X2); ld.p2.y = (int32_t) std::lround(Y2);
  lv_draw_line(c.l, &ld);
}
void tri(const Ctx &c, const float (*p)[2], lv_color_t col) {
  lv_draw_triangle_dsc_t td;
  lv_draw_triangle_dsc_init(&td);
  td.color = col;
  td.opa = LV_OPA_COVER;
  for (int i = 0; i < 3; i++) td.p[i] = {(int32_t) std::lround(c.X(p[i][0])), (int32_t) std::lround(c.Y(p[i][1]))};
  lv_draw_triangle(c.l, &td);
}
// a rotated bar: from a to b along angle `deg` about (cx, cy) in local mm, thickness w (mm): a thick line
void bar_rot(const Ctx &c, float cx, float cy, float deg, float a, float b, float off, float w, lv_color_t col, bool round = false) {
  const float r = deg * PI / 180, cs_ = std::cos(r), sn = std::sin(r);
  const float x1 = cx + a * cs_ - off * sn, y1 = cy + a * sn + off * cs_, x2 = cx + b * cs_ - off * sn, y2 = cy + b * sn + off * cs_;
  line(c, x1, y1, x2, y2, w, col, round);
}
void text(const Ctx &c, fw::ty::Id st, const std::string &s, float x, float base, bool right, lv_color_t col, lv_opa_t opa = LV_OPA_COVER) {
  if (s.empty()) return;
  const lv_font_t *f = fw::font(st);
  const fw::ty::Style &ts = fw::style(st);
  const std::string t = fw::cased(st, s.c_str());
  lv_point_t sz;
  lv_text_get_size(&sz, t.c_str(), f, ts.letter_space, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
  const int32_t X = (int32_t) std::lround(c.X(x) - (right ? sz.x : 0));
  const int32_t Y = (int32_t) std::lround(c.Y(base) - (f->line_height - f->base_line));
  lv_draw_label_dsc_t td;
  lv_draw_label_dsc_init(&td);
  td.font = f;
  td.color = col;
  td.opa = opa;
  td.letter_space = ts.letter_space;
  td.text = t.c_str();
  td.text_local = 1;
  lv_area_t a = {X, Y, X + sz.x, Y + f->line_height};
  lv_draw_label(c.l, &td, &a);
}
// ASCII dots: the label glyph sets may not carry the ellipsis
std::string clip_text(const std::string &v, size_t n) { return v.size() > n ? v.substr(0, n - 3) + "..." : v; }

// ---- 1 cassette
void draw_cassette(const Ctx &c) {
  const fw::Theme &t = *fw::T;
  const MV *d = c.d;
  const float hubX[2] = {cs::w / 2 - cs::hubPitch / 2, cs::w / 2 + cs::hubPitch / 2};
  rrect(c, 0, 0, cs::w, cs::h, cs::corner, t.mv_shell, LV_OPA_COVER, px_w(c, 0.35f), t.mv_shell_edge);
  outline_rr(c, 1.1f, 1.1f, cs::w - 2.2f, cs::h - 2.2f, cs::corner - 1, 1, t.mv_shell_edge, LV_OPA_70);
  // head area: a trapezoid (two triangles) + its edges
  const float h0[3][2] = {{cs::head[0][0], cs::head[0][1]}, {cs::head[1][0], cs::head[1][1]}, {cs::head[2][0], cs::head[2][1]}};
  const float h1[3][2] = {{cs::head[0][0], cs::head[0][1]}, {cs::head[2][0], cs::head[2][1]}, {cs::head[3][0], cs::head[3][1]}};
  tri(c, h0, t.mv_shell_2);
  tri(c, h1, t.mv_shell_2);
  for (int i = 0; i < 3; i++) line(c, cs::head[i][0], cs::head[i][1], cs::head[i + 1][0], cs::head[i + 1][1], 0.3f, t.mv_shell_edge, false);
  // label: paper, four stripes (the palette), two writing rules, the title / artist, side A, C60
  rrect(c, cs::labX, cs::labY, cs::labW, cs::labH, cs::labR, t.mv_paper);
  const lv_color_t pal[4] = {t.accent, t.blue, t.warn, t.steel};
  for (int i = 0; i < 4; i++) rrect(c, cs::labX, 38.4f + i * 2.0f, cs::labW, 1.6f, 0, pal[i]);
  for (float y : {12.2f, 17.6f}) line(c, cs::labX + 3, y, cs::labX + cs::labW - 3, y, 0.18f, t.mv_ink, false);
  text(c, fw::ty::c_label_sm, clip_text(d->title.empty() ? "Side A" : d->title, 26), 93.4f, 11.6f, true, t.mv_ink);
  text(c, fw::ty::c_caps_sm, clip_text(d->artist, 30), 93.4f, 17.0f, true, t.mv_ink);
  text(c, fw::ty::body, "A", 7.4f, 16.6f, false, t.mv_ink);
  text(c, fw::ty::c_caps_sm, "C60", 93.4f, 36.8f, true, t.mv_ink);
  // window: smoked plastic; inside it (clipped) the packs, the bores and the turning hubs
  rrect(c, cs::winX, cs::winY, cs::winW, cs::winH, cs::winR, t.mv_window);
  lv_area_t keep = c.l->_clip_area, win = {(int32_t) std::lround(c.X(cs::winX)), (int32_t) std::lround(c.Y(cs::winY)),
                                           (int32_t) std::lround(c.X(cs::winX + cs::winW)) - 1, (int32_t) std::lround(c.Y(cs::winY + cs::winH)) - 1};
  lv_area_t &cl = c.l->_clip_area;  // clip to the window (the packs are bigger than it)
  cl = {std::max(keep.x1, win.x1), std::max(keep.y1, win.y1), std::min(keep.x2, win.x2), std::min(keep.y2, win.y2)};
  if (cl.x1 <= cl.x2 && cl.y1 <= cl.y2) {
    const float pr[2] = {d->sup, d->tak};
    for (int i = 0; i < 2; i++) {
      disc(c, hubX[i], cs::hubY, pr[i], t.mv_tape);
      ring(c, hubX[i], cs::hubY, std::max(cs::hubR, pr[i] - 0.7f), 0.5f, t.mv_tape_hi, LV_OPA_70);
      disc(c, hubX[i], cs::hubY, cs::boreR, t.mv_gap);
      annulus(c, hubX[i], cs::hubY, cs::boreR, cs::hubR, t.mv_hub);
      for (int k = 0; k < cs::teeth; k++)  // drive teeth, pointing in from the bore
        bar_rot(c, hubX[i], cs::hubY, d->ang[i] - 90 + 360.0f / cs::teeth * k, cs::boreR + 0.2f, cs::boreR - cs::toothLen, 0, cs::toothW, t.mv_hub);
      ring(c, hubX[i], cs::hubY, cs::hubR - 0.9f, 0.3f, t.mv_window, LV_OPA_40);
      bar_rot(c, hubX[i], cs::hubY, d->ang[i] - 90, cs::hubR + 0.1f, cs::hubR - 1.4f, 0, 2.6f, t.mv_gap);  // leader clamp slot
    }
  }
  c.l->_clip_area = keep;
  // the window's rounded corners over the packs: a paper ring just outside its rounded rect, then the edge
  const float g = cs::winR * 0.35f + 1 / d->k;
  outline_rr(c, cs::winX - g, cs::winY - g, cs::winW + 2 * g, cs::winH + 2 * g, cs::winR + g, std::max(1, (int) std::ceil(c.S(g))), t.mv_paper);
  outline_rr(c, cs::winX, cs::winY, cs::winW, cs::winH, cs::winR, px_w(c, 0.45f), t.mv_shell_edge);
  // bottom edge: openings + the tape across them, capstan and pin holes, screws
  for (const auto &o : cs::openings) {
    rrect(c, o[0], cs::h - cs::openingDepth, o[1] - o[0], cs::openingDepth, 0, t.mv_gap);
    rrect(c, o[0], cs::h - 1.6f, o[1] - o[0], 0.9f, 0, t.mv_tape_hi);
  }
  for (const auto &h : cs::capstan) disc(c, h[0], h[1], cs::capstanR, t.mv_gap);
  for (const auto &h : cs::pins) rrect(c, h[0] - cs::pinSide / 2, h[1] - cs::pinSide / 2, cs::pinSide, cs::pinSide, 0.3f, t.mv_gap);
  for (const auto &s : cs::screws) {
    disc(c, s[0], s[1], cs::screwR, t.mv_screw);
    ring(c, s[0], s[1], cs::screwR, 0.2f, t.mv_shell_edge);
    line(c, s[0] - 1, s[1], s[0] + 1, s[1], 0.45f, t.mv_gap);
    line(c, s[0], s[1] - 1, s[0], s[1] + 1, 0.45f, t.mv_gap);
  }
}

// ---- 2 CD (label side up, no art)
void draw_cd(const Ctx &c) {
  const fw::Theme &t = *fw::T;
  const float C = cd::d / 2;
  disc(c, C, C, cd::d / 2, t.mv_clear);
  annulus(c, C, C, cd::infoIn / 2, cd::infoOut / 2, t.mv_mirror);
  // the printed mark that shows the turn (docs: a hairline just outside the metallised band's inner edge)
  bar_rot(c, C, C, c.d->ang[0] - 90, cd::infoIn / 2 + 0.4f, cd::infoIn / 2 + 0.4f + std::max(0.9f, 2 / c.d->k), 0, 0.5f, t.mv_clear_2);
  annulus(c, C, C, cd::clampIn / 2, cd::clampOut / 2, t.mv_clear_2);
  ring(c, C, C, cd::stackRing / 2, 0.7f, t.mv_clear_2);
  ring(c, C, C, cd::printIn / 2, 0.25f, t.mv_mirror);
  ring(c, C, C, cd::d / 2 - 0.15f - 0.15f, 0.3f, t.mv_clear_2);
  disc(c, C, C, cd::hole / 2, t.bg);  // the hole shows the canvas
  ring(c, C, C, cd::hole / 2 + 0.1f, 0.3f, t.mv_clear_2);
}

// ---- 3 vinyl + tonearm
void arm_box(const MV *d, float a, float *x0, float *y0, float *x1, float *y1);
void draw_vinyl(const Ctx &c) {
  const fw::Theme &t = *fw::T;
  const MV *d = c.d;
  const float R = lp::d / 2;
  disc(c, 0, 0, R, t.mv_vinyl);
  annulus(c, 0, 0, lp::outerGroove / 2, R, t.mv_vinyl_edge);
  annulus(c, 0, 0, lp::music / 2, lp::outerGroove / 2, t.mv_vinyl_gloss);
  const float pitch = fw::sp::s1 / d->k;  // grooves every 4 px (the real 0.92 mm pitch is sub-pixel here)
  for (float rg = lp::lastMusic / 2 + pitch / 2; rg < lp::music / 2; rg += pitch) ring(c, 0, 0, rg, 0.32f, t.mv_groove);
  for (float q : {0.22f, 0.47f, 0.7f}) ring(c, 0, 0, lp::lastMusic / 2 + q * (lp::music / 2 - lp::lastMusic / 2), 1.1f, t.mv_vinyl_gloss);
  annulus(c, 0, 0, lp::lockGroove / 2, lp::lastMusic / 2, t.mv_vinyl_gloss);
  ring(c, 0, 0, lp::lockGroove / 2, 0.5f, t.mv_groove);
  disc(c, 0, 0, lp::label / 2, t.mv_paper);
  ring(c, 0, 0, lp::label / 2 - 0.4f, 0.5f, t.mv_vinyl, LV_OPA_60);
  disc(c, 0, 0, lp::hole / 2 - 0.3f, t.mv_metal);
  // rest post, arm base, then the arm (pivot-local: along +x, rotated about the pivot)
  // (the first port, from the docs: Darren 2026-10-22 wants a design round on the vinyl later, this stays till then)
  const float L = arm::effective, restA = arm_angle(d->Px, d->Py, L, arm::restR, -1) * PI / 180;
  disc(c, d->Px + (L - 14) * std::cos(restA), d->Py + (L - 14) * std::sin(restA), 4.5f, t.mv_metal_2);
  disc(c, d->Px, d->Py, arm::baseR, t.mv_deck);
  ring(c, d->Px, d->Py, arm::baseR, 0.8f, t.mv_shell_edge);
  disc(c, d->Px, d->Py, arm::bearingR, t.mv_metal);
  const float A = d->armA;
  bar_rot(c, d->Px, d->Py, A, -arm::weightTo, -arm::weightFrom, 0, arm::weightW, t.mv_metal_2);
  bar_rot(c, d->Px, d->Py, A, -arm::weightFrom, L - 26, 0, arm::tubeW, t.mv_metal, true);
  // headshell at the arm's end, turned by the offset angle
  const float ar = A * PI / 180, hx = d->Px + L * std::cos(ar), hy = d->Py + L * std::sin(ar), hA = A + arm::offset;
  bar_rot(c, hx, hy, hA, -34, 6, 0, 17, t.mv_metal_2);
  const float hr = hA * PI / 180, ch = std::cos(hr), sh = std::sin(hr);
  line(c, hx + 2 * ch + 8.5f * sh, hy + 2 * sh - 8.5f * ch, hx + 10 * ch + 16 * sh, hy + 10 * sh - 16 * ch, 2.2f, t.mv_metal);
  bar_rot(c, hx, hy, hA, -6, 4, 0, 10, t.mv_gap);
}
// the arm's bounding box (screen px relative to the object) at angle a: weight end, tube, headshell + lift
void arm_box(const MV *d, float a, float *x0, float *y0, float *x1, float *y1) {
  const float r = a * PI / 180, L = arm::effective;
  const float pts[3][2] = {{d->Px - arm::weightTo * std::cos(r), d->Py - arm::weightTo * std::sin(r)}, {d->Px, d->Py},
                           {d->Px + L * std::cos(r), d->Py + L * std::sin(r)}};
  float mx0 = 1e9f, my0 = 1e9f, mx1 = -1e9f, my1 = -1e9f;
  for (const auto &p : pts) mx0 = std::min(mx0, p[0]), my0 = std::min(my0, p[1]), mx1 = std::max(mx1, p[0]), my1 = std::max(my1, p[1]);
  const float pad = 36;  // headshell (34 + its width) / weight half-width / lift, mm
  *x0 = (mx0 - pad - d->ox) * d->k; *y0 = (my0 - pad - d->oy) * d->k;
  *x1 = (mx1 + pad - d->ox) * d->k; *y1 = (my1 + pad - d->oy) * d->k;
}

// ---- 4 the Media disc (absorbed from media_disc, 2026-10-30): units = px at the screen size (lay::disc_screen), k
// scales them to the card. A dotted ring (72 dots over 355 degrees from 12 o'clock, stroke::arc, line-hi), the accent
// progress arc sp::s2 inside it (round caps), the face (disc, a hair line edge) with two groove rings
// (lay::disc_groove smaller each), the accent label spot with its hole in the canvas colour, the blue marker circling
// the face sp::s5 inside its edge (one turn per Motion disc-marker while playing).
void draw_disc(const Ctx &c) {
  const fw::Theme &t = *fw::T;
  const MV *d = c.d;
  const float D = (float) fw::lay::disc_screen, C = D / 2, F = (float) fw::lay::disc_face_screen;
  const int w = std::max(1, (int) std::lround(fw::stroke::arc * d->k)), hair = std::max(1, (int) std::lround(fw::stroke::hair * d->k));
  const float dot = fw::stroke::arc / 2.0f;
  for (int i = 0; i < 72; i++) {
    const float a = (270 + i * 355.0f / 71) * PI / 180;
    arc(c, C + (C - dot) * std::cos(a), C + (C - dot) * std::sin(a), std::max(dot, 0.5f / d->k), 0, t.line_hi);
  }
  if (d->shown > 0.001f) {
    const float r = C - fw::sp::s2;
    const float X = c.X(C), Y = c.Y(C), Rp = c.S(r);
    lv_draw_arc_dsc_t ad;
    lv_draw_arc_dsc_init(&ad);
    ad.color = t.accent;
    ad.center = {(int32_t) std::lround(X), (int32_t) std::lround(Y)};
    ad.radius = (uint16_t) std::lround(Rp);
    ad.width = w;
    ad.rounded = 1;
    ad.start_angle = 270;
    ad.end_angle = (lv_value_precise_t) (270 + 359 * d->shown);
    lv_draw_arc(c.l, &ad);
  }
  disc(c, C, C, F / 2, t.disc);
  arc(c, C, C, F / 2, hair, t.line);
  for (int i = 1; i <= 2; i++) arc(c, C, C, (F - i * fw::lay::disc_groove) / 2, hair, t.groove);
  disc(c, C, C, fw::lay::disc_spot_screen / 2.0f, t.accent);
  disc(c, C, C, fw::lay::disc_hole / 2.0f, t.bg);
  const float orb = F / 2 - fw::sp::s5, a = (270 + d->mark) * PI / 180;
  disc(c, C + orb * std::cos(a), C + orb * std::sin(a), fw::lay::disc_marker / 2.0f, t.blue);
}

void draw(lv_event_t *e) {
  auto *o = (lv_obj_t *) lv_event_get_current_target(e);
  Ctx c{lv_event_get_layer(e), {}, of(o)};
  lv_obj_get_coords(o, &c.o);
  const lv_area_t keep = c.l->_clip_area;
  if (c.d->card) {  // the card: a square crop, nothing past it
    lv_area_t &k = c.l->_clip_area;
    k = {LV_MAX(keep.x1, c.o.x1), LV_MAX(keep.y1, c.o.y1), LV_MIN(keep.x2, c.o.x2), LV_MIN(keep.y2, c.o.y2)};
    if (k.x1 > k.x2 || k.y1 > k.y2) { k = keep; return; }
  }
  switch (c.d->kind) {
    case CASSETTE: draw_cassette(c); break;
    case CD: draw_cd(c); break;
    case VINYL: draw_vinyl(c); break;
    case DISC: draw_disc(c); break;
  }
  c.l->_clip_area = keep;
}

// ---- geometry per visual + the pack radii / arm target from the progress
void layout(MV *d) {
  const float H = (float) (d->card ? fw::lay::mv_card : fw::lay::mv_h);
  switch (d->kind) {
    case DISC: d->k = H / fw::lay::disc_screen; d->ox = 0; d->oy = 0; d->W = (int) H; break;
    case CASSETTE:  // a pixel to spare (H - 1): the bottom edge's lines anti-alias one row past it
      d->k = (H - 1) / cs::h; d->ox = 0; d->oy = 0; d->W = ((int) std::ceil(cs::w * d->k) + fw::sp::GRID - 1) / fw::sp::GRID * fw::sp::GRID;
      d->ox = -(d->W / d->k - cs::w) / 2;  // centred in its grid-sized box
      break;
    case CD: d->k = H / cd::d; d->ox = 0; d->oy = 0; d->W = (int) H; break;
    case VINYL: {
      const float R = lp::d / 2;
      d->k = H / lp::d;
      d->Px = (H / 2 - fw::sp::s5) / d->k;  // pivot: sp::s5 in from the slot's right edge, top right (docs 16)
      d->Py = -std::sqrt(arm::pivot * arm::pivot - d->Px * d->Px);
      d->ox = -R;
      d->oy = d->Py - 34;
      // the first port's box (the counterweight reaches past its top: a known audit exception until the vinyl's
      // design round, Darren 2026-10-22), its size rounded up to the grid
      d->W = ((int) std::ceil((R + d->Px + 36) * d->k) + fw::sp::GRID - 1) / fw::sp::GRID * fw::sp::GRID;
      d->H = ((int) std::ceil((R - d->oy) * d->k) + fw::sp::GRID - 1) / fw::sp::GRID * fw::sp::GRID;
      if (d->card) { d->ox = -R, d->oy = -R, d->W = d->H = (int) H; }  // the card: the record alone, a square
      return;
    }
  }
  d->H = (int) H;
  if (d->card && d->W > d->H) { d->ox += (d->W - d->H) / d->k; d->W = d->H; }  // the card: a square, the right end (as the screen shows it)
}
void feed(MV *d) {
  if (d->kind == CASSETTE) packs(cs::hubR, full_radius(cs::hubR, cs::tapeLength, cs::tapeThickness), d->progress, &d->sup, &d->tak);
  if (d->kind == VINYL) {
    const bool idle = !d->playing && d->progress <= 0;
    const float rs = lp::music / 2 - d->progress * (lp::music / 2 - lp::lastMusic / 2);
    d->armT = idle ? arm_angle(d->Px, d->Py, arm::effective, arm::restR, -1) : arm_angle(d->Px, d->Py, arm::effective, rs, -1);
  }
}

// invalidate the parts that turn (object-local px boxes)
void dirty_box(lv_obj_t *o, float x0, float y0, float x1, float y1) {
  lv_area_t c;
  lv_obj_get_coords(o, &c);
  lv_area_t a = {c.x1 + (int32_t) std::floor(x0) - 2, c.y1 + (int32_t) std::floor(y0) - 2, c.x1 + (int32_t) std::ceil(x1) + 2,
                 c.y1 + (int32_t) std::ceil(y1) + 2};
  lv_obj_invalidate_area(o, &a);
}
void dirty_circle(lv_obj_t *o, const MV *d, float cx, float cy, float r) {
  dirty_box(o, (cx - r - d->ox) * d->k, (cy - r - d->oy) * d->k, (cx + r - d->ox) * d->k, (cy + r - d->oy) * d->k);
}
void dirty_turning(lv_obj_t *o, const MV *d) {
  switch (d->kind) {
    case CASSETTE:
      for (float x : {cs::w / 2 - cs::hubPitch / 2, cs::w / 2 + cs::hubPitch / 2}) dirty_circle(o, d, x, cs::hubY, cs::hubR + 0.2f);
      break;
    case CD: {  // the mark only: a few px at its radius
      const float r = cd::infoIn / 2 + 1, a = (d->ang[0] - 90) * PI / 180, C = cd::d / 2;
      dirty_circle(o, d, C + r * std::cos(a), C + r * std::sin(a), 2.5f / d->k + 1.5f);
      break;
    }
    case DISC: {  // the marker: its old and new place (both inside this box)
      const float D = (float) fw::lay::disc_screen, orb = fw::lay::disc_face_screen / 2.0f - fw::sp::s5, m = fw::lay::disc_marker / 2.0f + 2;
      const float a = (270 + d->mark) * PI / 180;
      dirty_circle(o, d, D / 2 + orb * std::cos(a), D / 2 + orb * std::sin(a), m);
      break;
    }
    case VINYL: break;  // the plain label's turn is invisible (no art): nothing to redraw
  }
}

void frame(void *var, int32_t) {
  auto *o = (lv_obj_t *) var;
  auto *d = of(o);
  const uint32_t now = lv_tick_get();
  const float dt = d->last ? std::min(0.1f, lv_tick_diff(now, d->last) / 1000.0f) : 0;
  d->last = now;
  const bool still = fw::mo::reduced(o);
  // spin: eases up / down (docs time constants per visual)
  const float tauUp[4] = {0.25f, 0.6f, 0.5f, 0.05f}, tauDown[4] = {0.12f, 0.9f, 0.7f, 0.05f};
  d->m = ease(d->m, d->playing ? 1.0f : 0.0f, dt, d->playing ? tauUp[d->kind] : tauDown[d->kind]);
  bool moving = false;
  // the plain vinyl label has no art, so its turn is invisible (dirty_turning draws nothing for it): it does not keep
  // the clock running (2026-10-22 visual check: a frame clock every frame for no pixels); revisit with the vinyl round
  const bool turns = d->kind != VINYL;
  if (d->m > 0.0005f && !still && turns) {
    moving = true;
    if (d->kind == CASSETTE) {  // both spools counter-clockwise at the tape speed at the pack edge
      d->ang[0] -= deg_per_sec(cs::speed, d->sup) * dt * d->m;
      d->ang[1] -= deg_per_sec(cs::speed, d->tak) * dt * d->m;
    } else if (d->kind == CD) {  // CLV 1.3 m/s at the program radius, / 16
      const float r = cd::program / 2 + d->progress * (cd::programMax / 2 - cd::program / 2);
      d->ang[0] += deg_per_sec(cd::scan, r) / 16 * dt * d->m;
    } else if (d->kind == DISC) {  // the marker: one turn per Motion disc-marker (old place, then new)
      dirty_turning(o, d);
      d->mark = std::fmod(d->mark + 360.0f * dt * d->m * 1000 / fw::mo::ms::disc_marker, 360.0f);
    } else {
      d->ang[0] += lp::rpm * 6 * dt * d->m;
    }
    for (float &a : d->ang) a = std::fmod(a, 360.0f);
    dirty_turning(o, d);
  }
  if (d->kind == DISC && std::fabs(d->shown - d->progress) > 0.0005f) {  // the progress arc sweeps (Motion value's feel)
    d->shown = still ? d->progress : ease(d->shown, d->progress, dt, 0.12f);
    if (std::fabs(d->shown - d->progress) <= 0.0005f) d->shown = d->progress;
    lv_obj_invalidate(o);
    moving = true;
  }
  if (d->kind == VINYL && std::fabs(d->armA - d->armT) > 0.01f) {
    float x0, y0, x1, y1;
    arm_box(d, d->armA, &x0, &y0, &x1, &y1);
    dirty_box(o, x0, y0, x1, y1);
    d->armA = still ? d->armT : ease(d->armA, d->armT, dt, 0.45f);
    arm_box(d, d->armA, &x0, &y0, &x1, &y1);
    dirty_box(o, x0, y0, x1, y1);
    moving = true;
  }
  if (!moving && (!d->playing || !turns) && !d->starting) {  // settled: the clock stops (no idle redraw; never inside lv_anim_start,
    // which runs this once and then still uses the anim: ASan 2026-10-22, heap-use-after-free)
    d->running = false;
    d->last = 0;
    lv_anim_delete(o, (lv_anim_exec_xcb_t) frame);
  }
}
void run(lv_obj_t *o) {
  auto *d = of(o);
  if (d->running) return;
  d->running = true;
  d->last = 0;
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, o);
  lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t) frame);
  lv_anim_set_values(&a, 0, 1000);
  lv_anim_set_duration(&a, 1000);  // a frame clock, not a timed motion: every frame, forever, until it settles
  lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
  d->starting = true;
  lv_anim_start(&a);
  d->starting = false;
}

Kind kind_of(fw::Props p) {
  const char *v = p["visual"] | "cassette";
  for (int i = 0; i < 4; i++) if (!strcmp(v, KIND_IDS[i])) return (Kind) i;
  return CASSETTE;
}
void apply(MV *d, fw::Props p) {
  d->progress = LV_CLAMP(0.0f, (float) (p["progress"] | 38.0) / 100.0f, 1.0f);
  d->playing = p["playing"] | true;
  d->title = p["title"] | "Harbour Lights";
  d->artist = p["artist"] | "The Lanterns";
  feed(d);
}
void fill(lv_obj_t *o, fw::Props p) {
  auto *d = of(o);
  *d = MV{};
  d->kind = kind_of(p);
  d->card = !strcmp(p["size"] | "screen", "card");
  layout(d);
  apply(d, p);
  d->armA = d->armT;  // the first draw: the arm already at its groove
  d->shown = d->progress;
  // a playing visual starts at speed (it was playing before the screen opened), not from a standstill
  d->m = d->playing ? 1.0f : 0.0f;
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_size(o, d->W, d->H);
}
}  // namespace

FW_COMPONENT(media_visual, "Media visuals", "components", R"({
  "about": "The picture on the Media screen and its Home card: a plain disc with a progress ring, or a cassette, CD or vinyl record drawn to the real sizes. They turn at the real speeds while playing; the tape or tonearm follows the track's progress.",
  "frame": [296, 208],
  "props": [
    {"name": "visual", "about": "What is drawn: the plain disc with a progress ring, a cassette, a CD or a vinyl record.", "type": "select", "options": ["disc", "cassette", "cd", "vinyl"], "default": "cassette"},
    {"name": "size", "about": "screen is the Media screen's picture; card is the small picture at the top right of its Home card (a square: the right end of a cassette).", "type": "select", "options": ["screen", "card"], "default": "screen"},
    {"name": "progress", "about": "How far through the track it is, from 0 to 100. The tape moves from one reel to the other, or the tonearm moves in.", "type": "range", "min": 0, "max": 100, "step": 1, "default": 38},
    {"name": "playing", "about": "Whether it is playing. While it plays the cassette reels or the disc turn at their real speed.", "type": "bool", "default": true},
    {"name": "title", "about": "The track title, written on the cassette label or the record.", "type": "text", "default": "Harbour Lights"},
    {"name": "artist", "about": "The artist, written under the title.", "type": "text", "default": "The Lanterns"}
  ],
  "variants": [
    {"label": "Disc", "props": {"visual": "disc", "progress": 38}},
    {"label": "Disc · paused 72 %", "props": {"visual": "disc", "progress": 72, "playing": false}},
    {"label": "Cassette", "props": {"visual": "cassette", "progress": 30}},
    {"label": "Disc · card", "props": {"visual": "disc", "size": "card"}},
    {"label": "Cassette · card", "props": {"visual": "cassette", "size": "card"}},
    {"label": "CD · card", "props": {"visual": "cd", "size": "card"}},
    {"label": "CD", "props": {"visual": "cd", "progress": 45}},
    {"label": "Vinyl", "props": {"visual": "vinyl", "progress": 60}, "audit": {"skip": {"clip": "vinyl awaiting design round, Darren 2026-10-22"}}},
    {"label": "Vinyl · stopped at the start", "props": {"visual": "vinyl", "progress": 0, "playing": false}, "audit": {"skip": {"clip": "vinyl awaiting design round, Darren 2026-10-22"}}}
  ]
})") {
  auto *d = new MV;
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_set_user_data(o, d);
  lv_obj_add_event_cb(o, [](lv_event_t *e) { delete (MV *) lv_event_get_user_data(e); }, LV_EVENT_DELETE, d);
  lv_obj_add_event_cb(o, draw, LV_EVENT_DRAW_MAIN, nullptr);
  fill(o, p);
  if (d->playing) run(o);
}

FW_UPDATE(media_visual) {
  auto *d = of(root);
  if (!d) return false;
  if (kind_of(p) != d->kind || d->card != !strcmp(p["size"] | "screen", "card")) {
    lv_anim_delete(root, (lv_anim_exec_xcb_t) frame);
    fw::refill(root);
    fill(root, p);
    if (d->playing) run(root);
    lv_obj_invalidate(root);
    return true;
  }
  const float was = d->progress;
  const std::string t0 = d->title, a0 = d->artist;
  apply(d, p);
  if (fw::mo::reduced(root)) d->armA = d->armT;
  if (std::fabs(was - d->progress) > 1e-6f || t0 != d->title || a0 != d->artist) lv_obj_invalidate(root);  // packs / label
  run(root);
  return true;
}
