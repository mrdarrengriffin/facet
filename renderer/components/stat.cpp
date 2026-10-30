// Stat: the Facet stat (docs: @facet/ui layout.ts stat(label) + set(value, tone, label), CSS .fs-stat). A caps label
// over one line of body text: "trend / 48 km/h", "lit / 63%". Screens lay stats out in columns; a stat fills the width
// its parent gives it.
//   box      flex column, gap space 2: label box + 8 + title box = 24 + 8 + 40 = 72 at 720, 16 + 4 + title 1x box
//            at 480; contract (C5): fills its parent's width, so the value ellipsises in its column (a caller that
//            wants Home columns gives each Stat a card-w cell; the old internal prop "fill" is ignored)
//   label    screen type label (caps), text-dim (docs text-mute: disabled only here, the contrast rule)
//   value    screen type title (heavier than the docs body: the value is what you read; prop size body = the docs body, a
//            narrow row of facts such as the Flight radar hero), one line, ellipsis; tone dim (default, docs) = text-dim, text = text (docs moon facts
//            pass 'text'), warn = warn, ok = ok
// Unavailable (states.json; a stat often shows one device value): label stays text-dim (4.5:1), value = "Unavailable" in
// st::unavailable_note. Static: no press, no events, no sound (docs). The docs explorer sample is 240 px wide. Contrast pairs: tokens/colour.json section "stat".
#include "fw.h"
#include "fx.h"

namespace {
const char *tone(const char *t) {
  return !strcmp(t, "text") ? "fg:text" : !strcmp(t, "warn") ? "fg:warn" : !strcmp(t, "ok") ? "fg:ok" : "fg:text-dim";
}
void line(lv_obj_t *s, const char *type, const char *txt, const char *fg) {
  lv_obj_t *l = fx::text(s, txt, type, fg);
  fx::use(l, "fill-w");
  lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_DOTS);
}
void fill(lv_obj_t *s, fw::Props p) {
  const bool off = !strcmp(p["state"] | "default", "unavailable");
  for (const char *t : {"box", "col", "fill-w", "gap:2"}) fx::use(s, t);
  fx::name(s, "stat");
  line(s, "type:label", p["label"] | "", "fg:text-dim");  // unavailable keeps the label readable (4.5:1, C15); the value says so
  line(s, !strcmp(p["size"] | "title", "body") ? "type:body" : "type:title", off ? fw::st::UNAVAILABLE_TEXT : (p["value"] | ""), off ? "fg:unavailable-note" : tone(p["tone"] | "dim"));
}
}  // namespace

FW_COMPONENT(stat, "Stat", "components", R"({
  "about": "A caps label over one line of text: one fact in a column of facts.",
  "contract": 2,
  "parts": [],
  "min": ["layout:card-min-w"],
  "frame": [240, 64],
  "props": [
    {"name": "label", "about": "The small capitals line that says what the fact is, such as trend.", "type": "text", "default": "trend"},
    {"name": "value", "about": "The fact itself, such as 48 km/h.", "type": "text", "default": "48 km/h"},
    {"name": "tone", "about": "The colour of the value: dim for quiet, text for normal, warn for something to watch, ok for good news.", "type": "select", "options": ["dim", "text", "warn", "ok"], "default": "dim"},
    {"name": "size", "about": "title is the usual value; body is a smaller value, for a row of facts in a narrow column (the Flight radar's hero, docs .fs-stat).", "type": "select", "options": ["title", "body"], "default": "title"},
    {"name": "state", "debug": true, "about": "default shows the value; unavailable shows a dash when it cannot be read.", "type": "select", "options": ["default", "unavailable"], "default": "default"}
  ],
  "variants": [
    {"label": "dim · default", "props": {}},
    {"label": "text", "props": {"label": "lit", "value": "63%", "tone": "text"}},
    {"label": "warn", "props": {"label": "gusts", "value": "71 km/h", "tone": "warn"}},
    {"label": "ok", "props": {"label": "backup", "value": "done 04:12", "tone": "ok"}},
    {"label": "long value", "props": {"label": "next", "value": "Bin day tomorrow: recycling and garden waste", "tone": "text"}},
    {"label": "body size", "props": {"label": "distance", "value": "26 km SE", "size": "body"}},
    {"label": "unavailable", "props": {"label": "outside", "value": "12°", "state": "unavailable"}}
  ]
})") {
  lv_obj_t *s = lv_obj_create(parent);  // styles: fill() (an update refills the same root)
  lv_obj_remove_style_all(s);
  lv_obj_remove_flag(s, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  fill(s, p);
}

// Update path: label, value, tone and unavailable snap in the same root (fw::refill), silent.
FW_UPDATE(stat) {
  fw::st::refill_fade(root, [&] { fw::refill(root); fill(root, p); }, true, true);  // a new tone / mark fades (an update animates like a tap)
  return true;
}
