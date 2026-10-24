// Sports surfaces runtime (core/surface.h): the surface table lookups and the one-object tennis court.
#include "surface.h"
#include <strings.h>

namespace fw::sf {
namespace {
int snap4(float v) { return (int) lroundf(v / 4.0f) * 4; }
struct Court {
  const Surface *s;
  TennisFit fit;
  Box mark;
  bool show = false;
};
Court *court_of(lv_obj_t *o) { return (Court *) lv_obj_get_user_data(o); }
void free_court(lv_event_t *e) { delete (Court *) lv_event_get_user_data(e); }
void fill(lv_layer_t *layer, const lv_area_t &o, int x, int y, int w, int h, lv_color_t c, lv_opa_t opa = LV_OPA_COVER) {
  if (w <= 0 || h <= 0) return;
  lv_draw_rect_dsc_t d;
  lv_draw_rect_dsc_init(&d);
  d.bg_color = c, d.bg_opa = opa;
  lv_area_t a = {o.x1 + x, o.y1 + y, o.x1 + x + w - 1, o.y1 + y + h - 1};
  lv_draw_rect(layer, &d, &a);
}
void draw(lv_event_t *e) {
  lv_obj_t *obj = (lv_obj_t *) lv_event_get_target(e);
  Court *c = court_of(obj);
  if (!c) return;
  lv_layer_t *layer = lv_event_get_layer(e);
  lv_area_t o;
  lv_obj_get_coords(obj, &o);
  const Theme &t = *T;
  const Surface &s = *c->s;
  const TennisFit &f = c->fit;
  using namespace tennis;
  fill(layer, o, 0, 0, f.w, f.h, t.*s.out);                                       // run-off
  fill(layer, o, f.court.x, f.court.y, f.court.w, f.court.h, t.*s.court);         // court
  if (s.stripe && s.stripes > 0) {                                                // mown bands across the lawn, from the net out
    const float band = length / 2 / s.stripes;
    for (int side = 0; side < 2; side++)
      for (int k = 1;; k += 2) {
        const float m0 = side ? length / 2 + k * band : length / 2 - (k + 1) * band, m1 = m0 + band;
        const int x0 = std::max(0, f.X(m0)), x1 = std::min(f.w, f.X(m1));
        if (x1 <= 0 || x0 >= f.w) break;
        fill(layer, o, x0, 0, x1 - x0, f.h, t.*s.stripe);
      }
  }
  const lv_color_t ink = t.*s.line;
  if (c->show) fill(layer, o, c->mark.x, c->mark.y, c->mark.w, c->mark.h, t.*s.mark, (lv_opa_t) lroundf(s.mark_opa * 255));  // the surface's mark mix
  const int e2 = LINE_W, cx = f.court.x, cy = f.court.y, cw = f.court.w, ch = f.court.h;
  fill(layer, o, cx, cy, cw, e2, ink), fill(layer, o, cx, cy + ch - e2, cw, e2, ink);          // doubles sidelines
  fill(layer, o, cx, cy, e2, ch, ink), fill(layer, o, cx + cw - e2, cy, e2, ch, ink);          // baselines
  const float s0 = (width - singles) / 2, s1 = width - s0;
  fill(layer, o, cx, f.Y(s0) - e2 / 2, cw, e2, ink), fill(layer, o, cx, f.Y(s1) - e2 / 2, cw, e2, ink);  // singles sidelines
  for (float m : {length / 2 - service, length / 2 + service}) fill(layer, o, f.X(m) - e2 / 2, f.Y(s0), e2, f.Y(s1) - f.Y(s0), ink);  // service lines
  fill(layer, o, f.X(length / 2 - service), f.Y(width / 2) - e2 / 2, f.X(length / 2 + service) - f.X(length / 2 - service), e2, ink);  // centre service line
  fill(layer, o, cx, f.Y(width / 2) - e2 / 2, sp::s2, e2, ink), fill(layer, o, cx + cw - sp::s2, f.Y(width / 2) - e2 / 2, sp::s2, e2, ink);  // centre marks: one grid step
  fill(layer, o, f.X(length / 2) - NET_W / 2, f.Y(-post), NET_W, f.Y(width + post) - f.Y(-post), ink);  // the net, post to post
}
void invalidate(lv_obj_t *obj, const Box &a, const Box &b) {
  lv_area_t o;
  lv_obj_get_coords(obj, &o);
  lv_area_t u = {o.x1 + std::min(a.x, b.x), o.y1 + std::min(a.y, b.y), o.x1 + std::max(a.x + a.w, b.x + b.w) - 1, o.y1 + std::max(a.y + a.h, b.y + b.h) - 1};
  if (u.x2 >= u.x1 && u.y2 >= u.y1) lv_obj_invalidate_area(obj, &u);
}
void set_mx(void *v, int32_t x) {
  Court *c = court_of((lv_obj_t *) v);
  if (!c || c->mark.x == x) return;
  Box old = c->mark;
  c->mark.x = x;
  invalidate((lv_obj_t *) v, old, c->mark);
}
void set_my(void *v, int32_t y) {
  Court *c = court_of((lv_obj_t *) v);
  if (!c || c->mark.y == y) return;
  Box old = c->mark;
  c->mark.y = y;
  invalidate((lv_obj_t *) v, old, c->mark);
}
}  // namespace

const Surface &find(const char *id) {
  for (const Surface &s : SURFACES)
    if (id && !strcmp(s.id, id)) return s;
  for (const Surface &s : SURFACES)
    if (!strcmp(s.id, "facet")) return s;
  return SURFACES[0];
}
const Surface *from_data(const char *name) {
  if (!name || !*name) return nullptr;
  for (int i = 0; i < N_BASES; i++)
    if (strcmp(SURFACES[i].id, "facet") && (!strcasecmp(SURFACES[i].id, name) || !strcasecmp(SURFACES[i].label, name))) return &SURFACES[i];
  return nullptr;
}
TennisFit tennis_fit(int w) {
  using namespace tennis;
  const float tl = length + 2 * runoff_back, tw = width + 2 * runoff_side;
  TennisFit f;
  f.w = w, f.h = snap4(w * tw / tl);
  f.court.x = snap4(w * runoff_back / tl), f.court.w = w - 2 * f.court.x;
  f.court.y = snap4(f.h * runoff_side / tw), f.court.h = f.h - 2 * f.court.y;
  return f;
}
lv_obj_t *court(lv_obj_t *parent, int w, const Surface &s) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  Court *c = new Court{&s, tennis_fit(w), {}, false};
  lv_obj_set_size(o, c->fit.w, c->fit.h);
  lv_obj_set_user_data(o, c);
  lv_obj_add_event_cb(o, free_court, LV_EVENT_DELETE, c);
  lv_obj_add_event_cb(o, draw, LV_EVENT_DRAW_MAIN, nullptr);
  return o;
}
const TennisFit &tennis_geo(lv_obj_t *court) {
  static const TennisFit none;
  Court *c = court_of(court);
  return c ? c->fit : none;
}
void tennis_surface(lv_obj_t *court, const Surface &s) {
  Court *c = court_of(court);
  if (!c || c->s == &s) return;
  c->s = &s;
  lv_obj_invalidate(court);
}
void tennis_mark(lv_obj_t *court, Box b, bool show, bool glide) {
  Court *c = court_of(court);
  if (!c) return;
  lv_anim_delete(court, set_mx), lv_anim_delete(court, set_my);
  if (!show || !c->show || !glide || c->mark.w != b.w || c->mark.h != b.h) {
    const Box old = c->mark;
    const bool was = c->show;
    c->mark = b, c->show = show;
    if (was) invalidate(court, old, old);
    if (show) invalidate(court, b, b);
    return;
  }
  if (c->mark.x != b.x) animate(court, mo::value, {set_mx, c->mark.x, b.x});
  if (c->mark.y != b.y) animate(court, mo::value, {set_my, c->mark.y, b.y});
}
}  // namespace fw::sf
