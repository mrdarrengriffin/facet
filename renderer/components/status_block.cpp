// Status block: a device's state in words at the top of its screen (the panel lab's lock / vacuum / cover column:
// caption, state, one line), on the contract (renderer/CONTRACT.md, "contract": 2). Three type roles, nothing else:
//   caption  type:label (caps), in the tone: dim = fg:text-dim, accent = fg:accent-text (needs a look: not locked,
//            cleaning), ok / warn / error = fg:ok / fg:warn / fg:error, off = fg:text-dim
//   value    type:value, fg:text; error = fg:error, off = fg:text-dim (an em dash when empty)
//   line     type:body, fg:text-dim (since 08:14 · by Alex, battery 100%); empty = no line
//   size     value (above, a stack: space 2 apart) | hero (the panel lab's Vacuum Rooms view: the state in type:hero,
//            fg:accent-text when the tone is accent, the line in type:title, fg:text-dim, space 4 above it: the lab's
//            112 / 76 hero box) | title (its Map view: the state in type:title, the line in type:label, fg:text-dim,
//            no gap). An empty caption takes no room.
//   parts    value2 joins the value and line2..line4 the line with " · ", empty parts left out ("Docked · 100%",
//            "100% · charged · last run yesterday 18:42"): one bind per fact
//   box      fills its parent's width (C5); each text one line on its type box, "..." (the hero value wraps instead:
//            its glyphs run past the type box, as the Hero block's)
// Static, not a control: no press, no events, no sound. States: the tones above; n/a pressed, on, disabled, loading.
// Update path: texts in place; a new size or tone = the contents again in the same root, the colours fading like a
// tap (fw::st::refill_fade, shared styles), silent. Bleed: none. Contrast: tokens/colour.json section "status_block".
#include "fw.h"
#include "fx.h"

namespace {
struct SB { lv_obj_t *cap = nullptr, *val = nullptr, *line = nullptr; };
void sb_free(lv_event_t *e) { delete (SB *) lv_event_get_user_data(e); }
SB *of(lv_obj_t *r) {  // the SB rides on the root's DELETE event (fw::refill keeps event callbacks)
  for (uint32_t i = 0, n = lv_obj_get_event_count(r); i < n; i++) {
    lv_event_dsc_t *d = lv_obj_get_event_dsc(r, i);
    if (lv_event_dsc_get_cb(d) == sb_free) return (SB *) lv_event_dsc_get_user_data(d);
  }
  return nullptr;
}
const char *val_ty(const std::string &z) { return z == "hero" ? "type:hero" : z == "title" ? "type:title" : "type:value"; }
const char *line_ty(const std::string &z) { return z == "hero" ? "type:title" : z == "title" ? "type:label" : "type:body"; }
const char *cap_ink(const std::string &t) {
  return t == "accent" ? "fg:accent-text" : t == "ok" ? "fg:ok" : t == "warn" ? "fg:warn" : t == "error" ? "fg:error" : "fg:text-dim";
}
const char *val_ink(const std::string &z, const std::string &t) {
  return t == "error" ? "fg:error" : t == "off" ? "fg:text-dim" : (z == "hero" && t == "accent") ? "fg:accent-text" : "fg:text";
}
// "a · b · c", empty parts left out
std::string join(fw::Props p, std::initializer_list<const char *> keys) {
  std::string o;
  for (const char *k : keys) {
    const char *v = p[k] | "";
    if (*v) o += (o.empty() ? "" : " \xC2\xB7 ") + std::string(v);
  }
  return o;
}
std::string sig(fw::Props p) { return std::string(p["size"] | "value") + "|" + (p["tone"] | "dim"); }
void put(lv_obj_t *l, const char *type, const std::string &t) {
  if (l) fx::set_text(l, t.c_str(), type);
  if (l && t.empty() != lv_obj_has_flag(l, LV_OBJ_FLAG_HIDDEN)) lv_obj_set_flag(l, LV_OBJ_FLAG_HIDDEN, t.empty());
}
void texts(SB *b, fw::Props p) {
  // a prop left out = its meta default (an empty string = none: every spec passes caption, value and line)
  const std::string z = p["size"] | "value";
  const std::string v = p["value"].isNull() ? "Locked" : join(p, {"value", "value2"});
  const std::string l = p["line"].isNull() ? "since 08:14 \xC2\xB7 by Alex" : join(p, {"line", "line2", "line3", "line4"});
  put(b->cap, "type:label", p["caption"].isNull() ? "secure" : (p["caption"] | ""));
  fx::set_text(b->val, v.empty() ? "\xE2\x80\x94" : v.c_str(), val_ty(z));
  put(b->line, line_ty(z), l);
}
void fill(lv_obj_t *r, fw::Props p) {
  SB *b = of(r);
  const std::string z = p["size"] | "value", tone = p["tone"] | "dim";
  fw::shape(r, sig(p));
  fx::use(r, "box");
  fx::use(r, "col");
  if (z == "value") fx::use(r, "gap:2");  // the value size: space 2 between its texts (the Layout lab moves it)
  b->cap = fx::text(r, "", "type:label", cap_ink(tone));
  b->val = fx::text(r, "", val_ty(z), val_ink(z, tone));
  b->line = fx::text(r, "", line_ty(z), "fg:text-dim");
  for (lv_obj_t *l : {b->cap, b->val, b->line}) {
    fx::use(l, "fill-w");
    lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_DOTS);
  }
  if (z == "hero") {  // the hero glyphs run past the type box (no ellipsis clip); the line space 4 under it
    lv_label_set_long_mode(b->val, LV_LABEL_LONG_MODE_WRAP);
    fx::use(b->line, "above:4");
  }
  texts(b, p);
}
}  // namespace

FW_COMPONENT(status_block, "Status block", "components", R"({
  "about": "A device's state in words: a caps caption in its tone, the state as a value, one quiet line.",
  "contract": 2,
  "parts": [],
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
  lv_obj_t *r = lv_obj_create(parent);  // styles: fill() (an update refills the same root)
  lv_obj_remove_style_all(r);
  lv_obj_remove_flag(r, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  fx::name(r, "status_block");
  lv_obj_add_event_cb(r, sb_free, LV_EVENT_DELETE, new SB);
  fill(r, p);
}

// Update path: the same size + tone = texts in place; else the contents again, the colours fading (shared styles)
FW_UPDATE(status_block) {
  SB *b = of(root);
  if (!b) return false;
  if (fw::shape(root, sig(p))) { texts(b, p); return true; }
  fw::st::refill_fade(root, [&] { fw::refill(root); fill(root, p); }, true, true);
  return true;
}
