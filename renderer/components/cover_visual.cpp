// Cover visual: the Facet window drawing for blinds and curtains (docs: Screens > Blinds and curtains, @facet/ui
// covers.ts coverVisual(), CSS .f-cover). A window with a cover in it; positions are Home Assistant's (0 = closed,
// 100 = open). Plain boxes only (fills, 1-2 px lines, radius), as the docs ask, so it is the same drawing on the panel.
//   window  lay::card_w x lay::content_h (320 x 576 / 212 x 388 = the Covers screen's window column, docs 320 x CH):
//           frame stroke::edge line-hi on surface, rad::button; glass lay::cover_pad (16 / 12) in from the outer edge,
//           colour token disc, rad::part (4), a 2 px line mullion each way (stroke::edge)
//   roller  head rail lay::cover_rail (16 / 12) steel across the whole window, centred on the glass top; fabric
//           steel-dim from the glass top down to the position; the handle = bottom bar lay::cover_bar (8) accent,
//           sp::s1 wider than the glass each side, rad::part
//   Venetian the same rail + bar; under the rail a lay::cover_stack (16 / 12) slat stack (steel) that stays when it is
//           open; slats every lay::cover_slat (24 / 16) down to the bar, sp::s1 (4) thick when flat (tilt 100), the
//           whole pitch when shut (tilt 0), so they close up into a wall; two cords stroke::edge line-hi at 1/4 and 3/4
//   curtain a rod sp::s1 tall line-hi sp::s3 above the glass; two steel-dim panels from sp::s2 above to sp::s2 below
//           the glass, each 5 hair pleats (steel) and an sp::s1 accent leading edge; open = each folds to
//           lay::cover_panel_min (24 / 16), closed = they meet in the middle
//   heading where a moving cover is going (prop heading, -1 = none): blinds = a stroke::edge line across the glass
//           (docs 4 px at half opacity: a full-colour thin line instead, half opacity of a thin line is muddy in 565),
//           curtains = a mark on the rod between the leading edges' goals
//   marks   bar, leading edges and heading = st::on_ink (accent-text), ringed by stroke::edge of the canvas colour
//           (the Dial knob-ring rule): the docs accent on the fabric is 2.1-2.95:1, the ring makes every mark meet bg
// Drawn in one draw event on a cover layer over the window (the frame + glass are objects): a position change
// invalidates only the strip between the old and new edge (+ the bar), so a blind drag repaints a thin band, not the
// window (curtains: the two panels, their pleats spread with the width).
// Drag (draggable, default on; off = the cover has no position, docs): vertical on a blind (the edge follows the
// finger), sideways on curtains (the distance from the middle); press = the value jumps there and the edge sweeps
// (Motion value; docs jumps: the Slider's tap sweep, so every value drag in Facet feels the same), moving follows the
// finger. Sound: fw::step_ticks per 1 % step crossed (0 and 100 = landmarks, closed / open). Events: "change" (detail =
// position) on release when it moved; "blocked" when disabled / unavailable.
// States: disabled (st::OFF) = the marks (bar, edges, heading) st::disabled_knob (text-dim: text-mute is 1.7:1 on the
// fabric), a press = blocked + error;
// unavailable = the empty window with "Unavailable" (body, text-dim) on the glass, no cover. Update path: position and
// tilt sweep (Motion value, silent, no ticks), heading snaps (invalidate), kind / draggable / state refill (state
// changes fade in). While a finger holds it the user wins. Contrast pairs: tokens/colour.json section "cover_visual".
#include "fw.h"

namespace {
constexpr lv_state_t ST_OFF = fw::st::OFF;
enum Kind { ROLLER, VENETIAN, CURTAIN };
struct Cover {
  Kind kind = ROLLER;
  int state = 0;  // 0 default, 1 disabled, 2 unavailable
  bool drag = true, quiet = false, grab = false, active = false, moved = false;
  int pos = 0, tilt = 50, heading = -1, press_pos = 0;
  int32_t shown = 0, shown_tilt = 500;  // x 10
  int shown_step = 0;
  lv_point_t press_pt = {0, 0};
  lv_obj_t *layer = nullptr;
};
Cover *of(lv_obj_t *o) { return (Cover *) lv_obj_get_user_data(o); }
Kind kind_of(fw::Props p) {
  const char *k = p["kind"] | "roller";
  return !strcmp(k, "venetian") ? VENETIAN : !strcmp(k, "curtain") ? CURTAIN : ROLLER;
}
int clamp100(double v) { return (int) std::lround(LV_CLAMP(0.0, v, 100.0)); }
int W() { return fw::lay::card_w; }
int H() { return fw::lay::content_h; }
int P() { return fw::lay::cover_pad; }
int GH() { return H() - 2 * P(); }

// geometry of what is shown (local px of the cover layer = the window box)
float fabric_h(const Cover *d, int32_t shown) {  // blinds: fabric height under the glass top
  const float min = d->kind == VENETIAN ? (float) fw::lay::cover_stack : 0.0f;
  return min + (GH() - min) * (1.0f - shown / 1000.0f);
}
float panel_w(int32_t shown) {  // curtains: each panel's width
  const float mn = (float) fw::lay::cover_panel_min;
  return mn + (W() / 2.0f - mn) * (1.0f - shown / 1000.0f);
}

void rect(lv_layer_t *l, const lv_area_t &o, float x1, float y1, float x2, float y2, lv_color_t c, int r = 0, lv_opa_t opa = LV_OPA_COVER) {
  if (x2 <= x1 || y2 <= y1) return;
  lv_draw_rect_dsc_t d;
  lv_draw_rect_dsc_init(&d);
  d.bg_color = c;
  d.bg_opa = opa;
  d.radius = r;
  lv_area_t a = {o.x1 + (int32_t) std::lround(x1), o.y1 + (int32_t) std::lround(y1), o.x1 + (int32_t) std::lround(x2) - 1,
                 o.y1 + (int32_t) std::lround(y2) - 1};
  lv_draw_rect(l, &d, &a);
}

// a mark (handle bar, leading edge, heading) ringed by stroke::edge of the canvas colour: the knob-ring rule, so the
// mark meets bg on every side (the accent on the fabric itself is 2.1-2.95:1)
void mark_rect(lv_layer_t *l, const lv_area_t &o, float x1, float y1, float x2, float y2, lv_color_t c, int r = 0) {
  const int g = fw::stroke::edge;
  rect(l, o, x1 - g, y1 - g, x2 + g, y2 + g, fw::T->bg, r ? r + g : 0);
  rect(l, o, x1, y1, x2, y2, c, r);
}

void cover_draw(lv_event_t *e) {
  auto *lo = (lv_obj_t *) lv_event_get_current_target(e);
  auto *s = (lv_obj_t *) lv_event_get_user_data(e);
  const Cover *d = of(s);
  const fw::Theme &t = *fw::T;
  lv_layer_t *l = lv_event_get_layer(e);
  lv_area_t o;
  lv_obj_get_coords(lo, &o);
  const int w = W(), p = P(), gh = GH(), gw = w - 2 * p;
  const lv_color_t mark = d->state ? fw::st::disabled_knob(t) : fw::st::on_ink(t);  // accent-text / text-dim: 3:1 on bg
  const int s1 = fw::sp::s1;
  if (d->kind == CURTAIN) {
    const float top = p - fw::sp::s2, bot = H() - p + fw::sp::s2, pw = panel_w(d->shown);
    rect(l, o, 0, p - fw::sp::s3, w, p - fw::sp::s3 + s1, t.line_hi);  // rod
    for (int side = 0; side < 2; side++) {
      const float x0 = side ? w - pw : 0, x1 = side ? w : pw;
      rect(l, o, x0, top, x1, bot, t.steel_dim);
      for (int i = 1; i <= 5; i++) {
        const float x = x0 + pw * i / 6.0f;
        rect(l, o, x, top, x + fw::stroke::hair, bot, t.steel);
      }
      if (side) mark_rect(l, o, x0, top, x0 + s1, bot, mark);  // leading edges
      else mark_rect(l, o, x1 - s1, top, x1, bot, mark);
    }
    if (d->heading >= 0) {
      const float pw2 = panel_w(d->heading * 10);
      mark_rect(l, o, pw2, p - fw::sp::s3, w - pw2, p - fw::sp::s3 + s1, mark);
    }
    return;
  }
  const float fh = fabric_h(d, d->shown);
  if (d->kind == ROLLER) {
    rect(l, o, p, p, w - p, p + fh, t.steel_dim);
  } else {
    // Venetian: cords, then the stack and the slats down to the fabric edge (clipped there)
    const float cx[2] = {p + gw / 4.0f, p + gw * 3 / 4.0f};
    for (float x : cx) rect(l, o, x - fw::stroke::edge / 2.0f, p, x + fw::stroke::edge / 2.0f, p + fh, t.line_hi);
    rect(l, o, p, p, w - p, p + std::min(fh, (float) fw::lay::cover_stack), t.steel);
    const float th = s1 + (fw::lay::cover_slat - s1) * (1000 - d->shown_tilt) / 1000.0f;
    for (int y = fw::lay::cover_slat; y < gh; y += fw::lay::cover_slat) {
      const float y0 = p + y - th / 2.0f, y1 = std::min(p + y + th / 2.0f, p + fh);
      rect(l, o, p, y0, w - p, y1, t.steel);
    }
  }
  rect(l, o, 0, p - fw::lay::cover_rail / 2.0f, w, p + fw::lay::cover_rail / 2.0f, t.steel, fw::rad::part);  // head rail
  if (d->heading >= 0) {  // a thin line where it is going (docs: 4 px at half opacity; here stroke::edge, ringed, full)
    const float y = p + gh * (1.0f - d->heading / 100.0f);
    mark_rect(l, o, p, y - fw::stroke::edge / 2.0f, w - p, y + fw::stroke::edge / 2.0f, mark);
  }
  mark_rect(l, o, p - s1, p + fh - fw::lay::cover_bar / 2.0f, w - p + s1, p + fh + fw::lay::cover_bar / 2.0f, mark, fw::rad::part);
}

// the band a position change repaints: between the old and new edge, padded by the bar / leading edge
void dirty(lv_obj_t *s, int32_t a, int32_t b) {
  auto *d = of(s);
  if (!d->layer) return;
  lv_area_t o;
  lv_obj_get_coords(d->layer, &o);
  lv_area_t r;
  if (d->kind == CURTAIN) {
    const float w0 = std::min(panel_w(a), panel_w(b)), w1 = std::max(panel_w(a), panel_w(b));
    const int pad = fw::sp::s1 + fw::stroke::edge + 1;  // the edge + its ring
    (void) w0;  // the pleats sit at fractions of the panel width: the whole panel moves, not just its edge
    lv_area_t l = {o.x1, o.y1, o.x1 + (int32_t) std::ceil(w1) + pad, o.y2};
    lv_area_t rr = {o.x2 - (int32_t) std::ceil(w1) - pad, o.y1, o.x2, o.y2};
    lv_obj_invalidate_area(d->layer, &l);
    lv_obj_invalidate_area(d->layer, &rr);
    return;
  }
  const float y0 = std::min(fabric_h(d, a), fabric_h(d, b)), y1 = std::max(fabric_h(d, a), fabric_h(d, b));
  const int pad = fw::lay::cover_bar / 2 + fw::lay::cover_slat;  // the bar + a slat cut at the edge
  r = {o.x1, o.y1 + P() + (int32_t) y0 - pad, o.x2, o.y1 + P() + (int32_t) std::ceil(y1) + pad};
  lv_obj_invalidate_area(d->layer, &r);
}

void show(void *o, int32_t v) {
  auto *s = (lv_obj_t *) o;
  auto *d = of(s);
  v = LV_CLAMP(0, v, 1000);
  if (v != d->shown) dirty(s, d->shown, v);
  d->shown = v;
  const int i = (v + 5) / 10;
  if (i != d->shown_step && !d->quiet && !lv_obj_has_state(s, ST_OFF)) fw::step_ticks(s, d->shown_step, i, 100);
  d->shown_step = i;
}
void show_tilt(void *o, int32_t v) {
  auto *s = (lv_obj_t *) o;
  auto *d = of(s);
  if (v == d->shown_tilt) return;
  d->shown_tilt = v;
  if (d->layer) lv_obj_invalidate(d->layer);
}
void stop_sweep(lv_obj_t *s) { lv_anim_delete(s, (lv_anim_exec_xcb_t) show); }
void unquiet(lv_anim_t *a) { of((lv_obj_t *) lv_anim_get_user_data(a))->quiet = false; }
void sweep_quiet(lv_obj_t *s, int32_t from, int32_t to) {
  of(s)->quiet = true;
  fw::animate(s, fw::st::SWEEP, {show, from, to, fw::mo::VALUE, 0, false, unquiet, s});
}
int pos_at(lv_obj_t *s, lv_point_t pt) {
  auto *d = of(s);
  lv_area_t a;
  lv_obj_get_coords(s, &a);
  if (d->kind == CURTAIN) {
    const float dd = std::fabs(pt.x - a.x1 - W() / 2.0f), mn = (float) fw::lay::cover_panel_min;
    return clamp100((dd - mn) / (W() / 2.0f - mn) * 100);
  }
  return clamp100(100 - (pt.y - a.y1 - P()) / (float) GH() * 100);
}

lv_obj_t *box(lv_obj_t *parent) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_add_flag(o, LV_OBJ_FLAG_IGNORE_LAYOUT);
  return o;
}

void fill(lv_obj_t *s, fw::Props p) {
  const fw::Theme &t = *fw::T;
  auto *d = of(s);
  const char *state = p["state"] | "default";
  const bool unavail = !strcmp(state, "unavailable"), off = unavail || !strcmp(state, "disabled");
  *d = Cover{};
  d->kind = kind_of(p);
  d->state = unavail ? 2 : off ? 1 : 0;
  d->drag = p["draggable"] | true;
  d->pos = clamp100(p["position"] | 40.0);
  d->tilt = clamp100(p["tilt"] | 50.0);
  d->heading = (p["heading"] | -1.0) < 0 ? -1 : clamp100(p["heading"] | -1.0);
  d->shown = d->pos * 10;
  d->shown_step = d->pos;
  d->shown_tilt = d->tilt * 10;

  lv_obj_remove_style_all(s);
  lv_obj_remove_flag(s, (lv_obj_flag_t) (LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_SCROLL_CHAIN | LV_OBJ_FLAG_GESTURE_BUBBLE));
  lv_obj_set_flag(s, LV_OBJ_FLAG_CLICKABLE, d->drag || off);
  lv_obj_set_size(s, W(), H());
  lv_obj_set_style_clip_corner(s, true, 0);

  lv_obj_t *fr = box(s);  // frame
  lv_obj_set_size(fr, W(), H());
  lv_obj_set_style_radius(fr, fw::rad::button, 0);
  lv_obj_set_style_border_width(fr, fw::stroke::edge, 0);
  lv_obj_set_style_border_color(fr, t.line_hi, 0);
  lv_obj_set_style_bg_color(fr, t.surface, 0);
  lv_obj_set_style_bg_opa(fr, LV_OPA_COVER, 0);
  lv_obj_t *g = box(fr);  // glass (inside the frame's border: cover_pad from the outer edge)
  lv_obj_set_size(g, W() - 2 * P(), H() - 2 * P());
  lv_obj_set_pos(g, P() - fw::stroke::edge, P() - fw::stroke::edge);
  lv_obj_set_style_radius(g, fw::rad::part, 0);
  lv_obj_set_style_bg_color(g, t.disc, 0);
  lv_obj_set_style_bg_opa(g, LV_OPA_COVER, 0);
  lv_obj_t *mv = box(g);
  lv_obj_set_size(mv, fw::stroke::edge, LV_PCT(100));
  lv_obj_align(mv, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_bg_color(mv, t.line, 0);
  lv_obj_set_style_bg_opa(mv, LV_OPA_COVER, 0);
  lv_obj_t *mh = box(g);
  lv_obj_set_size(mh, LV_PCT(100), fw::stroke::edge);
  lv_obj_align(mh, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_bg_color(mh, t.line, 0);
  lv_obj_set_style_bg_opa(mh, LV_OPA_COVER, 0);
  if (unavail) {
    lv_obj_center(fw::text(g, fw::ty::body, fw::st::UNAVAILABLE_TEXT, fw::st::unavailable_note(t)));
  } else {
    lv_obj_t *cl = d->layer = box(s);
    lv_obj_set_size(cl, W(), H());
    lv_obj_add_event_cb(cl, cover_draw, LV_EVENT_DRAW_MAIN, s);
  }
  if (off) lv_obj_add_state(s, ST_OFF);
}
}  // namespace

FW_COMPONENT(cover_visual, "Cover visual", "components", R"({
  "about": "A window with a roller blind, a Venetian blind or curtains in it, at the cover's position. Drag the blind's edge, or the curtains sideways, to set it.",
  "frame": [224, 400],
  "props": [
    {"name": "kind", "type": "select", "options": ["roller", "venetian", "curtain"], "default": "roller"},
    {"name": "position", "type": "range", "min": 0, "max": 100, "step": 1, "default": 40},
    {"name": "tilt", "type": "range", "min": 0, "max": 100, "step": 1, "default": 50},
    {"name": "heading", "type": "range", "min": -1, "max": 100, "step": 1, "default": -1},
    {"name": "draggable", "type": "bool", "default": true},
    {"name": "state", "type": "select", "options": ["default", "disabled", "unavailable"], "default": "default"}
  ],
  "variants": [
    {"label": "roller · 40 %", "props": {}},
    {"label": "Venetian · 30 % · tilt 50", "props": {"kind": "venetian", "position": 30}},
    {"label": "Venetian · shut", "props": {"kind": "venetian", "position": 0, "tilt": 0}},
    {"label": "curtains · 60 %", "props": {"kind": "curtain", "position": 60}},
    {"label": "roller · heading to 90", "props": {"position": 20, "heading": 90}},
    {"label": "curtains · heading to 0", "props": {"kind": "curtain", "position": 70, "heading": 0}},
    {"label": "disabled", "props": {"state": "disabled"}},
    {"label": "unavailable", "props": {"state": "unavailable"}}
  ]
})") {
  auto *d = new Cover;
  lv_obj_t *s = lv_obj_create(parent);
  lv_obj_set_user_data(s, d);
  fill(s, p);
  lv_obj_add_event_cb(s, [](lv_event_t *e) { delete (Cover *) lv_event_get_user_data(e); }, LV_EVENT_DELETE, d);
  lv_obj_add_event_cb(s, [](lv_event_t *e) {
    auto *s = (lv_obj_t *) lv_event_get_current_target(e);
    auto *d = of(s);
    const lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_PRESSED && code != LV_EVENT_PRESSING && code != LV_EVENT_RELEASED && code != LV_EVENT_PRESS_LOST) return;
    lv_point_t pt = {0, 0};
    if (lv_indev_t *in = lv_indev_active()) lv_indev_get_point(in, &pt);
    if (lv_obj_has_state(s, ST_OFF)) {
      if (code == LV_EVENT_PRESSED) fw::st::blocked(s, d->layer ? std::to_string(d->pos).c_str() : "unavailable");
      return;
    }
    if (!d->drag || !d->layer) return;
    if (code == LV_EVENT_PRESSED) {
      stop_sweep(s);
      d->quiet = false;
      d->active = true;
      d->press_pos = d->pos;
      d->press_pt = pt;
      d->moved = false;
      // on the edge (bar / leading edge, within touch::min / 2) = grab; elsewhere = jump there with a sweep
      lv_area_t a;
      lv_obj_get_coords(s, &a);
      const float edge = d->kind == CURTAIN ? std::fabs(std::fabs(pt.x - a.x1 - W() / 2.0f) - (W() / 2.0f - panel_w(d->shown)))
                                            : std::fabs(pt.y - a.y1 - P() - fabric_h(d, d->shown));
      d->grab = edge <= fw::touch::min / 2;
      d->pos = pos_at(s, pt);
      if (d->grab) show(s, d->pos * 10);
      else fw::animate(s, fw::st::SWEEP, {show, d->shown, d->pos * 10, fw::mo::VALUE});
    } else if (!d->active) {
      return;
    } else if (code == LV_EVENT_PRESSING) {
      if (!d->grab && !d->moved && std::hypot(pt.x - d->press_pt.x, pt.y - d->press_pt.y) < fw::sp::s2) return;  // still a tap
      d->moved = true;
      if (!d->grab) { stop_sweep(s); d->grab = true; }
      d->pos = pos_at(s, pt);
      show(s, d->pos * 10);
    } else {
      d->active = false;
      if (d->pos != d->press_pos) fw::emit(s, "change", std::to_string(d->pos).c_str());
    }
  }, LV_EVENT_ALL, nullptr);
}

FW_UPDATE(cover_visual) {
  auto *d = of(root);
  if (!d) return false;
  const char *state = p["state"] | "default";
  const int st = !strcmp(state, "unavailable") ? 2 : !strcmp(state, "disabled") ? 1 : 0;
  if (st != d->state || kind_of(p) != d->kind || (bool) (p["draggable"] | true) != d->drag) {
    const bool state_changed = st != d->state;
    fw::refill(root);
    fill(root, p);
    if (state_changed) fw::st::fade_in_children(root);
    return true;
  }
  const int hd = (p["heading"] | -1.0) < 0 ? -1 : clamp100(p["heading"] | -1.0);
  if (hd != d->heading) { d->heading = hd; if (d->layer) lv_obj_invalidate(d->layer); }
  const int tl = clamp100(p["tilt"] | 50.0);
  if (tl != d->tilt) { d->tilt = tl; fw::animate(root, fw::st::SWEEP, {show_tilt, d->shown_tilt, tl * 10, fw::mo::VALUE}); }
  if (d->active) return true;  // the user holds it: the position waits
  const int ps = clamp100(p["position"] | 40.0);
  d->pos = ps;
  if (ps * 10 != d->shown || lv_anim_get(root, (lv_anim_exec_xcb_t) show)) sweep_quiet(root, d->shown, ps * 10);
  return true;
}
