// Check row: the Facet check row (docs: Components > Lists > Check row, @facet/ui lists.ts checkRow()): one CI check in
// lay::check_row_h (32 / 20): a Status chip dot (fw::find("status_chip")) in the tone, the name (c-meta, text, grows,
// ellipsis) and the state word (c-caps-sm, the tone's text colour, right-aligned in lay::check_state_w), sp::s2 apart.
// pass ok, fail error, pending working (accent-text), skipped idle (text-dim). Width: the prop (dp), default a card.
// Static. Update path: refills (snap). Contrast pairs: colour.json section "check_row".
#include "fw.h"

namespace {
const char *tone_of(const char *s) { return !strcmp(s, "pass") ? "ok" : !strcmp(s, "fail") ? "error" : !strcmp(s, "pending") ? "working" : "idle"; }
lv_color_t ink_of(const fw::Theme &t, const char *s) {
  return !strcmp(s, "pass") ? t.ok : !strcmp(s, "fail") ? t.error : !strcmp(s, "pending") ? t.accent_text : t.text_dim;
}
void fill(lv_obj_t *r, fw::Props p) {
  const fw::Theme &t = *fw::T;
  const char *st = p["state"] | "pass";
  lv_obj_remove_style_all(r);
  lv_obj_remove_flag(r, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_size(r, fw::px((float) (p["width"] | 224)), fw::lay::check_row_h);
  lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(r, fw::sp::s2, 0);
  if (const fw::Component *c = fw::find("status_chip")) {
    ArduinoJson::JsonDocument dp;
    dp["tone"] = tone_of(st);
    dp["size"] = "dot";
    c->build(r, dp.as<fw::Props>());
  }
  lv_obj_t *n = fw::text(r, fw::ty::c_meta, p["name"] | "test", t.text);
  lv_label_set_long_mode(n, LV_LABEL_LONG_MODE_DOTS);
  lv_obj_set_height(n, fw::style(fw::ty::c_meta).box);
  lv_obj_set_flex_grow(n, 1);
  lv_obj_t *s = fw::text(r, fw::ty::c_caps_sm, st, ink_of(t, st));
  lv_obj_set_size(s, fw::lay::check_state_w, fw::style(fw::ty::c_caps_sm).box);
  lv_obj_set_style_text_align(s, LV_TEXT_ALIGN_RIGHT, 0);
}
}  // namespace

FW_COMPONENT(check_row, "Check row", "components", R"({
  "about": "A row for one CI check: a status dot, the name and the state.",
  "frame": [256, 64],
  "props": [
    {"name": "name", "about": "The name of the check, such as test or build.", "type": "text", "default": "test"},
    {"name": "state", "about": "How the check went: pass, fail, pending while it still runs, or skipped. The dot and the word change to match.", "type": "select", "options": ["pass", "fail", "pending", "skipped"], "default": "pass"},
    {"name": "width", "about": "How wide the row is, in pixels.", "type": "range", "min": 160, "max": 440, "step": 8, "default": 224}
  ],
  "variants": [
    {"label": "pass", "props": {"name": "build"}},
    {"label": "fail", "props": {"name": "build", "state": "fail"}},
    {"label": "pending", "props": {"name": "build", "state": "pending"}},
    {"label": "skipped", "props": {"name": "build", "state": "skipped"}},
    {"label": "long name", "props": {"name": "integration tests on the 480 panel build", "width": 200}}
  ]
})") {
  lv_obj_t *r = lv_obj_create(parent);
  fill(r, p);
}

FW_UPDATE(check_row) {
  fw::refill(root);
  fill(root, p);
  return true;
}
