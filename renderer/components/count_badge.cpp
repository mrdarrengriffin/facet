// Count badge: the Facet count badge (docs: @facet/ui lists.ts countBadge(n, tone), CSS .f-badge), on the contract
// (renderer/CONTRACT.md, "contract": 2): a tinted pill with a number, next to a section title or a row ("Needs you 3").
//   size     a fixed part from tokens ("badge", core/fx_core.cpp): layout:badge-h tall (32 / 24) and at least as wide
//            (one digit = a circle); padding space 2; radius round; the number box at least badge-h - 2 x space 2
//            ("badge-num") and on the 4 px grid (fx::fit_grid), so 10-99 and 99+ grow the pill by whole grid steps
//   minimum  layout:badge-h x layout:badge-h. Steps: one
//   number   type c-caps-sm, fg:text (as the Status chip: the status colours fail 4.5:1 on their own tint in light)
//   fill     the tone at chip.tint (tint:<colour>): ok, warn, error, working (accent), info (blue), idle (text-dim)
//   counts   0 = "zero": muted (shows "0" in the idle tone, whatever the tone) or hide (no badge); 1-99 as is; 100+ = 99+
// Static: no events, no sound. Bleed: none. Contrast pairs: tokens/colour.json section "count_badge".
#include "fw.h"
#include "fx.h"

namespace {
const char *tint_of(const char *t) {
  if (!strcmp(t, "ok")) return "tint:ok";
  if (!strcmp(t, "warn")) return "tint:warn";
  if (!strcmp(t, "error")) return "tint:error";
  if (!strcmp(t, "working")) return "tint:accent";
  if (!strcmp(t, "info")) return "tint:blue";
  return "tint:text-dim";  // idle (text-dim: text-mute is for disabled only, Darren 2026-10-21)
}
void fill(lv_obj_t *b, fw::Props p) {
  // a number, or a bound Home Assistant state (text: "4", "4.0")
  const int n = std::max(0, p["count"].is<const char *>() ? atoi(p["count"].as<const char *>()) : (int) (p["count"] | 0));
  const bool zero = n == 0;
  char txt[8];
  if (n > 99) snprintf(txt, sizeof txt, "99+");
  else snprintf(txt, sizeof txt, "%d", n);
  // zero + hide: the badge stays as a hidden root (the update path needs one object; hidden = out of the layout)
  lv_obj_set_flag(b, LV_OBJ_FLAG_HIDDEN, zero && !strcmp(p["zero"] | "muted", "hide"));
  lv_obj_remove_flag(b, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  fx::use(b, "badge");
  fx::use(b, tint_of(zero ? "idle" : (p["tone"] | "idle")));
  lv_obj_t *l = fx::text(b, txt, "type:c-caps-sm", "fg:text");
  fx::fit_grid(l);
  fx::use(l, "badge-num");
}
}  // namespace

FW_COMPONENT(count_badge, "Count badge", "components", R"({
  "about": "A small round number, such as how many items need attention.",
  "contract": 2,
  "min": ["layout:badge-h", "layout:badge-h"],
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
  lv_obj_t *b = lv_obj_create(parent);
  lv_obj_remove_style_all(b);
  fx::name(b, "count_badge");
  fill(b, p);
}

// Update path: the new count / tone / zero rule snaps in the same root (fw::refill), silent.
FW_UPDATE(count_badge) {
  fw::st::refill_fade(root, [&] { fw::refill(root); fill(root, p); }, true, true);  // shared: the tint is a token style  // a new tone / mark fades (an update animates like a tap)
  return true;
}
