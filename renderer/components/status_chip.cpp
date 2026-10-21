// Status chip: the Facet status chip (docs: @facet/ui atoms.ts statusChip(tone, text), CSS .f-chip): a small tinted
// pill with a status dot and caps text, e.g. "3 working". The small way to show status (rows, headers, cards).
//   box      height lay::chip_h (32 at 720 = docs; 480 24: docs 20 would put the 16 px text box at y 2, off the grid),
//            padding space 2 left / right (8 / 4), gap space 2, radius rad::chip, width hugs the content
//   fill     the tone colour at component token chip.tint (colour.json, docs opacity chip-bg 0.14) as bg_opa, so the
//            chip works on the canvas and on a card (contrast pairs test both: mixes chip-<tone>, chip-<tone>-surface)
//   dot      the Status dot component (fw::find), on "chip" (working = accent-text on its tint)
//   text     the Label component, type caps-sm, colour text. The docs colour the text in the tone; on its own tint the
//            status colours drop under 4.5:1 in light (ok 3.8, warn 3.8, error 3.8 at 14 %, still 4.2 at 6 %), so the
//            tone is carried by the dot + fill and the words stay readable
// Tones = Status dot's: ok / warn / error / working (accent) / info (blue) / idle (text-mute fill, text-dim dot).
// Static: no events, no sound. Contrast pairs: tokens/colour.json section "status_chip".
#include "fw.h"

namespace {
lv_color_t fill_of(const char *t) {
  const fw::Theme &T = *fw::T;
  if (!strcmp(t, "ok")) return T.ok;
  if (!strcmp(t, "warn")) return T.warn;
  if (!strcmp(t, "error")) return T.error;
  if (!strcmp(t, "working")) return T.accent;
  if (!strcmp(t, "info")) return T.blue;
  return T.text_mute;  // idle
}
}  // namespace

FW_COMPONENT(status_chip, "Status chip", "components", R"({
  "frame": [240, 64],
  "props": [
    {"name": "tone", "type": "select", "options": ["ok", "warn", "error", "working", "info", "idle"], "default": "working"},
    {"name": "text", "type": "text", "default": "3 working"},
    {"name": "pulse", "type": "bool", "default": false}
  ],
  "variants": [
    {"label": "ok", "props": {"tone": "ok", "text": "sys: ok"}},
    {"label": "warn", "props": {"tone": "warn", "text": "needs you"}},
    {"label": "error", "props": {"tone": "error", "text": "sys: error"}},
    {"label": "working", "props": {"tone": "working", "text": "sys: working"}},
    {"label": "info", "props": {"tone": "info", "text": "sys: info"}},
    {"label": "idle", "props": {"tone": "idle", "text": "sys: idle"}},
    {"label": "working · pulse", "props": {"tone": "working", "text": "3 working", "pulse": true}}
  ]
})") {
  const char *tone = p["tone"] | "working";
  lv_obj_t *c = lv_obj_create(parent);
  lv_obj_remove_style_all(c);
  lv_obj_remove_flag(c, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_size(c, LV_SIZE_CONTENT, fw::lay::chip_h);
  lv_obj_set_style_pad_hor(c, fw::sp::s2, 0);
  lv_obj_set_style_pad_column(c, fw::sp::s2, 0);
  lv_obj_set_style_radius(c, fw::rad::chip, 0);
  lv_obj_set_style_bg_color(c, fill_of(tone), 0);
  lv_obj_set_style_bg_opa(c, (lv_opa_t) lroundf(fw::tokens::comp::chip_tint * 255), 0);
  lv_obj_set_flex_flow(c, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(c, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  if (const fw::Component *dc = fw::find("status_dot")) {
    ArduinoJson::JsonDocument dp;
    dp["tone"] = tone;
    dp["on"] = "chip";
    dp["pulse"] = p["pulse"] | false;
    dc->build(c, dp.as<fw::Props>());
  }
  if (const fw::Component *lc = fw::find("label")) {
    ArduinoJson::JsonDocument lp;
    lp["text"] = p["text"] | "";
    lp["type"] = "caps-sm";
    lp["colour"] = "text";
    lc->build(c, lp.as<fw::Props>());
  }
}
