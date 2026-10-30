// Facet icons for this density (README "Icons"): the generated A8 masks (tools/gen_icons.mjs -> facet_icons*.h, from
// tokens/icons.json + icons/), in their own translation unit so a component change never recompiles the pixels.
// One icon = an S x S box with one lv_image per colour layer, each cropped to its ink and placed at its offset.
#include <algorithm>
#include <cmath>
#include <cstring>
#include "fw.h"
#include "fx.h"
#ifdef FW_1X
#include "facet_icons_1x.h"
#else
#include "facet_icons.h"
#endif

namespace {
// the image descriptors, filled once (C++17 has no designated initialisers for lv_image_dsc_t)
lv_image_dsc_t *dscs() {
  static lv_image_dsc_t d[fw::icd::N_LAYERS > 0 ? fw::icd::N_LAYERS : 1];
  static bool ready = false;
  if (!ready) {
    for (int i = 0; i < fw::icd::N_LAYERS; i++) {
      const fw::icd::Layer &l = fw::icd::LAYERS[i];
      lv_image_dsc_t &x = d[i];
      memset(&x, 0, sizeof x);
      x.header.magic = LV_IMAGE_HEADER_MAGIC;
      x.header.cf = LV_COLOR_FORMAT_A8;
      x.header.w = l.w;
      x.header.h = l.h;
      x.header.stride = l.w;
      x.data_size = (uint32_t) l.w * l.h;
      x.data = fw::icd::DATA + l.off;
    }
    ready = true;
  }
  return d;
}
const fw::icd::Entry *entry(int index, fw::ic::Size size) {
  for (const auto &e : fw::icd::ENTRIES)
    if (e.icon == index && e.size == size) return &e;
  return nullptr;
}
lv_color_t role_colour(uint8_t role, lv_color_t ink) {
  switch (role) {
    case fw::ic::R_accent: return fw::T->accent;
    case fw::ic::R_text_dim: return fw::T->text_dim;
    case fw::ic::R_blue: return fw::T->blue;
    case fw::ic::R_steel: return fw::T->steel;
    default: return ink;
  }
}
}  // namespace

int fw::icon_index(const char *name) {
  for (int i = 0; i < ic::N; i++)
    if (!strcmp(ic::ICONS[i].name, name)) return i;
  return -1;
}
bool fw::icon_has(int index, ic::Size size) { return entry(index, size) != nullptr; }
int fw::icon_bytes(int index, ic::Size size) {
  const icd::Entry *e = entry(index, size);
  int b = 0;
  if (e) for (int i = 0; i < e->n; i++) b += icd::LAYERS[e->first + i].w * icd::LAYERS[e->first + i].h;
  return b;
}
int fw::icons_bytes() { return icd::DATA_BYTES + icd::TABLE_BYTES; }
int fw::icons_entries() { return icd::N_ENTRIES; }
bool fw::icons_used_only() { return icd::USED_ONLY; }

lv_obj_t *fw::icon(lv_obj_t *parent, const char *name, ic::Size size, lv_color_t colour) {
  const icd::Entry *e = entry(icon_index(name), size);
  if (!e) return nullptr;
  lv_obj_t *box = lv_obj_create(parent);
  lv_obj_remove_style_all(box);
  lv_obj_remove_flag(box, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_add_flag(box, LV_OBJ_FLAG_STATE_TRICKLE);  // the layers follow the parent's state (pressed, checked, ...)
  lv_obj_set_size(box, e->px, e->px);
  lv_image_dsc_t *d = dscs();
  for (int i = 0; i < e->n; i++) {
    const icd::Layer &l = icd::LAYERS[e->first + i];
    lv_obj_t *im = lv_image_create(box);
    lv_obj_remove_flag(im, LV_OBJ_FLAG_CLICKABLE);
    lv_image_set_src(im, &d[e->first + i]);
    lv_obj_set_pos(im, l.x, l.y);
    lv_obj_set_style_image_recolor(im, role_colour(l.role, colour), 0);
    lv_obj_set_style_image_recolor_opa(im, LV_OPA_COVER, 0);
    lv_obj_set_user_data(im, (void *) (uintptr_t) l.role);
  }
  return box;
}

// ---------------------------------------------------------------- turn icons (icons.json turn, README "Icons" > "Turn
// icons"): the master's outline (ic::TURN_PTS) filled at any heading into an A8 mask, exact span coverage on 16
// sub-rows per px row (an SVG fill's edges), turned clockwise about the box centre (CSS rotate, origin 50% 50%).
extern "C" void lv_image_cache_drop(const void *src);  // LVGL 9 (misc/cache/instance/lv_image_cache.h, not in lvgl.h)
lv_draw_buf_t *fw::icon_turned_mask(int index, int px, float deg) {
  const ic::Turn *tn = nullptr;
  for (int i = 0; i < ic::N_TURNS; i++)
    if (ic::TURNS[i].icon == index) tn = &ic::TURNS[i];
  if (!tn || px < 1) return nullptr;
  lv_draw_buf_t *b = lv_draw_buf_create(px, px, LV_COLOR_FORMAT_A8, LV_STRIDE_AUTO);
  if (!b) return nullptr;
  std::memset(b->data, 0, b->data_size);
  const float k = px / 24.f, h = px / 2.f, an = deg * 3.14159265f / 180.f, c = std::cos(an), sn = std::sin(an);
  float p[64][2];
  const int n = std::min<int>(tn->n, 64);
  for (int i = 0; i < n; i++) {
    const float x = (ic::TURN_PTS[tn->first + i][0] - 12) * k, y = (ic::TURN_PTS[tn->first + i][1] - 12) * k;
    p[i][0] = h + x * c - y * sn, p[i][1] = h + x * sn + y * c;
  }
  constexpr int SUB = 16;
  float acc[256], xs[64];
  for (int py = 0; py < px; py++) {
    const int w = std::min(px, 256);
    std::fill(acc, acc + w, 0.f);
    for (int j = 0; j < SUB; j++) {
      const float y = py + (j + .5f) / SUB;
      int m = 0;
      for (int i = 0; i < n; i++) {
        const float *a = p[i], *e = p[(i + 1) % n];
        if ((a[1] <= y) != (e[1] <= y)) xs[m++] = a[0] + (y - a[1]) / (e[1] - a[1]) * (e[0] - a[0]);
      }
      std::sort(xs, xs + m);
      for (int i = 0; i + 1 < m; i += 2) {
        const float x0 = std::max(0.f, xs[i]), x1 = std::min((float) w, xs[i + 1]);
        for (int x = (int) x0; x < w && x < x1; x++) acc[x] += (std::min(x1, x + 1.f) - std::max(x0, (float) x)) / SUB;
      }
    }
    uint8_t *row = (uint8_t *) b->data + py * b->header.stride;
    for (int x = 0; x < w; x++) row[x] = (uint8_t) std::lround(std::min(1.f, acc[x]) * 255);
  }
  return b;
}
void fw::icon_mask_free(lv_draw_buf_t *b) {
  if (!b) return;
  lv_image_cache_drop(b);
  lv_draw_buf_destroy(b);
}
lv_obj_t *fw::icon_turned(lv_obj_t *parent, const char *name, int px, float deg, lv_color_t colour) {
  lv_draw_buf_t *b = icon_turned_mask(icon_index(name), px, deg);
  if (!b) return nullptr;
  lv_obj_t *im = lv_image_create(parent);
  lv_obj_remove_flag(im, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_size(im, px, px);
  lv_image_set_src(im, b);
  lv_obj_set_style_image_recolor(im, colour, 0);
  lv_obj_set_style_image_recolor_opa(im, LV_OPA_COVER, 0);
  lv_obj_set_user_data(im, (void *) (uintptr_t) ic::R_ink);
  lv_obj_add_event_cb(im, [](lv_event_t *e) { icon_mask_free((lv_draw_buf_t *) lv_event_get_user_data(e)); }, LV_EVENT_DELETE, b);
  return im;
}

void fw::icon_colour(lv_obj_t *icon, lv_color_t colour, lv_style_selector_t sel, const lv_style_transition_dsc_t *tr) {
  if (!icon) return;
  for (uint32_t i = 0; i < lv_obj_get_child_count(icon); i++) {
    lv_obj_t *im = lv_obj_get_child(icon, (int32_t) i);
    if ((uintptr_t) lv_obj_get_user_data(im) != ic::R_ink) continue;
    lv_obj_set_style_image_recolor(im, colour, sel);
    if (tr) lv_obj_set_style_transition(im, tr, sel);
  }
}

// ---------------------------------------------------------------- animated weather icons (README "Icons" > "Animated
// weather"): the master's parts (gen_icons.mjs icd::APARTS, each rasterised alone at this size) moved / faded by the
// motion.json weather keyframes. One lv_anim per icon is only a clock; each part is written when its whole-px offset,
// rotation step or opacity step changes, so most frames touch nothing.
namespace {
using namespace fw;
// turning sun rays: drawn into a small A8 buffer from their geometry at each step of the turn (no image rotation:
// a rotated A8 mask is resampled, so the rays go soft and the centre wanders by the half-pixel pivot rounding)
struct RayBuf { const icd::Rays *r; int ox, oy, B; uint8_t *buf; lv_image_dsc_t dsc; };
void rays_draw(RayBuf *b, float deg) {
  const icd::Rays &r = *b->r;
  memset(b->buf, 0, (size_t) b->B * b->B);
  const float hw = r.w / 2, cx = r.cx - b->ox, cy = r.cy - b->oy;
  for (int i = 0; i < r.n; i++) {
    const float a = (r.a0 + deg + i * 360.f / r.n) * 0.01745329f, ca = cosf(a), sa = sinf(a);
    const float x1 = cx + r.rin * ca, y1 = cy + r.rin * sa, x2 = cx + r.rout * ca, y2 = cy + r.rout * sa;
    const float dx = x2 - x1, dy = y2 - y1, ll = dx * dx + dy * dy;
    const int X0 = std::max(0, (int) floorf(std::min(x1, x2) - hw - 1)), X1 = std::min(b->B - 1, (int) ceilf(std::max(x1, x2) + hw + 1));
    const int Y0 = std::max(0, (int) floorf(std::min(y1, y2) - hw - 1)), Y1 = std::min(b->B - 1, (int) ceilf(std::max(y1, y2) + hw + 1));
    for (int y = Y0; y <= Y1; y++)
      for (int x = X0; x <= X1; x++) {  // distance from the pixel centre to the segment -> round-capped stroke coverage
        const float px = x + 0.5f - x1, py = y + 0.5f - y1;
        const float t = ll > 0 ? std::max(0.f, std::min(1.f, (px * dx + py * dy) / ll)) : 0;
        const float ex = px - t * dx, ey = py - t * dy;
        const float c = std::max(0.f, std::min(1.f, hw + 0.5f - sqrtf(ex * ex + ey * ey)));
        uint8_t &o = b->buf[y * b->B + x];
        o = std::max<uint8_t>(o, (uint8_t) lroundf(c * 255));
      }
  }
}
// one part, drawn by the icon box's own draw event (no child objects: moving a child invalidates its old + new area
// separately, and a page of animated icons overflowed LVGL's 32 invalid areas into a full-screen redraw); the box
// invalidates the union of what changed, once per frame
struct WxPart {
  const icd::APart *a;
  const lv_image_dsc_t *src;
  lv_color_t col;
  int32_t dx = 0, dy = 0, rot = INT32_MIN, opa = LV_OPA_COVER;
  float rot_step = 0;
  RayBuf *rays = nullptr;
  int bx() const { return rays ? rays->ox : a->x; }
  int by() const { return rays ? rays->oy : a->y; }
  int bw() const { return rays ? rays->B : a->w; }
  int bh() const { return rays ? rays->B : a->h; }
};
struct Wx {
  int index;
  ic::Size size;
  bool animate, moving = false;
  lv_color_t ink, behind;
  uint32_t t0 = 0;
  bool live = false;          // fx::weather (contract): inks read at draw time from the master's layers (a theme switch)
  lv_obj_t *master = nullptr;  // live: the static master (fx::icon), hidden while the parts move
  std::vector<WxPart> parts;
};
void wx_clock(void *var, int32_t);
void wx_clear(Wx *w) {
  for (WxPart &p : w->parts)
    if (p.rays) { lv_free(p.rays->buf); delete p.rays; }
  w->parts.clear();
}
void wx_free(lv_event_t *ev) {
  lv_anim_delete(lv_event_get_target(ev), wx_clock);
  Wx *w = (Wx *) lv_event_get_user_data(ev);
  wx_clear(w);
  delete w;
}
Wx *wx_of(lv_obj_t *o) {  // the box's state = the user data of its wx_free delete callback
  if (!o) return nullptr;
  for (uint32_t i = 0; i < lv_obj_get_event_count(o); i++) {
    lv_event_dsc_t *d = lv_obj_get_event_dsc(o, i);
    if (lv_event_dsc_get_cb(d) == wx_free) return (Wx *) lv_event_dsc_get_user_data(d);
  }
  return nullptr;
}
const icd::Entry *aentry(int index, ic::Size size) {
  for (int i = 0; i < icd::N_AENTRIES; i++)
    if (icd::AENTRIES[i].icon == index && icd::AENTRIES[i].size == size) return &icd::AENTRIES[i];
  return nullptr;
}
lv_image_dsc_t *adscs() {
  static lv_image_dsc_t d[icd::N_APARTS > 0 ? icd::N_APARTS : 1];
  static bool ready = false;
  if (!ready) {
    for (int i = 0; i < icd::N_APARTS; i++) {
      const icd::APart &l = icd::APARTS[i];
      lv_image_dsc_t &x = d[i];
      memset(&x, 0, sizeof x);
      x.header.magic = LV_IMAGE_HEADER_MAGIC;
      x.header.cf = LV_COLOR_FORMAT_A8;
      x.header.w = l.w;
      x.header.h = l.h;
      x.header.stride = l.w;
      x.data_size = (uint32_t) l.w * l.h;
      x.data = icd::DATA + l.off;
    }
    ready = true;
  }
  return d;
}
// eased keyframe value of a track at phase ms of a part (CSS keyframes: the easing runs per segment)
float wx_sample(const mo::wx::Part &P, const mo::wx::Track &tr, int32_t ms) {
  const int32_t pm = (int32_t) ((int64_t) ms * 1000 / (int32_t) P.ms);
  int j = 0;
  while (j + 2 < tr.n && tr.keys[j + 1].t <= pm) j++;
  const mo::wx::Key &a = tr.keys[j], &b = tr.keys[j + 1];
  if (b.t == a.t) return b.v;
  lv_anim_t an;
  lv_anim_init(&an);
  an.duration = 1024;
  an.act_time = (int32_t) (((int64_t) ms * 1000 - (int64_t) a.t * P.ms) * 1024 / ((int64_t) (b.t - a.t) * P.ms));
  an.act_time = std::max<int32_t>(0, std::min<int32_t>(1024, an.act_time));
  an.start_value = 0;
  an.end_value = 1024;
  const int32_t e = mo::EASINGS[P.easing].path(&an);
  return a.v + (b.v - a.v) * e / 1024.f;
}
// pose every moving part at time t; returns the union (box-relative) of the old + new areas of what changed
bool wx_pose(Wx *w, uint32_t t, lv_area_t &u) {
  const float k = ic::PX[w->size] / 48.f;  // px per grid unit
  bool any = false;
  auto add = [&](const WxPart &p) {
    const lv_area_t r{p.bx() + p.dx, p.by() + p.dy, p.bx() + p.dx + p.bw() - 1, p.by() + p.dy + p.bh() - 1};
    if (!any) u = r, any = true;
    else u = {std::min(u.x1, r.x1), std::min(u.y1, r.y1), std::max(u.x2, r.x2), std::max(u.y2, r.y2)};
  };
  for (WxPart &p : w->parts) {
    if (p.a->motion == 255) continue;
    const mo::wx::Part &P = mo::wx::PARTS[p.a->motion];
    const uint32_t local = t + p.a->inst * P.stagger;
    int32_t ph = (int32_t) (local % (P.alternate ? 2 * P.ms : P.ms));
    if (P.alternate && ph >= (int32_t) P.ms) ph = 2 * (int32_t) P.ms - ph;
    float x = 0, y = 0, rot = 0, opa = 1;
    for (int i = 0; i < P.n_tracks; i++) {
      const float v = wx_sample(P, P.tracks[i], ph);
      switch (P.tracks[i].prop) {
        case mo::wx::P_X: x = v; break;
        case mo::wx::P_Y: y = v; break;
        case mo::wx::P_ROT: rot = v; break;
        case mo::wx::P_OPA: opa = v; break;
      }
    }
    const int32_t dx = lroundf(x * k), dy = lroundf(y * k);
    int32_t o = lroundf(opa * 255.f / mo::wx::OPA_STEP) * mo::wx::OPA_STEP;
    o = std::max<int32_t>(0, std::min<int32_t>(255, o));
    int32_t r = p.rot;
    if (p.rays) {  // whole steps that move a ray tip rot-px; one step of the n-fold rays repeats (360 / n)
      const float per = 360.f / p.rays->r->n;
      r = (int32_t) floorf(fmodf(rot, per) / p.rot_step);
    }
    if (dx == p.dx && dy == p.dy && o == p.opa && r == p.rot) continue;
    add(p);  // where it was
    if (r != p.rot) { p.rot = r; rays_draw(p.rays, r * p.rot_step); }
    p.dx = dx, p.dy = dy, p.opa = o;
    add(p);  // where it is now
  }
  return any;
}
void wx_clock(void *var, int32_t) {
  lv_obj_t *box = (lv_obj_t *) var;
  Wx *w = wx_of(box);
  lv_area_t u;
  if (!w || !w->moving || !wx_pose(w, lv_tick_elaps(w->t0), u)) return;
  lv_area_t c;
  lv_obj_get_coords(box, &c);
  lv_area_move(&u, c.x1, c.y1);
  lv_obj_invalidate_area(box, &u);  // one area per icon per frame
}
// live (fx::weather): a part's ink = the master layer of its role (shared ink styles: the theme's, now); the cloud
// fill = the nearest box behind with a background, else the canvas
lv_color_t wx_live_colour(lv_obj_t *box, const Wx *w, uint8_t role) {
  if (role == ic::R_occluder) {
    for (lv_obj_t *a = lv_obj_get_parent(box); a; a = lv_obj_get_parent(a))
      if (lv_obj_get_style_bg_opa(a, LV_PART_MAIN) >= LV_OPA_COVER) return lv_obj_get_style_bg_color(a, LV_PART_MAIN);
    return fx::bg(lv_obj_get_display(box));
  }
  lv_obj_t *ink = nullptr;
  for (uint32_t i = 0; w->master && i < lv_obj_get_child_count(w->master); i++) {
    lv_obj_t *l = lv_obj_get_child(w->master, (int32_t) i);
    const uint8_t r = (uint8_t) (uintptr_t) lv_obj_get_user_data(l);
    if (r == role) return lv_obj_get_style_image_recolor(l, LV_PART_MAIN);
    if (r == ic::R_ink && !ink) ink = l;
  }
  if (ink) return lv_obj_get_style_image_recolor(ink, LV_PART_MAIN);
  return role_colour(role, w->ink);
}
// the parts, in master order (occluders before their cloud), straight from the A8 masks with the image recolour
void wx_draw(lv_event_t *ev) {
  lv_obj_t *box = (lv_obj_t *) lv_event_get_current_target(ev);
  Wx *w = wx_of(box);
  if (!w || !w->moving) return;
  lv_layer_t *layer = lv_event_get_layer(ev);
  lv_area_t c;
  lv_obj_get_coords(box, &c);
  const lv_opa_t base = lv_obj_get_style_opa_recursive(box, LV_PART_MAIN);
  for (const WxPart &p : w->parts) {
    if (p.opa <= LV_OPA_MIN) continue;
    lv_draw_image_dsc_t d;
    lv_draw_image_dsc_init(&d);
    d.src = p.src;
    d.recolor = w->live ? wx_live_colour(box, w, p.a->role) : p.col;
    d.recolor_opa = LV_OPA_COVER;
    d.opa = (lv_opa_t) LV_OPA_MIX2(p.opa, base);
    const lv_area_t a{c.x1 + p.bx() + p.dx, c.y1 + p.by() + p.dy, c.x1 + p.bx() + p.dx + p.bw() - 1, c.y1 + p.by() + p.dy + p.bh() - 1};
    lv_draw_image(layer, &d, &a);
  }
}
// parts move past the box (drops fall 5 units, flakes drift): draw that far outside it
void wx_ext(lv_event_t *ev) {
  lv_obj_t *box = (lv_obj_t *) lv_event_get_current_target(ev);
  Wx *w = wx_of(box);
  if (w) lv_event_set_ext_draw_size(ev, (int32_t) ceilf(ic::PX[w->size] * 6 / 48.f));
}
lv_color_t wx_colour(uint8_t role, const Wx *w) {
  if (role == ic::R_occluder) return w->behind;
  return role_colour(role, w->ink);
}
void wx_fill(lv_obj_t *box, Wx *w) {
  lv_anim_delete(box, wx_clock);
  lv_obj_clean(box);
  wx_clear(w);
  const icd::Entry *e = (w->animate && !mo::reduced(box)) ? aentry(w->index, w->size) : nullptr;
  w->moving = e != nullptr;
  if (w->live) {  // the static master through fx::icon (its shared ink styles); hidden under the moving parts
    w->master = fx::icon(box, ic::ICONS[w->index].name, ic::SIZE_NAMES[w->size], "fg:text");
    for (uint32_t i = 0; w->master && i < lv_obj_get_child_count(w->master); i++)  // an ink restyled (theme switch): draw again
      lv_obj_add_event_cb(lv_obj_get_child(w->master, (int32_t) i), [](lv_event_t *ev) {
        lv_obj_invalidate((lv_obj_t *) lv_event_get_user_data(ev));
      }, LV_EVENT_STYLE_CHANGED, box);
    if (!e) return;
    if (w->master) lv_obj_add_flag(w->master, LV_OBJ_FLAG_HIDDEN);
  } else if (!e) {  // the static master, exactly fw::icon
    icon(box, ic::ICONS[w->index].name, w->size, w->ink);
    return;
  }
  lv_image_dsc_t *d = adscs();
  for (int i = 0; i < e->n; i++) {
    const icd::APart &a = icd::APARTS[e->first + i];
    WxPart p;
    p.a = &a;
    p.src = &d[e->first + i];
    p.col = wx_colour(a.role, w);
    for (int r = 0; r < icd::N_RAYS; r++)
      if (icd::RAYS[r].part == e->first + i) {  // turning rays: own buffer around the sun centre
        const icd::Rays &g = icd::RAYS[r];
        const float reach = g.rout + g.w / 2 + 1;
        RayBuf *b = new RayBuf{&g, (int) floorf(g.cx - reach), (int) floorf(g.cy - reach), 0, nullptr, {}};
        b->B = (int) ceilf(g.cx + reach) - b->ox;
        b->buf = (uint8_t *) lv_malloc((size_t) b->B * b->B);
        memset(&b->dsc, 0, sizeof b->dsc);
        b->dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
        b->dsc.header.cf = LV_COLOR_FORMAT_A8;
        b->dsc.header.w = b->dsc.header.h = b->dsc.header.stride = b->B;
        b->dsc.data_size = (uint32_t) b->B * b->B;
        b->dsc.data = b->buf;
        rays_draw(b, 0);
        p.src = &b->dsc;
        p.rays = b;
        p.rot_step = mo::wx::ROT_PX / g.rout * 57.29578f;
        p.rot = 0;
      }
    w->parts.push_back(p);
  }
  w->t0 = lv_tick_get();
  lv_area_t u;
  wx_pose(w, 0, u);
  lv_obj_refresh_ext_draw_size(box);
  lv_obj_invalidate(box);
  lv_anim_t an;
  lv_anim_init(&an);
  lv_anim_set_var(&an, box);
  lv_anim_set_exec_cb(&an, wx_clock);
  lv_anim_set_values(&an, 0, 1000);
  lv_anim_set_duration(&an, 1000);  // a clock only: wx_clock reads lv_tick itself
  lv_anim_set_repeat_count(&an, LV_ANIM_REPEAT_INFINITE);
  lv_anim_start(&an);
}
}  // namespace

lv_obj_t *fw::weather_icon(lv_obj_t *parent, const char *name, ic::Size size, bool animate, lv_color_t ink, lv_color_t behind) {
  const int index = icon_index(name);
  if (index < 0 || strcmp(ic::ICONS[index].kind, "weather") || !entry(index, size)) return nullptr;
  lv_obj_t *box = lv_obj_create(parent);
  lv_obj_remove_style_all(box);
  lv_obj_remove_flag(box, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_add_flag(box, (lv_obj_flag_t) (LV_OBJ_FLAG_STATE_TRICKLE | LV_OBJ_FLAG_OVERFLOW_VISIBLE));
  lv_obj_set_size(box, ic::PX[size], ic::PX[size]);
  Wx *w = new Wx{index, size, animate, false, ink, behind};
  lv_obj_add_event_cb(box, wx_free, LV_EVENT_DELETE, w);
  lv_obj_add_event_cb(box, wx_draw, LV_EVENT_DRAW_MAIN, nullptr);
  lv_obj_add_event_cb(box, wx_ext, LV_EVENT_REFR_EXT_DRAW_SIZE, nullptr);
  wx_fill(box, w);
  return box;
}

// the contract weather icon (core/fx.h fx::weather): fw::weather_icon with live inks
lv_obj_t *fx::weather(lv_obj_t *parent, const char *name, const char *size, bool animate) {
  int s = -1;
  for (int i = 0; i < ic::N_SIZES; i++) if (!strcmp(ic::SIZE_NAMES[i], size)) s = i;
  const int index = icon_index(name);
  if (s < 0 || index < 0 || strcmp(ic::ICONS[index].kind, "weather") || !entry(index, (ic::Size) s)) return nullptr;
  lv_obj_t *box = lv_obj_create(parent);
  lv_obj_remove_style_all(box);
  lv_obj_remove_flag(box, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_add_flag(box, (lv_obj_flag_t) (LV_OBJ_FLAG_STATE_TRICKLE | LV_OBJ_FLAG_OVERFLOW_VISIBLE | LV_OBJ_FLAG_EVENT_BUBBLE));
  lv_obj_set_size(box, ic::PX[s], ic::PX[s]);
  Wx *w = new Wx{index, (ic::Size) s, animate, false, lv_color_black(), lv_color_black()};
  w->live = true;
  lv_obj_add_event_cb(box, wx_free, LV_EVENT_DELETE, w);
  lv_obj_add_event_cb(box, wx_draw, LV_EVENT_DRAW_MAIN, nullptr);
  lv_obj_add_event_cb(box, wx_ext, LV_EVENT_REFR_EXT_DRAW_SIZE, nullptr);
  wx_fill(box, w);
  return box;
}
bool fx::weather_set(lv_obj_t *box, const char *name, bool animate) { return fw::weather_icon_set(box, name, animate); }

bool fw::weather_icon_set(lv_obj_t *box, const char *name, bool animate) {
  Wx *w = wx_of(box);
  const int index = icon_index(name);
  if (!w || index < 0 || strcmp(ic::ICONS[index].kind, "weather") || !entry(index, w->size)) return false;
  if (index == w->index && animate == w->animate && w->moving == (animate && !mo::reduced(box) && aentry(index, w->size))) return true;
  w->index = index;
  w->animate = animate;
  wx_fill(box, w);
  return true;
}
bool fw::weather_icon_animated(lv_obj_t *box) {
  Wx *w = wx_of(box);
  return w && w->moving;
}
const char *fw::weather_icon_name(lv_obj_t *box) {
  Wx *w = wx_of(box);
  return w ? ic::ICONS[w->index].name : nullptr;
}
int fw::weather_anim_bytes(int index, ic::Size size) {
  const icd::Entry *e = aentry(index, size);
  int b = 0;
  if (e) for (int i = 0; i < e->n; i++) b += icd::APARTS[e->first + i].w * icd::APARTS[e->first + i].h;
  return b;
}
int fw::weather_anim_parts(int index, ic::Size size) {
  const icd::Entry *e = aentry(index, size);
  return e ? e->n : 0;
}

// ---------------------------------------------------------------- moon phase glyph (Facet packages/icons moon.ts,
// README "Icons" > "Moon phase"): on the weather 48 grid a disc r 20 (moon-dark), the lit part = the lit limb (half
// circle) + the terminator (half ellipse, rx = r x |2k - 1|, k = lit fraction), moon-lit, and the outline (moon-edge,
// weather stroke). Each mask is computed at the exact px: 8 sub-rows per pixel row, each covering the exact span of
// the shape in that sub-row, so edges are anti-aliased like the rasterised icons. Disc + outline once per size.
namespace {
constexpr int MOON_SUB = 8;  // sub-rows per pixel row
inline float span(float a, float b, int x) {  // length of [a, b] inside pixel column [x, x + 1]
  const float l = std::max(a, (float) x), r = std::min(b, (float) x + 1);
  return r > l ? r - l : 0;
}
float moon_lit_k(float p) { return (1 - cosf(2 * 3.14159265f * p)) / 2; }
// one mask: span(y) -> up to two [a, b] intervals per sub-row (sign -1 = subtract the second: the outline's hole)
template <typename F> void moon_mask(int S, uint32_t stride, uint8_t *out, F spans) {
  std::vector<float> acc(S);
  for (int j = 0; j < S; j++) {
    std::fill(acc.begin(), acc.end(), 0.f);
    for (int s = 0; s < MOON_SUB; s++) {
      const float y = j + (s + 0.5f) / MOON_SUB;
      float a0, b0, a1, b1;
      int n = spans(y, a0, b0, a1, b1);
      if (n <= 0) continue;
      for (int x = std::max(0, (int) floorf(a0)); x < S && x <= (int) b0; x++) acc[x] += span(a0, b0, x);
      if (n == 2)
        for (int x = std::max(0, (int) floorf(a1)); x < S && x <= (int) b1; x++) acc[x] -= span(a1, b1, x);
    }
    for (int x = 0; x < S; x++) out[j * stride + x] = (uint8_t) std::max(0, std::min(255, (int) lroundf(acc[x] * 255.f / MOON_SUB)));
  }
}
struct MoonGeo { float c, r, w; };
MoonGeo moon_geo(int S) { return {S / 2.f, 20.f * S / 48.f, S >= 96 ? fw::ic::WX_STROKE_96 : fw::ic::WX_STROKE}; }
float half_chord(float r, float dy) { return dy * dy < r * r ? sqrtf(r * r - dy * dy) : -1; }
// The masks are LVGL draw buffers (A8, LVGL's own stride): an image source LVGL draws straight from. A plain buffer
// with stride = width was copied into an aligned one on EVERY draw (no image cache on the panels): a hero moon cost
// three copies of its size per strip of every frame, 4 s per frame on the Waveshare and a watchdog reset (2026-10-30).
lv_draw_buf_t *moon_buf(int S) {
  lv_draw_buf_t *b = lv_draw_buf_create(S, S, LV_COLOR_FORMAT_A8, LV_STRIDE_AUTO);
  if (b) lv_draw_buf_clear(b, nullptr);
  return b;
}
void moon_lit(int S, float frac, bool south, lv_draw_buf_t *buf) {
  const MoonGeo g = moon_geo(S);
  const float p = fmodf(fmodf(frac, 1) + 1, 1), k = moon_lit_k(p);
  const bool right = (p < 0.5f) != south;
  const float rx = g.r * fabsf(2 * k - 1), e = k < 0.5f ? 1.f : -1.f;
  moon_mask(S, buf->header.stride, buf->data, [&](float y, float &a0, float &b0, float &, float &) {
    const float dy = y - g.c, h = half_chord(g.r, dy);
    if (h < 0 || k < 0.005f) return 0;
    a0 = g.c - h, b0 = g.c + h;
    if (k > 0.995f) return 1;
    const float t = rx * sqrtf(std::max(0.f, 1 - (dy / g.r) * (dy / g.r)));
    if (right) a0 = std::max(a0, g.c + e * t);
    else b0 = std::min(b0, g.c - e * t);
    return b0 > a0 ? 1 : 0;
  });
}
// disc + outline masks of one size (px), made once per size and kept (moons on screen draw from them); a size past
// the slots gets none (nullptr): that moon makes its own and frees them with itself (never a buffer a live image uses)
struct MoonStatic { int px = 0; lv_draw_buf_t *disc = nullptr, *edge = nullptr; };
void moon_masks(int S, lv_draw_buf_t *disc, lv_draw_buf_t *edge) {
  const MoonGeo g = moon_geo(S);
  moon_mask(S, disc->header.stride, disc->data, [&](float y, float &a0, float &b0, float &, float &) {
    const float h = half_chord(g.r, y - g.c);
    if (h < 0) return 0;
    a0 = g.c - h, b0 = g.c + h;
    return 1;
  });
  moon_mask(S, edge->header.stride, edge->data, [&](float y, float &a0, float &b0, float &a1, float &b1) {
    const float ho = half_chord(g.r + g.w / 2, y - g.c), hi = half_chord(g.r - g.w / 2, y - g.c);
    if (ho < 0) return 0;
    a0 = g.c - ho, b0 = g.c + ho;
    if (hi < 0) return 1;
    a1 = g.c - hi, b1 = g.c + hi;
    return 2;
  });
}
MoonStatic *moon_static(int S) {
  static MoonStatic m[fw::ic::N_SIZES + 4];
  for (MoonStatic &x : m) {
    if (x.px == S) return &x;
    if (x.px) continue;
    x.disc = moon_buf(S), x.edge = moon_buf(S);
    if (!x.disc || !x.edge) { if (x.disc) lv_draw_buf_destroy(x.disc); if (x.edge) lv_draw_buf_destroy(x.edge); x.disc = x.edge = nullptr; return nullptr; }
    x.px = S;
    moon_masks(S, x.disc, x.edge);
    return &x;
  }
  return nullptr;
}
struct Moon { int px; float frac; bool south; lv_draw_buf_t *lit, *disc, *edge; bool own; lv_obj_t *im; };
void moon_free(lv_event_t *ev) {
  Moon *m = (Moon *) lv_event_get_user_data(ev);
  if (m->lit) lv_draw_buf_destroy(m->lit);
  if (m->own) { if (m->disc) lv_draw_buf_destroy(m->disc); if (m->edge) lv_draw_buf_destroy(m->edge); }
  delete m;
}
Moon *moon_of(lv_obj_t *o) {
  if (!o) return nullptr;
  for (uint32_t i = 0; i < lv_obj_get_event_count(o); i++) {
    lv_event_dsc_t *d = lv_obj_get_event_dsc(o, i);
    if (lv_event_dsc_get_cb(d) == moon_free) return (Moon *) lv_event_dsc_get_user_data(d);
  }
  return nullptr;
}
lv_obj_t *moon_layer(lv_obj_t *box, const lv_draw_buf_t *d, const char *ink) {  // ink = a shared recolour style
  lv_obj_t *im = lv_image_create(box);                                              // ("ink:moon-lit"): a theme switch
  lv_obj_remove_flag(im, LV_OBJ_FLAG_CLICKABLE);                                    // repaints it (C16, C44)
  lv_image_set_src(im, d);
  lv_obj_set_pos(im, 0, 0);
  fx::use(im, ink);
  return im;
}
}  // namespace

lv_obj_t *fw::moon_icon(lv_obj_t *parent, float frac, ic::Size size, bool south) { return moon_icon_px(parent, frac, ic::PX[size], south); }
lv_obj_t *fw::moon_icon_px(lv_obj_t *parent, float frac, int S, bool south) {
  lv_obj_t *box = lv_obj_create(parent);
  lv_obj_remove_style_all(box);
  lv_obj_remove_flag(box, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_add_flag(box, LV_OBJ_FLAG_STATE_TRICKLE);
  lv_obj_set_size(box, S, S);
  Moon *m = new Moon{S, frac, south, moon_buf(S), nullptr, nullptr, false, nullptr};
  if (MoonStatic *st = moon_static(S)) m->disc = st->disc, m->edge = st->edge;
  else if ((m->disc = moon_buf(S)) && (m->edge = moon_buf(S))) { m->own = true; moon_masks(S, m->disc, m->edge); }
  else { m->own = true; if (m->disc) lv_draw_buf_destroy(m->disc); m->disc = nullptr; }
  lv_obj_add_event_cb(box, moon_free, LV_EVENT_DELETE, m);
  if (!m->lit || !m->disc || !m->edge) return box;  // no memory: an empty box of the right size
  moon_lit(S, frac, south, m->lit);
  moon_layer(box, m->disc, "ink:moon-dark");
  m->im = moon_layer(box, m->lit, "ink:moon-lit");
  moon_layer(box, m->edge, "ink:moon-edge");
  return box;
}
lv_obj_t *fx::moon(lv_obj_t *parent, float frac, const char *size, bool south) {
  if (!strncmp(size, "layout:", 7)) return fw::moon_icon_px(parent, frac, fx::size(size), south);
  for (int i = 0; i < fw::ic::N_SIZES; i++)
    if (!strcmp(fw::ic::SIZE_NAMES[i], size)) return fw::moon_icon(parent, frac, (fw::ic::Size) i, south);
  return fw::moon_icon(parent, frac, fw::ic::card, south);
}
bool fw::moon_icon_set(lv_obj_t *box, float frac, bool south) {
  Moon *m = moon_of(box);
  if (!m) return false;
  if (fabsf(frac - m->frac) < 0.0005f && south == m->south) return true;
  m->frac = frac, m->south = south;
  if (!m->lit || !m->im) return true;
  lv_draw_buf_clear(m->lit, nullptr);
  moon_lit(m->px, frac, south, m->lit);
  lv_obj_invalidate(m->im);
  return true;
}
