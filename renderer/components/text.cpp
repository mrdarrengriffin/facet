// Text: words that wrap, on the contract (renderer/CONTRACT.md, "contract": 2): a headline, a paragraph of body text or
// a small label, as many lines as it declares (1 to 4) and never more.
//   box      a column that fills its parent's width (C5) round one fx::text, exactly `lines` line boxes of its type tall, in the live theme's font
//            (fx::lines; a theme changes only the font inside them, C45)
//   text     style headline (type headline), body (type body) or label (type label, caps); tone default = fg:text,
//            dim = fg:text-dim. It wraps; what does not fit ends in "..." on the last line (one line = dot, C40)
// Steps: none (the line count is declared; a narrower parent wraps sooner). Bleed: none. States: n/a (static text, no
// events, no sound). Update path: the words in place (same object, silent); a new tone fades in (refill_fade);
// another style or line count = rebuilt (false). Contrast pairs: tokens/colour.json section "text".
#include "fw.h"
#include "fx.h"

namespace {
const char *type_of(fw::Props p) {
  const char *s = p["style"] | "body";
  return !strcmp(s, "headline") ? "type:headline" : !strcmp(s, "label") ? "type:label" : "type:body";
}
std::string sig(fw::Props p) { return std::string(p["style"] | "body") + "|" + std::to_string(p["lines"] | 2); }
const char *ink_of(fw::Props p) { return !strcmp(p["tone"] | "default", "dim") ? "fg:text-dim" : "fg:text"; }
void fill(lv_obj_t *root, fw::Props p) {
  const int n = LV_CLAMP(1, (int) (p["lines"] | 2), 4);
  fx::lines(fx::text(root, p["text"] | "", type_of(p), ink_of(p), n == 1 ? "dot" : "fit"), type_of(p), n);
}
}  // namespace

FW_COMPONENT(text, "Text", "components", R"({
  "about": "Words that wrap: a headline, a paragraph or a small label, up to the number of lines it declares. Longer words end in an ellipsis on the last line.",
  "contract": 2,
  "parts": [],
  "frame": [480, 160],
  "props": [
    {"name": "text", "about": "The words to show.", "type": "text", "default": "Scan the code with your phone to join this panel's own Wi-Fi."},
    {"name": "style", "about": "headline for a screen's big line, body for a paragraph, label for a small line in capitals.", "type": "select", "options": ["body", "headline", "label"], "default": "body"},
    {"name": "tone", "about": "default is the main text colour; dim is the quieter one for explanations.", "type": "select", "options": ["default", "dim"], "default": "default"},
    {"name": "lines", "about": "How many lines it may take, from 1 to 4. It is always that tall; words that do not fit end in an ellipsis.", "type": "range", "min": 1, "max": 4, "step": 1, "default": 2}
  ],
  "variants": [
    {"label": "body · 2 lines", "props": {}},
    {"label": "headline · 1 line", "props": {"style": "headline", "text": "Add me in Home Assistant", "lines": 1}},
    {"label": "body · dim · 3 lines", "props": {"tone": "dim", "lines": 3, "text": "Open Home Assistant on your phone. It finds this panel by itself and asks to add it; tap Add, then pick the area it is in."}},
    {"label": "label · 1 line", "props": {"style": "label", "tone": "dim", "lines": 1, "text": "facet-panel-a1b2 · 192.168.4.1"}},
    {"label": "long · cut on line 2", "props": {"text": "Every word here will not fit in two lines, so the last line ends in an ellipsis instead of growing the box past what it declares."}}
  ]
})") {
  lv_obj_t *root = fx::box(parent, "col");
  fx::name(root, "text");
  fw::shape(root, sig(p));
  fw::shape(root, ink_of(p), "tone");
  fill(root, p);
}

FW_UPDATE(text) {
  if (!fw::shape(root, sig(p))) return false;  // another style or line count: rebuilt
  lv_obj_t *l = lv_obj_get_child(root, 0);
  if (!l || !fx::lines_of(l)) return false;
  if (fw::shape(root, ink_of(p), "tone")) fx::set_text(l, p["text"] | "", type_of(p));
  else fw::st::refill_fade(root, [&] { fw::refill(root); fill(root, p); }, true, true);  // a new tone fades
  return true;
}
