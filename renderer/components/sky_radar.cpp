// Sky radar (docs: @facet/ui screens/overhead.ts skyRadar(), CSS 50-screens.css .fs-oh-*): the aircraft round home by
// distance (square-root scale) and bearing, each a plane seen from above turned to its track; the lead (the selected
// one, else the closest) in the accent with a ring, an emergency in the error colour. On the contract
// (renderer/CONTRACT.md, "contract": 2, 2026-10-29): drawn with the Pen only, every ink a colour token and every size a
// size token read at draw time, so a theme and the Layout lab restyle it with no code here.
//   box     size hero = layout:sky-radar square (docs 208 / 136); card = layout:sky-card (the Home card preview, 72 / 48)
//   rings   5 / 15 / 30 / 50 km at rmax x sqrt(km / 50), rmax = R - half a plane (card: R - space:1); inner ones
//           stroke:hair fg:line, the outer one stroke:edge fg:line-hi (part "arc"); the card draws 15 and 50 only
//   compass N / E / S / W on the outer ring (hero only): type:label fg:text-dim centred in a label-box square of fg:bg
//           that cuts the ring (docs .fs-oh-cmp 24 x 24)
//   home    a layout:sky-dot fg:text dot in the middle
//   planes  the Icons foundation's plane (a turn icon, Pen::turned) layout:sky-plane, turned to the track: fg:text-dim,
//           the lead fg:accent with a stroke:edge ring space:2 out (docs .fs-oh-halo inset -8), an emergency fg:error;
//           the lead is kept a ring's width inside the edge and painted last. Card: layout:sky-dot dots, same inks
// Data: prop planes = the flights feed's [[km, bearing, track, mark], ...] (mark bit 1 = the lead, bit 2 = an
// emergency); empty = a clear sky (rings and home only). Static: no touch, no sound (stage 1; the Radar view's taps come
// with it). Update path: new planes = one invalidate of the box (the feed moves at most every few seconds).
// Contrast: tokens/colour.json section "overhead".
#include <cmath>
#include <vector>
#include "fw.h"
#include "fx.h"

namespace {
constexpr float RINGS[4] = {5, 15, 30, 50};
struct Plane { float km, brg, track; int mark; };
struct Sky { std::vector<Plane> planes; bool card = false; };
Sky *of(lv_obj_t *o) { return (Sky *) lv_obj_get_user_data(o); }
bool card_of(fw::Props p) { return !strcmp(p["size"] | "hero", "card"); }
void load(Sky &s, fw::Props p) {
  s.planes.clear();
  ArduinoJson::JsonDocument j;
  if (deserializeJson(j, p["planes"] | "[]")) return;
  for (ArduinoJson::JsonArrayConst a : j.as<ArduinoJson::JsonArrayConst>())
    s.planes.push_back({a[0] | 0.0f, a[1] | 0.0f, a[2] | 0.0f, a[3] | 0});
}
const char *ink_of(int mark) { return mark & 2 ? "fg:error" : mark & 1 ? "fg:accent" : "fg:text-dim"; }

void draw(const fx::Pen &p) {
  const Sky *s = of(p.obj);
  if (!s) return;
  const int R = p.half, glyph = fx::size("layout:sky-plane"), s2 = fx::size("space:2"), edge = fx::size("stroke:edge");
  const float rmax = (float) (R - (s->card ? fx::size("space:1") : glyph / 2));
  auto rpx = [&](float km) { return rmax * std::sqrt(std::min(std::max(km, 0.0f), RINGS[3]) / RINGS[3]); };
  for (int i = s->card ? 1 : 0; i < 4; i++) {
    if (s->card && i == 2) continue;
    const char *st = i == 3 ? "stroke:edge" : "stroke:hair";
    const int r = (int) std::lround(rpx(RINGS[i]));
    p.arc(r - fx::size(st) / 2, 0.0f, 360.0f, st, i == 3 ? "fg:line-hi" : "fg:line", false, "arc");
  }
  if (!s->card) {  // N / E / S / W, each in a canvas square that cuts the ring under it
    const int B = fx::type_box("type:label"), c = R - B / 2;
    const struct { const char *t; int x, y; } NESW[] = {{"N", 0, -c}, {"E", c, 0}, {"S", 0, c}, {"W", -c, 0}};
    for (const auto &n : NESW) {
      p.rect(n.x - B / 2, n.y - B / 2, B, B, "fg:bg");
      p.text(n.x, n.y, n.t, "type:label", "fg:text-dim");
    }
  }
  p.dot(0, 0, "layout:sky-dot", "fg:text");
  for (int pass = 0; pass < 2; pass++)  // the lead last, on top
    for (const Plane &a : s->planes) {
      const bool lead = a.mark & 1;
      if (lead != (pass == 1)) continue;
      const float r = std::min(rpx(a.km), lead && !s->card ? rmax - s2 - edge : rmax), an = a.brg * 3.14159265f / 180;
      const int x = (int) lroundf(r * sinf(an)), y = (int) lroundf(-r * cosf(an));
      if (s->card) { p.dot(x, y, "layout:sky-dot", ink_of(a.mark)); continue; }
      p.turned(x, y, "plane", "layout:sky-plane", a.track, ink_of(a.mark));
      if (lead) {
        const int d = glyph + 2 * s2;
        p.rect(x - d / 2, y - d / 2, d, d, ink_of(a.mark), d / 2, edge);
      }
    }
}
void set_size(lv_obj_t *o, bool card) {
  // max-h: lifts drawn()'s "fit" cap (a hugging parent's height, 0)
  for (const char *t : card ? std::initializer_list<const char *>{"w:sky-card", "h:sky-card", "max-h:sky-card"} : std::initializer_list<const char *>{"w:sky-radar", "h:sky-radar", "max-h:sky-radar"})
    fx::use(o, t);
}
}  // namespace

FW_COMPONENT(sky_radar, "Sky radar", "components", R"({
  "about": "The aircraft round home by distance and bearing, each a plane turned to its track; the lead one in the accent with a ring, an emergency in red.",
  "contract": 2,
  "parts": ["arc"],
  "frame": [240, 240],
  "props": [
    {"name": "planes", "about": "The aircraft to draw, as the flights feed gives them: a list of [km, bearing in degrees, track in degrees, mark], mark 1 for the lead one, 2 for an emergency. Empty is a clear sky.", "type": "text", "default": "[[26,118,302,0],[12,200,210,0],[33,42,318,0],[38,140,255,0],[41,250,75,0],[6.5,300,330,1]]"},
    {"name": "size", "about": "hero is the radar beside the aircraft on the Flight radar; card is the small picture at the top right of a Home card, with dots.", "type": "select", "options": ["hero", "card"], "default": "hero"}
  ],
  "variants": [
    {"label": "busy sky", "props": {}},
    {"label": "an emergency", "props": {"planes": "[[26,118,302,0],[33,42,318,0],[6.2,175,200,3]]"}},
    {"label": "clear sky", "props": {"planes": "[]"}},
    {"label": "card size", "props": {"size": "card"}}
  ]
})") {
  auto *s = new Sky;
  lv_obj_t *o = fx::drawn(parent, draw, "centre");
  fx::name(o, "sky_radar");
  fx::use(o, "box");
  s->card = card_of(p);
  set_size(o, s->card);
  lv_obj_set_user_data(o, s);
  lv_obj_add_event_cb(o, [](lv_event_t *e) { delete (Sky *) lv_event_get_user_data(e); }, LV_EVENT_DELETE, s);
  lv_obj_remove_flag(o, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  load(*s, p);
}

FW_UPDATE(sky_radar) {
  Sky *s = of(root);
  if (!s || s->card != card_of(p)) return false;  // another size: rebuild
  load(*s, p);
  lv_obj_invalidate(root);
  return true;
}
