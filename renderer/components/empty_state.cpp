// Empty state: the Facet empty state (docs: Components > Lists > Empty state, @facet/ui lists.ts emptyState()), on the
// contract (renderer/CONTRACT.md, "contract": 2): a quiet box for "nothing here". Fills its parent's width (C5; in a
// list Body = the content width, as before), h:empty-h tall (136 / 92), border:line (1 px), radius card, padding
// empty-pad (gutter less the hair), a centred column with gap:2: the icon at the feature size (48 / 32) in the tone,
// the title (c-tile, text) and one line of body (c-meta, text-dim), both centred, one line ending in "...".
// Minimum: layout:card-min-w x layout:empty-h. Steps: one. Tones: ok, warn, error, working (accent-text), info (blue),
// idle (text-dim): a token name per tone, never a colour. States: n/a (it IS the empty state; static, no taps).
// Bleed: none. Update path: refills (the new tone / words fade, like a tap). Contrast pairs: "empty_state".
#include "fw.h"
#include "fx.h"

namespace {
const char *tone(const char *s) {
  return !strcmp(s, "warn") ? "fg:warn" : !strcmp(s, "error") ? "fg:error" : !strcmp(s, "working") ? "fg:accent-text"
       : !strcmp(s, "info") ? "fg:blue" : !strcmp(s, "idle") ? "fg:text-dim" : "fg:ok";
}
void line(lv_obj_t *c, const char *type, const char *s, const char *fg) {
  lv_obj_t *l = fx::text(c, s, type, fg);
  fx::use(l, "cell-text");  // the full width, centred
  lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_DOTS);
}
void fill(lv_obj_t *c, fw::Props p) {
  for (const char *t : {"box", "centre", "gap:2", "h:empty-h", "radius:card", "border:line", "empty-pad"}) fx::use(c, t);
  fx::name(c, "empty_state");
  fx::icon(c, p["icon"] | "done", "feature", tone(p["tone"] | "ok"));
  line(c, "type:c-tile", p["title"] | "Nothing needs you", "fg:text");
  line(c, "type:c-meta", p["body"] | "No review requests or assignments", "fg:text-dim");
}
}  // namespace

FW_COMPONENT(empty_state, "Empty state", "components", R"({
  "about": "A quiet box for nothing here: an icon in the tone, a title and one line.",
  "contract": 2,
  "min": ["layout:card-min-w", "layout:empty-h"],
  "frame": [464, 128],
  "icons": ["done@feature", "alert@feature", "pr@feature"],
  "props": [
    {"name": "icon", "about": "The icon at the top: done for all clear, alert for a problem, pr for pull requests.", "type": "select", "options": ["done", "alert", "pr"], "default": "done"},
    {"name": "title", "about": "The short heading that says nothing is here.", "type": "text", "default": "Nothing needs you"},
    {"name": "body", "about": "One quieter line under the heading that explains it.", "type": "text", "default": "No review requests or assignments"},
    {"name": "tone", "about": "The icon's colour, by what the emptiness means: ok, warn, error, working, info or idle.", "type": "select", "options": ["ok", "warn", "error", "working", "info", "idle"], "default": "ok"}
  ],
  "variants": [
    {"label": "all clear · ok", "props": {}},
    {"label": "offline · warn", "props": {"icon": "alert", "tone": "warn", "title": "GitHub is not reachable", "body": "Showing what we had at 14:02"}},
    {"label": "no pull requests · idle", "props": {"icon": "pr", "tone": "idle", "title": "No open pull requests", "body": ""}}
  ]
})") {
  lv_obj_t *c = lv_obj_create(parent);  // styles: fill() (an update refills the same root)
  lv_obj_remove_style_all(c);
  lv_obj_remove_flag(c, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  fill(c, p);
}

FW_UPDATE(empty_state) {
  fw::st::refill_fade(root, [&] { fw::refill(root); fill(root, p); }, true, true);  // a new tone / mark fades like a tap (shared styles too)
  return true;
}
