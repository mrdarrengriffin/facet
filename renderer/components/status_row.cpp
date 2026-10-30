// Status row: a hairline row of a list that opens things (docs @facet/ui layout.ts row(), CSS .fs-row: the GitHub
// screen's pull requests, the Overhead list), on the contract (renderer/CONTRACT.md, "contract": 2). What a List row
// can't draw: a status (its dot + word) at the end of the small line above the title.
//   box      fills its parent's width (C5), h:gh-row-h (112 / 76), a hairline on top (rule-top, line:line); the meta
//            line h:gh-row-top under the hairline (docs padding-top 24)
//   top      a row, items centred, gap:gutter: the label (type:label caps, text-dim, the free width, "..."; docs
//            text-mute, which is disabled-only here: C21), then the status: a real Status chip at size dot (C8b) in the
//            tone, space 2, its word (type:label caps, fg:text; docs the tone's colour, which drops under 4.5:1 on the
//            canvas in light: the dot carries the tone). No status = no dot, no word
//   title    above:2 (docs margin-top space-2), type:title, fg:text, one line, "..."
// Touch: none of its own (the Grid cell around it takes the tap: pick = the item's id, pressed = the cell's). Static:
// no sound. States: n/a (pressed is the Grid cell's; the data's empty / error = the screen's words). Update path: the
// words in place, a new tone = the dot's own update (a fade); status in or out = made again (faded like a tap). Silent.
// Contrast: colour.json section "status_row" + the Status chip's dot.
#include "fw.h"
#include "fx.h"

namespace {
using Doc = ArduinoJson::JsonDocument;
bool has_status(fw::Props p) { return *(p["status"] | ""); }
void dot_props(fw::Props p, Doc &d) {
  static const char *const OK[] = {"ok", "warn", "error", "working", "info", "idle"};
  const char *t = p["tone"] | "idle";
  d["tone"] = "idle";
  for (const char *k : OK) if (!strcmp(t, k)) d["tone"] = k;  // feed data never aborts the chip
  d["size"] = "dot";
}
void fill(lv_obj_t *root, fw::Props p) {
  fw::shape(root, has_status(p) ? "s" : "-");
  for (const char *t : {"box", "col", "line:line", "rule-top", "h:gh-row-h"}) fx::use(root, t);
  fx::use(fx::box(root, "free"), "h:gh-row-top");
  lv_obj_t *top = fx::box(root, "row-mid");
  fx::use(top, "gap:gutter");
  lv_obj_t *lb = fx::text(top, p["label"] | "", "type:label", "fg:text-dim");
  fx::use(lb, "grow");
  lv_label_set_long_mode(lb, LV_LABEL_LONG_MODE_DOTS);  // one line, then "..."
  if (has_status(p)) {
    lv_obj_t *st = fx::box(top, "row-mid");
    for (const char *t : {"hug", "gap:2"}) fx::use(st, t);
    Doc d;
    dot_props(p, d);
    fw::find("status_chip")->build(st, d.as<fw::Props>());
    fx::text(st, p["status"] | "", "type:label", "fg:text");
  }
  lv_obj_t *ti = fx::text(root, p["title"] | "", "type:title", "fg:text");
  for (const char *t : {"fill-w", "above:2"}) fx::use(ti, t);
  lv_label_set_long_mode(ti, LV_LABEL_LONG_MODE_DOTS);
}
}  // namespace

FW_COMPONENT(status_row, "Status row", "components", R"({
  "about": "A row of a list that opens things: a small line with a status at its end, over a title, such as a pull request.",
  "contract": 2,
  "parts": [],
  "frame": [448, 0],
  "props": [
    {"name": "label", "about": "The small line above the title, such as the repository, number and age.", "type": "text", "default": "lantern · #412 · 26h"},
    {"name": "title", "about": "The main words of the row, one line.", "type": "text", "default": "Add a quiet hours schedule to the notification router"},
    {"name": "status", "about": "A short status word at the end of the small line, such as review or ci fail. Leave empty for none.", "type": "text", "default": "review"},
    {"name": "tone", "about": "The status dot's colour: ok, warn, error, working, info or idle.", "type": "select", "options": ["ok", "warn", "error", "working", "info", "idle"], "default": "warn"}
  ],
  "variants": [
    {"label": "needs a review", "props": {}},
    {"label": "ci fail", "props": {"label": "seedling · #1290 · 3d", "title": "Fix the sunrise time drifting by a minute each week", "status": "ci fail", "tone": "error"}},
    {"label": "approved", "props": {"label": "lantern · #409 · 2d", "title": "Docs: getting started on a Raspberry Pi", "status": "approved", "tone": "ok"}},
    {"label": "no status", "props": {"label": "tidepool · #37 · 2w", "title": "Experiment: ambient screen with drifting dots", "status": ""}}
  ]
})") {
  lv_obj_t *root = fx::box(parent, "col");
  fx::name(root, "status_row");
  fill(root, p);
}

FW_UPDATE(status_row) {
  if (!fw::shape(root, has_status(p) ? "s" : "-")) {  // a status in or out: made again, faded like a tap
    fw::st::refill_fade(root, [&] { fw::refill(root); fill(root, p); }, true, true);
    return true;
  }
  lv_obj_t *top = lv_obj_get_child(root, 1);
  fx::set_text(lv_obj_get_child(top, 0), p["label"] | "", "type:label");
  if (has_status(p)) {
    lv_obj_t *st = lv_obj_get_child(top, 1);
    Doc d;
    dot_props(p, d);
    fw::update_child(fw::find("status_chip"), lv_obj_get_child(st, 0), d.as<fw::Props>());
    fx::set_text(lv_obj_get_child(st, 1), p["status"] | "", "type:label");
  }
  fx::set_text(lv_obj_get_child(root, 2), p["title"] | "", "type:title");
  return true;
}
