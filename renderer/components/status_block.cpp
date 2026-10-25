// Status block: a device's state in words at the top of its screen (the panel lab's lock / vacuum / cover column:
// caption, state, one line). Three type roles, nothing else:
//   caption  type label (caps), in the tone: dim = text-dim, accent = accent-text (needs a look: not locked,
//            cleaning), ok / warn / error = their colours, off = text-dim
//   value    type value (64 / 44), text; error = error, off = text-dim (an em dash when empty)
//   line     type body, text-dim (since 08:14 · by Alex, battery 100%); empty = no line
//   size     value (above) | hero (the panel lab's Vacuum Rooms view: the state in type hero, accent-text when the tone
//            is accent, the line in type title, text-dim, no gap) | title (its Map view: the state in type title, the
//            line in type label, text-dim, no gap). An empty caption takes no room.
//   parts    value2 joins the value and line2..line4 the line with " · ", empty parts left out ("Docked · 100%",
//            "100% · charged · last run yesterday 18:42"): one bind per fact
// sp::s2 apart (value size), as wide as its parent (a Split column or the content width), one line each with an ellipsis. A spec
// feeds it with binds that map an entity's state to words and a tone (core/ha.h map / tpl).
// Static, not a control. Update path: texts in place, a tone change fades (fw::st::restyle, silent).
// Contrast: section "status_block".
#include "fw.h"

namespace {
// a text only when it changes (an update with the same props redraws nothing)
void put(lv_obj_t *l, fw::ty::Id ty, const char *t) {
  if (fw::cased(ty, t) != lv_label_get_text(l)) fw::set_text(l, ty, t);
}
struct SB { lv_obj_t *cap, *val, *line; std::string tone, size; };
fw::ty::Id val_ty(const std::string &z) { return z == "hero" ? fw::ty::hero : z == "title" ? fw::ty::title : fw::ty::value; }
fw::ty::Id line_ty(const std::string &z) { return z == "hero" ? fw::ty::title : z == "title" ? fw::ty::label : fw::ty::body; }
// "a · b · c", empty parts left out
std::string join(fw::Props p, std::initializer_list<const char *> keys) {
  std::string o;
  for (const char *k : keys) {
    const char *v = p[k] | "";
    if (*v) o += (o.empty() ? "" : " \xC2\xB7 ") + std::string(v);
  }
  return o;
}
void colours(SB *b, const char *tone) {
  const fw::Theme &t = *fw::T;
  const bool err = !strcmp(tone, "error"), off = !strcmp(tone, "off");
  const lv_color_t c = !strcmp(tone, "accent") ? t.accent_text : !strcmp(tone, "ok") ? t.ok : !strcmp(tone, "warn") ? t.warn : err ? t.error : t.text_dim;
  lv_obj_set_style_text_color(b->cap, c, 0);
  const bool acc = b->size == "hero" && !strcmp(tone, "accent");  // the lab's hero: accent while it works
  lv_obj_set_style_text_color(b->val, err ? t.error : off ? t.text_dim : acc ? t.accent_text : t.text, 0);
}
void texts(SB *b, fw::Props p) {
  const std::string v = join(p, {"value", "value2"}), l = join(p, {"line", "line2", "line3", "line4"});
  const char *cap = p["caption"] | "";
  put(b->cap, fw::ty::label, cap);
  if (*cap == lv_obj_has_flag(b->cap, LV_OBJ_FLAG_HIDDEN)) lv_obj_set_flag(b->cap, LV_OBJ_FLAG_HIDDEN, !*cap);
  put(b->val, val_ty(b->size), v.empty() ? "\xE2\x80\x94" : v.c_str());
  put(b->line, line_ty(b->size), l.c_str());
  if (l.empty() == !lv_obj_has_flag(b->line, LV_OBJ_FLAG_HIDDEN)) lv_obj_set_flag(b->line, LV_OBJ_FLAG_HIDDEN, l.empty());
}
}  // namespace

FW_COMPONENT(status_block, "Status block", "components", R"({
  "about": "A device's state in words: a caps caption in its tone, the state as a value, one quiet line.",
  "frame": [480, 0],
  "props": [
    {"name": "caption", "about": "The small capitals line at the top in the tone colour, such as secure. Leave empty for none.", "type": "text", "default": "secure"},
    {"name": "value", "about": "The state in words, such as Locked.", "type": "text", "default": "Locked"},
    {"name": "line", "about": "One quieter line under the state, such as since when.", "type": "text", "default": "since 08:14 · by Alex"},
    {"name": "tone", "about": "The caption's colour: dim, accent, ok, warn, error, or off.", "type": "select", "options": ["dim", "accent", "ok", "warn", "error", "off"], "default": "dim"},
    {"name": "size", "about": "How big the state is: value for the usual size, hero for a screen's main figure, title for a smaller heading.", "type": "select", "options": ["value", "hero", "title"], "default": "value"},
    {"name": "value2", "about": "A second value beside the first, such as a battery level. Leave empty for none.", "type": "text", "default": ""},
    {"name": "line2", "about": "A second quieter line. Leave empty for none.", "type": "text", "default": ""},
    {"name": "line3", "about": "A third quieter line. Leave empty for none.", "type": "text", "default": ""},
    {"name": "line4", "about": "A fourth quieter line. Leave empty for none.", "type": "text", "default": ""}
  ],
  "variants": [
    {"label": "calm", "props": {}},
    {"label": "needs a look", "props": {"caption": "not locked", "value": "Unlocked", "line": "since 19:18 · by Sam", "tone": "accent"}},
    {"label": "error", "props": {"caption": "jammed · check the door", "value": "Jammed", "line": "", "tone": "error"}},
    {"label": "offline", "props": {"caption": "offline", "value": "", "line": "", "tone": "off"}},
    {"label": "hero · vacuum", "props": {"size": "hero", "caption": "", "value": "Docked", "line": "100%", "line2": "charged", "line3": "last run yesterday 18:42"}},
    {"label": "hero · working", "props": {"size": "hero", "caption": "", "value": "Cleaning", "line": "Kitchen", "line2": "28%", "line3": "10 min", "line4": "8.4 m²", "tone": "accent"}},
    {"label": "title · map", "props": {"size": "title", "caption": "", "value": "Docked", "value2": "100%", "line": "last run yesterday 18:42"}}
  ]
})") {
  const fw::Theme &t = *fw::T;
  lv_obj_t *r = lv_obj_create(parent);
  lv_obj_remove_style_all(r);
  lv_obj_remove_flag(r, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_size(r, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(r, LV_FLEX_FLOW_COLUMN);
  auto *b = new SB;
  b->size = p["size"] | "value";
  lv_obj_set_style_pad_row(r, b->size == "value" ? fw::sp::s2 : 0, 0);
  lv_obj_set_user_data(r, b);
  lv_obj_add_event_cb(r, [](lv_event_t *e) { delete (SB *) lv_event_get_user_data(e); }, LV_EVENT_DELETE, b);
  b->cap = fw::text(r, fw::ty::label, "", t.text_dim);
  b->val = fw::text(r, val_ty(b->size), "", t.text);
  b->line = fw::text(r, line_ty(b->size), "", t.text_dim);
  for (auto [l, ty] : {std::pair{b->cap, fw::ty::label}, std::pair{b->val, val_ty(b->size)}, std::pair{b->line, line_ty(b->size)}}) {
    lv_obj_set_width(l, LV_PCT(100));
    lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
    lv_obj_set_height(l, fw::style(ty).box);  // the type box: one line on the grid
  }
  if (b->size == "hero") {  // the lab's 112; no ellipsis clip (the hero glyphs run past the type box, as Hero block's)
    lv_obj_set_height(b->val, fw::style(fw::ty::hero).box + fw::sp::s4);
    lv_label_set_long_mode(b->val, LV_LABEL_LONG_WRAP);
  }
  texts(b, p);
  b->tone = p["tone"] | "dim";
  colours(b, b->tone.c_str());
}

FW_UPDATE(status_block) {
  auto *b = (SB *) lv_obj_get_user_data(root);
  if (!b || b->size != (p["size"] | "value")) return false;  // another size = built again
  texts(b, p);
  const std::string tone = p["tone"] | "dim";
  if (tone != b->tone) {
    b->tone = tone;
    fw::st::restyle(root, [&] { colours(b, tone.c_str()); });
  }
  return true;
}
