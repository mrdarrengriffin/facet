// Status line: one quiet line of body text with a status dot in front, on the contract (renderer/CONTRACT.md,
// "contract": 2). The small way to say what is true now under a screen's hero: "Normal 26.2p until 16:00, then 39.4p"
// (docs screens/energy.ts: the rate line, statusDot(tone) + body text-dim).
//   dot    a real Status chip at size dot (fw::find("status_chip"), C8b), in the tone
//   text   type:body, fg:text (ink dim: fg:text-dim), one line on the 4 px grid (fx::fit_grid), gap:3 after the dot (the WASM originals:
//          energy_view rate line, weather_view rain line: sp::s3, t.text)
//   box    a row, items centred on it, hugs its content
// Steps: none (it hugs). Bleed: none. States: n/a (static: no events, no sound). Update path: the words in place; a new
// tone updates the dot in place (its own update path: a fade, silent). Always a dot: a line without one would move its
// words sideways in one frame when the dot came or went.
// Contrast pairs: tokens/colour.json section "status_chip" (the dot) + text on the canvas.
#include "fw.h"
#include "fx.h"

namespace {
const char *ink(fw::Props p) { return !strcmp(p["ink"] | "text", "dim") ? "fg:text-dim" : "fg:text"; }  // dim: a quieter line (a passing check)
void dot_props(fw::Props p, ArduinoJson::JsonDocument &d) { d["tone"] = p["tone"] | "idle", d["size"] = "dot"; }
void fill(lv_obj_t *root, fw::Props p) {
  fw::shape(root, ink(p));
  for (const char *t : {"box", "row-mid", "hug", "gap:3"}) fx::use(root, t);
  ArduinoJson::JsonDocument d;
  dot_props(p, d);
  fw::find("status_chip")->build(root, d.as<fw::Props>());
  fx::fit_grid(fx::text(root, p["text"] | "", "type:body", ink(p)));
}
}  // namespace

FW_COMPONENT(status_line, "Status line", "components", R"({
  "about": "One quiet line of text with a coloured status dot in front, for what is true now.",
  "contract": 2,
  "parts": [],
  "frame": [448, 64],
  "props": [
    {"name": "text", "about": "The words of the line.", "type": "text", "default": "Normal 26.2p until 16:00, then 39.4p"},
    {"name": "tone", "about": "The dot's colour, by what the line reports: ok, warn, error, working, info or idle.", "type": "select", "options": ["ok", "warn", "error", "working", "info", "idle"], "default": "idle"},
    {"name": "ink", "about": "The words' colour: text, or dim for a quieter line, such as a check that passed beside one that failed.", "type": "select", "options": ["text", "dim"], "default": "text"}
  ],
  "variants": [
    {"label": "normal rate", "props": {}},
    {"label": "peak", "props": {"tone": "working", "text": "Peak 39.4p until 19:00, then 26.2p"}},
    {"label": "cheap", "props": {"tone": "info", "text": "Cheap 13.1p until 16:00, then 39.4p"}},
    {"label": "ok", "props": {"tone": "ok", "text": "All bins collected"}},
    {"label": "dim · a passing check", "props": {"tone": "ok", "text": "build", "ink": "dim"}}
  ]
})") {
  lv_obj_t *root = lv_obj_create(parent);
  lv_obj_remove_style_all(root);
  lv_obj_remove_flag(root, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  fx::name(root, "status_line");
  fill(root, p);
}

FW_UPDATE(status_line) {
  if (lv_obj_get_child_count(root) != 2 || !fw::shape(root, ink(p))) {  // another ink: the line again
    fw::refill(root);
    fill(root, p);
    return true;
  }
  lv_obj_t *lab = lv_obj_get_child(root, 1);
  fx::set_text(lab, p["text"] | "", "type:body");
  fx::fit_grid(lab);
  ArduinoJson::JsonDocument d;
  dot_props(p, d);
  fw::update_child(fw::find("status_chip"), lv_obj_get_child(root, 0), d.as<fw::Props>());
  return true;
}
