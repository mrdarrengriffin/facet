// Facet icons for this density (README "Icons"): the generated A8 masks (tools/gen_icons.mjs -> facet_icons*.h, from
// tokens/icons.json + icons/), in their own translation unit so a component change never recompiles the pixels.
// One icon = an S x S box with one lv_image per colour layer, each cropped to its ink and placed at its offset.
#include "fw.h"
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

void fw::icon_colour(lv_obj_t *icon, lv_color_t colour, lv_style_selector_t sel, const lv_style_transition_dsc_t *tr) {
  if (!icon) return;
  for (uint32_t i = 0; i < lv_obj_get_child_count(icon); i++) {
    lv_obj_t *im = lv_obj_get_child(icon, (int32_t) i);
    if ((uintptr_t) lv_obj_get_user_data(im) != ic::R_ink) continue;
    lv_obj_set_style_image_recolor(im, colour, sel);
    if (tr) lv_obj_set_style_transition(im, tr, sel);
  }
}
