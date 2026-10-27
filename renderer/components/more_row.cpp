// More row: the Facet "more" row (docs: @facet/ui layout.ts moreRow(label, onTap) + set(text), CSS .fs-more), on the
// contract (renderer/CONTRACT.md, "contract": 2). Ends a list that shows only its top items and opens the rest: "all
// pull requests · 3 more".
//   size     fills its parent's width (C5; in a list Body = inset to inset, = List row), more-row tall (more-row-h 64 / 44,
//            top space 7 less the hairline: the label at 24 / 16 from the top like the docs rows), pad-x:4 (16 / 8) = List
//            row, so the label lines up with the rows above and the press fill does not run into the text; hairline on top
//   minimum  layout:card-min-w x layout:more-row-h. Steps: one (the label ends in "...")
//   label    type label (caps), text-dim, one line, grows
//   chevron  icon chevron at the icon size (24 / 16), a gutter after the label, top-aligned with it, text-dim
// States: default; pressed (fill pressed-tint fading in over nothing, = List row); disabled (fw::st::OFF: disabled-ink,
// no press look, tap = "blocked" + error on finger down). n/a: on, unavailable, loading, unknown, empty, error (a list's
// own row: the list says those). Themed through the list-row part (C46). Tap = a real CLICKED: "tap" (detail = the label)
// + sound "open" (it opens the full list). Bleed: none. Contrast pairs: tokens/colour.json section "more_row" (+ list_row).
#include "fw.h"
#include "fx.h"

namespace {
constexpr lv_state_t ST_OFF = fw::st::OFF;

void fill(lv_obj_t *r, fw::Props p) {
  const char *state = p["state"] | "default";
  for (const char *t : {"box", "row-top", "fill-w", "more-row", "pad-x:4", "gap:gutter", "line:line", "rule-top",
                        "bg:pressed-tint", "bg-none", "fade-release-fill"}) fx::use(r, t);
  fx::use(r, "bg:pressed-tint", LV_STATE_PRESSED);
  fx::use(r, "fade-press-fill", LV_STATE_PRESSED);
  fx::use(r, "bg-none", ST_OFF);  // no press look when disabled
  fx::use(r, "bg-none", ST_OFF | LV_STATE_PRESSED);
  fx::use(r, "part:list-row");  // the theme's list row (C46), after the row's own state tokens
  fx::name(r, "more_row");
  lv_obj_add_flag(r, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(r, LV_OBJ_FLAG_STATE_TRICKLE);  // the press reaches the label and chevron (the theme's pressed ink)

  lv_obj_t *l = fx::text(r, p["text"] | "", "type:label", "fg:text-dim");
  fx::use(l, "grow-w");
  fx::use(l, "fg:disabled-ink", ST_OFF);
  lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_DOTS);
  fx::icon_use(fx::icon(r, "chevron", "icon", "fg:text-dim"), "ink:disabled-ink", ST_OFF);

  if (!strcmp(state, "pressed")) lv_obj_add_state(r, LV_STATE_PRESSED);
  if (!strcmp(state, "disabled")) fx::state(r, ST_OFF, true);
}
}  // namespace

FW_COMPONENT(more_row, "More row", "components", R"({
  "about": "The last row of a short list: a caps label and a chevron that open the rest.",
  "contract": 2,
  "parts": ["list-row"],
  "min": ["layout:card-min-w", "layout:more-row-h"],
  "frame": [480, 64],
  "icons": ["chevron@icon"],
  "props": [
    {"name": "text", "about": "The words of the row, such as how many more items there are.", "type": "text", "default": "all pull requests · 3 more"},
    {"name": "state", "about": "How the row looks: default ready to tap, pressed while held, or disabled when there is nothing more to open.", "type": "select", "options": ["default", "pressed", "disabled"], "default": "default"}
  ],
  "variants": [
    {"label": "default", "props": {}},
    {"label": "pressed", "props": {"state": "pressed"}},
    {"label": "disabled", "props": {"text": "all notifications · 0 more", "state": "disabled"}},
    {"label": "long label", "props": {"text": "every automation that ran in the last 24 hours · 128 more"}}
  ]
})") {
  lv_obj_t *r = lv_obj_create(parent);
  lv_obj_remove_style_all(r);
  lv_obj_remove_flag(r, LV_OBJ_FLAG_SCROLLABLE);
  fill(r, p);
  fw::st::built(r, p);

  fw::st::on_tap(r, [](lv_event_t *e) {  // blocked on finger down, a real tap on CLICKED (states.json)
    if (!fw::st::tap_now(e)) return;
    auto *o = (lv_obj_t *) lv_event_get_current_target(e);
    const char *d = lv_label_get_text(lv_obj_get_child(o, 0));  // the label, looked up (updates make it again)
    if (lv_obj_has_state(o, ST_OFF)) fw::st::blocked(o, d);
    else { fw::emit(o, "tap", d); fw::sound(o, "open"); }
  }, nullptr);
}

// Update path: default <-> pressed = the press fade in place; any other prop builds the contents again in the same
// root (fw::refill, snap), disabled in / out fades them in (fw::st::update). Silent.
FW_UPDATE(more_row) { return fw::st::update(root, p, fill); }
