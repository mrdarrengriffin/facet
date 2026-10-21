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
// Optional icon before the label (line icon at touch icon size). Width hugs the label rounded up to the grid, or a
// fixed width with the label centred. Tap = fw::emit "tap" + fw::sound "tap" (disabled: "blocked" + "error").
// Press feedback = colour transitions of PRESS_MS with ease-out (Facet motion.duration.press / easing.ease-out;
// constants until a Motion foundation exists).
#include "fw.h"

namespace {
// Motion (Facet main tokens.json motion): no WASM motion tokens yet -> named constants for the Motion foundation
constexpr uint32_t PRESS_MS = 80;  // motion.duration.press
lv_anim_path_cb_t const PRESS_PATH = lv_anim_path_ease_out;  // motion.easing.ease-out

// "disabled" is drawn with USER_1, not LV_STATE_DISABLED: LVGL sends no events to a disabled object, and Facet
// plays Error when a disabled button is pressed. USER_1 outranks PRESSED, so a disabled button shows no press look.
constexpr lv_state_t ST_OFF = LV_STATE_USER_1;

struct Look { lv_color_t bg; lv_opa_t bg_opa; lv_color_t bd; lv_opa_t bd_opa; lv_color_t fg; };
struct Kind { Look normal, pressed, active, active_pressed, off; };

Kind kind_of(const char *k) {
  const fw::Theme &t = *fw::T;
  const lv_opa_t C = LV_OPA_COVER, N = LV_OPA_TRANSP;
  if (!strcmp(k, "secondary"))
    return {{t.surface, C, t.line, C, t.text}, {t.surface_press, C, t.line_hi, C, t.text},
            {t.surface, C, t.line_hi, C, t.text}, {t.surface_press, C, t.line_hi, C, t.text},
            {t.surface, C, t.line, C, t.text_mute}};
  if (!strcmp(k, "outline"))
    return {{t.bg, N, t.accent, C, t.accent_text}, {t.accent_dim, C, t.accent, C, t.text},
            {t.accent, C, t.accent, C, t.on_accent}, {t.accent_press, C, t.accent_press, C, t.on_accent},
            {t.bg, N, t.accent_dim, C, t.text_mute}};
  if (!strcmp(k, "ghost"))
    return {{t.bg, N, t.bg, N, t.text_dim}, {t.surface_press, C, t.bg, N, t.text},
            {t.bg, N, t.bg, N, t.accent_text}, {t.surface_press, C, t.bg, N, t.accent_text},
            {t.bg, N, t.bg, N, t.text_mute}};
  // primary
  return {{t.accent, C, t.bg, N, t.on_accent}, {t.accent_press, C, t.bg, N, t.on_accent},
          {t.accent, C, t.bg, N, t.on_accent}, {t.accent_press, C, t.bg, N, t.on_accent},
          {t.accent_dim, C, t.bg, N, t.text_dim}};
}

const lv_style_prop_t TR_PROPS[] = {LV_STYLE_BG_COLOR, LV_STYLE_BG_OPA, LV_STYLE_BORDER_COLOR, LV_STYLE_BORDER_OPA,
                                    LV_STYLE_TEXT_COLOR, LV_STYLE_LINE_COLOR, LV_STYLE_PROP_INV};
const lv_style_transition_dsc_t *press_tr() {
  static lv_style_transition_dsc_t d;
  static bool ready = false;
  if (!ready) { lv_style_transition_dsc_init(&d, TR_PROPS, PRESS_PATH, PRESS_MS, 0, nullptr); ready = true; }
  return &d;
}

// the five looks on their selectors (CHECKED = active; CHECKED|PRESSED outranks PRESSED; USER_1 = disabled wins)
const lv_style_selector_t SEL[5] = {0, LV_STATE_PRESSED, LV_STATE_CHECKED, LV_STATE_CHECKED | LV_STATE_PRESSED, ST_OFF};
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
    lv_obj_set_style_transition(b, press_tr(), SEL[i]);
  }
}
// label / icon strokes follow the button's state (STATE_TRICKLE)
void fg_looks(lv_obj_t *o, const Kind &k, bool line) {
  for (int i = 0; i < 5; i++) {
    if (line) lv_obj_set_style_line_color(o, look(k, i).fg, SEL[i]);
    else lv_obj_set_style_text_color(o, look(k, i).fg, SEL[i]);
    lv_obj_set_style_transition(o, press_tr(), SEL[i]);
  }
}

// ---- icons: the 3 glyphs this component needs, from the Facet icon masters (packages/icons/svg, 24 grid, round
// caps). Minimal stand-in until the Icons foundation: polylines drawn with lv_line at touch::icon size.
struct Glyph { const char *id; int n; const float (*pts)[2]; int npts[2]; };
const float PLUS[][2] = {{12, 5}, {12, 19}, {5, 12}, {19, 12}};    // M12 5 L12 19 M5 12 L19 12
const float CLOSE[][2] = {{6, 6}, {18, 18}, {18, 6}, {6, 18}};     // M6 6 L18 18 M18 6 L6 18
const float CHECK[][2] = {{5, 12.5f}, {10, 17}, {19, 7}};          // M5 12.5 L10 17 L19 7
const Glyph GLYPHS[] = {{"plus", 2, PLUS, {2, 2}}, {"close", 2, CLOSE, {2, 2}}, {"check", 1, CHECK, {3, 0}}};

lv_obj_t *icon(lv_obj_t *parent, const char *id, const Kind &k) {
  const Glyph *g = nullptr;
  for (auto &x : GLYPHS)
    if (!strcmp(x.id, id)) g = &x;
  if (!g) return nullptr;
  // point arrays live as long as the module (lv_line keeps the pointer): one table per glyph, density fixed per build
  static lv_point_precise_t store[3][4];
  const int gi = (int) (g - GLYPHS);
  const float s = fw::touch::icon / 24.0f;
  lv_obj_t *box = lv_obj_create(parent);
  lv_obj_remove_style_all(box);
  lv_obj_set_size(box, fw::touch::icon, fw::touch::icon);
  lv_obj_remove_flag(box, LV_OBJ_FLAG_CLICKABLE);
  int at = 0;
  for (int seg = 0; seg < g->n; seg++) {
    lv_point_precise_t *p = &store[gi][at];
    for (int i = 0; i < g->npts[seg]; i++)
      p[i] = {(lv_value_precise_t) std::lround(g->pts[at + i][0] * s), (lv_value_precise_t) std::lround(g->pts[at + i][1] * s)};
    lv_obj_t *l = lv_line_create(box);
    lv_obj_remove_style_all(l);
    lv_line_set_points(l, p, g->npts[seg]);
    lv_obj_set_style_line_width(l, fw::stroke::edge, 0);  // opticalStroke(24) = 2 px
    lv_obj_set_style_line_rounded(l, true, 0);
    lv_obj_remove_flag(l, LV_OBJ_FLAG_CLICKABLE);
    fg_looks(l, k, true);
    at += g->npts[seg];
  }
  lv_obj_add_flag(box, LV_OBJ_FLAG_STATE_TRICKLE);
  return box;
}

// accent mark shown only while active (CHECKED): a corner tick (L of two border sides) or the ghost underline
lv_obj_t *active_mark(lv_obj_t *b, int w, int h, lv_border_side_t sides) {
  lv_obj_t *o = lv_obj_create(b);
  lv_obj_remove_style_all(o);
  lv_obj_add_flag(o, LV_OBJ_FLAG_IGNORE_LAYOUT);
  lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_size(o, w, h);
  if (sides) {
    lv_obj_set_style_border_side(o, sides, 0);
    lv_obj_set_style_border_width(o, fw::stroke::edge, 0);  // btn-tick-thick 2
    lv_obj_set_style_border_color(o, fw::T->accent, 0);
    lv_obj_set_style_border_opa(o, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_opa(o, LV_OPA_COVER, LV_STATE_CHECKED);
  } else {
    lv_obj_set_style_bg_color(o, fw::T->accent, 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, LV_STATE_CHECKED);
  }
  return o;
}
}  // namespace

FW_COMPONENT(button, "Button", "components", R"({
  "frame": [240, 112],
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
  if (!strcmp(kn, "primary")) {  // active: focus ring, 1 px accent, focus-offset outside the box
    lv_obj_set_style_outline_color(b, fw::T->accent, 0);
    lv_obj_set_style_outline_width(b, 0, 0);
    lv_obj_set_style_outline_width(b, fw::stroke::focus, LV_STATE_CHECKED);
    lv_obj_set_style_outline_pad(b, fw::touch::focus_offset - fw::stroke::focus, LV_STATE_CHECKED);
  }

  const char *ic = p["icon"] | "none";
  if (strcmp(ic, "none")) icon(b, ic, k);
  lv_obj_t *l = fw::text(b, fw::ty::c_label, txt, k.normal.fg);
  lv_obj_remove_flag(l, LV_OBJ_FLAG_CLICKABLE);
  fg_looks(l, k, false);
  if (width > 0) {
    lv_obj_set_flex_grow(l, 1);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
  } else {  // fit8: label box = text width rounded up to the grid, so the button stays on the grid
    lv_obj_update_layout(l);
    const int w = lv_obj_get_width(l), g = fw::sp::GRID;
    lv_obj_set_width(l, (w + g - 1) / g * g);
  }

  // active marks sit on the border box: offsets = the button's padding + border (LVGL children start inside both)
  const int sl = lv_obj_get_style_space_left(b, LV_PART_MAIN), st = lv_obj_get_style_space_top(b, LV_PART_MAIN);
  if (!strcmp(kn, "secondary")) {  // corner ticks: btn-tick-len (space 2) x btn-tick-thick (stroke edge)
    const int t = fw::sp::s2;
    lv_obj_align(active_mark(b, t, t, (lv_border_side_t) (LV_BORDER_SIDE_TOP | LV_BORDER_SIDE_LEFT)), LV_ALIGN_TOP_LEFT, -sl, -st);
    lv_obj_align(active_mark(b, t, t, (lv_border_side_t) (LV_BORDER_SIDE_TOP | LV_BORDER_SIDE_RIGHT)), LV_ALIGN_TOP_RIGHT, sl, -st);
    lv_obj_align(active_mark(b, t, t, (lv_border_side_t) (LV_BORDER_SIDE_BOTTOM | LV_BORDER_SIDE_LEFT)), LV_ALIGN_BOTTOM_LEFT, -sl, st);
    lv_obj_align(active_mark(b, t, t, (lv_border_side_t) (LV_BORDER_SIDE_BOTTOM | LV_BORDER_SIDE_RIGHT)), LV_ALIGN_BOTTOM_RIGHT, sl, st);
  } else if (!strcmp(kn, "ghost")) {  // underline: space 9 wide, stroke edge high, space 2 above the padding box bottom
    lv_obj_align(active_mark(b, fw::sp::s9, fw::stroke::edge, LV_BORDER_SIDE_NONE), LV_ALIGN_BOTTOM_MID, 0, -fw::sp::s2);
  }

  if (!strcmp(state, "pressed")) lv_obj_add_state(b, LV_STATE_PRESSED);
  if (!strcmp(state, "active")) lv_obj_add_state(b, LV_STATE_CHECKED);
  if (!strcmp(state, "active-pressed")) lv_obj_add_state(b, (lv_state_t) (LV_STATE_CHECKED | LV_STATE_PRESSED));
  if (!strcmp(state, "disabled")) lv_obj_add_state(b, ST_OFF);

  // a real tap (LVGL drops CLICKED after a scroll, like the docs' "moved > 8 px stays silent")
  lv_obj_add_event_cb(b, [](lv_event_t *e) {
    auto *o = (lv_obj_t *) lv_event_get_current_target(e);
    const char *d = lv_label_get_text((lv_obj_t *) lv_event_get_user_data(e));
    if (lv_obj_has_state(o, ST_OFF)) { fw::emit(o, "blocked", d); fw::sound(o, "error"); }
    else { fw::emit(o, "tap", d); fw::sound(o, "tap"); }
  }, LV_EVENT_CLICKED, l);
}
