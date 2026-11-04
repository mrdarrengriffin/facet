// Media visuals: the Media screen's picture and its Home card preview, on the contract (renderer/CONTRACT.md,
// "contract": 2). The Facet media visuals (docs: Components > Controls > Media visuals, @facet/ui media-visuals/, CSS
// .f-mv-*): a plain disc with a progress ring, a cassette, a CD, or a 12" vinyl LP with its tonearm, drawn from the
// real-world dimensions (docs specs.ts: IEC 60094-7 cassette, ECMA-130 CD, RIAA E4 LP, Technics SL-1200 arm), turning
// at the real tape and disc speeds while the track plays (CD slowed 16 x: docs), the tape packs (area-conserving) and
// the tonearm following the track's progress.
//   box      a strip (fx::strip) sized by layout tokens, the real object scaled to layout:mv-h (256 / 168): disc and
//            CD mv-h square; cassette mv-cassette-w x mv-h; vinyl mv-vinyl-w x mv-vinyl-h (the arm pivot above the
//            record). size card = an mv-card square (72 / 48): the record (its arm's top bleeds into the Card's padding), a cassette's right end. The box sizes
//            are the real sizes scaled (tests/functional/media_visual.test.mjs proves the tokens). Nothing draws past
//            the box; on the Media screen the cassette runs past its mv-h slot: the slot's declared bleed (Media hero).
//   drawing  one Pen (C24) in mm scaled to px: data-sized shapes (Pen disc / ring / seg / tri / rect, within for the
//            cassette window), inks by name: the mv-* colour tokens, disc / groove / line / line-hi / bg, the label
//            stripes accent / blue / warn / steel; the docs' translucent rims are worked-out tokens (mv-shell-rim,
//            mv-tape-rim, mv-hub-ring, mv-label-ring: C28, no opacity). mm strokes under 1 px are 1 px.
//   text     the cassette label: title (c-label-sm, "Side A" when empty), artist (c-caps-sm), "A" (body), "C60",
//            right-aligned on the label, cut to 26 / 30 characters with "..." (ASCII: the label glyphs)
// Motion: one frame clock (lv_anim) runs only while something moves: the spin eases up / down (docs: platter start
// 0.7 s, brake 1 s; tape ~0.25 s), the arm eases to its groove (docs tau 0.45 s), the disc's progress arc sweeps. Each
// frame redraws only what moved (C26, fx::redraw / redraw_arc): the two cassette hubs, the CD's print mark, the arm's
// box, the disc marker. Reduced motion = nothing turns; packs and the arm jump to the track's place.
// art (the fifth visual): the album art alone, a square the size of the CD's box, the theme's tile corner (radius:tile;
// radius:icon at card size); it never turns; none or loading = a plain square, surface ink with a line edge.
// Album art (art: the feed's picture URL, core/images.h): printed on the disc face, the CD's label side (out to the
// end of the info band, the clear hub stays) and the vinyl's label, a circle scaled to that place at this size (screen
// or card; one view each). It stands still (a turning picture would be a full redraw every frame): the disc's marker
// and the CD's print mark still show the turn. While it loads, with none, or when it fails: the plain drawing, no
// error. A new track keeps the old art until the new one is in (two views: shown + coming); a URL that differs only
// in HA's access token is the same picture (images::same). The cassette has no disc: no art.
// Docs differences: no iridescent / gloss sheens and no window glare; vinyl grooves every space:1 instead of the real
// 0.92 mm pitch (sub-pixel here); the vinyl label's turn is invisible (plain or art: not redrawn).
// A read-out: no touch, no events, no sound. States: n/a (a picture; the Media hero shows the player's state).
// Update path: progress / playing / title / artist move in place (packs, arm, spin ease); another visual or size =
// rebuilt (false). Bleed: the vinyl draws its arm's pivot and counterweight above its box, top: up to space:3 on the
// Media screen (the Media hero declares the same) and layout:card-pad at card size (the Card declares the same). Contrast pairs: tokens/colour.json section "media_visual".
#include <algorithm>
#include <cmath>
#include "fw.h"
#include "fx.h"
#include "images.h"

namespace {
constexpr float PI = 3.14159265f;
enum Kind { CASSETTE, CD, VINYL, DISC, ART };
const char *KIND_IDS[] = {"cassette", "cd", "vinyl", "disc", "art"};
constexpr int KINDS = 5;

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

void art_changed(void *mv);
struct MV {
  Kind kind = CASSETTE;
  bool card = false;              // size card: the Home card preview (an mv-card square)
  float shown = 0, mark = 0;     // disc: the progress drawn (eases to progress), the marker angle (degrees)
  float k = 1, ox = 0, oy = 0;  // px per mm; the box's top-left in mm
  int W = 0, H = 0;              // the box (its tokens)
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
  // album art (core/images.h Swap: shown + coming); obj = the strip (its redraw when the art changes)
  lv_obj_t *obj = nullptr;
  fw::img::Swap art{art_changed, this};
};
MV *of(lv_obj_t *o) { return (MV *) lv_obj_get_user_data(o); }
int S(const char *t) { return fx::size(t); }

// ---- mm -> Pen px (centre-relative)
struct Ctx {
  const fx::Pen &p;
  const MV *d;
  int X(float mm) const { return (int) std::lround((mm - d->ox) * d->k - d->W / 2.0f); }
  int Y(float mm) const { return (int) std::lround((mm - d->oy) * d->k - d->H / 2.0f); }
  int R(float mm) const { return (int) std::lround(mm * d->k); }
  int px(float mm) const { return std::max(1, R(mm)); }  // a stroke: at least 1 px
};
void rrect(const Ctx &c, float x, float y, float w, float h, float r, const char *ink, int border = 0) {
  const int X = c.X(x), Y = c.Y(y);
  c.p.rect(X, Y, c.X(x + w) - X, c.Y(y + h) - Y, ink, c.R(r), border);
}
void disc(const Ctx &c, float x, float y, float r, const char *ink) { c.p.disc(c.X(x), c.Y(y), c.R(r), ink); }
void ring(const Ctx &c, float x, float y, float r, float wmm, const char *ink) {  // centred on r like an SVG stroke
  c.p.ring(c.X(x), c.Y(y), c.R(r + wmm / 2), c.px(wmm), ink);
}
void annulus(const Ctx &c, float x, float y, float r1, float r2, const char *ink) { c.p.ring(c.X(x), c.Y(y), c.R(r2), c.px(r2 - r1), ink); }
void line(const Ctx &c, float x1, float y1, float x2, float y2, float wmm, const char *ink, bool round = true) {
  c.p.seg(c.X(x1), c.Y(y1), c.X(x2), c.Y(y2), c.px(wmm), ink, round);
}
void tri(const Ctx &c, const float (&q)[3][2], const char *ink) {
  const int t[3][2] = {{c.X(q[0][0]), c.Y(q[0][1])}, {c.X(q[1][0]), c.Y(q[1][1])}, {c.X(q[2][0]), c.Y(q[2][1])}};
  c.p.tri(t, ink);
}
// a rotated bar: from a to b along angle `deg` about (cx, cy) in mm, thickness w (mm): a thick line
void bar_rot(const Ctx &c, float cx, float cy, float deg, float a, float b, float w, const char *ink, bool round = false) {
  const float r = deg * PI / 180, cs_ = std::cos(r), sn = std::sin(r);
  line(c, cx + a * cs_, cy + a * sn, cx + b * cs_, cy + b * sn, w, ink, round);
}
// a short text right-aligned at x (mm), its baseline on `base` (mm): the Pen centres a text's digits on y, so the
// baseline is raised by half the digits' height (about 0.27 of the type's line box)
void text(const Ctx &c, const char *type, const std::string &s, float x, float base, bool right) {
  if (s.empty()) return;
  const int w = c.p.text_w(s.c_str(), type);
  const int y = c.Y(base) - (int) std::lround(fx::type_box(type) * 0.27f);
  c.p.text(right ? c.X(x) - w / 2 : c.X(x) + w / 2, y, s.c_str(), type, "fg:mv-ink");
}
// ASCII dots: the label glyph sets may not carry the ellipsis
std::string clip_text(const std::string &v, size_t n) { return v.size() > n ? v.substr(0, n - 3) + "..." : v; }

// ---- 1 cassette
void draw_cassette(const Ctx &c) {
  const MV *d = c.d;
  const float hubX[2] = {cs::w / 2 - cs::hubPitch / 2, cs::w / 2 + cs::hubPitch / 2};
  rrect(c, 0, 0, cs::w, cs::h, cs::corner, "fg:mv-shell");
  rrect(c, 0, 0, cs::w, cs::h, cs::corner, "fg:mv-shell-edge", c.px(0.35f));
  rrect(c, 1.1f, 1.1f, cs::w - 2.2f, cs::h - 2.2f, cs::corner - 1, "fg:mv-shell-rim", 1);
  // head area: a trapezoid (two triangles) + its edges
  const float h0[3][2] = {{cs::head[0][0], cs::head[0][1]}, {cs::head[1][0], cs::head[1][1]}, {cs::head[2][0], cs::head[2][1]}};
  const float h1[3][2] = {{cs::head[0][0], cs::head[0][1]}, {cs::head[2][0], cs::head[2][1]}, {cs::head[3][0], cs::head[3][1]}};
  tri(c, h0, "fg:mv-shell-2");
  tri(c, h1, "fg:mv-shell-2");
  for (int i = 0; i < 3; i++) line(c, cs::head[i][0], cs::head[i][1], cs::head[i + 1][0], cs::head[i + 1][1], 0.3f, "fg:mv-shell-edge", false);
  // label: paper, four stripes (the palette), two writing rules, the title / artist, side A, C60
  rrect(c, cs::labX, cs::labY, cs::labW, cs::labH, cs::labR, "fg:mv-paper");
  const char *pal[4] = {"fg:accent", "fg:blue", "fg:warn", "fg:steel"};
  for (int i = 0; i < 4; i++) rrect(c, cs::labX, 38.4f + i * 2.0f, cs::labW, 1.6f, 0, pal[i]);
  for (float y : {12.2f, 17.6f}) line(c, cs::labX + 3, y, cs::labX + cs::labW - 3, y, 0.18f, "fg:mv-ink", false);
  text(c, "type:c-label-sm", clip_text(d->title.empty() ? "Side A" : d->title, 26), 93.4f, 11.6f, true);
  text(c, "type:c-caps-sm", clip_text(d->artist, 30), 93.4f, 17.0f, true);
  text(c, "type:body", "A", 7.4f, 16.6f, false);
  text(c, "type:c-caps-sm", "C60", 93.4f, 36.8f, true);
  // window: smoked plastic; inside it (clipped) the packs, the bores and the turning hubs
  rrect(c, cs::winX, cs::winY, cs::winW, cs::winH, cs::winR, "fg:mv-window");
  const int wx = c.X(cs::winX), wy = c.Y(cs::winY);
  c.p.within(wx, wy, c.X(cs::winX + cs::winW) - wx, c.Y(cs::winY + cs::winH) - wy, [&] {
    const float pr[2] = {d->sup, d->tak};
    for (int i = 0; i < 2; i++) {
      disc(c, hubX[i], cs::hubY, pr[i], "fg:mv-tape");
      ring(c, hubX[i], cs::hubY, std::max(cs::hubR, pr[i] - 0.7f), 0.5f, "fg:mv-tape-rim");
      disc(c, hubX[i], cs::hubY, cs::boreR, "fg:mv-gap");
      annulus(c, hubX[i], cs::hubY, cs::boreR, cs::hubR, "fg:mv-hub");
      for (int k = 0; k < cs::teeth; k++)  // drive teeth, pointing in from the bore
        bar_rot(c, hubX[i], cs::hubY, d->ang[i] - 90 + 360.0f / cs::teeth * k, cs::boreR + 0.2f, cs::boreR - cs::toothLen, cs::toothW, "fg:mv-hub");
      ring(c, hubX[i], cs::hubY, cs::hubR - 0.9f, 0.3f, "fg:mv-hub-ring");
      bar_rot(c, hubX[i], cs::hubY, d->ang[i] - 90, cs::hubR + 0.1f, cs::hubR - 1.4f, 2.6f, "fg:mv-gap");  // leader clamp slot
    }
  });
  // the window's rounded corners over the packs: a paper ring just outside its rounded rect, then the edge
  const float g = cs::winR * 0.35f + 1 / d->k;
  rrect(c, cs::winX - g, cs::winY - g, cs::winW + 2 * g, cs::winH + 2 * g, cs::winR + g, "fg:mv-paper", std::max(1, (int) std::ceil(g * d->k)));
  rrect(c, cs::winX, cs::winY, cs::winW, cs::winH, cs::winR, "fg:mv-shell-edge", c.px(0.45f));
  // bottom edge: openings + the tape across them, capstan and pin holes, screws
  for (const auto &o : cs::openings) {
    rrect(c, o[0], cs::h - cs::openingDepth, o[1] - o[0], cs::openingDepth, 0, "fg:mv-gap");
    rrect(c, o[0], cs::h - 1.6f, o[1] - o[0], 0.9f, 0, "fg:mv-tape-hi");
  }
  for (const auto &h : cs::capstan) disc(c, h[0], h[1], cs::capstanR, "fg:mv-gap");
  for (const auto &h : cs::pins) rrect(c, h[0] - cs::pinSide / 2, h[1] - cs::pinSide / 2, cs::pinSide, cs::pinSide, 0.3f, "fg:mv-gap");
  for (const auto &s : cs::screws) {
    disc(c, s[0], s[1], cs::screwR, "fg:mv-screw");
    ring(c, s[0], s[1], cs::screwR, 0.2f, "fg:mv-shell-edge");
    line(c, s[0] - 1, s[1], s[0] + 1, s[1], 0.45f, "fg:mv-gap");
    line(c, s[0], s[1] - 1, s[0], s[1] + 1, 0.45f, "fg:mv-gap");
  }
}

// ---- album art: where each visual prints it (mm or disc units: its centre and diameter); false = no place for it
bool art_place(const MV *d, float *cx, float *cy, float *dia) {
  switch (d->kind) {
    case DISC: *cx = *cy = S("layout:disc-screen") / 2.0f, *dia = (float) S("layout:disc-face-screen"); return true;
    case CD: *cx = *cy = cd::d / 2, *dia = cd::infoOut; return true;
    case VINYL: *cx = *cy = 0, *dia = lp::label; return true;
    default: return false;
  }
}
// the art's box in Pen px (top-left, centre-relative) and its diameter
bool art_box(const MV *d, int *x, int *y, int *D) {
  float cx, cy, dia;
  if (!art_place(d, &cx, &cy, &dia)) return false;
  *D = std::max(2, (int) std::lround(dia * d->k));
  *x = (int) std::lround((cx - d->ox) * d->k - d->W / 2.0f) - *D / 2;
  *y = (int) std::lround((cy - d->oy) * d->k - d->H / 2.0f) - *D / 2;
  return true;
}
// the square art's corner radius: the theme's tile corner on the Media screen, its icon corner on the Home card
int art_radius(const MV *d) { return std::min(d->H / 2, S(d->card ? "radius:icon" : "radius:tile")); }
bool art(const Ctx &c) {  // the art drawn at its place: true; none (yet): false, the plain drawing
  const lv_image_dsc_t *px = c.d->art.pixels();
  int x, y, D;
  if (!px || !art_box(c.d, &x, &y, &D)) return false;
  c.p.image(x, y, px);
  return true;
}

// ---- 5 the square art: the picture alone, the box's size, the theme's corner; none or loading = a plain square in
// the surface ink with a line edge (the theme's)
void draw_art(const Ctx &c) {
  const MV *d = c.d;
  if (const lv_image_dsc_t *px = d->art.pixels()) { c.p.image(-d->W / 2, -d->H / 2, px); return; }
  const int r = art_radius(d);
  c.p.rect(-d->W / 2, -d->H / 2, d->W, d->H, "fg:surface", r);
  c.p.rect(-d->W / 2, -d->H / 2, d->W, d->H, "fg:line", r, std::max(1, S("stroke:hair")));
}

// ---- 2 CD (label side up; the art printed from the hub's edge out to the info band's)
void draw_cd(const Ctx &c) {
  const float C = cd::d / 2;
  disc(c, C, C, cd::d / 2, "fg:mv-clear");
  if (art(c)) disc(c, C, C, cd::printIn / 2, "fg:mv-clear");  // the clear hub inside the print
  else annulus(c, C, C, cd::infoIn / 2, cd::infoOut / 2, "fg:mv-mirror");
  // the printed mark that shows the turn (docs: a hairline just outside the metallised band's inner edge)
  bar_rot(c, C, C, c.d->ang[0] - 90, cd::infoIn / 2 + 0.4f, cd::infoIn / 2 + 0.4f + std::max(0.9f, 2 / c.d->k), 0.5f, "fg:mv-clear-2");
  annulus(c, C, C, cd::clampIn / 2, cd::clampOut / 2, "fg:mv-clear-2");
  ring(c, C, C, cd::stackRing / 2, 0.7f, "fg:mv-clear-2");
  ring(c, C, C, cd::printIn / 2, 0.25f, "fg:mv-mirror");
  ring(c, C, C, cd::d / 2 - 0.15f - 0.15f, 0.3f, "fg:mv-clear-2");
  disc(c, C, C, cd::hole / 2, "fg:bg");  // the hole shows the canvas
  ring(c, C, C, cd::hole / 2 + 0.1f, 0.3f, "fg:mv-clear-2");
}

// ---- 3 vinyl + tonearm
void draw_vinyl(const Ctx &c) {
  const MV *d = c.d;
  const float R = lp::d / 2;
  disc(c, 0, 0, R, "fg:mv-vinyl");
  annulus(c, 0, 0, lp::outerGroove / 2, R, "fg:mv-vinyl-edge");
  annulus(c, 0, 0, lp::music / 2, lp::outerGroove / 2, "fg:mv-vinyl-gloss");
  const float pitch = S("space:1") / d->k;  // grooves every space:1 (the real 0.92 mm pitch is sub-pixel here)
  for (float rg = lp::lastMusic / 2 + pitch / 2; rg < lp::music / 2; rg += pitch) ring(c, 0, 0, rg, 0.32f, "fg:mv-groove");
  for (float q : {0.22f, 0.47f, 0.7f}) ring(c, 0, 0, lp::lastMusic / 2 + q * (lp::music / 2 - lp::lastMusic / 2), 1.1f, "fg:mv-vinyl-gloss");
  annulus(c, 0, 0, lp::lockGroove / 2, lp::lastMusic / 2, "fg:mv-vinyl-gloss");
  ring(c, 0, 0, lp::lockGroove / 2, 0.5f, "fg:mv-groove");
  if (!art(c)) disc(c, 0, 0, lp::label / 2, "fg:mv-paper");
  ring(c, 0, 0, lp::label / 2 - 0.4f, 0.5f, "fg:mv-label-ring");
  disc(c, 0, 0, lp::hole / 2 - 0.3f, "fg:mv-metal");
  // rest post, arm base, then the arm (pivot-local: along +x, rotated about the pivot)
  const float L = arm::effective, restA = arm_angle(d->Px, d->Py, L, arm::restR, -1) * PI / 180;
  disc(c, d->Px + (L - 14) * std::cos(restA), d->Py + (L - 14) * std::sin(restA), 4.5f, "fg:mv-metal-2");
  disc(c, d->Px, d->Py, arm::baseR, "fg:mv-deck");
  ring(c, d->Px, d->Py, arm::baseR, 0.8f, "fg:mv-shell-edge");
  disc(c, d->Px, d->Py, arm::bearingR, "fg:mv-metal");
  const float A = d->armA;
  bar_rot(c, d->Px, d->Py, A, -arm::weightTo, -arm::weightFrom, arm::weightW, "fg:mv-metal-2");
  bar_rot(c, d->Px, d->Py, A, -arm::weightFrom, L - 26, arm::tubeW, "fg:mv-metal", true);
  // headshell at the arm's end, turned by the offset angle
  const float ar = A * PI / 180, hx = d->Px + L * std::cos(ar), hy = d->Py + L * std::sin(ar), hA = A + arm::offset;
  bar_rot(c, hx, hy, hA, -34, 6, 17, "fg:mv-metal-2");
  const float hr = hA * PI / 180, ch = std::cos(hr), sh = std::sin(hr);
  line(c, hx + 2 * ch + 8.5f * sh, hy + 2 * sh - 8.5f * ch, hx + 10 * ch + 16 * sh, hy + 10 * sh - 16 * ch, 2.2f, "fg:mv-metal");
  bar_rot(c, hx, hy, hA, -6, 4, 10, "fg:mv-gap");
}
// the arm's bounding box (px from the box's top-left) at angle a: weight end, tube, headshell + lift
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

// ---- 4 the Media disc: units = px at the screen size (layout:disc-screen), k scales them to the card. A dotted ring
// (72 dots over 355 degrees from 12 o'clock, stroke:arc, line-hi), the accent progress arc space:2 inside it (round
// caps), the face (disc, a hair line edge) with two groove rings (disc-groove smaller each), the accent label spot
// with its hole in the canvas colour, the blue marker circling the face space:5 inside its edge (one turn per Motion
// disc-marker while playing).
void draw_disc(const Ctx &c) {
  const MV *d = c.d;
  const float D = (float) S("layout:disc-screen"), C = D / 2, F = (float) S("layout:disc-face-screen");
  const int hair = c.px((float) S("stroke:hair"));
  const float dot = S("stroke:arc") / 2.0f;
  for (int i = 0; i < 72; i++) {
    const float a = (270 + i * 355.0f / 71) * PI / 180;
    c.p.disc(c.X(C + (C - dot) * std::cos(a)), c.Y(C + (C - dot) * std::sin(a)), std::max(1, c.R(dot)), "fg:line-hi");
  }
  if (d->shown > 0.001f) c.p.ring(c.X(C), c.Y(C), c.R(C - S("space:2")), c.px((float) S("stroke:arc")), "fg:accent", 270, 270 + 359 * d->shown, true);
  if (art(c)) c.p.ring(c.X(C), c.Y(C), c.R(F / 2), hair, "fg:line");  // the art is the face: no grooves, no label spot
  else {
    disc(c, C, C, F / 2, "fg:disc");
    c.p.ring(c.X(C), c.Y(C), c.R(F / 2), hair, "fg:line");
    for (int i = 1; i <= 2; i++) c.p.ring(c.X(C), c.Y(C), c.R((F - i * S("layout:disc-groove")) / 2), hair, "fg:groove");
    disc(c, C, C, S("layout:disc-spot-screen") / 2.0f, "fg:accent");
  }
  disc(c, C, C, S("layout:disc-hole") / 2.0f, "fg:bg");
  const float orb = F / 2 - S("space:5"), a = (270 + d->mark) * PI / 180;
  disc(c, C + orb * std::cos(a), C + orb * std::sin(a), S("layout:disc-marker") / 2.0f, "fg:blue");
}

void draw(const fx::Pen &p) {
  const MV *d = of(p.obj);
  if (!d) return;
  const Ctx c{p, d};
  auto all = [&] {
    switch (d->kind) {
      case CASSETTE: draw_cassette(c); break;
      case CD: draw_cd(c); break;
      case VINYL: draw_vinyl(c); break;
      case DISC: draw_disc(c); break;
      case ART: draw_art(c); break;
    }
  };
  // the card: a square crop, nothing past it but the vinyl arm's declared top bleed (layout:card-pad)
  const int up = d->card && d->kind == VINYL ? fx::size("layout:card-pad") : 0;
  if (d->card) p.within(-d->W / 2, -d->H / 2 - up, d->W, d->H + up, all);
  else all();
}

// ---- geometry per visual (the box = its tokens) + the pack radii / arm target from the progress
const char *const BOX[KINDS][2] = {{"w:mv-cassette-w", "h:mv-h"}, {"w:mv-h", "h:mv-h"}, {"w:mv-vinyl-w", "h:mv-vinyl-h"}, {"w:mv-h", "h:mv-h"}, {"w:mv-h", "h:mv-h"}};
const char *const BOX_W[KINDS] = {"layout:mv-cassette-w", "layout:mv-h", "layout:mv-vinyl-w", "layout:mv-h", "layout:mv-h"};
void layout(MV *d) {
  const float H = (float) S(d->card ? "layout:mv-card" : "layout:mv-h");
  d->H = (int) H;
  d->W = d->card ? (int) H : S(BOX_W[d->kind]);
  d->ox = d->oy = 0;
  switch (d->kind) {
    case DISC: d->k = H / S("layout:disc-screen"); break;
    case ART: d->k = 1; break;
    case CD: d->k = H / cd::d; break;
    case CASSETTE:  // a pixel to spare (H - 1): the bottom edge's lines anti-alias one row past it
      d->k = (H - 1) / cs::h;
      d->ox = d->card ? cs::w - H / d->k : -(d->W / d->k - cs::w) / 2;  // the card: the right end (as the screen shows it); else centred
      break;
    case VINYL: {
      const float R = lp::d / 2;
      d->k = H / lp::d;
      d->Px = (H / 2 - S("space:5")) / d->k;  // pivot: space:5 in from the slot's right edge, top right (docs 16)
      d->Py = -std::sqrt(arm::pivot * arm::pivot - d->Px * d->Px);
      d->ox = -R;
      d->oy = d->card ? -R : d->Py - 34;  // the card: the record alone
      if (!d->card) d->H = S("layout:mv-vinyl-h");
      break;
    }
  }
}
void feed(MV *d) {
  if (d->kind == CASSETTE) packs(cs::hubR, full_radius(cs::hubR, cs::tapeLength, cs::tapeThickness), d->progress, &d->sup, &d->tak);
  if (d->kind == VINYL) {
    const bool idle = !d->playing && d->progress <= 0;
    const float rs = lp::music / 2 - d->progress * (lp::music / 2 - lp::lastMusic / 2);
    d->armT = idle ? arm_angle(d->Px, d->Py, arm::effective, arm::restR, -1) : arm_angle(d->Px, d->Py, arm::effective, rs, -1);
  }
}

// redraw only what moved (px from the box's top-left, made centre-relative for fx::redraw)
void dirty_box(lv_obj_t *o, const MV *d, float x0, float y0, float x1, float y1) {
  const int X0 = (int) std::floor(x0) - 2, Y0 = (int) std::floor(y0) - 2;
  fx::redraw(o, X0 - d->W / 2, Y0 - d->H / 2, (int) std::ceil(x1) + 3 - X0, (int) std::ceil(y1) + 3 - Y0);
}
void dirty_circle(lv_obj_t *o, const MV *d, float cx, float cy, float r) {
  dirty_box(o, d, (cx - r - d->ox) * d->k, (cy - r - d->oy) * d->k, (cx + r - d->ox) * d->k, (cy + r - d->oy) * d->k);
}
void dirty_all(lv_obj_t *o, const MV *d) { fx::redraw(o, -d->W / 2, -d->H / 2, d->W, d->H); }
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
      const float D = (float) S("layout:disc-screen"), orb = S("layout:disc-face-screen") / 2.0f - S("space:5"), m = S("layout:disc-marker") / 2.0f + 2;
      const float a = (270 + d->mark) * PI / 180;
      dirty_circle(o, d, D / 2 + orb * std::cos(a), D / 2 + orb * std::sin(a), m);
      break;
    }
    case VINYL: break;  // the plain label's turn is invisible (no art): nothing to redraw
    case ART: break;    // the square art never turns
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
  const float tauUp[KINDS] = {0.25f, 0.6f, 0.5f, 0.05f, 0.05f}, tauDown[KINDS] = {0.12f, 0.9f, 0.7f, 0.05f, 0.05f};
  d->m = ease(d->m, d->playing ? 1.0f : 0.0f, dt, d->playing ? tauUp[d->kind] : tauDown[d->kind]);
  bool moving = false;
  // the plain vinyl label has no art, so its turn is invisible (dirty_turning draws nothing for it): it does not keep
  // the clock running
  const bool turns = d->kind != VINYL && d->kind != ART;  // the square art never turns
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
    }
    for (float &a : d->ang) a = std::fmod(a, 360.0f);
    dirty_turning(o, d);
  }
  if (d->kind == DISC && std::fabs(d->shown - d->progress) > 0.0005f) {  // the progress arc sweeps (Motion value's feel)
    const float was = d->shown;
    d->shown = still ? d->progress : ease(d->shown, d->progress, dt, 0.12f);
    if (std::fabs(d->shown - d->progress) <= 0.0005f) d->shown = d->progress;
    const float C = S("layout:disc-screen") / 2.0f;
    fx::redraw_arc(o, (int) std::lround((C - S("space:2")) * d->k), 270 + 359 * was, 270 + 359 * d->shown, std::max(2, (int) std::lround(S("stroke:arc") * d->k)) + 2);
    moving = true;
  }
  if (d->kind == VINYL && std::fabs(d->armA - d->armT) > 0.01f) {
    float x0, y0, x1, y1;
    arm_box(d, d->armA, &x0, &y0, &x1, &y1);
    dirty_box(o, d, x0, y0, x1, y1);
    d->armA = still ? d->armT : ease(d->armA, d->armT, dt, 0.45f);
    arm_box(d, d->armA, &x0, &y0, &x1, &y1);
    dirty_box(o, d, x0, y0, x1, y1);
    moving = true;
  }
  if (!moving && (!d->playing || !turns) && !d->starting) {  // settled: the clock stops (no idle redraw; never inside lv_anim_start,
    // which runs this once and then still uses the anim: ASan: heap-use-after-free)
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
  for (int i = 0; i < KINDS; i++) if (!strcmp(v, KIND_IDS[i])) return (Kind) i;
  return CASSETTE;
}
bool card_of(fw::Props p) { return !strcmp(p["size"] | "screen", "card"); }
void art_changed(void *mv) {
  MV *d = (MV *) mv;
  if (d->obj) dirty_all(d->obj, d);
}
// the art prop: printed where the visual has a place for it, at this size (the cassette has none)
void art_set(MV *d, const std::string &url) {
  if (d->kind == ART) { d->art.set(url, d->W, d->H, art_radius(d)); return; }
  int x, y, D;
  if (!art_box(d, &x, &y, &D)) return;
  d->art.set(url, D, D, D / 2);
}
void apply(MV *d, fw::Props p) {
  d->progress = LV_CLAMP(0.0f, (float) (p["progress"] | 38.0) / 100.0f, 1.0f);
  d->playing = p["playing"] | true;
  d->title = p["title"] | "Harbour Lights";
  d->artist = p["artist"] | "The Lanterns";
  feed(d);
  art_set(d, p["art"] | "");
}
}  // namespace

FW_COMPONENT(media_visual, "Media visuals", "components", R"({
  "about": "The picture on the Media screen and its Home card: a plain disc with a progress ring, or a cassette, CD or vinyl record drawn to the real sizes. They turn at the real speeds while playing; the tape or tonearm follows the track's progress.",
  "contract": 2,
  "parts": [],
  "min": ["layout:mv-card", "layout:mv-card"],
  "frame": [296, 208],
  "props": [
    {"name": "visual", "about": "What is drawn: the plain disc with a progress ring, a cassette, a CD, a vinyl record, or art: the album art alone, a square.", "type": "select", "options": ["disc", "cassette", "cd", "vinyl", "art"], "default": "cassette"},
    {"name": "size", "about": "screen is the Media screen's picture; card is the small picture at the top right of its Home card (a square: the right end of a cassette).", "type": "select", "options": ["screen", "card"], "default": "screen"},
    {"name": "progress", "about": "How far through the track it is, from 0 to 100. The tape moves from one reel to the other, or the tonearm moves in.", "type": "range", "min": 0, "max": 100, "step": 1, "default": 38},
    {"name": "playing", "about": "Whether it is playing. While it plays the cassette reels or the disc turn at their real speed.", "type": "bool", "default": true},
    {"name": "title", "about": "The track title, written on the cassette label or the record.", "type": "text", "default": "Harbour Lights"},
    {"name": "artist", "about": "The artist, written under the title.", "type": "text", "default": "The Lanterns"},
    {"name": "art", "about": "The album art's address (the player's picture): printed on the disc face, the CD or the record's label. Empty, loading or failed: the plain drawing.", "type": "text", "default": ""}
  ],
  "variants": [
    {"label": "Disc", "props": {"visual": "disc", "progress": 38}},
    {"label": "Disc · paused 72 %", "props": {"visual": "disc", "progress": 72, "playing": false}},
    {"label": "Cassette", "props": {"visual": "cassette", "progress": 30}},
    {"label": "Disc · card", "props": {"visual": "disc", "size": "card"}},
    {"label": "Cassette · card", "props": {"visual": "cassette", "size": "card"}},
    {"label": "CD · card", "props": {"visual": "cd", "size": "card"}},
    {"label": "CD", "props": {"visual": "cd", "progress": 45}},
    {"label": "Art · no art yet", "props": {"visual": "art"}},
    {"label": "Art · card", "props": {"visual": "art", "size": "card"}},
    {"label": "Vinyl", "props": {"visual": "vinyl", "progress": 60}, "audit": {"skip": {"clip": "vinyl awaiting design round"}}},
    {"label": "Vinyl · stopped at the start", "props": {"visual": "vinyl", "progress": 0, "playing": false}, "audit": {"skip": {"clip": "vinyl awaiting design round"}}}
  ]
})") {
  auto *d = new MV;
  lv_obj_t *o = fx::strip(parent, draw, "free");
  fx::name(o, "media_visual");
  lv_obj_set_user_data(o, d);
  d->obj = o;
  lv_obj_add_event_cb(o, [](lv_event_t *e) {
    auto *d = (MV *) lv_event_get_user_data(e);
    d->obj = nullptr;
    delete d;  // its Swap lets the art go
  }, LV_EVENT_DELETE, d);
  lv_obj_remove_flag(o, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  d->kind = kind_of(p);
  d->card = card_of(p);
  if (d->kind == VINYL) fx::overflow(o, "top", d->card ? "layout:card-pad" : "space:3");  // the arm's pivot + counterweight above the record (screen 7 / 5 px; card 23.7 / 15.9)  // the arm's pivot + counterweight above the record: inside the card's padding (24 / 16 px; the arm needs 23.7 / 15.9)
  if (d->card) for (const char *t : {"w:mv-card", "h:mv-card"}) fx::use(o, t);
  else for (const char *t : BOX[d->kind]) fx::use(o, t);
  layout(d);
  apply(d, p);
  d->armA = d->armT;  // the first draw: the arm already at its groove
  d->shown = d->progress;
  // a playing visual starts at speed (it was playing before the screen opened), not from a standstill
  d->m = d->playing ? 1.0f : 0.0f;
  if (d->playing) run(o);
}

FW_UPDATE(media_visual) {
  auto *d = of(root);
  if (!d || kind_of(p) != d->kind || d->card != card_of(p)) return false;  // another visual or size: rebuilt
  const float was = d->progress;
  const std::string t0 = d->title, a0 = d->artist;
  apply(d, p);
  if (fw::mo::reduced(root)) d->armA = d->armT;
  if (std::fabs(was - d->progress) > 1e-6f || t0 != d->title || a0 != d->artist) {
    if (d->kind == CASSETTE) dirty_all(root, d);  // the packs and the label
  }
  run(root);
  return true;
}
