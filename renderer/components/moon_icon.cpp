// Moon phase: the Facet moon glyph (docs @facet/icons moonIcon(frac, { size, south })), drawn on the device by core
// fw::moon_icon: disc moon-dark, lit part moon-lit (lit limb + terminator half ellipse), outline moon-edge at the
// weather stroke (2 px, 3 from 96). Masks are computed at the exact px (no stored phase frames): a new phase redraws
// only the lit mask, in place.
//   phase  0..1 through the synodic month (0 new, 0.25 first quarter, 0.5 full, 0.75 last quarter)
//   size   an icon size token (docs: 24 in lists, 48 rows, 72 Home card, hero)
//   south  southern hemisphere (mirrored: the lit side grows from the left)
// Update path: phase / south redraw in place; size refills. Static: no events, no sound.
#include "fw.h"

namespace {
using namespace fw;
ic::Size size_of(const char *s) {
  for (int i = 0; i < ic::N_SIZES; i++)
    if (!strcmp(ic::SIZE_NAMES[i], s)) return (ic::Size) i;
  return ic::card;
}
void fill(lv_obj_t *r, Props p) {
  fw::shape(r, p["size"] | "card");
  lv_obj_remove_style_all(r);
  lv_obj_remove_flag(r, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_size(r, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  moon_icon(r, p["phase"] | 0.25f, size_of(p["size"] | "card"), p["south"] | false);
}
}  // namespace

FW_COMPONENT(moon_icon, "Moon phase", "components", R"({
  "about": "The moon as it looks tonight: a dark disc with the lit part for any point in the month.",
  "audit": {"snaps": {"phase": "the phase moves once a day: a redraw, nothing to animate"}},
  "frame": [240, 200],
  "props": [
    {"name": "phase", "about": "Where the moon is in its month: 0 is new, 0.25 first quarter, 0.5 full, 0.75 last quarter.", "type": "range", "min": 0, "max": 1, "step": 0.01, "default": 0.25},
    {"name": "size", "about": "How big the moon is drawn, from the small icon size up to the hero size of a screen.", "type": "select", "options": ["icon", "glyph", "detail", "feature", "card", "hero"], "default": "card"},
    {"name": "south", "about": "Draws the moon as seen from the southern half of the world, where the lit side is the other way round.", "type": "bool", "default": false}
  ],
  "variants": [
    {"label": "new", "props": {"phase": 0}},
    {"label": "waxing crescent", "props": {"phase": 0.12}},
    {"label": "first quarter", "props": {"phase": 0.25}},
    {"label": "waxing gibbous", "props": {"phase": 0.37}},
    {"label": "full", "props": {"phase": 0.5}},
    {"label": "waning gibbous", "props": {"phase": 0.63}},
    {"label": "last quarter", "props": {"phase": 0.75}},
    {"label": "waning crescent", "props": {"phase": 0.88}},
    {"label": "first quarter · south", "props": {"phase": 0.25, "south": true}}
  ]
})") {
  fill(lv_obj_create(parent), p);
}

// Update path: a new phase (or hemisphere) redraws the lit mask in place; size = refill. Silent.
FW_UPDATE(moon_icon) {
  lv_obj_t *m = lv_obj_get_child(root, 0);
  if (fw::shape(root, p["size"] | "card") && m && fw::moon_icon_set(m, p["phase"] | 0.25f, p["south"] | false)) return true;
  fw::refill(root);
  fill(root, p);
  return true;
}
