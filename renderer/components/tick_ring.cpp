// Tick ring: the Facet tick ring (docs: Components > Controls > Tick ring, @facet/ui controls.ts tickRing(), LVGL scale
// ticks): `count` ticks over `range` degrees from 12 o'clock, drawn inward from the edge, every `stride`-th a major.
// range 360 (default) = a full turn: `count` ticks evenly round, 360 / count apart, the first at 12 o'clock (docs 354 =
// 360 x 59 / 60, right only for 60 ticks: 24 ticks over 354 put the 3 / 6 / 9 o'clock ticks off their axes, the
// "seriously wrong" 96 px ring, Darren 2026-10-22). Under 360 = an arc of that many degrees from 12 o'clock.
// The basis of the dial scales, the media disc's dotted ring and the clock faces: this page shows the core atom
// fw::tick_ring (atoms.h "Dials and rings"), the same call the Dial and the Thermostat dial make, so the ring is one
// rule everywhere.
//   box     size x size (dp, snapped to the grid), the ticks drawn inward from its edge in ONE draw event
//   ticks   minor lay::dial_tick (12 / 8) line-hi, major lay::dial_tick_major (20 / 12) text-dim, stroke::tick (2)
//           (docs sample: 8 / 16 at 720; here the Dial's tick lengths so every ring in Facet has the same marks)
//   numbers optional: each major's index (c-meta, text-dim) sp::s3 inside it, centred on the cap height (Dial rule)
//   lit     optional progress: ticks up to that share of the ring take the accent (st::on_mark), the rest stay (the
//           ring clock / timer progress; Astro draws that per screen, here it is the ring's own)
// Static (no touch, no sound). Update path: every prop moves in place (spec changed + one invalidate; size resizes),
// silent; lit sweeps with Motion value. Contrast pairs: tokens/colour.json section "tick_ring".
#include "fw.h"

namespace {
struct TR {
  lv_obj_t *ring = nullptr;
  int lit = -1;     // shown lit percentage x 10 (-1 = none)
};
TR *of(lv_obj_t *o) { return (TR *) lv_obj_get_user_data(o); }

void spec_of(fw::Props p, fw::TickRing *r) {
  const fw::Theme &t = *fw::T;
  const int count = std::max(2, (int) (p["count"] | 60));
  const int stride = p["stride"] | 5;
  r->lo = 0;
  r->range = (float) (p["range"] | 360);
  r->hi = r->range >= 360.0f ? count : count - 1;  // a full turn: count ticks evenly round (360 / count apart)
  r->minor = 1;
  r->major = stride > 0 ? stride : 0;
  r->start = 270.0f;
  r->minor_len = fw::lay::dial_tick;
  r->major_len = fw::lay::dial_tick_major;
  r->width = fw::stroke::tick;
  r->minor_c = t.line_hi;
  r->major_c = t.text_dim;
  r->lit_c = fw::st::on_mark(t);
  r->numbers = p["numbers"] | false;
  r->num_c = t.text_dim;
  r->num_gap = fw::sp::s3;
  r->num_style = fw::ty::c_meta;
  r->labels = nullptr;
}

void set_lit(void *o, int32_t v) {  // v = percentage x 10; -1 = none
  auto *s = (lv_obj_t *) o;
  auto *d = of(s);
  d->lit = v;
  fw::TickRing *r = fw::tick_ring_spec(d->ring);
  const double lit = v < 0 ? NAN : (r->hi - r->lo) * v / 1000.0 + r->lo;
  if (std::isnan(lit) != std::isnan(r->lit) || (!std::isnan(lit) && std::fabs(lit - r->lit) > 1e-9)) {
    r->lit = lit;
    fw::tick_ring_changed(d->ring);
  }
}
int lit_of(fw::Props p) { const int v = p["lit"] | -1; return v < 0 ? -1 : std::min(100, v) * 10; }
}  // namespace

FW_COMPONENT(tick_ring, "Tick ring", "components", R"({
  "about": "A ring of ticks drawn inward from its edge, every few a longer major: the basis of the dial scales, the media disc and the clock faces.",
  "frame": [416, 416],
  "props": [
    {"name": "count", "type": "range", "min": 12, "max": 120, "step": 12, "default": 60},
    {"name": "range", "type": "range", "min": 90, "max": 360, "step": 6, "default": 360},
    {"name": "size", "type": "range", "min": 96, "max": 400, "step": 16, "default": 240},
    {"name": "stride", "type": "select", "options": ["5", "3", "6", "10", "0"], "default": "5"},
    {"name": "numbers", "type": "bool", "default": false},
    {"name": "lit", "type": "range", "min": -1, "max": 100, "step": 1, "default": -1}
  ],
  "variants": [
    {"label": "60 · majors every 5", "props": {}},
    {"label": "12 · full turn · numbers", "props": {"count": 12, "stride": "3", "numbers": true}},
    {"label": "dial scale · 270°", "props": {"count": 51, "range": 270, "stride": "5", "size": 320}},
    {"label": "lit 40 %", "props": {"lit": 40}},
    {"label": "small · 96", "props": {"size": 96, "count": 24, "stride": "6"}}
  ]
})") {
  auto *d = new TR;
  lv_obj_t *s = lv_obj_create(parent);
  lv_obj_remove_style_all(s);
  lv_obj_remove_flag(s, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_user_data(s, d);
  lv_obj_add_event_cb(s, [](lv_event_t *e) { delete (TR *) lv_event_get_user_data(e); }, LV_EVENT_DELETE, d);
  const int size = fw::px((float) (p["size"] | 240));
  lv_obj_set_size(s, size, size);
  fw::TickRing r;
  spec_of(p, &r);
  // "stride" comes from a select as a string ("5"); numbers from JSON as an int: accept both
  if (p["stride"].is<const char *>()) r.major = atoi(p["stride"].as<const char *>());
  d->ring = fw::tick_ring(s, size, r);
  set_lit(s, lit_of(p));
}

FW_UPDATE(tick_ring) {
  auto *d = of(root);
  if (!d || !d->ring) return false;
  const int size = fw::px((float) (p["size"] | 240));
  if (lv_obj_get_width(root) != size) {
    lv_obj_set_size(root, size, size);
    lv_obj_set_size(d->ring, size, size);
  }
  fw::TickRing *r = fw::tick_ring_spec(d->ring);
  const double lit = r->lit;
  spec_of(p, r);
  if (p["stride"].is<const char *>()) r->major = atoi(p["stride"].as<const char *>());
  r->lit = lit;
  fw::tick_ring_changed(d->ring);
  const int to = lit_of(p);
  if (to < 0 || d->lit < 0) set_lit(root, to);  // appears / goes at once
  else if (to != d->lit) fw::animate(root, fw::st::SWEEP, {set_lit, d->lit, to, fw::mo::VALUE});
  return true;
}
