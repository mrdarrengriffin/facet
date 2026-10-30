// Moon phase: the Facet moon glyph (docs @facet/icons moonIcon(frac, { size, south })), built to the contract
// (renderer/CONTRACT.md) but NOT yet marked as on it (meta contract 2): in the dark theme its dark side (moon-dark) and outline
// (moon-edge) are under 3:1 against the canvas (pixel contrast 1.8 / 2.0), the original's colours; which colours
// change is Darren's call (2026-10-30). Drawn by the core atom fx::moon: disc ink:moon-dark, the lit part
// ink:moon-lit (lit limb + terminator half ellipse), outline ink:moon-edge at the weather stroke (2 px, 3 from 96);
// the inks are shared recolour styles, so a theme switch repaints it with no component code. Masks are computed at the
// exact px (no stored phase frames): a new phase redraws only the lit mask, in place. The words that go with it (the
// Moon screen, docs screens/moon.ts; layout picks how):
//   icon     the moon alone, its own size (hug)
//   column   one of the next phases (docs .fs-mn-col): the moon, then label (type:label, fg:text-dim, space 2 + gap 2
//            under the moon: above:2), then value (type:body, fg:text), gap:2, centred
//   hero     the Moon screen's moon (docs moonParts): the moon at size screen (layout:moon-hero), gap:9, then one line
//            centred: label (type:title, fg:text) " · " line (type:title, fg:text-dim, "..." when it runs out)
//   line     the hero's line alone (the next-days view: the evening now over the chart, docs rName / rRest)
//   phase    0..1 through the synodic month (0 new, 0.25 first quarter, 0.5 full, 0.75 last quarter); a bound text
//            ("0.37") works too
//   size     an icon size (docs: 24 in lists, 48 rows, 72 Home card), screen (layout:moon-hero, docs 384) or tab
//            (layout:moon-hero-tab, docs 304)
//   south    southern hemisphere (mirrored: the lit side grows from the left)
// Feed: on the Moon screen every word binds to panel.moon (core/moon_source.cpp). Static: no touch, no sound. Update
// path: phase / south redraw the lit mask in place, the words in place; a new layout, size or set of words = refilled
// (fw::st::refill_fade, silent). States: default only (a picture, no press; loading = the feed's words "Loading").
// Contrast: section "special" (moon-lit on moon-dark) + text, text-dim on bg.
#include <cstdlib>
#include "fw.h"
#include "fx.h"

namespace {
using fw::Props;
const char *layout_of(Props p) { return p["layout"] | "icon"; }
const char *size_name(Props p) { return p["size"] | (!strcmp(layout_of(p), "hero") ? "screen" : !strcmp(layout_of(p), "column") ? "feature" : "card"); }
lv_obj_t *moon(lv_obj_t *parent, Props p, float ph, bool south) {  // the glyph at its size (an icon size or a layout token)
  const char *s = size_name(p);
  return fx::moon(parent, ph, !strcmp(s, "screen") ? "layout:moon-hero" : !strcmp(s, "tab") ? "layout:moon-hero-tab" : s, south);
}
float phase_of(Props p) {
  if (p["phase"].is<const char *>()) return strtof(p["phase"].as<const char *>(), nullptr);
  const float f = p["phase"] | 0.25f;
  return f == f ? f : 0.25f;  // NaN = first quarter (C39)
}
bool south_of(Props p) {
  if (p["south"].is<const char *>()) { const char *s = p["south"].as<const char *>(); return !strcmp(s, "true") || !strcmp(s, "on") || !strcmp(s, "1"); }
  return p["south"] | false;
}
std::string sig(Props p) {
  const char *L = layout_of(p);
  std::string s = std::string(L) + "|" + size_name(p);
  if (!strcmp(L, "column")) s += std::string("|") + (*(p["label"] | "") ? "l" : "-") + (*(p["value"] | "") ? "v" : "-");
  if (!strcmp(L, "hero") || !strcmp(L, "line")) s += std::string("|") + (*(p["line"] | "") ? "r" : "-");
  return s;
}
void fill(lv_obj_t *root, Props p) {
  fw::shape(root, sig(p));
  const char *L = layout_of(p);
  const float ph = phase_of(p);
  const bool south = south_of(p);
  if (!strcmp(L, "column")) {
    for (const char *t : {"box", "col-centre", "gap:2"}) fx::use(root, t);
    moon(root, p, ph, south);
    if (*(p["label"] | "")) {  // one line, "..." when it is wider than the column (C40; docs: nowrap, it ran over)
      lv_obj_t *k = fx::text(root, p["label"] | "", "type:label", "fg:text-dim");  // docs text-mute: 3.2:1, under 4.5
      for (const char *t : {"above:2", "cell-text"}) fx::use(k, t);  // the column wide, centred
      lv_label_set_long_mode(k, LV_LABEL_LONG_MODE_DOTS);
    }
    if (*(p["value"] | "")) fx::text(root, p["value"] | "", "type:body", "fg:text");
  } else if (!strcmp(L, "hero") || !strcmp(L, "line")) {
    for (const char *t : {"box", "col-centre", "fill-w", "gap:9"}) fx::use(root, t);
    if (!strcmp(L, "hero")) moon(root, p, ph, south);
    lv_obj_t *line = fx::box(root, "row");
    for (const char *t : {"hug", "gap:2"}) fx::use(line, t);  // a word space between the two (docs: one text run)
    fx::text(line, p["label"] | "", "type:title", "fg:text");
    if (*(p["line"] | "")) {
      lv_obj_t *r = fx::text(line, (std::string("\xC2\xB7 ") + (p["line"] | "")).c_str(), "type:title", "fg:text-dim");
      lv_label_set_long_mode(r, LV_LABEL_LONG_MODE_DOTS);
    }
  } else {
    for (const char *t : {"box", "centre", "hug"}) fx::use(root, t);
    moon(root, p, ph, south);
  }
}
// every descendant in build order: the moon box (moon_icon_set takes it) and the word labels
void walk(lv_obj_t *o, Props p, std::vector<lv_obj_t *> &texts) {
  for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) {
    lv_obj_t *c = lv_obj_get_child(o, (int32_t) i);
    if (lv_obj_check_type(c, &lv_label_class)) texts.push_back(c);
    else if (!fw::moon_icon_set(c, phase_of(p), south_of(p))) walk(c, p, texts);
  }
}
}  // namespace

FW_COMPONENT(moon_icon, "Moon phase", "components", R"({
  "about": "The moon as it looks tonight: a dark disc with the lit part for any point in the month. With its words it is one of the next phases, or the moon itself at the top of the Moon screen.",
  "audit": {"snaps": {"phase": "the phase moves once a day: a redraw, nothing to animate"}},
  "frame": [480, 360],
  "props": [
    {"name": "phase", "about": "Where the moon is in its month: 0 is new, 0.25 first quarter, 0.5 full, 0.75 last quarter.", "type": "range", "min": 0, "max": 1, "step": 0.01, "default": 0.25},
    {"name": "size", "about": "How big the moon is drawn: the icon sizes, screen for the Moon screen's moon, tab a size down. Empty = card alone, feature in a column, screen as the hero.", "type": "select", "options": ["icon", "glyph", "detail", "feature", "card", "hero", "screen", "tab"], "default": "card"},
    {"name": "south", "about": "Draws the moon as seen from the southern half of the world, where the lit side is the other way round.", "type": "bool", "default": false},
    {"name": "layout", "about": "How the words sit with the moon: icon alone; column for one of the next phases (the phase and its day under it); hero for the Moon screen (the moon, then the phase and a line under it); line for that line alone.", "type": "select", "options": ["icon", "column", "hero", "line"], "default": "icon"},
    {"name": "label", "about": "The main words: the phase under the moon (column) or the phase name (hero), such as Full or Waxing gibbous.", "type": "text", "default": ""},
    {"name": "value", "about": "When it comes, under the phase (column), such as Fri or 24 Oct.", "type": "text", "default": ""},
    {"name": "line", "about": "The quieter rest of the hero line, such as 78% · full on Fri.", "type": "text", "default": ""}
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
    {"label": "first quarter · south", "props": {"phase": 0.25, "south": true}},
    {"label": "next phase · column", "props": {"layout": "column", "size": "feature", "phase": 0.5, "label": "Full", "value": "Fri"}},
    {"label": "hero · the Moon screen", "props": {"layout": "hero", "size": "tab", "phase": 0.37, "label": "Waxing gibbous", "line": "78% · full on Fri"}}
  ]
})") {
  lv_obj_t *root = lv_obj_create(parent);
  lv_obj_remove_style_all(root);
  lv_obj_remove_flag(root, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_add_flag(root, LV_OBJ_FLAG_EVENT_BUBBLE);
  fx::name(root, "moon_icon");
  fill(root, p);
}

// Update path: a new phase (or hemisphere) redraws the lit mask in place, the words in place; a new layout, size or
// set of words = refill (silent).
FW_UPDATE(moon_icon) {
  if (!fw::shape(root, sig(p))) {
    fw::st::refill_fade(root, [&] { fw::refill(root); fill(root, p); }, true, true);
    return true;
  }
  std::vector<lv_obj_t *> t;
  walk(root, p, t);
  const char *L = layout_of(p);
  if (!strcmp(L, "column")) {
    size_t i = 0;
    if (*(p["label"] | "") && i < t.size()) fx::set_text(t[i++], p["label"] | "", "type:label");
    if (*(p["value"] | "") && i < t.size()) fx::set_text(t[i++], p["value"] | "", "type:body");
  } else if ((!strcmp(L, "hero") || !strcmp(L, "line")) && !t.empty()) {
    fx::set_text(t[0], p["label"] | "", "type:title");
    if (t.size() > 1) fx::set_text(t[1], (std::string("\xC2\xB7 ") + (p["line"] | "")).c_str(), "type:title");
  }
  return true;
}
