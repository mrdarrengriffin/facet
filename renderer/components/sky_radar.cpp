// Sky radar (docs: @facet/ui screens/overhead.ts skyRadar(), CSS 50-screens.css .fs-oh-*): the aircraft round home by
// distance (square-root scale) and bearing, each a plane seen from above turned to its track; the lead (the selected
// one, else the closest) in the accent with a ring, an emergency in the error colour. On the contract
// (renderer/CONTRACT.md, "contract": 2, 2026-10-29): drawn with the Pen only, every ink a colour token and every size a
// size token read at draw time, so a theme and the Layout lab restyle it with no code here.
//   box     size hero = layout:sky-radar square (docs 208 / 136); radar = layout:sky-radar-lg (the Radar view, docs
//           576); card = layout:sky-card (the Home card preview, 72 / 48)
//   rings   unit km: 5 / 15 / 30 / 50 km, mi: 3 / 10 / 20 / 30 mi (Settings > Units) at rmax x sqrt(d / edge), rmax =
//           R - half a plane (card: R - space:1); inner ones stroke:hair fg:line, the outer one stroke:edge fg:line-hi
//           (part "arc"); the card draws the second and the last only. Radar: each ring's number (the last with its
//           unit) type:label fg:text-dim, space:2 right of the centre line, just inside the ring (docs .fs-oh-rlbl)
//   compass N / E / S / W on the outer ring (hero only): type:label fg:text-dim centred in a label-box square of fg:bg
//           that cuts the ring (docs .fs-oh-cmp 24 x 24)
//   home    a layout:sky-dot fg:text dot in the middle
//   planes  the Icons foundation's plane (a turn icon, Pen::turned) layout:sky-plane, turned to the track: fg:text-dim,
//           the lead fg:accent with a stroke:edge ring space:2 out (docs .fs-oh-halo inset -8), an emergency fg:error;
//           the lead is kept a ring's width inside the edge and painted last. Card: layout:sky-dot dots, same inks.
//           Radar: layout:sky-plane-lg planes, the callsign (when the feed gives one: the 5 closest + the lead) type:label
//           space:2 right of the plane, fg:text-dim, the lead's fg:text (docs .fs-oh-cs)
// Data: prop planes = the flights feed's [[km, bearing, track, mark, callsign?, hex?], ...] (mark bit 1 = the lead,
// bit 2 = an emergency); empty = a clear sky (rings and home only). Touch (size radar only): a tap within touch:min of a
// plane (the nearest) emits "pick" with its hex (else its callsign) + sound open (states: fw::st::on_tap, no press look:
// the planes move); hero and card are static. Update path: new planes = one invalidate of the box (the feed moves at
// most every few seconds); another size = rebuilt.
// Contrast: tokens/colour.json section "overhead".
#include <cmath>
#include <vector>
#include "fw.h"
#include "fx.h"

namespace {
constexpr float RINGS[2][4] = {{5, 15, 30, 50}, {3, 10, 20, 30}};  // km, mi
constexpr float KM_PER_MI = 1.609344f;
enum Size { HERO, CARD, RADAR };
struct Plane { float km, brg, track; int mark; std::string cs, hex; };
struct Sky { std::vector<Plane> planes; Size size = HERO; int mi = 0; };
Sky *of(lv_obj_t *o) { return (Sky *) lv_obj_get_user_data(o); }
Size size_of(fw::Props p) { const char *s = p["size"] | "hero"; return !strcmp(s, "card") ? CARD : !strcmp(s, "radar") ? RADAR : HERO; }
void load(Sky &s, fw::Props p) {
  s.planes.clear();
  s.mi = !strcmp(p["unit"] | "km", "mi");
  ArduinoJson::JsonDocument j;
  if (deserializeJson(j, p["planes"] | "[]")) return;
  for (ArduinoJson::JsonArrayConst a : j.as<ArduinoJson::JsonArrayConst>())
    s.planes.push_back({a[0] | 0.0f, a[1] | 0.0f, a[2] | 0.0f, a[3] | 0, a[4] | "", a[5] | ""});
}
const char *glyph_of(const Sky &s) { return s.size == RADAR ? "layout:sky-plane-lg" : "layout:sky-plane"; }
float rmax_of(const Sky &s, int R) { return (float) (R - (s.size == CARD ? fx::size("space:1") : fx::size(glyph_of(s)) / 2)); }
// the radius of a distance in km (the unit's square-root scale, capped at the edge)
float rpx(const Sky &s, float rmax, float km) {
  const float edge = RINGS[s.mi][3], d = s.mi ? km / KM_PER_MI : km;
  return rmax * std::sqrt(std::min(std::max(d, 0.0f), edge) / edge);
}
// where a plane sits, centre-relative (the lead kept a ring's width inside the edge)
void pos(const Sky &s, const Plane &a, int R, int *x, int *y) {
  const float rmax = rmax_of(s, R), lift = (a.mark & 1) && s.size != CARD ? fx::size("space:2") + fx::size("stroke:edge") : 0;
  const float r = std::min(rpx(s, rmax, a.km), rmax - lift), an = a.brg * 3.14159265f / 180;
  *x = (int) lroundf(r * sinf(an)), *y = (int) lroundf(-r * cosf(an));
}
const char *ink_of(int mark) { return mark & 2 ? "fg:error" : mark & 1 ? "fg:accent" : "fg:text-dim"; }

void draw(const fx::Pen &p) {
  const Sky *s = of(p.obj);
  if (!s) return;
  const bool card = s->size == CARD;
  const int R = p.half, glyph = fx::size(glyph_of(*s)), s2 = fx::size("space:2"), edge = fx::size("stroke:edge");
  const float rmax = rmax_of(*s, R);
  const float *rings = RINGS[s->mi];
  for (int i = card ? 1 : 0; i < 4; i++) {
    if (card && i == 2) continue;
    const char *st = i == 3 ? "stroke:edge" : "stroke:hair";
    const int r = (int) std::lround(rpx(*s, rmax, s->mi ? rings[i] * KM_PER_MI : rings[i]));
    p.arc(r - fx::size(st) / 2, 0.0f, 360.0f, st, i == 3 ? "fg:line-hi" : "fg:line", false, "arc");
  }
  if (s->size == RADAR) {  // the rings' numbers, down the south line just inside each ring (docs: left R + 8, top R + r - 32)
    const int B = fx::type_box("type:label");
    for (int i = 0; i < 4; i++) {
      char t[16];
      snprintf(t, sizeof t, i == 3 ? "%g %s" : "%g", rings[i], s->mi ? "mi" : "km");
      const int r = (int) std::lround(rpx(*s, rmax, s->mi ? rings[i] * KM_PER_MI : rings[i]));
      p.text(s2 + p.text_w(t, "type:label") / 2, r - s2 - B / 2, t, "type:label", "fg:text-dim");
    }
  }
  if (!card) {  // N / E / S / W, each in a canvas square that cuts the ring under it
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
      int x, y;
      pos(*s, a, R, &x, &y);
      if (card) { p.dot(x, y, "layout:sky-dot", ink_of(a.mark)); continue; }
      p.turned(x, y, "plane", glyph_of(*s), a.track, ink_of(a.mark));
      if (lead) {
        const int d = glyph + 2 * s2;
        p.rect(x - d / 2, y - d / 2, d, d, ink_of(a.mark), d / 2, edge);
      }
      if (s->size == RADAR && !a.cs.empty()) {
        const int dx = glyph / 2 + (lead ? s2 + edge : 0) + s2;
        p.text(x + dx + p.text_w(a.cs.c_str(), "type:label") / 2, y, a.cs.c_str(), "type:label", lead ? "fg:text" : "fg:text-dim");
      }
    }
}
void set_size(lv_obj_t *o, Size z) {
  // max-h: lifts drawn()'s "fit" cap (a hugging parent's height, 0)
  for (const char *t : z == CARD ? std::initializer_list<const char *>{"w:sky-card", "h:sky-card", "max-h:sky-card"}
                       : z == RADAR ? std::initializer_list<const char *>{"w:sky-radar-lg", "h:sky-radar-lg", "max-h:sky-radar-lg"}
                                    : std::initializer_list<const char *>{"w:sky-radar", "h:sky-radar", "max-h:sky-radar"})
    fx::use(o, t);
}
// the Radar view's tap: the nearest plane within touch:min of the finger, "pick" with its hex (else its callsign)
void tapped(lv_event_t *e) {
  if (!fw::st::tap_now(e)) return;
  auto *o = (lv_obj_t *) lv_event_get_current_target(e);
  const Sky *s = of(o);
  if (!s) return;
  float deg;
  int dist, half;
  fx::pointer(o, &deg, &dist, &half);
  const float px = dist * cosf(deg * 3.14159265f / 180), py = dist * sinf(deg * 3.14159265f / 180);
  const Plane *best = nullptr;
  float bd = (float) fx::size("touch:min") * 0.75f;  // a finger's reach round a plane (more than half a touch target)
  for (const Plane &a : s->planes) {
    int x, y;
    pos(*s, a, half, &x, &y);
    const float d = std::hypot(px - x, py - y);
    if (d <= bd) bd = d, best = &a;
  }
  if (!best) return;
  fw::emit(o, "pick", (!best->hex.empty() ? best->hex : best->cs).c_str());
  fw::sound(o, "open");
}
}  // namespace

FW_COMPONENT(sky_radar, "Sky radar", "components", R"({
  "about": "The aircraft round home by distance and bearing, each a plane turned to its track; the lead one in the accent with a ring, an emergency in red.",
  "contract": 2,
  "parts": ["arc"],
  "frame": [240, 240],
  "props": [
    {"name": "planes", "about": "The aircraft to draw, as the flights feed gives them: a list of [km, bearing in degrees, track in degrees, mark, callsign, hex], mark 1 for the lead one, 2 for an emergency; the callsign shows beside the plane on the radar size. Empty is a clear sky.", "type": "text", "default": "[[26,118,302,0,\"RYR6BA\",\"4ca1f3\"],[12,200,210,0,\"HLE19\",\"406a3c\"],[33,42,318,0,\"EXS13DK\",\"40792b\"],[38,140,255,0,\"DLH3VK\",\"3c6589\"],[41,250,75,0,\"TARTN24\",\"43c6f1\"],[6.5,300,330,1,\"EZY45UK\",\"4ca8d2\"]]"},
    {"name": "size", "about": "hero is the radar beside the aircraft on the Flight radar; radar is the big one on its Radar view, with ring numbers, callsigns and planes to tap; card is the small picture at the top right of a Home card, with dots.", "type": "select", "options": ["hero", "radar", "card"], "default": "hero"},
    {"name": "unit", "about": "The distance unit of the rings: km (5, 15, 30, 50) or mi (3, 10, 20, 30), as Settings > Units says.", "type": "select", "options": ["km", "mi"], "default": "km"}
  ],
  "variants": [
    {"label": "busy sky", "props": {}},
    {"label": "an emergency", "props": {"planes": "[[26,118,302,0],[33,42,318,0],[6.2,175,200,3]]"}},
    {"label": "clear sky", "props": {"planes": "[]"}},
    {"label": "radar size", "props": {"size": "radar"}},
    {"label": "miles", "props": {"size": "radar", "unit": "mi"}},
    {"label": "card size", "props": {"size": "card"}}
  ]
})") {
  auto *s = new Sky;
  lv_obj_t *o = fx::drawn(parent, draw, "centre");
  fx::name(o, "sky_radar");
  fx::use(o, "box");
  s->size = size_of(p);
  set_size(o, s->size);
  lv_obj_set_user_data(o, s);
  lv_obj_add_event_cb(o, [](lv_event_t *e) { delete (Sky *) lv_event_get_user_data(e); }, LV_EVENT_DELETE, s);
  lv_obj_remove_flag(o, s->size == RADAR ? LV_OBJ_FLAG_SCROLLABLE : (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  if (s->size == RADAR) {
    lv_obj_add_flag(o, LV_OBJ_FLAG_CLICKABLE);
    fw::st::on_tap(o, tapped, nullptr);
  }
  load(*s, p);
}

FW_UPDATE(sky_radar) {
  Sky *s = of(root);
  if (!s || s->size != size_of(p)) return false;  // another size: rebuild
  load(*s, p);
  lv_obj_invalidate(root);
  return true;
}
