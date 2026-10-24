// Status block: a device's state in words at the top of its screen (the panel lab's lock / vacuum / cover column:
// caption, state, one line). Three type roles, nothing else:
//   caption  type label (caps), in the tone: dim = text-dim, accent = accent-text (needs a look: not locked,
//            cleaning), ok / warn / error = their colours, off = text-dim
//   value    type value (64 / 44), text; error = error, off = text-dim (an em dash when empty)
//   line     type body, text-dim (since 08:14 · by Alex, battery 100%); empty = no line
// sp::s2 apart, as wide as its parent (a Split column or the content width), one line each with an ellipsis. A spec
// feeds it with binds that map an entity's state to words and a tone (core/ha.h map / tpl).
// Static, not a control. Update path: texts in place, a tone change fades (fw::st::restyle, silent).
// Contrast: section "status_block".
#include "fw.h"

namespace {
// a text only when it changes (an update with the same props redraws nothing)
void put(lv_obj_t *l, fw::ty::Id ty, const char *t) {
  if (fw::cased(ty, t) != lv_label_get_text(l)) fw::set_text(l, ty, t);
}
struct SB { lv_obj_t *cap, *val, *line; std::string tone; };
void colours(SB *b, const char *tone) {
  const fw::Theme &t = *fw::T;
  const bool err = !strcmp(tone, "error"), off = !strcmp(tone, "off");
  const lv_color_t c = !strcmp(tone, "accent") ? t.accent_text : !strcmp(tone, "ok") ? t.ok : !strcmp(tone, "warn") ? t.warn : err ? t.error : t.text_dim;
  lv_obj_set_style_text_color(b->cap, c, 0);
  lv_obj_set_style_text_color(b->val, err ? t.error : off ? t.text_dim : t.text, 0);
}
void texts(SB *b, fw::Props p) {
  const char *v = p["value"] | "";
  put(b->cap, fw::ty::label, p["caption"] | "");
  put(b->val, fw::ty::value, *v ? v : "\xE2\x80\x94");
  const char *l = p["line"] | "";
  put(b->line, fw::ty::body, l);
  if (*l == lv_obj_has_flag(b->line, LV_OBJ_FLAG_HIDDEN)) lv_obj_set_flag(b->line, LV_OBJ_FLAG_HIDDEN, !*l);
}
}  // namespace

FW_COMPONENT(status_block, "Status block", "components", R"({
  "about": "A device's state in words: a caps caption in its tone, the state as a value, one quiet line.",
  "frame": [480, 0],
  "props": [
    {"name": "caption", "type": "text", "default": "secure"},
    {"name": "value", "type": "text", "default": "Locked"},
    {"name": "line", "type": "text", "default": "since 08:14 · by Alex"},
    {"name": "tone", "type": "select", "options": ["dim", "accent", "ok", "warn", "error", "off"], "default": "dim"}
  ],
  "variants": [
    {"label": "calm", "props": {}},
    {"label": "needs a look", "props": {"caption": "not locked", "value": "Unlocked", "line": "since 19:18 · by Sam", "tone": "accent"}},
    {"label": "error", "props": {"caption": "jammed · check the door", "value": "Jammed", "line": "", "tone": "error"}},
    {"label": "offline", "props": {"caption": "offline", "value": "", "line": "", "tone": "off"}}
  ]
})") {
  const fw::Theme &t = *fw::T;
  lv_obj_t *r = lv_obj_create(parent);
  lv_obj_remove_style_all(r);
  lv_obj_remove_flag(r, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_size(r, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(r, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(r, fw::sp::s2, 0);
  auto *b = new SB;
  lv_obj_set_user_data(r, b);
  lv_obj_add_event_cb(r, [](lv_event_t *e) { delete (SB *) lv_event_get_user_data(e); }, LV_EVENT_DELETE, b);
  b->cap = fw::text(r, fw::ty::label, "", t.text_dim);
  b->val = fw::text(r, fw::ty::value, "", t.text);
  b->line = fw::text(r, fw::ty::body, "", t.text_dim);
  for (auto [l, ty] : {std::pair{b->cap, fw::ty::label}, std::pair{b->val, fw::ty::value}, std::pair{b->line, fw::ty::body}}) {
    lv_obj_set_width(l, LV_PCT(100));
    lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
    lv_obj_set_height(l, fw::style(ty).box);  // the type box: one line on the grid
  }
  texts(b, p);
  b->tone = p["tone"] | "dim";
  colours(b, b->tone.c_str());
}

FW_UPDATE(status_block) {
  auto *b = (SB *) lv_obj_get_user_data(root);
  if (!b) return false;
  texts(b, p);
  const std::string tone = p["tone"] | "dim";
  if (tone != b->tone) {
    b->tone = tone;
    fw::st::restyle(root, [&] { colours(b, tone.c_str()); });
  }
  return true;
}
