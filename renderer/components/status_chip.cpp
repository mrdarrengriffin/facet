// Status chip: the Facet status chip (docs: @facet/ui atoms.ts statusChip(tone, text), CSS .f-chip): a small tinted
// pill with a status dot and caps text, e.g. "3 working". The small way to show status (rows, headers, cards).
// CONTRACT v2 (Header round 3, 2026-10-27): built only from token names (core/fx.h), so a live theme switch repaints
// it (before: the colours were baked from fw::T at build, so a dark -> light restyle left the dark theme's near-white
// words on the light tint: tools/pixel_contrast.mjs measured 1.1:1).
//   box      hugs its content (C7 note: a chip is as wide as its words), tall:chip-h (exactly) (32 at 720 = docs; 480 24),
//            pad-x:2, gap:2, radius:chip
//   fill     tint:<tone colour>: the tone at chip.tint (colour.json, docs opacity chip-bg 0.14) over what is behind, so
//            the chip works on the canvas and on a card (contrast pairs test both: mixes chip-<tone>, chip-<tone>-surface)
//   dot      w:dot round, bg:<tone>; on its own tint working is accent-text (the accent drops under 3:1 on its tint in
//            light); prop pulse = the Motion pulse (fx::pulse)
//   text     type:c-caps-sm, fg:text (the docs colour it in the tone; on its own tint the status colours drop under
//            4.5:1 in light, so the tone is carried by the dot + fill and the words stay readable). One line, its box fitted up to the 4 px grid.
// Tones: ok / warn / error / working (accent) / info (blue) / idle (text-dim). Changes against a reference ("▲ 12% vs
// yday") are plain Status chips: fw::chart::delta() (core/chart.h) gives the tone + words.
// Steps: none (it hugs; a parent narrower than its words is the bounds proof's to report). Bleed: none. States: n/a
// (static: no events, no sound; the tone is its state). Update path: the words in place (fx::set_text); a new tone or
// pulse = the contents again, faded in (fw::st::refill_fade, silent). Contrast pairs: colour.json "status_chip".
#include "fw.h"
#include "fx.h"

namespace {
const char *fill_of(const char *t) {
  if (!strcmp(t, "ok")) return "tint:ok";
  if (!strcmp(t, "warn")) return "tint:warn";
  if (!strcmp(t, "error")) return "tint:error";
  if (!strcmp(t, "working")) return "tint:accent";
  if (!strcmp(t, "info")) return "tint:blue";
  return "tint:text-dim";  // idle (text-mute is for disabled only, Darren 2026-10-21)
}
const char *dot_of(const char *t) {
  if (!strcmp(t, "ok")) return "bg:ok";
  if (!strcmp(t, "warn")) return "bg:warn";
  if (!strcmp(t, "error")) return "bg:error";
  if (!strcmp(t, "working")) return "bg:accent-text";
  if (!strcmp(t, "info")) return "bg:blue";
  return "bg:text-dim";
}
std::string sig(fw::Props p) { return std::string(p["tone"] | "working") + ((p["pulse"] | false) ? "|p" : ""); }
void fill(lv_obj_t *c, fw::Props p) {
  const char *tone = p["tone"] | "working";
  lv_obj_remove_style_all(c);
  lv_obj_remove_flag(c, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  fw::shape(c, sig(p), "chip");
  for (const char *t : {"box", "row", "hug", "tall:chip-h", "pad-x:2", "gap:2", "radius:chip"}) fx::use(c, t);
  fx::use(c, fill_of(tone));
  lv_obj_t *d = fx::box(c, "free");
  for (const char *t : {"w:dot", "h:dot", "radius:round"}) fx::use(d, t);
  fx::use(d, dot_of(tone));
  if (p["pulse"] | false) fx::pulse(d);
  lv_obj_t *l = fx::text(c, p["text"] | "", "type:c-caps-sm", "fg:text");
  fx::fit_grid(l);  // the chip's width on the 4 px grid (the Label rule, docs fit8)
}
}  // namespace

FW_COMPONENT(status_chip, "Status chip", "components", R"({
  "about": "A small tinted pill with a status dot and a short caps label.",
  "contract": 2,
  "parts": [],
  "min": ["layout:chip-h", "layout:chip-h"],
  "frame": [240, 64],
  "props": [
    {"name": "tone", "about": "The chip's colour, by what it reports: ok, warn, error, working, info or idle.", "type": "select", "options": ["ok", "warn", "error", "working", "info", "idle"], "default": "working"},
    {"name": "text", "about": "The short words on the chip.", "type": "text", "default": "3 working"},
    {"name": "pulse", "about": "Makes the dot pulse gently, for something happening now.", "type": "bool", "default": false}
  ],
  "variants": [
    {"label": "ok", "props": {"tone": "ok", "text": "sys: ok"}},
    {"label": "warn", "props": {"tone": "warn", "text": "needs you"}},
    {"label": "error", "props": {"tone": "error", "text": "sys: error"}},
    {"label": "working", "props": {"tone": "working", "text": "sys: working"}},
    {"label": "info", "props": {"tone": "info", "text": "sys: info"}},
    {"label": "idle", "props": {"tone": "idle", "text": "sys: idle"}},
    {"label": "working · pulse", "props": {"tone": "working", "text": "3 working", "pulse": true}},
    {"label": "delta · down, down is good", "props": {"tone": "ok", "text": "▼ 12% vs yday"}},
    {"label": "delta · up, up is bad", "props": {"tone": "warn", "text": "▲ 18% vs yday"}},
    {"label": "delta · up, up is good", "props": {"tone": "ok", "text": "▲ 18% vs yday"}},
    {"label": "delta · flat", "props": {"tone": "idle", "text": "= 0% vs yday"}}
  ]
})") {
  lv_obj_t *c = lv_obj_create(parent);
  fx::name(c, "status_chip");
  fill(c, p);
}

FW_UPDATE(status_chip) {
  lv_obj_t *lab = lv_obj_get_child(root, 1);
  if (lab && fw::shape(root, sig(p), "chip")) {  // the words in place
    fx::set_text(lab, p["text"] | "", "type:c-caps-sm");
    fx::fit_grid(lab);
    return true;
  }
  fw::st::refill_fade(root, [&] { fw::refill(root); fill(root, p); }, true, true);  // a new tone fades in, like a tap
  return true;
}
