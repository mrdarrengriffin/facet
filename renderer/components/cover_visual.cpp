// Cover visual: the Facet window drawing for blinds and curtains (docs: Screens > Blinds and curtains, @facet/ui
// covers.ts coverVisual() + previewCover(), CSS .f-cover / .f-pcover), on the contract (renderer/CONTRACT.md,
// "contract": 2). Built only from token names and the Pen: a theme / accent switch repaints it with no code here.
// Positions are Home Assistant's (0 = closed, 100 = open).
//   size     screen: fills its parent (fx::strip "fill": the Covers screen's window column, docs 320 x CH, the Split's
//            art column with art_fill); card: layout:cover-card square (72 / 48), the Home card's preview
//   window   frame radius:button in surface with a stroke:edge line-hi border; glass layout:cover-pad (16 / 12) in
//            from the outer edge, disc, radius:part, a stroke:edge line mullion each way
//   roller   head rail layout:cover-rail (16 / 12) steel across the whole window, centred on the glass top; fabric
//            steel-dim from the glass top down to the position; the handle = bottom bar layout:cover-bar (8)
//            accent-text, space:1 wider than the glass each side, radius:part
//   Venetian the same rail + bar; under the rail a layout:cover-stack slat stack (steel) that stays when it is open;
//            slats every layout:cover-slat (24 / 16) down to the bar, space:1 thick when flat (tilt 100), the whole
//            pitch when shut (tilt 0), so they close up into a wall; two cords stroke:edge line-hi at 1/4 and 3/4
//   curtain  a rod space:1 tall line-hi space:3 above the glass; two steel-dim panels from space:2 above to space:2
//            below the glass, each 5 hair pleats (steel) and a space:1 accent-text leading edge; open = each folds to
//            layout:cover-panel-min (24 / 16), closed = they meet in the middle
//   heading  where a moving cover is going (prop heading, -1 = none): blinds = a stroke:edge line across the glass,
//            curtains = a mark on the rod between the leading edges' goals
//   marks    bar, leading edges and heading = accent-text, ringed by stroke:edge of bg (the Dial knob-ring rule):
//            the docs accent on the fabric is 2.1-2.95:1, the ring makes every mark meet bg
//   card     (docs previewCover) a frame stroke:edge line-hi space:2 in, a stroke:indicator steel rail space:1 in
//            across its top, the steel-dim fabric down to the position and a space:1 accent bar: always a roller
//            (the docs card shows the first window that way); no drag
// Drag (draggable, default on; off = the cover has no position, docs): vertical on a blind, sideways on curtains (the
// distance from the middle). A drag sets a GHOST target, never the drawing (Darren 2026-10-26): the drawn cover is
// the real position (prop position, HA's). Blinds: the accent handle bar becomes the ghost and follows the finger
// (press = it jumps there with a sweep, Motion value, like the Slider's tap), the real blind stays put with a plain
// steel bar; curtains: the panels trail the finger with a light lerp (Motion fabric-follow, no overshoot; reduced
// motion = they follow at once). On release the ghost stays until the position reaches it (+-1 %) or no move is
// reported for Motion cover-ghost-wait; then it goes (curtains sweep back to the real position). Sound:
// fw::step_ticks per 1 % step the ghost crosses (0 and 100 = landmarks); real moves are silent. Events: "change"
// (detail = position) once, on release, when the target moved; "blocked" when disabled / unavailable.
// States: disabled (st::OFF) = the marks text-dim, a press = blocked + error; unavailable = the empty window with
// "Unavailable" (type:body, text-dim) on the glass, no cover. Redraw (C26): a position change repaints only the band
// between the old and new edge (+ the bar); curtains the two panels. Update path (C35): position and tilt sweep
// (Motion value, silent, no ticks), heading snaps, kind / draggable / state / size redraw the window. While a finger
// holds it the user wins. Contrast pairs: tokens/colour.json section "cover_visual".
#include <cmath>
#include "fw.h"
#include "fx.h"

namespace {
constexpr lv_state_t ST_OFF = fw::st::OFF;
enum Kind { ROLLER, VENETIAN, CURTAIN };
struct Cover {
  Kind kind = ROLLER;
  int state = 0;  // 0 default, 1 disabled, 2 unavailable
  bool card = false, drag = true, grab = false, active = false, moved = false;
  int pos = 0, real = 0, tilt = 50, heading = -1, press_pos = 0;  // pos = the finger's target, real = HA's
  int32_t shown = 0, shown_tilt = 500;                            // x 10: the drawn (real) position, tilt
  int32_t ghost = -1;                                             // x 10 target (-1 none)
  int ghost_step = 0;
  uint32_t ghost_since = 0, lerp_at = 0;  // the last reported move (or release); the curtain lerp's last frame
  lv_point_t press_pt = {0, 0};
  lv_timer_t *timer = nullptr;  // curtain lerp + ghost wait, only while a ghost exists
};
Cover *of(lv_obj_t *o) { return (Cover *) lv_obj_get_user_data(o); }
Kind kind_of(fw::Props p) {
  const char *k = p["kind"] | "roller";
  return !strcmp(k, "venetian") ? VENETIAN : !strcmp(k, "curtain") ? CURTAIN : ROLLER;
}
int clamp100(double v) { return (int) std::lround(LV_CLAMP(0.0, v, 100.0)); }
int num(fw::Props p, const char *k, int d) {  // a bound value arrives as text or a number
  ArduinoJson::JsonVariantConst v = p[k];
  if (v.is<double>()) return (int) std::lround(v.as<double>());
  if (v.is<const char *>() && *(v.as<const char *>())) { char *e; const double x = strtod(v.as<const char *>(), &e); if (!*e) return (int) std::lround(x); }
  return d;
}
int S(const char *t) { return fx::size(t); }
int W(lv_obj_t *s) { return lv_obj_get_content_width(s); }
int H(lv_obj_t *s) { return lv_obj_get_content_height(s); }
int P() { return S("layout:cover-pad"); }

// geometry (window-local px: 0, 0 = the box's top left)
float fabric_h(lv_obj_t *s, const Cover *d, int32_t shown) {  // blinds: fabric height under the glass top
  const float min = d->kind == VENETIAN ? (float) S("layout:cover-stack") : 0.0f;
  return min + (H(s) - 2 * P() - min) * (1.0f - shown / 1000.0f);
}
float panel_w(lv_obj_t *s, int32_t shown) {  // curtains: each panel's width
  const float mn = (float) S("layout:cover-panel-min");
  return mn + (W(s) / 2.0f - mn) * (1.0f - shown / 1000.0f);
}

// a box in window-local px through the Pen (centre-relative)
struct R {
  const fx::Pen &p; int w, h;
  void operator()(float x1, float y1, float x2, float y2, const char *ink, int r = 0, int border = 0) const {
    const int a = (int) std::lround(x1), b = (int) std::lround(y1), c = (int) std::lround(x2), e = (int) std::lround(y2);
    if (c > a && e > b) p.rect(a - w / 2, b - h / 2, c - a, e - b, ink, r, border);
  }
  // a mark (handle bar, leading edge, heading) ringed by stroke:edge of the canvas colour: the knob-ring rule
  void mark(float x1, float y1, float x2, float y2, const char *ink, int r = 0) const {
    const int g = S("stroke:edge");
    (*this)(x1 - g, y1 - g, x2 + g, y2 + g, "fg:bg", r ? r + g : 0);
    (*this)(x1, y1, x2, y2, ink, r);
  }
};

void draw_card(const fx::Pen &p, const Cover *d) {  // docs previewCover (72 box): frame, rail, fabric, bar
  const R r{p, p.w, p.h};
  const int w = p.w, s1 = S("space:1"), s2 = S("space:2"), e = S("stroke:edge"), rail = S("stroke:indicator"), rp = S("radius:part");
  r(s2, s2, w - s2, w - s2, "fg:line-hi", rp, e);                    // frame
  const float top = s2 + e, fh = (w - 2 * top) * (1.0f - d->shown / 1000.0f);
  r(s2 + e + s1 / 2.0f, top, w - s2 - e - s1 / 2.0f, top + fh, "fg:steel-dim");  // fabric
  r(s1, s2 - e, w - s1, s2 - e + rail, "fg:steel", rp);               // rail
  r(top, top + fh - s1 / 2.0f, w - top, top + fh + s1 / 2.0f, d->state ? "fg:text-dim" : "fg:accent", rp);  // bar
}

void draw(const fx::Pen &p) {
  const Cover *d = of(p.obj);
  if (!d) return;
  if (d->card) { draw_card(p, d); return; }
  lv_obj_t *s = p.obj;
  const R r{p, p.w, p.h};
  const int w = p.w, h = p.h, pd = P(), gh = h - 2 * pd, gw = w - 2 * pd;
  const int e = S("stroke:edge"), s1 = S("space:1"), s2 = S("space:2"), s3 = S("space:3"), rp = S("radius:part");
  r(0, 0, w, h, "fg:surface", S("radius:button"));  // frame
  r(0, 0, w, h, "fg:line-hi", S("radius:button"), e);
  r(pd, pd, w - pd, h - pd, "fg:disc", rp);  // glass + mullions
  r(w / 2.0f - e / 2.0f, pd, w / 2.0f + e / 2.0f, h - pd, "fg:line");
  r(pd, h / 2.0f - e / 2.0f, w - pd, h / 2.0f + e / 2.0f, "fg:line");
  if (d->state == 2) {
    p.text(0, 0, fw::st::UNAVAILABLE_TEXT, "type:body", "fg:text-dim");
    return;
  }
  const char *mark = d->state ? "fg:text-dim" : "fg:accent-text";  // st::disabled_knob / st::on_ink: 3:1 on bg
  if (d->kind == CURTAIN) {
    const float top = pd - s2, bot = h - pd + s2, pw = panel_w(s, d->shown);
    r(0, pd - s3, w, pd - s3 + s1, "fg:line-hi");  // rod
    for (int side = 0; side < 2; side++) {
      const float x0 = side ? w - pw : 0, x1 = side ? w : pw;
      r(x0, top, x1, bot, "fg:steel-dim");
      for (int i = 1; i <= 5; i++) {
        const float x = x0 + pw * i / 6.0f;
        r(x, top, x + S("stroke:hair"), bot, "fg:steel");
      }
      if (side) r.mark(x0, top, x0 + s1, bot, mark);  // leading edges
      else r.mark(x1 - s1, top, x1, bot, mark);
    }
    if (d->heading >= 0) {
      const float pw2 = panel_w(s, d->heading * 10);
      r.mark(pw2, pd - s3, w - pw2, pd - s3 + s1, mark);
    }
    return;
  }
  const float fh = fabric_h(s, d, d->shown);
  const bool ghosted = d->ghost >= 0;
  const int slat = S("layout:cover-slat"), stack = S("layout:cover-stack"), bar_h = S("layout:cover-bar"), rail = S("layout:cover-rail");
  if (d->kind == ROLLER) {
    r(pd, pd, w - pd, pd + fh, "fg:steel-dim");
  } else {  // Venetian: cords, then the stack and the slats down to the fabric edge (clipped there)
    const float cx[2] = {pd + gw / 4.0f, pd + gw * 3 / 4.0f};
    for (float x : cx) r(x - e / 2.0f, pd, x + e / 2.0f, pd + fh, "fg:line-hi");
    r(pd, pd, w - pd, pd + std::min(fh, (float) stack), "fg:steel");
    const float th = s1 + (slat - s1) * (1000 - d->shown_tilt) / 1000.0f;
    for (int y = slat; y < gh; y += slat) {
      const float y0 = pd + y - th / 2.0f, y1 = std::min(pd + y + th / 2.0f, pd + fh);
      r(pd, y0, w - pd, y1, "fg:steel");
    }
  }
  r(0, pd - rail / 2.0f, w, pd + rail / 2.0f, "fg:steel", rp);  // head rail
  if (d->heading >= 0 && !ghosted) {  // a thin line where it is going
    const float y = pd + gh * (1.0f - d->heading / 100.0f);
    r.mark(pd, y - e / 2.0f, w - pd, y + e / 2.0f, mark);
  }
  // the handle bar: the real blind's; while a ghost target is out the accent is the ghost's and the real bar is steel
  const auto bar = [&](float y, const char *ink) { r.mark(pd - s1, pd + y - bar_h / 2.0f, w - pd + s1, pd + y + bar_h / 2.0f, ink, rp); };
  bar(fh, ghosted ? "fg:steel" : mark);
  if (ghosted) bar(fabric_h(s, d, d->ghost), mark);
}

void redraw_all(lv_obj_t *s) { fx::redraw(s, -W(s) / 2, -H(s) / 2, W(s), H(s)); }
// the band a position change repaints: between the old and new edge, padded by the bar / leading edge
void dirty(lv_obj_t *s, int32_t a, int32_t b) {
  auto *d = of(s);
  const int w = W(s), h = H(s);
  if (d->card) { redraw_all(s); return; }
  if (d->kind == CURTAIN) {  // the pleats sit at fractions of the panel width: the whole panel moves, not just its edge
    const int w1 = (int) std::ceil(std::max(panel_w(s, a), panel_w(s, b))) + S("space:1") + S("stroke:edge") + 1;
    fx::redraw(s, -w / 2, -h / 2, w1, h);
    fx::redraw(s, w / 2 - w1, -h / 2, w1, h);
    return;
  }
  const float y0 = std::min(fabric_h(s, d, a), fabric_h(s, d, b)), y1 = std::max(fabric_h(s, d, a), fabric_h(s, d, b));
  const int pad = S("layout:cover-bar") / 2 + S("layout:cover-slat") + S("stroke:edge");  // the bar + its ring + a cut slat
  const int top = P() + (int) y0 - pad;
  fx::redraw(s, -w / 2, top - h / 2, w, (int) std::ceil(y1) - (int) y0 + 2 * pad);
}

void show(void *o, int32_t v) {  // the drawn (real) position: silent
  auto *s = (lv_obj_t *) o;
  auto *d = of(s);
  v = LV_CLAMP(0, v, 1000);
  if (v != d->shown) dirty(s, d->shown, v);
  d->shown = v;
}
void show_ghost(void *o, int32_t v) {  // the finger's target: ticks per step crossed
  auto *s = (lv_obj_t *) o;
  auto *d = of(s);
  v = LV_CLAMP(0, v, 1000);
  if (d->kind != CURTAIN && v != d->ghost) dirty(s, d->ghost < 0 ? d->shown : d->ghost, v);
  d->ghost = v;
  const int i = (v + 5) / 10;
  if (i != d->ghost_step) fw::step_ticks(s, d->ghost_step, i, 100);
  d->ghost_step = i;
}
void show_tilt(void *o, int32_t v) {
  auto *s = (lv_obj_t *) o;
  auto *d = of(s);
  if (v == d->shown_tilt) return;
  d->shown_tilt = v;
  redraw_all(s);
}
void sweep_quiet(lv_obj_t *s, int32_t from, int32_t to) { fw::animate(s, fw::st::SWEEP, {show, from, to, fw::mo::VALUE}); }
// the ghost is done (HA got there, or the wait ran out): blinds drop it into the real bar; curtains end at the real
// position (they are already there when HA got there, else they sweep back)
void ghost_done(lv_obj_t *s) {
  auto *d = of(s);
  if (d->ghost < 0) return;
  lv_anim_delete(s, (lv_anim_exec_xcb_t) show_ghost);
  const int32_t g = d->ghost;
  d->ghost = -1;
  if (d->kind == CURTAIN) {
    if (d->shown != d->real * 10) sweep_quiet(s, d->shown, d->real * 10);
    return;
  }
  dirty(s, g, d->shown);
}
// one timer while a ghost is out: curtains lerp toward it (fabric), and the wait for HA runs out
void ghost_tick(lv_timer_t *tm) {
  auto *s = (lv_obj_t *) lv_timer_get_user_data(tm);
  auto *d = of(s);
  const uint32_t dt = lv_tick_elaps(d->lerp_at);
  d->lerp_at = lv_tick_get();
  if (d->kind == CURTAIN && d->ghost >= 0 && d->shown != d->ghost) {
    const int32_t gap = d->ghost - d->shown;
    int32_t step = fw::mo::reduced(s) ? gap : (int32_t) ((int64_t) gap * dt / (fw::mo::ms::fabric_follow + dt));
    if (!step) step = gap > 0 ? 1 : -1;
    show(s, d->shown + step);
  }
  if (d->ghost >= 0 && !d->active && lv_tick_elaps(d->ghost_since) > fw::mo::ms::cover_ghost_wait) ghost_done(s);
  if (d->ghost < 0) { lv_timer_delete(tm); d->timer = nullptr; }
}
void ghost_timer(lv_obj_t *s) {
  auto *d = of(s);
  d->lerp_at = lv_tick_get();
  if (!d->timer) d->timer = lv_timer_create(ghost_tick, LV_DEF_REFR_PERIOD, s);
}
int pos_at(lv_obj_t *s, lv_point_t pt) {
  auto *d = of(s);
  lv_area_t a;
  lv_obj_get_content_coords(s, &a);
  if (d->kind == CURTAIN) {
    const float dd = std::fabs(pt.x - a.x1 - W(s) / 2.0f), mn = (float) S("layout:cover-panel-min");
    return clamp100((dd - mn) / (W(s) / 2.0f - mn) * 100);
  }
  return clamp100(100 - (pt.y - a.y1 - P()) / (float) (H(s) - 2 * P()) * 100);
}

void set(lv_obj_t *s, fw::Props p) {  // the props that redraw the window (kind, state, drag, size)
  auto *d = of(s);
  const char *state = p["state"] | "default";
  const bool unavail = !strcmp(state, "unavailable"), off = unavail || !strcmp(state, "disabled");
  if (d->timer) { lv_timer_delete(d->timer); d->timer = nullptr; }
  lv_anim_delete(s, (lv_anim_exec_xcb_t) show_ghost);
  lv_anim_delete(s, (lv_anim_exec_xcb_t) show);
  lv_anim_delete(s, (lv_anim_exec_xcb_t) show_tilt);
  *d = Cover{};
  d->card = !strcmp(p["size"] | "screen", "card");
  d->kind = d->card ? ROLLER : kind_of(p);
  d->state = unavail ? 2 : off ? 1 : 0;
  d->drag = !d->card && (p["draggable"] | true);
  d->pos = d->real = clamp100(num(p, "position", 40));
  d->tilt = clamp100(num(p, "tilt", 50));
  d->heading = num(p, "heading", -1) < 0 ? -1 : clamp100(num(p, "heading", -1));
  d->shown = d->pos * 10;
  d->shown_tilt = d->tilt * 10;
  if (d->drag || off) lv_obj_add_flag(s, LV_OBJ_FLAG_CLICKABLE);
  else lv_obj_remove_flag(s, LV_OBJ_FLAG_CLICKABLE);
  fx::state(s, ST_OFF, off);
  redraw_all(s);
}
std::string sig(fw::Props p) {
  const bool card = !strcmp(p["size"] | "screen", "card");
  return std::string(p["size"] | "screen") + "|" + (card ? "roller" : p["kind"] | "roller") + "|" + (p["state"] | "default") + "|" +
         ((p["draggable"] | true) ? "drag" : "");
}
}  // namespace

FW_COMPONENT(cover_visual, "Cover visual", "components", R"({
  "about": "A window with a roller blind, a Venetian blind or curtains in it, at the cover's position. Drag the blind's edge, or the curtains sideways, to set it.",
  "contract": 2,
  "parts": [],
  "audit": {"snaps": {"kind": "a roller, Venetian or curtain is a different drawing: structure", "size": "the screen window or the card picture: structure"}},
  "frame": [320, 576],
  "props": [
    {"name": "kind", "about": "What hangs in the window: a roller blind, a Venetian blind with slats, or a pair of curtains.", "type": "select", "options": ["roller", "venetian", "curtain"], "default": "roller"},
    {"name": "position", "about": "How open the cover is, from 0 (closed) to 100 (fully open).", "type": "range", "min": 0, "max": 100, "step": 1, "default": 40},
    {"name": "tilt", "about": "For Venetian blinds: how far the slats are turned, from 0 (shut) to 100 (fully open).", "type": "range", "min": 0, "max": 100, "step": 1, "default": 50},
    {"name": "heading", "about": "Where the cover is moving to while it moves, from 0 to 100, shown as a thin target line. -1 means it is not moving.", "type": "range", "min": -1, "max": 100, "step": 1, "default": -1},
    {"name": "draggable", "about": "Lets you drag the blind's edge, or the curtains sideways, to move the cover.", "type": "bool", "default": true},
    {"name": "size", "about": "screen fills the space it is in, the Blinds screen's window; card is the small picture at the top right of a Home card.", "type": "select", "options": ["screen", "card"], "default": "screen"},
    {"name": "state", "debug": true, "about": "default can be dragged; disabled cannot be moved; unavailable shows the cover cannot be reached.", "type": "select", "options": ["default", "disabled", "unavailable"], "default": "default"}
  ],
  "variants": [
    {"label": "roller · 40 %", "props": {}},
    {"label": "Venetian · 30 % · tilt 50", "props": {"kind": "venetian", "position": 30}},
    {"label": "Venetian · shut", "props": {"kind": "venetian", "position": 0, "tilt": 0}},
    {"label": "curtains · 60 %", "props": {"kind": "curtain", "position": 60}},
    {"label": "roller · heading to 90", "props": {"position": 20, "heading": 90}},
    {"label": "curtains · heading to 0", "props": {"kind": "curtain", "position": 70, "heading": 0}},
    {"label": "card · 60 %", "props": {"size": "card", "position": 60}},
    {"label": "disabled", "props": {"state": "disabled"}},
    {"label": "unavailable", "props": {"state": "unavailable"}}
  ]
})") {
  auto *d = new Cover;
  lv_obj_t *s = fx::strip(parent, draw, "free");
  fx::name(s, "cover_visual");
  lv_obj_set_user_data(s, d);
  lv_obj_remove_flag(s, (lv_obj_flag_t) (LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_SCROLL_CHAIN | LV_OBJ_FLAG_GESTURE_BUBBLE));
  fw::shape(s, sig(p));
  if (!strcmp(p["size"] | "screen", "card")) for (const char *t : {"w:cover-card", "h:cover-card"}) fx::use(s, t);
  else fx::use(s, "fill");
  set(s, p);
  lv_obj_add_event_cb(s, [](lv_event_t *e) {
    auto *d = (Cover *) lv_event_get_user_data(e);
    if (d->timer) lv_timer_delete(d->timer);
    delete d;
  }, LV_EVENT_DELETE, d);
  lv_obj_add_event_cb(s, [](lv_event_t *e) {
    auto *s = (lv_obj_t *) lv_event_get_current_target(e);
    auto *d = of(s);
    const lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_PRESSED && code != LV_EVENT_PRESSING && code != LV_EVENT_RELEASED && code != LV_EVENT_PRESS_LOST) return;
    lv_point_t pt = {0, 0};
    if (lv_indev_t *in = lv_indev_active()) lv_indev_get_point(in, &pt);
    if (lv_obj_has_state(s, ST_OFF)) {
      if (code == LV_EVENT_PRESSED) fw::st::blocked(s, d->state == 2 ? "unavailable" : std::to_string(d->real).c_str());
      return;
    }
    if (!d->drag) return;
    if (code == LV_EVENT_PRESSED) {
      lv_anim_delete(s, (lv_anim_exec_xcb_t) show_ghost);
      if (d->kind == CURTAIN) lv_anim_delete(s, (lv_anim_exec_xcb_t) show);  // the fabric follows the finger from where it is
      const int32_t cur = d->ghost >= 0 ? d->ghost : d->shown;  // what the finger takes hold of
      d->active = true;
      d->press_pos = (cur + 5) / 10;
      d->press_pt = pt;
      d->moved = false;
      // on the edge (bar / leading edge, within touch:min / 2) = grab; elsewhere = the ghost jumps there with a sweep
      lv_area_t a;
      lv_obj_get_content_coords(s, &a);
      const float edge = d->kind == CURTAIN ? std::fabs(std::fabs(pt.x - a.x1 - W(s) / 2.0f) - (W(s) / 2.0f - panel_w(s, cur)))
                                            : std::fabs(pt.y - a.y1 - P() - fabric_h(s, d, cur));
      d->grab = edge <= S("touch:min") / 2;
      d->pos = pos_at(s, pt);
      if (d->ghost < 0) d->ghost = cur, d->ghost_step = d->press_pos;
      ghost_timer(s);
      if (d->grab) show_ghost(s, d->pos * 10);
      else fw::animate(s, fw::st::SWEEP, {show_ghost, d->ghost, d->pos * 10, fw::mo::VALUE});
    } else if (!d->active) {
      return;
    } else if (code == LV_EVENT_PRESSING) {
      if (!d->grab && !d->moved && std::hypot(pt.x - d->press_pt.x, pt.y - d->press_pt.y) < S("space:2")) return;  // still a tap
      d->moved = true;
      if (!d->grab) { lv_anim_delete(s, (lv_anim_exec_xcb_t) show_ghost); d->grab = true; }
      d->pos = pos_at(s, pt);
      show_ghost(s, d->pos * 10);
    } else {
      d->active = false;
      d->ghost_since = lv_tick_get();
      if (d->pos != d->press_pos) fw::emit(s, "change", std::to_string(d->pos).c_str());  // the one command
      if (d->pos == d->real && d->kind != CURTAIN) ghost_done(s);  // back on the real position: nothing to wait for
    }
  }, LV_EVENT_ALL, nullptr);
}

// Update path: kind / state / draggable / size = the window again (snap); heading snaps; tilt and position sweep
// (silent). While a finger holds it the drawing waits.
FW_UPDATE(cover_visual) {
  auto *d = of(root);
  if (!d) return false;
  if (d->card != !strcmp(p["size"] | "screen", "card")) return false;  // the other size: built again
  if (!fw::shape(root, sig(p))) { set(root, p); return true; }
  const int hd = num(p, "heading", -1) < 0 ? -1 : clamp100(num(p, "heading", -1));
  if (hd != d->heading) { d->heading = hd; redraw_all(root); }
  const int tl = clamp100(num(p, "tilt", 50));
  if (tl != d->tilt) { d->tilt = tl; fw::animate(root, fw::st::SWEEP, {show_tilt, d->shown_tilt, tl * 10, fw::mo::VALUE}); }
  const int ps = clamp100(num(p, "position", 40));
  if (ps != d->real) d->real = ps, d->ghost_since = lv_tick_get();  // a reported move: the ghost keeps waiting
  if (d->active) return true;  // the user holds it: the drawing waits
  if (d->ghost >= 0) {
    if (std::abs(ps * 10 - d->ghost) <= 10) ghost_done(root);  // HA got there
    else if (d->kind == CURTAIN) return true;                   // the fabric holds the target till then
  }
  if (ps * 10 != d->shown || lv_anim_get(root, (lv_anim_exec_xcb_t) show)) sweep_quiet(root, d->shown, ps * 10);
  return true;
}
