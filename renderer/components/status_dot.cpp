// Status dot: the Facet status dot (docs: @facet/ui atoms.ts statusDot(tone), CSS .f-dot + [data-tone]): a round dot in
// a status tone, the smallest way to show state (next to a label, in a chip, on a card).
//   size     lay::dot (8 at 720 = docs size dot; 480: docs 6 is off the 4 px grid -> 4, = Card's dot)
//   tones    ok / warn / error = those status tokens, working = accent, info = blue, idle = text-dim (the docs use
//            text-mute; text-mute is for disabled only, and it drops under 3:1 on the idle chip fill)
//   on       internal prop, not in the playground (Darren round 2): the Status chip passes "chip" when it builds its
//            dot, so on the chip's own tone fill working is accent-text (the accent fill drops under 3:1 on its own 14 %
//            tint in light); every other tone is the same colour. Default "canvas".
//   pulse    optional Motion pattern "pulse" (opacity breathes to motion pulse-min and back, forever; reduced motion =
//            still): working / live indicators (motion.json pattern use). Not in the docs atom.
// Static: no events, no sound. Contrast pairs: tokens/colour.json sections "status_dot" (+ "status_chip").
#include "fw.h"

namespace {
lv_color_t tone_of(const char *t, bool chip) {
  const fw::Theme &T = *fw::T;
  if (!strcmp(t, "ok")) return T.ok;
  if (!strcmp(t, "warn")) return T.warn;
  if (!strcmp(t, "error")) return T.error;
  if (!strcmp(t, "working")) return chip ? T.accent_text : T.accent;
  if (!strcmp(t, "info")) return T.blue;
  return T.text_dim;  // idle
}
void set_opa(void *o, int32_t v) { lv_obj_set_style_opa((lv_obj_t *) o, (lv_opa_t) v, 0); }
}  // namespace

namespace {
void fill(lv_obj_t *d, fw::Props p) {
  lv_obj_remove_style_all(d);
  lv_obj_remove_flag(d, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_size(d, fw::lay::dot, fw::lay::dot);
  lv_obj_set_style_radius(d, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(d, tone_of(p["tone"] | "working", !strcmp(p["on"] | "canvas", "chip")), 0);
  lv_obj_set_style_bg_opa(d, LV_OPA_COVER, 0);
  if (p["pulse"] | false) {
    fw::Anim a{set_opa, LV_OPA_COVER, (int32_t) lroundf(fw::mo::OPA_PULSE_MIN * 255), fw::mo::OPA};
    a.loop = true;
    fw::animate(d, fw::mo::pulse, a);
  }
}
}  // namespace

FW_COMPONENT(status_dot, "Status dot", "components", R"({
  "about": "A small coloured dot that shows a state: ok, warning, error, working, info or idle.",
  "frame": [96, 64],
  "props": [
    {"name": "tone", "type": "select", "options": ["ok", "warn", "error", "working", "info", "idle"], "default": "working"},
    {"name": "pulse", "type": "bool", "default": false}
  ],
  "variants": [
    {"label": "ok", "props": {"tone": "ok"}},
    {"label": "warn", "props": {"tone": "warn"}},
    {"label": "error", "props": {"tone": "error"}},
    {"label": "working", "props": {"tone": "working"}},
    {"label": "info", "props": {"tone": "info"}},
    {"label": "idle", "props": {"tone": "idle"}},
    {"label": "working · pulse", "props": {"tone": "working", "pulse": true}}
  ]
})") {
  fill(lv_obj_create(parent), p);
}

// Update path: everything snaps to the new props in the same root (fw::refill; a pulse restarts), silent.
FW_UPDATE(status_dot) {
  fw::refill(root);
  fill(root, p);
  return true;
}
