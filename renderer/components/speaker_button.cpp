// Speaker button: the Facet speaker button (docs: Components > Pickers > Speaker button, @facet/ui pickers.ts
// speakerButton(), CSS .f-spk): sits on the right of the Media header and opens the speaker picker (a Bottom sheet).
//   box      height lay::speaker_btn_h (48 / 32 = touch min, so it fits the header row), hug width on the grid,
//            surface fill + stroke::hair line border, rad::button; padding sp::s4 - hair left / right (docs space-screen
//            16 - hair at 720; 8 at 480), flex row centred, gap sp::s2 (8 / 4)
//   icon     speaker at ic::small (16 / 12 = docs speaker-btn-icon), text-dim; accent-text while playing (docs: "the
//            icon turns orange while something's playing", tone working)
//   name     component type c-label (caps, mono, 18 / 12: docs speaker-btn-font 18 / 13), text, one line; width = the
//            text rounded up to the grid, at most lay::speaker_btn_max (240 / 160), then ellipsis (docs fit8 + max)
//   chevron  chevron-down at ic::small, text-dim (docs .f-spk-chev)
// Pressed (states.json pressed, boxed control on a surface): surface-press fill + line-hi border, Motion press /
// release. Disabled (st::OFF, Button rule; not in the docs): icon, name and chevron text-mute, no press look, finger
// down = "blocked" + error. Tap = a real CLICKED: event "tap" (detail = the name) + sound open (docs data-snd open: it
// opens the speaker sheet). Contrast pairs: tokens/colour.json section "speaker_button".
#include "fw.h"

static_assert(fw::lay::speaker_btn_h <= fw::lay::row, "the speaker button must fit the header row");
static_assert(fw::lay::speaker_btn_h >= fw::touch::min, "the speaker button is a touch target");

namespace {
constexpr lv_state_t ST_OFF = fw::st::OFF;

const lv_style_prop_t TR_PROPS[] = {LV_STYLE_BG_COLOR, LV_STYLE_BORDER_COLOR, LV_STYLE_PROP_INV};
const lv_style_transition_dsc_t *tr(bool pressed) {
  static lv_style_transition_dsc_t d[2];
  static bool ready = false;
  if (!ready) { fw::st::press_transitions(d, TR_PROPS); ready = true; }  // states.json pressed motion
  return &d[pressed];
}
struct Data { std::string name; };
}  // namespace

namespace {
void fill(lv_obj_t *b, fw::Props p) {
  const fw::Theme &t = *fw::T;
  const char *st = p["state"] | "default";
  const bool off = !strcmp(st, "disabled"), playing = (p["playing"] | true) && !off;
  const lv_color_t dim = off ? fw::st::disabled_ink(t) : t.text_dim;

  lv_obj_remove_style_all(b);
  lv_obj_add_flag(b, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_STATE_TRICKLE));
  lv_obj_remove_flag(b, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(b, LV_SIZE_CONTENT, fw::lay::speaker_btn_h);
  lv_obj_set_style_pad_hor(b, fw::sp::s4 - fw::stroke::hair, 0);
  lv_obj_set_style_radius(b, fw::rad::button, 0);
  lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(b, t.surface, 0);
  lv_obj_set_style_border_width(b, fw::stroke::hair, 0);
  lv_obj_set_style_border_color(b, t.line, 0);
  lv_obj_set_style_transition(b, tr(false), 0);
  lv_obj_set_style_bg_color(b, fw::st::pressed_surface(t), LV_STATE_PRESSED);
  lv_obj_set_style_border_color(b, fw::st::pressed_line(t), LV_STATE_PRESSED);
  lv_obj_set_style_transition(b, tr(true), LV_STATE_PRESSED);
  lv_obj_set_style_bg_color(b, t.surface, ST_OFF | LV_STATE_PRESSED);  // no press look when disabled
  lv_obj_set_style_border_color(b, t.line, ST_OFF | LV_STATE_PRESSED);
  lv_obj_set_flex_flow(b, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(b, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(b, fw::sp::s2, 0);

  fw::icon(b, "speaker", fw::ic::small, playing ? t.accent_text : dim);
  const char *name = p["name"] | "";
  if (auto *d = (Data *) lv_obj_get_user_data(b)) d->name = name;  // what a tap reports
  lv_obj_t *l = fw::text(b, fw::ty::c_label, name, off ? fw::st::disabled_ink(t) : t.text);
  lv_obj_remove_flag(l, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_update_layout(l);
  const int g = fw::sp::GRID, w = std::min(fw::lay::speaker_btn_max, (int) (lv_obj_get_width(l) + g - 1) / g * g);
  lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_DOTS);
  lv_obj_set_size(l, std::max(g, w), fw::style(fw::ty::c_label).box);  // set_long_mode resets the text-box height
  fw::icon(b, "chevron-down", fw::ic::small, dim);

  if (!strcmp(st, "pressed")) lv_obj_add_state(b, LV_STATE_PRESSED);
  if (off) lv_obj_add_state(b, ST_OFF);


}
}  // namespace

FW_COMPONENT(speaker_button, "Speaker button", "components", R"({
  "about": "The header button that opens the speaker picker; its icon turns orange while something is playing.",
  "frame": [320, 96],
  "icons": ["speaker@small", "chevron-down@small"],
  "props": [
    {"name": "name", "type": "text", "default": "auto · study"},
    {"name": "playing", "type": "bool", "default": true},
    {"name": "state", "type": "select", "options": ["default", "pressed", "disabled"], "default": "default"}
  ],
  "variants": [
    {"label": "playing", "props": {}},
    {"label": "idle", "props": {"name": "kitchen", "playing": false}},
    {"label": "long name", "props": {"name": "downstairs speakers and the soundbar"}},
    {"label": "pressed", "props": {"state": "pressed"}},
    {"label": "disabled", "props": {"name": "no speakers", "playing": false, "state": "disabled"}}
  ]
})") {
  lv_obj_t *b = lv_obj_create(parent);
  auto *d = new Data;
  lv_obj_set_user_data(b, d);
  fill(b, p);
  fw::st::built(b, p);

  lv_obj_add_event_cb(b, [](lv_event_t *e) { delete (Data *) lv_event_get_user_data(e); }, LV_EVENT_DELETE, d);
  fw::st::on_tap(b, [](lv_event_t *e) {  // blocked on finger down, a real tap on CLICKED (states.json)
    if (!fw::st::tap_now(e)) return;
    auto *o = (lv_obj_t *) lv_event_get_current_target(e);
    auto *d = (Data *) lv_event_get_user_data(e);
    if (lv_obj_has_state(o, ST_OFF)) { fw::st::blocked(o, d->name.c_str()); return; }
    fw::emit(o, fw::st::TAP_EVENT, d->name.c_str());
    fw::sound(o, "open");  // states.json rest sounds: open = going deeper (it opens the speaker sheet)
  }, d);
}

// Update path: default <-> pressed = the press fade in place; any other prop builds the contents again in the same
// root (fw::refill, snap), disabled in / out fades them in (fw::st::update). Silent.
FW_UPDATE(speaker_button) { return fw::st::update(root, p, fill); }
