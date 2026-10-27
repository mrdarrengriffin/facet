// Hero block: the Facet hero block (docs: @facet/ui layout.ts heroBlock({label, role}) + set({n, label, line, accent}),
// CSS .fs-hero), on the contract (renderer/CONTRACT.md, "contract": 2). A screen's one hero: a big number with a caps
// label over one line to its right. Accent = it matters now. One per screen (design principles).
//   box      fills its parent's width (C5; in a Body that is the content width, = List row / Section header), as tall
//            as the number's type box (hero 96 / 64; role display 144 / 96): hero-row(-display), a row, top-aligned,
//            the gutter apart (16)
//   number   type hero (or display), text; accent = accent-text (docs .is-accent); its box on the grid (fx::fit_grid,
//            docs fit8), so the column after it starts on the grid
//   column   hero-col (the free width): label (type label, caps, text) over the line (type body, text-dim, one line,
//            "..."), space 2 apart, centred on the hero type box ((96 - (24 + 8 + 32)) / 2 = 16 = the docs padding-top
//            16 at 720, 12 at 480; a derived style, no literal). With role display it stays on the hero box offset
//            too, so the label / line read at the same height whichever number style is used.
//            label_type title (the Calendar hero, docs .fs-cal-htitle): hero-col-title, the label in title type as
//            written (not caps), text ((96 - (40 + 8 + 32)) / 2 = 8 = the docs padding-top 8)
// States: default; accent; unavailable (fw::st::OFF; the hero shows a device's state): number = the placeholder em
// dash in unavailable-ink, no accent, line = "Unavailable" in unavailable-note. n/a: pressed, on, disabled, loading,
// unknown, empty, error (a read-out: no press, no events, no sound, docs). Update path: the contents again in the same
// root, colours fading like a tap (fw::st::refill_fade, shared styles), silent. Bleed: none.
// Contrast pairs: tokens/colour.json section "hero_block".
#include "fw.h"
#include "fx.h"

namespace {
void fill(lv_obj_t *h, fw::Props p) {
  const bool off = !strcmp(p["state"] | "default", "unavailable");
  const bool accent = !off && (p["accent"] | false);
  const bool display = !strcmp(p["role"] | "hero", "display");
  fx::use(h, display ? "hero-row-display" : "hero-row");
  const char *n = off ? fw::st::PLACEHOLDER : (p["n"] | "");
  lv_obj_t *num = fx::text(h, n, display ? "type:display" : "type:hero",
                           off ? "fg:unavailable-ink" : accent ? "fg:accent-text" : "fg:text");
  fx::fit_grid(num);  // fit8: the box on the grid, the text at its left
  // label type: caps label (default) or title (the Calendar hero: the event's name as written, docs .fs-cal-htitle)
  const bool title = !strcmp(p["label_type"] | "label", "title");
  lv_obj_t *col = fx::box(h, "free");
  fx::use(col, title ? "hero-col-title" : "hero-col");
  fx::use(fx::text(col, p["label"] | "", title ? "type:title" : "type:label", off ? "fg:unavailable-ink" : "fg:text"), "fill-w");
  fx::use(fx::text(col, off ? fw::st::UNAVAILABLE_TEXT : (p["line"] | ""), "type:body", off ? "fg:unavailable-note" : "fg:text-dim"), "fill-w");
  if (off) fx::state(h, fw::st::OFF, true);  // the unavailable look (3:1)
}
}  // namespace

FW_COMPONENT(hero_block, "Hero block", "components", R"({
  "about": "A screen's one big number, with a label and one line beside it; accent when it matters now.",
  "contract": 2,
  "parts": [],
  "min": ["layout:card-min-w"],
  "frame": [480, 112],
  "props": [
    {"name": "n", "about": "The big number, such as how many things need you.", "type": "text", "default": "3"},
    {"name": "label", "about": "The words next to the number that say what it counts.", "type": "text", "default": "need you"},
    {"name": "line", "about": "One quieter line with the detail, under the label.", "type": "text", "default": "2 reviews · 2 assigned"},
    {"name": "accent", "about": "Draws the number in the accent colour, for when it matters now.", "type": "bool", "default": true},
    {"name": "role", "about": "hero is the largest number for a screen's main figure; display is a step smaller.", "type": "select", "options": ["hero", "display"], "default": "hero"},
    {"name": "label_type", "about": "How the words next to the number are written: label in small capitals, or title in larger plain text.", "type": "select", "options": ["label", "title"], "default": "label"},
    {"name": "state", "about": "default shows the number; unavailable shows a dash when the value cannot be read.", "type": "select", "options": ["default", "unavailable"], "default": "default"}
  ],
  "variants": [
    {"label": "accent · needs you", "props": {}},
    {"label": "quiet", "props": {"n": "12", "label": "open", "line": "3 drafts · 1 waiting on CI", "accent": false}},
    {"label": "money", "props": {"n": "£2.42", "label": "today", "line": "8.4 kWh · cheaper than yesterday", "accent": false}},
    {"label": "display role", "props": {"n": "21", "label": "inside", "line": "Feels like 19 · heating off", "accent": false, "role": "display"}},
    {"label": "title label · calendar", "props": {"n": "16:50", "label": "Pottery class", "line": "today · Larkfield studio · Family", "accent": false, "label_type": "title"}},
    {"label": "long line", "props": {"n": "7", "label": "strikes", "line": "nearest 14 km north-east, moving away at 30 km/h", "accent": true}},
    {"label": "unavailable", "props": {"n": "21", "label": "inside", "line": "", "accent": false, "state": "unavailable"}}
  ]
})") {
  lv_obj_t *h = fx::box(parent, "free");
  fx::name(h, "hero_block");
  fill(h, p);
}

// Update path: number, label, line, accent, role and unavailable in the same root (fw::refill), colours fading like a
// tap (shared styles), silent.
FW_UPDATE(hero_block) {
  fw::st::refill_fade(root, [&] { fw::refill(root); fill(root, p); }, true, true);
  return true;
}
