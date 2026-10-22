// Empty state: the Facet empty state (docs: Components > Lists > Empty state, @facet/ui lists.ts emptyState()): a quiet
// box for "nothing here": content_w x lay::empty_h (136 / 92), a 1 px line border, rad::card, padding gutter - hair, a
// centred column: the icon at ic::feature (48 / 32, the Icons foundation icon) in the tone, sp::s2, the title (c-tile,
// text), sp::s2, one line of body (c-meta, text-dim), both centred, ellipsis. Tones: ok, warn, error, working
// (accent-text), info (blue), idle (text-dim). Static. Update path: refills (snap). Contrast pairs: "empty_state".
#include "fw.h"

namespace {
lv_color_t tone(const fw::Theme &t, const char *s) {
  return !strcmp(s, "warn") ? t.warn : !strcmp(s, "error") ? t.error : !strcmp(s, "working") ? t.accent_text
       : !strcmp(s, "info") ? t.blue : !strcmp(s, "idle") ? t.text_dim : t.ok;
}
void line(lv_obj_t *c, fw::ty::Id st, const char *s, lv_color_t col) {
  lv_obj_t *l = fw::text(c, st, s, col);
  lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_DOTS);
  lv_obj_set_size(l, LV_PCT(100), fw::style(st).box);
  lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
}
void fill(lv_obj_t *c, fw::Props p) {
  const fw::Theme &t = *fw::T;
  lv_obj_remove_style_all(c);
  lv_obj_remove_flag(c, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_size(c, fw::lay::content_w, fw::lay::empty_h);
  lv_obj_set_style_radius(c, fw::rad::card, 0);
  lv_obj_set_style_border_width(c, fw::stroke::hair, 0);
  lv_obj_set_style_border_color(c, t.line, 0);
  lv_obj_set_style_pad_all(c, fw::lay::gutter - fw::stroke::hair, 0);
  lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(c, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(c, fw::sp::s2, 0);
  fw::icon(c, p["icon"] | "done", fw::ic::feature, tone(t, p["tone"] | "ok"));
  line(c, fw::ty::c_tile, p["title"] | "Nothing needs you", t.text);
  line(c, fw::ty::c_meta, p["body"] | "No review requests or assignments", t.text_dim);
}
}  // namespace

FW_COMPONENT(empty_state, "Empty state", "components", R"({
  "about": "A quiet box for nothing here: an icon in the tone, a title and one line.",
  "frame": [464, 128],
  "icons": ["done@feature", "alert@feature", "pr@feature"],
  "props": [
    {"name": "icon", "type": "select", "options": ["done", "alert", "pr"], "default": "done"},
    {"name": "title", "type": "text", "default": "Nothing needs you"},
    {"name": "body", "type": "text", "default": "No review requests or assignments"},
    {"name": "tone", "type": "select", "options": ["ok", "warn", "error", "working", "info", "idle"], "default": "ok"}
  ],
  "variants": [
    {"label": "all clear · ok", "props": {}},
    {"label": "offline · warn", "props": {"icon": "alert", "tone": "warn", "title": "GitHub is not reachable", "body": "Showing what we had at 14:02"}},
    {"label": "no pull requests · idle", "props": {"icon": "pr", "tone": "idle", "title": "No open pull requests", "body": ""}}
  ]
})") {
  lv_obj_t *c = lv_obj_create(parent);
  fill(c, p);
}

FW_UPDATE(empty_state) {
  fw::st::refill_fade(root, [&] { fw::refill(root); fill(root, p); });  // a new tone / mark fades (an update animates like a tap)
  return true;
}
