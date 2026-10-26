// Image: a picture from a URL (board facet-171, notes image-component.md, piece 1; WIP, features.json "image").
// Pixels come from core/images.h (one per-board byte-capped cache; the host fetches + decodes, the panel later too).
//   src       an http(s) URL; bindable (an entity's state or attribute) and "$item.<key>" in a grid's items cell
//   step      icon | glyph | detail | feature | card | hero (ic:: sizes) | content (the content / slot width): the
//             box width. Height = width / aspect, snapped down to the grid
//   fit       step (default) | cell: inside a grid cell or a layout slot, the box = the whole cell (elsewhere: the
//             parent's content box, snapped to the grid). A spec sets step
//             or fit: cell, not both (core/spec.h)
//   aspect    1:1 | 4:3 | 3:2 | 16:9 (with step; a cell keeps its own shape)
//   scale     cover (fills the box, edges cropped) | contain (all of it, centred) | pixel (whole-number zoom,
//             nearest neighbour: crisp pixel art, never dithered)
//   sampling  smooth (default) | nearest: how pixels are scaled UP in cover / contain. nearest = no blur, no dither (pixel
//             art at any scale); smooth can't be guessed from the picture, so it is the default. pixel is always nearest
//   radius    none | chip | button | ring | round (rad:: tokens; round = a circle / pill), baked into the pixels'
//             alpha (no clip layer)
//   fallback  an icon name shown when the image fails or there is no src ("" = the quiet box only); alt = the words
//             under it when there is room (and the image's name for the workbench / docs)
//   refresh   off | 1m | 5m | 15m | 1h: fetch the URL again that often (a failed refresh keeps the last image)
// States: loading = a quiet surface box (no spinner); ready = the picture; failed = surface box + fallback + alt.
// Update path: a new src keeps the current picture until the new one is in, then swaps (silent); if the new one
// fails, the last good picture stays. Static: no events, no sound (a grid cell around it takes the taps).
// Contrast: section "image" (the fallback icon + alt on the surface box).
#include "fw.h"
#include "spec.h"
#include "images.h"

namespace {
using namespace fw;
struct I;
struct Key { I *i; };
struct I {
  lv_obj_t *root = nullptr;
  Key cur{this}, prev{this};
  bool has_prev = false;
  int w = 0, h = 0, radius = 0;
  img::Scale sc = img::cover;
  bool nearest = false;
  std::string src, fallback, alt;
  lv_timer_t *every = nullptr;
};
int step_px(const char *s, lv_obj_t *parent) {
  if (!strcmp(s, "content")) {
    const int32_t pw = parent && lv_obj_has_flag(parent, spec::SLOT) ? lv_obj_get_style_width(parent, LV_PART_MAIN) : 0;
    return pw > 0 && !LV_COORD_IS_SPEC(pw) ? (int) pw : lay::content_w;
  }
  for (int i = 0; i < ic::N_SIZES; i++)
    if (!strcmp(ic::SIZE_NAMES[i], s)) return ic::PX[i];
  return ic::PX[ic::card];
}
void aspect_of(const char *a, int &n, int &d) {
  n = d = 1;
  if (!strcmp(a, "4:3")) { n = 4; d = 3; }
  else if (!strcmp(a, "3:2")) { n = 3; d = 2; }
  else if (!strcmp(a, "16:9")) { n = 16; d = 9; }
}
int radius_of(const char *r) {
  if (!strcmp(r, "chip")) return rad::chip;
  if (!strcmp(r, "button")) return rad::button;
  if (!strcmp(r, "ring")) return rad::ring;
  if (!strcmp(r, "round")) return LV_RADIUS_CIRCLE;
  return 0;
}
img::Scale scale_of(const char *s) { return !strcmp(s, "contain") ? img::contain : !strcmp(s, "pixel") ? img::pixel : img::cover; }
uint32_t every_ms(const char *s) {
  if (!strcmp(s, "1m")) return 60000;
  if (!strcmp(s, "5m")) return 300000;
  if (!strcmp(s, "15m")) return 900000;
  if (!strcmp(s, "1h")) return 3600000;
  return 0;
}
// the box: a cell's whole size (fit: cell), else the step's width x aspect (height snapped down to the grid)
void box_of(lv_obj_t *parent, Props p, int &w, int &h) {
  if (!strcmp(p["fit"] | "step", "cell") && parent && lv_obj_has_flag(parent, spec::SLOT)) {
    int32_t cw = lv_obj_get_style_width(parent, LV_PART_MAIN), ch = lv_obj_get_style_height(parent, LV_PART_MAIN);
    if (cw > 0 && !LV_COORD_IS_SPEC(cw)) {
      w = (int) cw;
      h = ch > 0 && !LV_COORD_IS_SPEC(ch) ? (int) ch : w;
      return;
    }
  }
  if (!strcmp(p["fit"] | "step", "cell") && parent) {  // not in a cell (a playground, a plain column): fill the parent's content box
    lv_obj_update_layout(parent);
    const int32_t cw = lv_obj_get_content_width(parent), ch = lv_obj_get_content_height(parent);
    if (cw >= sp::GRID) {
      w = (int) cw / sp::GRID * sp::GRID;
      h = ch >= sp::GRID ? (int) ch / sp::GRID * sp::GRID : w;
      return;
    }
  }
  int n, d;
  aspect_of(p["aspect"] | "1:1", n, d);
  w = step_px(p["step"] | "card", parent);
  h = std::max(sp::GRID, (w * d / n) / sp::GRID * sp::GRID);
}
void draw(I &i);
void changed(void *key) { draw(*((Key *) key)->i); }
// the largest icon size that fits the box less sp::s2 each side (and leaves room for the alt line when there is one)
// (snapped down to a size the icon ships at, like the Icon component; -1 = none fits / unknown icon)
int fallback_size(const char *name, int room) {
  const int idx = icon_index(name);
  int best = -1;
  for (int s = 0; idx >= 0 && s < ic::N_SIZES && s <= ic::card; s++)
    if (ic::PX[s] <= room && icon_has(idx, (ic::Size) s)) best = s;
  return best;
}
void draw(I &i) {
  lv_obj_t *r = i.root;
  const bool cur_ready = img::state(&i.cur) == img::ready && img::pixels(&i.cur);
  if (cur_ready && i.has_prev) { img::release(&i.prev); i.has_prev = false; }
  const lv_image_dsc_t *px = img::pixels(&i.cur);
  if (!px && i.has_prev) px = img::pixels(&i.prev);  // the last good picture until the new one is in (or failed)
  fw::refill(r);
  lv_obj_remove_flag(r, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_size(r, i.w, i.h);
  if (px) {
    lv_obj_t *o = lv_image_create(r);
    lv_image_set_src(o, px);
    lv_obj_remove_flag(o, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
    lv_obj_center(o);
    return;
  }
  lv_obj_set_style_bg_color(r, T->surface, 0);  // loading / failed: the quiet box
  lv_obj_set_style_bg_opa(r, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(r, i.radius, 0);
  if (img::state(&i.cur) != img::failed) return;
  const int line = ty::STYLES[ty::c_meta].box;
  const int room = std::min(i.w, i.h) - 2 * sp::s2;
  const bool alt = !i.alt.empty() && i.h >= 2 * sp::s2 + line + (i.fallback.empty() ? 0 : ic::PX[ic::icon] + sp::s1);
  const int s = i.fallback.empty() ? -1 : fallback_size(i.fallback.c_str(), alt ? std::min(room, i.h - 2 * sp::s2 - line - sp::s1) : room);
  if (s < 0 && !alt) return;
  lv_obj_set_flex_flow(r, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(r, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(r, sp::s1, 0);
  if (s >= 0) icon(r, i.fallback.c_str(), (ic::Size) s, T->text_dim);
  if (alt) {
    lv_obj_t *t = text(r, ty::c_meta, i.alt.c_str(), T->text_dim, i.w - 2 * sp::s2);
    lv_label_set_long_mode(t, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_style_text_align(t, LV_TEXT_ALIGN_CENTER, 0);
  }
}
void refresh_cb(lv_timer_t *t) { img::refresh(((I *) lv_timer_get_user_data(t))->src.c_str()); }
void fill(I &i, Props p, bool keep_old) {
  lv_obj_t *parent = lv_obj_get_parent(i.root);
  box_of(parent, p, i.w, i.h);
  const char *rs = p["radius"] | "none";
  i.radius = radius_of(rs);
  i.fallback = p["fallback"] | "";
  i.alt = p["alt"] | "";
  const std::string src = p["src"] | "";
  const img::Scale sc = scale_of(p["scale"] | "cover");
  const bool nearest = !strcmp(p["sampling"] | "smooth", "nearest");
  const int pr = i.radius == LV_RADIUS_CIRCLE ? 1 << 14 : i.radius;
  if (keep_old && img::pixels(&i.cur) && src != i.src && !src.empty()) {  // (no src = the quiet box at once)
    // keep the shown picture as prev while the new one loads
    if (i.has_prev) img::release(&i.prev);
    img::acquire(&i.prev, i.src.c_str(), i.w, i.h, i.sc, pr, changed, i.nearest);  // same view: shares its pixels, no fetch
    i.has_prev = img::pixels(&i.prev) != nullptr;
    if (!i.has_prev) img::release(&i.prev);
  }
  if (src.empty() && i.has_prev) { img::release(&i.prev); i.has_prev = false; }
  i.src = src;
  i.sc = sc;
  i.nearest = nearest;
  img::acquire(&i.cur, src.c_str(), i.w, i.h, sc, pr, changed, nearest);
  if (i.every) { lv_timer_delete(i.every); i.every = nullptr; }
  if (const uint32_t ms = every_ms(p["refresh"] | "off")) i.every = lv_timer_create(refresh_cb, ms, &i);
  draw(i);
}
}  // namespace

FW_COMPONENT(image, "Image", "components", R"({
  "about": "A picture from a web address (http or https), sized by a size step or the grid cell it sits in. Cover fills the box and crops the edges, contain shows all of it, pixel zooms small pixel art by whole steps so it stays crisp. While it loads the box is quiet; if it fails, a fallback icon and its alt words show instead, and a picture that was already there stays when a refresh fails.",
  "frame": [240, 200],
  "props": [
    {"name": "src", "about": "The picture's web address (http or https). Bind it to an entity's state or attribute to follow it; in a grid's item cell, $item.field takes it from each item.", "type": "text", "default": "https://mrdarrengriffin.github.io/facet-docs/demo/images/hills.png"},
    {"name": "step", "about": "How wide the picture is, as a size step from icon up to hero, or content for the full content width. Leave it out when fit is cell.", "type": "select", "options": ["icon", "glyph", "detail", "feature", "card", "hero", "content"], "default": "card"},
    {"name": "fit", "about": "step: the size comes from step and aspect. cell: inside a grid cell or a layout slot, the picture fills the cell. Use step or cell, not both.", "type": "select", "options": ["step", "cell"], "default": "step"},
    {"name": "aspect", "about": "The box shape with step: 1:1, 4:3, 3:2 or 16:9 (width to height).", "type": "select", "options": ["1:1", "4:3", "3:2", "16:9"], "default": "1:1"},
    {"name": "scale", "about": "cover fills the box and crops the edges, contain shows the whole picture inside the box, pixel zooms pixel art by whole steps with sharp pixels.", "type": "select", "options": ["cover", "contain", "pixel"], "default": "cover"},
    {"name": "sampling", "about": "How the picture is enlarged with cover or contain: smooth blends neighbouring pixels (photos), nearest keeps every pixel a sharp square (pixel art, icons). Smooth is the default because it can't be told from the picture; pixel scale is always nearest.", "type": "select", "options": ["smooth", "nearest"], "default": "smooth"},
    {"name": "radius", "about": "Rounded corners: none, chip, button, ring, or round for a circle.", "type": "select", "options": ["none", "chip", "button", "ring", "round"], "default": "none"},
    {"name": "fallback", "about": "An icon name shown if the picture can't be loaded; leave it empty for a plain quiet box.", "type": "text", "default": ""},
    {"name": "alt", "about": "A few words saying what the picture shows; shown under the fallback icon when the picture fails and there is room.", "type": "text", "default": ""},
    {"name": "refresh", "about": "Fetch the picture again every 1, 5 or 15 minutes or every hour (for a camera still or a chart); off loads it once. If a refresh fails, the last picture stays.", "type": "select", "options": ["off", "1m", "5m", "15m", "1h"], "default": "off"}
  ],
  "variants": [
    {"label": "card · 4:3 · cover", "props": {"src": "https://mrdarrengriffin.github.io/facet-docs/demo/images/hills.png", "step": "card", "aspect": "4:3"}},
    {"label": "hero · 16:9 · contain", "props": {"src": "https://mrdarrengriffin.github.io/facet-docs/demo/images/hills.png", "step": "hero", "aspect": "16:9", "scale": "contain", "radius": "button"}},
    {"label": "pixel art · hero", "props": {"src": "https://mrdarrengriffin.github.io/facet-docs/demo/images/sprite.png", "step": "hero", "scale": "pixel"}},
    {"label": "pixel art · contain · nearest", "props": {"src": "https://mrdarrengriffin.github.io/facet-docs/demo/images/sprite.png", "step": "hero", "aspect": "4:3", "scale": "contain", "sampling": "nearest"}},
    {"label": "round · card", "props": {"src": "https://mrdarrengriffin.github.io/facet-docs/demo/images/avatar.png", "step": "card", "radius": "round"}},
    {"label": "failed · fallback + alt", "props": {"src": "https://mrdarrengriffin.github.io/facet-docs/demo/images/missing.png", "step": "hero", "aspect": "4:3", "fallback": "house", "alt": "Front garden"}},
    {"label": "no src · quiet box", "props": {"src": "", "step": "card"}}
  ]
})") {
  lv_obj_t *r = lv_obj_create(parent);
  lv_obj_remove_style_all(r);
  auto *i = new I;
  i->root = r;
  lv_obj_set_user_data(r, i);
  lv_obj_add_event_cb(r, [](lv_event_t *e) {
    I *i = (I *) lv_event_get_user_data(e);
    if (i->every) lv_timer_delete(i->every);
    img::release(&i->cur);
    img::release(&i->prev);
    delete i;
  }, LV_EVENT_DELETE, i);
  fill(*i, p, false);
}

// Update path: another src keeps the shown picture until the new one is in (a failure keeps it); other props re-fit
// in place. Silent.
FW_UPDATE(image) {
  I *i = (I *) lv_obj_get_user_data(root);
  if (!i) return false;
  fill(*i, p, true);
  return true;
}
