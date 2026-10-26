// Icon: one Facet icon on its own (board facet-171, notes icon-grid-proposal §1, pieces 1). The Icons foundation's
// master at a size TOKEN (never px: specs carry no sizes), recoloured by a tone. A spec picks it by name, or lets an
// entity pick it with a bind's map ({"e": "sensor.x", "map": {"sunny": "clear-day"}, "else": "cloudy"}).
//   name   an icon id (tokens/icons.json); an unknown name (a bound value with no icon) = a quiet outlined box, never
//          a crash
//   step   small | icon | glyph | detail | feature | card | hero (ic:: sizes). Icons ship only at some sizes (pre-
//          rasterised masks, never scaled on the device): the step snaps down to the nearest size this icon ships at,
//          else its smallest
//   fit    step (default) | cell: inside a slot / grid cell, the largest shipped size that fits the cell less sp::s2
//          each side (snapped the same way; the on-device rasteriser for unshipped px is piece 4). Outside a cell =
//          step. A spec sets step or fit: cell, not both (core/spec.h)
//   tone   ink | dim | accent | ok | warn | error | info: token inks, pairs section "icon" (3:1 non-text on bg)
// Box = the icon's px square (the grid cell centres it). Static: no events, no sound.
// Update path: name / tone / size cross-fade from the old icon to the new (fw::st::refill_fade), silent.
#include "fw.h"
#include "spec.h"

namespace {
using namespace fw;
int step_of(const char *s) {
  for (int i = 0; i < ic::N_SIZES; i++)
    if (!strcmp(ic::SIZE_NAMES[i], s)) return i;
  return -1;
}
lv_color_t tone_of(const char *t) {
  if (!strcmp(t, "dim")) return T->text_dim;
  if (!strcmp(t, "accent")) return T->accent;
  if (!strcmp(t, "ok")) return T->ok;
  if (!strcmp(t, "warn")) return T->warn;
  if (!strcmp(t, "error")) return T->error;
  if (!strcmp(t, "info")) return T->blue;
  return T->text;
}
// the room a cell gives (fit: cell): its fixed size less sp::s2 each side; 0 = not in a slot / cell
int room_of(lv_obj_t *parent) {
  if (!parent || !lv_obj_has_flag(parent, spec::SLOT)) return 0;
  int32_t w = lv_obj_get_style_width(parent, LV_PART_MAIN), h = lv_obj_get_style_height(parent, LV_PART_MAIN);
  if (w <= 0 || LV_COORD_IS_SPEC(w)) return 0;
  if (h <= 0 || LV_COORD_IS_SPEC(h)) h = w;
  return LV_MIN(w, h) - 2 * sp::s2;
}
// the size drawn: the step (or the largest that fits the room), snapped down to a size the icon ships at, else its
// smallest (sizes are in ascending order)
int pick(int idx, int want, int room) {
  int best = -1, low = -1;
  for (int s = 0; s < ic::N_SIZES; s++) {
    if (idx >= 0 && !icon_has(idx, (ic::Size) s)) continue;
    if (low < 0) low = s;
    if (room > 0 ? ic::PX[s] <= room : s <= want) best = s;
  }
  return best >= 0 ? best : low >= 0 ? low : want;
}
void fill(lv_obj_t *r, Props p) {
  const char *name = p["name"] | "bulb";
  const int idx = icon_index(name);
  int want = step_of(p["step"] | "feature");
  if (want < 0) want = ic::feature;
  const int room = strcmp(p["fit"] | "step", "cell") ? 0 : room_of(lv_obj_get_parent(r));
  const int s = pick(idx, want, room), px = ic::PX[s];
  lv_obj_remove_style_all(r);
  lv_obj_remove_flag(r, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_size(r, px, px);
  if (idx < 0) {  // no such icon (a bound value nothing maps): a quiet outlined box the icon's size
    lv_obj_set_style_border_width(r, stroke::hair, 0);
    lv_obj_set_style_border_color(r, T->line_hi, 0);
    return;
  }
  icon(r, name, (ic::Size) s, tone_of(p["tone"] | "ink"));
}
}  // namespace

FW_COMPONENT(icon, "Icon", "components", R"({
  "about": "One icon from Facet's icon set, drawn at a size step and coloured by a tone. Pick it by name, or let an entity pick it: bind name to the entity with a map from its states to icon names. In a grid cell or a layout slot, fit: cell makes it the largest size that fits the cell.",
  "frame": [240, 200],
  "props": [
    {"name": "name", "about": "Which icon to show, by its name in the icon set (bulb, door, clear-day ...). Bind it to an entity with a map to change the icon with the entity's state; a name with no icon shows a quiet outlined box.", "type": "select", "options": )" FW_ICON_IDS R"(, "default": "bulb"},
    {"name": "step", "about": "How big the icon is, as a size step from small up to hero. Each icon is made for some sizes only, so the icon snaps down to the nearest size it has. Leave it out when fit is cell.", "type": "select", "options": )" FW_ICON_STEP_IDS R"(, "default": "feature"},
    {"name": "fit", "about": "step: the size comes from step. cell: inside a grid cell or a layout slot, the icon takes the largest size that fits the cell, leaving a small margin. Use step or cell, not both.", "type": "select", "options": ["step", "cell"], "default": "step"},
    {"name": "tone", "about": "The icon's colour by what it means: ink for normal, dim for quiet, accent for the screen's colour, ok, warn, error or info for a status.", "type": "select", "options": ["ink", "dim", "accent", "ok", "warn", "error", "info"], "default": "ink"}
  ],
  "variants": [
    {"label": "bulb · icon", "props": {"name": "bulb", "step": "icon"}},
    {"label": "bell · glyph · accent", "props": {"name": "bell", "step": "glyph", "tone": "accent"}},
    {"label": "alert · feature · warn", "props": {"name": "alert", "step": "feature", "tone": "warn"}},
    {"label": "done · feature · ok", "props": {"name": "done", "step": "feature", "tone": "ok"}},
    {"label": "pr · feature · info", "props": {"name": "pr", "step": "feature", "tone": "info"}},
    {"label": "rain-heavy · card · dim", "props": {"name": "rain-heavy", "step": "card", "tone": "dim"}},
    {"label": "clear-day · hero", "props": {"name": "clear-day", "step": "hero"}},
    {"label": "door at card snaps to icon", "props": {"name": "door", "step": "card"}}
  ]
})") {
  fill(lv_obj_create(parent), p);
}

// Update path: another name / tone / size fades from the old icon to the new one in the same box (silent).
FW_UPDATE(icon) {
  fw::st::refill_fade(root, [&] { fw::refill(root); fill(root, p); });
  return true;
}
