// Button: the Facet text button (docs: Components > Buttons > Button, @facet/ui button()). One height (touch
// button-h: 64 at 720, 44 at 480), four kinds, four states:
//   primary    accent fill, on-accent label; pressed = accent-press; active = 1 px accent focus ring outside
//   secondary  surface + 1 px line border; pressed = surface-press + line-hi; active = line-hi + accent corner ticks
//   outline    1 px accent border, accent-text label; pressed = accent-dim fill + text label; active = filled accent
//              (active + pressed = accent-press, like primary)
//   ghost      label only (text-dim); pressed = surface-press + text; active = accent-text + a short accent underline
// disabled: text-mute label (primary: accent-dim fill + text-dim label; outline: accent-dim border), no press look;
// a tap plays Error. Every label / icon colour pair is declared in tokens/colour.json contrast.pairs (section
// "button") and checked in every theme x accent set by tools/contrast.mjs: change a look here -> update the pair.
// Optional icon before the label (fw::icon, Icons foundation, at the icon size token). Width hugs the label rounded up to the grid, or a
// fixed width with the label centred. Tap = fw::emit "tap" + fw::sound "tap" (disabled: "blocked" + "error" on finger down,
// states.json; the focus ring hides when disabled).
// Press feedback = the Motion patterns press (into the pressed look) and release (back out), colour transitions from
// tokens/motion.json (80 ms ease-out each; reduced motion keeps them: colour only, nothing moves).
#include "fw.h"

namespace {
// "disabled" is drawn with USER_1, not LV_STATE_DISABLED: LVGL sends no events to a disabled object, and Facet
// plays Error when a disabled button is pressed. USER_1 outranks PRESSED, so a disabled button shows no press look.
constexpr lv_state_t ST_OFF = fw::st::OFF;

struct Look { lv_color_t bg; lv_opa_t bg_opa; lv_color_t bd; lv_opa_t bd_opa; lv_color_t fg; };
struct Kind { Look normal, pressed, active, active_pressed, off; };

// state colours = the roles of tokens/states.json (fw::st::<state>_<role>), resting looks = colour tokens
Kind kind_of(const char *k) {
  using namespace fw::st;
  const fw::Theme &t = *fw::T;
  const lv_opa_t C = LV_OPA_COVER, N = LV_OPA_TRANSP;
  if (!strcmp(k, "secondary"))
    return {{t.surface, C, t.line, C, t.text}, {pressed_surface(t), C, pressed_line(t), C, t.text},
            {t.surface, C, on_line(t), C, t.text}, {pressed_surface(t), C, on_line(t), C, t.text},
            {t.surface, C, t.line, C, disabled_ink(t)}};
  if (!strcmp(k, "outline"))
    return {{t.bg, N, t.accent, C, t.accent_text}, {pressed_outline(t), C, t.accent, C, pressed_ink(t)},
            {on_fill(t), C, on_fill(t), C, on_label(t)}, {pressed_accent(t), C, pressed_accent(t), C, on_label(t)},
            {t.bg, N, disabled_accent(t), C, disabled_ink(t)}};
  if (!strcmp(k, "ghost"))
    return {{t.bg, N, t.bg, N, t.text_dim}, {pressed_surface(t), C, t.bg, N, pressed_ink(t)},
            {t.bg, N, t.bg, N, on_ink(t)}, {pressed_surface(t), C, t.bg, N, on_ink(t)},
            {t.bg, N, t.bg, N, disabled_ink(t)}};
  // primary
  return {{t.accent, C, t.bg, N, t.on_accent}, {pressed_accent(t), C, t.bg, N, t.on_accent},
          {t.accent, C, t.bg, N, t.on_accent}, {pressed_accent(t), C, t.bg, N, t.on_accent},
          {disabled_accent(t), C, t.bg, N, disabled_on_fill(t)}};
}

const lv_style_prop_t TR_PROPS[] = {LV_STYLE_BG_COLOR, LV_STYLE_BG_OPA, LV_STYLE_BORDER_COLOR, LV_STYLE_BORDER_OPA,
                                    LV_STYLE_TEXT_COLOR, LV_STYLE_IMAGE_RECOLOR, LV_STYLE_PROP_INV};
// LVGL runs the transition of the state being entered: pressed looks take press, the others release
const lv_style_transition_dsc_t *tr(bool pressed) {
  static lv_style_transition_dsc_t d[2];
  static bool ready = false;
  if (!ready) { fw::st::press_transitions(d, TR_PROPS); ready = true; }  // states.json pressed motion
  return &d[pressed];
}

// the five looks on their selectors (CHECKED = active; CHECKED|PRESSED outranks PRESSED; USER_1 = disabled wins)
const lv_style_selector_t SEL[5] = {0, LV_STATE_PRESSED, LV_STATE_CHECKED, LV_STATE_CHECKED | LV_STATE_PRESSED, ST_OFF};
const bool PRESSED_SEL[5] = {false, true, false, true, false};
const Look &look(const Kind &k, int i) {
  const Look *l[5] = {&k.normal, &k.pressed, &k.active, &k.active_pressed, &k.off};
  return *l[i];
}
void box_looks(lv_obj_t *b, const Kind &k) {
  for (int i = 0; i < 5; i++) {
    const Look &l = look(k, i);
    lv_obj_set_style_bg_color(b, l.bg, SEL[i]);
    lv_obj_set_style_bg_opa(b, l.bg_opa, SEL[i]);
    lv_obj_set_style_border_color(b, l.bd, SEL[i]);
    lv_obj_set_style_border_opa(b, l.bd_opa, SEL[i]);
    lv_obj_set_style_transition(b, tr(PRESSED_SEL[i]), SEL[i]);
  }
}
// the label follows the button's state (STATE_TRICKLE)
void fg_looks(lv_obj_t *o, const Kind &k) {
  for (int i = 0; i < 5; i++) {
    lv_obj_set_style_text_color(o, look(k, i).fg, SEL[i]);
    lv_obj_set_style_transition(o, tr(PRESSED_SEL[i]), SEL[i]);
  }
}

// icon before the label: the Facet master at the icon size token (ic::icon = touch icon, 24 at 720, 16 at 480), A8
// mask from the Icons foundation; its ink follows the label colour in every state (same pairs as the label)
lv_obj_t *icon(lv_obj_t *parent, const char *id, const Kind &k) {
  lv_obj_t *o = fw::icon(parent, id, fw::ic::icon, k.normal.fg);
  for (int i = 0; o && i < 5; i++) fw::icon_colour(o, look(k, i).fg, SEL[i], tr(PRESSED_SEL[i]));
  return o;
}
// the ghost underline, shown only while active (CHECKED); secondary's corner ticks = core atom fw::corner_ticks
lv_obj_t *underline(lv_obj_t *b, int w, int h) {
  lv_obj_t *o = lv_obj_create(b);
  lv_obj_remove_style_all(o);
  lv_obj_add_flag(o, LV_OBJ_FLAG_IGNORE_LAYOUT);
  lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_size(o, w, h);
  lv_obj_set_style_bg_color(o, fw::st::on_mark(*fw::T), 0);
  lv_obj_set_style_bg_opa(o, LV_OPA_TRANSP, 0);
  lv_obj_set_style_bg_opa(o, LV_OPA_COVER, LV_STATE_CHECKED);
  return o;
}
}  // namespace

FW_COMPONENT(button, "Button", "components", R"({
  "frame": [240, 112],
  "icons": ["plus@icon", "check@icon", "close@icon"],
  "props": [
    {"name": "kind", "type": "select", "options": ["primary", "secondary", "outline", "ghost"], "default": "primary"},
    {"name": "label", "type": "text", "default": "Start"},
    {"name": "state", "type": "select", "options": ["default", "pressed", "active", "active-pressed", "disabled"], "default": "default"},
    {"name": "icon", "type": "select", "options": ["none", "plus", "check", "close"], "default": "none"},
    {"name": "width", "type": "range", "min": 0, "max": 224, "step": 8, "default": 0}
  ],
  "variants": [
    {"label": "primary · default", "props": {}},
    {"label": "primary · pressed", "props": {"state": "pressed"}},
    {"label": "primary · active", "props": {"state": "active"}},
    {"label": "primary · active + pressed", "props": {"state": "active-pressed"}},
    {"label": "primary · disabled", "props": {"state": "disabled"}},
    {"label": "secondary · default", "props": {"kind": "secondary", "label": "Cancel"}},
    {"label": "secondary · pressed", "props": {"kind": "secondary", "label": "Cancel", "state": "pressed"}},
    {"label": "secondary · active", "props": {"kind": "secondary", "label": "Cancel", "state": "active"}},
    {"label": "secondary · active + pressed", "props": {"kind": "secondary", "label": "Cancel", "state": "active-pressed"}},
    {"label": "secondary · disabled", "props": {"kind": "secondary", "label": "Cancel", "state": "disabled"}},
    {"label": "outline · default", "props": {"kind": "outline", "label": "Save"}},
    {"label": "outline · pressed", "props": {"kind": "outline", "label": "Save", "state": "pressed"}},
    {"label": "outline · active", "props": {"kind": "outline", "label": "Save", "state": "active"}},
    {"label": "outline · active + pressed", "props": {"kind": "outline", "label": "Save", "state": "active-pressed"}},
    {"label": "outline · disabled", "props": {"kind": "outline", "label": "Save", "state": "disabled"}},
    {"label": "ghost · default", "props": {"kind": "ghost", "label": "More"}},
    {"label": "ghost · pressed", "props": {"kind": "ghost", "label": "More", "state": "pressed"}},
    {"label": "ghost · active", "props": {"kind": "ghost", "label": "More", "state": "active"}},
    {"label": "ghost · active + pressed", "props": {"kind": "ghost", "label": "More", "state": "active-pressed"}},
    {"label": "ghost · disabled", "props": {"kind": "ghost", "label": "More", "state": "disabled"}},
    {"label": "primary · icon", "props": {"label": "Add", "icon": "plus"}},
    {"label": "secondary · icon", "props": {"kind": "secondary", "label": "Done", "icon": "check"}},
    {"label": "outline · icon", "props": {"kind": "outline", "label": "Clear", "icon": "close"}},
    {"label": "primary · fixed width", "props": {"width": 192}}
  ]
})") {
  const char *kn = p["kind"] | "primary";
  const char *state = p["state"] | "default";
  const char *txt = p["label"] | "Start";
  const int width = p["width"] | 0;  // dp, 0 = hug the label
  const Kind k = kind_of(kn);

  lv_obj_t *b = lv_obj_create(parent);
  lv_obj_remove_style_all(b);
  lv_obj_add_flag(b, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_STATE_TRICKLE));
  lv_obj_remove_flag(b, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_height(b, fw::touch::button_h);
  lv_obj_set_width(b, width > 0 ? fw::px(width) : LV_SIZE_CONTENT);
  lv_obj_set_style_radius(b, fw::rad::button, 0);
  // 1 px border on every kind (transparent on primary/ghost) inside the box; label at button-pad-x (space 7)
  lv_obj_set_style_border_width(b, fw::stroke::hair, 0);
  lv_obj_set_style_pad_hor(b, fw::sp::s7 - fw::stroke::hair, 0);
  lv_obj_set_style_pad_column(b, fw::sp::s2, 0);  // button-gap: icon to label
  lv_obj_set_flex_flow(b, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(b, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  box_looks(b, k);
  if (!strcmp(kn, "primary")) fw::focus_ring(b);  // active: the focus ring atom (core/atoms.h, states.json focus)

  const char *ic = p["icon"] | "none";
  if (strcmp(ic, "none")) icon(b, ic, k);
  lv_obj_t *l = fw::text(b, fw::ty::c_label, txt, k.normal.fg);
  lv_obj_remove_flag(l, LV_OBJ_FLAG_CLICKABLE);
  fg_looks(l, k);
  if (width > 0) {
    lv_obj_set_flex_grow(l, 1);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
  } else {  // fit8: label box = text width rounded up to the grid, so the button stays on the grid
    lv_obj_update_layout(l);
    const int w = lv_obj_get_width(l), g = fw::sp::GRID;
    lv_obj_set_width(l, (w + g - 1) / g * g);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);  // the rounding slack splits both sides, not all after the text
  }

  // active marks: corner ticks atom on the border box (all four, layout corner-tick x stroke corner-tick, shown while CHECKED)
  if (!strcmp(kn, "secondary")) fw::corner_ticks(b, fw::Corners::all);
  else if (!strcmp(kn, "ghost")) {  // underline: space 9 wide, stroke edge high, space 2 above the padding box bottom
    lv_obj_align(underline(b, fw::sp::s9, fw::stroke::edge), LV_ALIGN_BOTTOM_MID, 0, -fw::sp::s2);
  }

  if (!strcmp(state, "pressed")) lv_obj_add_state(b, LV_STATE_PRESSED);
  if (!strcmp(state, "active")) lv_obj_add_state(b, LV_STATE_CHECKED);
  if (!strcmp(state, "active-pressed")) lv_obj_add_state(b, (lv_state_t) (LV_STATE_CHECKED | LV_STATE_PRESSED));
  if (!strcmp(state, "disabled")) lv_obj_add_state(b, ST_OFF);

  // a real tap (LVGL drops CLICKED after a scroll, like the docs' "moved > 8 px stays silent")
  fw::st::on_tap(b, [](lv_event_t *e) {  // blocked on finger down, a real tap on CLICKED (states.json)
    if (!fw::st::tap_now(e)) return;
    auto *o = (lv_obj_t *) lv_event_get_current_target(e);
    const char *d = lv_label_get_text((lv_obj_t *) lv_event_get_user_data(e));
    if (lv_obj_has_state(o, ST_OFF)) fw::st::blocked(o, d);
    else { fw::emit(o, "tap", d); fw::sound(o, "tap"); }
  }, l);
}
