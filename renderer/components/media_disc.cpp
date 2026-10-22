// Media disc: the Facet media disc (docs: Components > Controls > Media disc, @facet/ui controls.ts disc(), CSS .f-disc;
// the Media screen's default hero): a dotted ring, an accent progress arc from 12 o'clock, a disc face with two grooves,
// an accent label spot with a hole, and a blue marker circling the face (one turn per Motion disc-marker, 8 s) while it
// plays. Built from the core "Dials and rings" atoms: the dotted ring is fw::tick_ring (dots), the progress fw::value_arc.
//   size     prop 256 (lay::disc_screen 256 / 168, the Media screen) or 320 (lay::disc 320 / 216, docs default size);
//            each part has its own token per size so every ring lands on the grid (docs: sizes on a 16 px step)
//   dots     72 round dots over 355 degrees from 12 o'clock at the edge, stroke::arc (4 / 2; docs 3) line-hi
//   progress inset sp::s2 from the edge (docs disc - disc-prog = 16), stroke::arc accent round caps, 0-100 % of the turn
//   face     disc-face(-screen), colour token disc, 1 px line edge; grooves = 1 px groove rings disc-groove and 2 x
//            disc-groove smaller (docs face - 48 / - 96)
//   spot     disc-spot(-screen) accent, the hole disc-hole in the canvas colour (bg)
//   marker   disc-marker (12 / 8; docs 10) blue, centred sp::s5 inside the face edge (docs 16)
// Motion: the marker orbits (small object, translate only, writes only on a whole-px change; Motion token disc-marker,
// linear) while playing; paused = it stops where it is (docs animation-play-state). Reduced motion = still.
// A read-out (docs: no touch): no events, no sound. Update path: progress sweeps (Motion value), playing starts / stops
// the orbit in place, size refills (snap). Contrast pairs: tokens/colour.json section "media_disc".
#include "fw.h"

namespace {
constexpr int32_t STEPS = 1000;
struct Disc {
  lv_obj_t *arc = nullptr, *marker = nullptr;
  int32_t shown = 0;   // progress x 10
  int32_t angle = 0;   // marker angle x 10 (0 = 12 o'clock)
  int orbit = 0, centre = 0;
  bool playing = false;
  bool large = false;
};
Disc *of(lv_obj_t *o) { return (Disc *) lv_obj_get_user_data(o); }
bool large_of(fw::Props p) { return !strcmp(p["size"] | "256", "320"); }
int32_t target_of(fw::Props p) { return (int32_t) std::lround(LV_CLAMP(0.0, (double) (p["progress"] | 38.0), 100.0) * 10); }

void show(void *o, int32_t v) {
  auto *d = of((lv_obj_t *) o);
  d->shown = v;
  if (d->arc) lv_arc_set_value(d->arc, v);
}
void orbit(void *o, int32_t v) {
  auto *d = of((lv_obj_t *) o);
  d->angle = v % 3600;
  if (!d->marker) return;
  const int M = fw::lay::disc_marker;  // (its width reads 0 until the first layout)
  const fw::StrokePt c = fw::polar((float) d->centre, (float) d->centre, (float) d->orbit, 270.0f + d->angle / 10.0f);
  const int32_t x = (int32_t) std::lround(c.x - M / 2.0f), y = (int32_t) std::lround(c.y - M / 2.0f);
  if (x != lv_obj_get_x(d->marker) || y != lv_obj_get_y(d->marker)) lv_obj_set_pos(d->marker, x, y);
}
void spin(lv_obj_t *s, bool on) {
  auto *d = of(s);
  d->playing = on;
  lv_anim_delete(s, (lv_anim_exec_xcb_t) orbit);
  if (!on || fw::mo::reduced(s)) return;  // paused: stays where it is; reduced motion: still
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, s);
  lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t) orbit);
  lv_anim_set_values(&a, d->angle, d->angle + 3600);
  lv_anim_set_duration(&a, fw::mo::ms::disc_marker);
  lv_anim_set_path_cb(&a, fw::mo::EASINGS[fw::mo::E_linear].path);
  lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
  lv_anim_start(&a);
}

lv_obj_t *circle(lv_obj_t *parent, int d, lv_color_t bg, bool fill, int border, lv_color_t bc) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_add_flag(o, LV_OBJ_FLAG_IGNORE_LAYOUT);
  lv_obj_set_size(o, d, d);
  lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(o, bg, 0);
  lv_obj_set_style_bg_opa(o, fill ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(o, border, 0);
  lv_obj_set_style_border_color(o, bc, 0);
  lv_obj_center(o);
  return o;
}

void fill(lv_obj_t *s, fw::Props p) {
  const fw::Theme &t = *fw::T;
  auto *d = of(s);
  const bool large = d->large = large_of(p);
  const int D = large ? fw::lay::disc : fw::lay::disc_screen;
  const int F = large ? fw::lay::disc_face : fw::lay::disc_face_screen;
  const int SPOT = large ? fw::lay::disc_spot : fw::lay::disc_spot_screen;
  static_assert(fw::lay::disc % 8 == 0 && fw::lay::disc_screen % 8 == 0, "the disc's centre must land on the 4 px grid");
  lv_obj_remove_style_all(s);
  lv_obj_remove_flag(s, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_size(s, D, D);

  fw::TickRing tr;  // 72 dots over 355 degrees from 12 o'clock (docs)
  tr.lo = 0;
  tr.hi = 71;
  tr.minor = 1;
  tr.major = 0;
  tr.start = 270.0f;
  tr.range = 355.0f;
  tr.dots = true;
  tr.width = fw::stroke::arc;
  tr.minor_c = tr.major_c = t.line_hi;
  fw::tick_ring(s, D, tr);

  fw::ArcLook k;
  k.start = 270.0f;
  k.range = 359.0f;  // docs end 269: a full turn that never closes
  k.steps = STEPS;
  k.ind_w = fw::stroke::arc;
  k.ind_c = t.accent;
  d->arc = fw::value_arc(s, D - 2 * fw::sp::s2, k);
  lv_obj_center(d->arc);

  d->centre = D / 2;
  d->orbit = F / 2 - fw::sp::s5;
  lv_obj_t *face = circle(s, F, t.disc, true, fw::stroke::hair, t.line);
  for (int i = 1; i <= 2; i++) circle(face, F - i * fw::lay::disc_groove, t.disc, false, fw::stroke::hair, t.groove);
  lv_obj_t *spot = circle(face, SPOT, t.accent, true, 0, t.accent);
  circle(spot, fw::lay::disc_hole, t.bg, true, 0, t.bg);

  d->marker = circle(s, fw::lay::disc_marker, t.blue, true, 0, t.blue);
  // placed by orbit() from the top-left, not centred: circle() centres it (LV_ALIGN_CENTER), and a set_pos on a
  // centred object is an offset FROM the centre, which threw the marker off its circle (Darren 2026-10-22: "the
  // complete wrong position"; the audit's circle check found it)
  lv_obj_set_align(d->marker, LV_ALIGN_TOP_LEFT);
  fw::qa::free(d->marker, "the marker orbits the face by angle");
  fw::qa::circle(d->marker, s, d->centre, d->centre, d->orbit);
  orbit(s, d->angle);
  show(s, d->shown);
}
}  // namespace

FW_COMPONENT(media_disc, "Media disc", "components", R"({
  "about": "Progress as an accent arc around a record; the blue marker circles while it plays.",
  "frame": [336, 336],
  "props": [
    {"name": "progress", "type": "range", "min": 0, "max": 100, "step": 1, "default": 38},
    {"name": "playing", "type": "bool", "default": true},
    {"name": "size", "type": "select", "options": ["256", "320"], "default": "256"}
  ],
  "variants": [
    {"label": "playing · 38 %", "props": {}},
    {"label": "paused · 72 %", "props": {"playing": false, "progress": 72}},
    {"label": "start · 0 %", "props": {"playing": false, "progress": 0}},
    {"label": "large · 320", "props": {"size": "320"}}
  ]
})") {
  auto *d = new Disc;
  lv_obj_t *s = lv_obj_create(parent);
  lv_obj_set_user_data(s, d);
  lv_obj_add_event_cb(s, [](lv_event_t *e) { delete (Disc *) lv_event_get_user_data(e); }, LV_EVENT_DELETE, d);
  d->shown = target_of(p);
  fill(s, p);
  spin(s, p["playing"] | true);
}

FW_UPDATE(media_disc) {
  auto *d = of(root);
  if (!d) return false;
  if (large_of(p) != d->large) {
    const bool playing = d->playing;
    lv_anim_delete(root, (lv_anim_exec_xcb_t) orbit);
    fw::refill(root);
    fill(root, p);
    spin(root, playing);
  }
  const bool playing = p["playing"] | true;
  if (playing != d->playing) spin(root, playing);
  const int32_t to = target_of(p);
  if (to != d->shown || lv_anim_get(root, (lv_anim_exec_xcb_t) show)) fw::animate(root, fw::st::SWEEP, {show, d->shown, to, fw::mo::VALUE});
  return true;
}
