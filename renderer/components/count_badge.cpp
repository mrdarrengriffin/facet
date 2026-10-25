// Count badge: the Facet count badge (docs: @facet/ui lists.ts countBadge(n, tone), CSS .f-badge): a tinted pill with a
// number, next to a section title or a row ("Needs you 3").
//   box      height lay::badge_h (32 / 24) = its minimum width, so one digit is a circle; padding space 2 (8 / 4),
//            radius round; the number box is at least badge_h - 2 x space 2 (16 at both densities, = docs .f-badge-text
//            min 16) and on the grid (Label fit8), so 10-99 and 99+ grow the pill by whole grid steps
//   number   the Label component, type caps-sm, colour text (as the Status chip: the status colours fail 4.5:1 on their
//            own tint in light, so the tone is the fill)
//   fill     the tone at chip.tint (= Status chip, docs opacity chip-bg), bg_opa; tones ok / warn / error / working /
//            info / idle (default idle = docs countBadge default)
//   counts   0 = "zero": muted (default: shows "0" in the idle tone, whatever the tone: nothing to do) or hide (no
//            badge); 1-99 as is; 100 and up = "99+"
// Static: no events, no sound. Contrast pairs: tokens/colour.json section "count_badge".
#include "fw.h"

namespace {
lv_color_t fill_of(const char *t) {
  const fw::Theme &T = *fw::T;
  if (!strcmp(t, "ok")) return T.ok;
  if (!strcmp(t, "warn")) return T.warn;
  if (!strcmp(t, "error")) return T.error;
  if (!strcmp(t, "working")) return T.accent;
  if (!strcmp(t, "info")) return T.blue;
  return T.text_dim;  // idle (text-dim: text-mute is for disabled only, Darren 2026-10-21)
}
}  // namespace

namespace {
void fill(lv_obj_t *b, fw::Props p) {
  const int n = std::max(0, (int) (p["count"] | 0));
  const bool zero = n == 0;
  const char *tone = zero ? "idle" : (p["tone"] | "idle");
  char txt[8];
  if (n > 99) snprintf(txt, sizeof txt, "99+");
  else snprintf(txt, sizeof txt, "%d", n);

  lv_obj_remove_style_all(b);
  // zero + hide: the badge stays as a hidden root (the update path needs one object; hidden = out of the layout)
  lv_obj_set_flag(b, LV_OBJ_FLAG_HIDDEN, zero && !strcmp(p["zero"] | "muted", "hide"));
  lv_obj_remove_flag(b, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_size(b, LV_SIZE_CONTENT, fw::lay::badge_h);
  lv_obj_set_style_min_width(b, fw::lay::badge_h, 0);
  lv_obj_set_style_pad_hor(b, fw::sp::s2, 0);
  lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(b, fill_of(tone), 0);
  lv_obj_set_style_bg_opa(b, (lv_opa_t) lroundf(fw::tokens::comp::chip_tint * 255), 0);
  lv_obj_set_flex_flow(b, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(b, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  if (const fw::Component *lc = fw::find("label")) {
    ArduinoJson::JsonDocument lp;
    lp["text"] = txt;
    lp["type"] = "caps-sm";
    lp["colour"] = "text";
    lc->build(b, lp.as<fw::Props>());
    lv_obj_t *l = lv_obj_get_child(b, -1);
    const int min_w = fw::lay::badge_h - 2 * fw::sp::s2;
    if (lv_obj_get_width(l) < min_w) lv_obj_set_width(l, min_w);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
  }
}
}  // namespace

FW_COMPONENT(count_badge, "Count badge", "components", R"({
  "about": "A small round number, such as how many items need attention.",
  "audit": {"snaps": {"count": "a count is a number that changes like text, no tap transition to copy"}},
  "frame": [160, 64],
  "props": [
    {"name": "count", "about": "The number in the badge. Above 99 it shows 99+.", "type": "range", "min": 0, "max": 150, "step": 1, "default": 3},
    {"name": "tone", "about": "The badge colour, by what the number means: ok, warn, error, working, info or idle.", "type": "select", "options": ["ok", "warn", "error", "working", "info", "idle"], "default": "idle"},
    {"name": "zero", "about": "What happens at zero: muted shows a quiet 0; hide removes the badge.", "type": "select", "options": ["muted", "hide"], "default": "muted"}
  ],
  "variants": [
    {"label": "0 · muted", "props": {"count": 0, "tone": "warn"}},
    {"label": "1", "props": {"count": 1, "tone": "warn"}},
    {"label": "9", "props": {"count": 9, "tone": "working"}},
    {"label": "12", "props": {"count": 12, "tone": "error"}},
    {"label": "99", "props": {"count": 99, "tone": "info"}},
    {"label": "128 = 99+", "props": {"count": 128, "tone": "ok"}},
    {"label": "idle", "props": {"count": 3, "tone": "idle"}}
  ]
})") {
  fill(lv_obj_create(parent), p);
}

// Update path: the new count / tone / zero rule snaps in the same root (fw::refill), silent.
FW_UPDATE(count_badge) {
  fw::st::refill_fade(root, [&] { fw::refill(root); fill(root, p); });  // a new tone / mark fades (an update animates like a tap)
  return true;
}
