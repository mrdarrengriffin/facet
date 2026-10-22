// Checkbox: the Facet checkbox (docs: Components > Pickers > Checkbox, @facet/ui checkbox.ts, CSS .f-checkbox): a small
// rounded box that is open, in progress (an accent dot) or done (filled accent with a check), with an optional label.
// Read-only unless interactive: a tap cycles open -> done and doing -> done, done -> open (docs).
//   box     lay::checkbox (24 / 16; docs 20 at the docs site size: = the Radio and the check icon's own icon size, so the
//           check fills the box exactly as the docs SVG does), stroke::edge border, rad::chip (8 / 4; docs 6)
//   open    border text-dim (a control boundary, ui 3:1), no fill
//   doing   border + lay::checkbox_dot (8) dot st::on_mark (accent), centred on the grid
//   done    fill + border st::on_fill (accent), the Icons foundation "check" at ic::icon in st::on_label (on-accent)
//   label   type body, text; done = text-dim (docs); sp::s2 after the box, the box centred on the label's line box
// States (states.json): disabled (st::OFF) = box, fill and label text-mute (st::disabled_ink), the check in bg on the
// muted fill; a tap = "blocked" + error on finger down. A read-only checkbox takes no touch at all (docs aria-readonly).
// Tap (interactive): event "toggle" (detail = the new state) + sound toggle-on (to done) / toggle-off (to open); the
// look switches with Motion toggle (colour / opacity fades, nothing moves). Hit area: at least touch::min tall.
// Update path: the state (open / doing / done) and disabled fade in place (LVGL state styles: st::ON = done, USER_2 =
// doing); label / interactive refill (snap). Contrast pairs: tokens/colour.json section "checkbox".
#include "fw.h"

namespace {
constexpr lv_state_t ST_DONE = fw::st::ON, ST_DOING = LV_STATE_USER_2, ST_OFF = fw::st::OFF;
const lv_style_prop_t BOX_PROPS[] = {LV_STYLE_BORDER_COLOR, LV_STYLE_BG_OPA, LV_STYLE_BG_COLOR, LV_STYLE_PROP_INV};
const lv_style_prop_t OPA_PROPS[] = {LV_STYLE_BG_OPA, LV_STYLE_IMAGE_OPA, LV_STYLE_OPA, LV_STYLE_PROP_INV};
const lv_style_prop_t TEXT_PROPS[] = {LV_STYLE_TEXT_COLOR, LV_STYLE_PROP_INV};
const lv_style_transition_dsc_t *tr(int i) {
  static lv_style_transition_dsc_t d[3];
  static bool ready = false;
  if (!ready) {  // states.json on: Motion toggle
    fw::mo::transition(&d[0], fw::st::SWITCH, BOX_PROPS);
    fw::mo::transition(&d[1], fw::st::SWITCH, OPA_PROPS);
    fw::mo::transition(&d[2], fw::st::SWITCH, TEXT_PROPS);
    ready = true;
  }
  return &d[i];
}
int check_of(fw::Props p) {
  const char *c = p["check"] | "done";
  return !strcmp(c, "open") ? 0 : !strcmp(c, "doing") ? 1 : 2;
}
const char *NAMES[] = {"open", "doing", "done"};
std::string sig_of(fw::Props p) { return std::string(p["label"] | "") + "|" + ((p["interactive"] | true) ? "1" : "0"); }
int check_now(lv_obj_t *o) { return lv_obj_has_state(o, ST_DONE) ? 2 : lv_obj_has_state(o, ST_DOING) ? 1 : 0; }
void set_check(lv_obj_t *o, int c, bool animate) {
  fw::st::set_state(o, ST_DONE, c == 2, animate);
  fw::st::set_state(o, ST_DOING, c == 1, animate);
}
void transitions(lv_obj_t *o, int i) {
  for (lv_style_selector_t s : {(lv_style_selector_t) 0, (lv_style_selector_t) ST_DONE, (lv_style_selector_t) ST_DOING})
    lv_obj_set_style_transition(o, tr(i), s);
}

void fill(lv_obj_t *r, fw::Props p) {
  const fw::Theme &t = *fw::T;
  const bool live = p["interactive"] | true;
  const char *label = p["label"] | "";
  const int B = fw::lay::checkbox, dot = fw::lay::checkbox_dot;
  static_assert((fw::lay::checkbox - fw::lay::checkbox_dot) % 8 == 0, "the checkbox dot must centre on the 4 px grid");

  lv_obj_remove_style_all(r);
  lv_obj_remove_flag(r, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flag(r, LV_OBJ_FLAG_CLICKABLE, live);
  lv_obj_set_size(r, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(r, fw::sp::s2, 0);
  const int lh = *label ? fw::style(fw::ty::body).box : B;
  lv_obj_set_ext_click_area(r, std::max(0, (fw::touch::min - std::max(lh, B)) / 2));

  lv_obj_t *b = lv_obj_create(r);  // the box
  lv_obj_remove_style_all(b);
  lv_obj_remove_flag(b, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_size(b, B, B);
  lv_obj_set_style_radius(b, fw::rad::chip, 0);
  lv_obj_set_style_border_width(b, fw::stroke::edge, 0);
  lv_obj_set_style_border_color(b, t.text_dim, 0);
  lv_obj_set_style_border_color(b, fw::st::on_mark(t), ST_DOING);
  lv_obj_set_style_border_color(b, fw::st::on_fill(t), ST_DONE);
  lv_obj_set_style_border_color(b, fw::st::disabled_ink(t), ST_OFF);
  lv_obj_set_style_border_color(b, fw::st::disabled_ink(t), ST_OFF | ST_DOING);
  lv_obj_set_style_border_color(b, fw::st::disabled_ink(t), ST_OFF | ST_DONE);
  lv_obj_set_style_bg_color(b, fw::st::on_fill(t), 0);
  lv_obj_set_style_bg_color(b, fw::st::disabled_ink(t), ST_OFF | ST_DONE);
  lv_obj_set_style_bg_opa(b, LV_OPA_TRANSP, 0);
  lv_obj_set_style_bg_opa(b, LV_OPA_COVER, ST_DONE);
  transitions(b, 0);

  lv_obj_t *k = lv_obj_create(b);  // doing: the dot
  lv_obj_remove_style_all(k);
  lv_obj_remove_flag(k, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_size(k, dot, dot);
  lv_obj_align(k, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_radius(k, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(k, fw::st::on_mark(t), 0);
  lv_obj_set_style_bg_color(k, fw::st::disabled_ink(t), ST_OFF | ST_DOING);
  lv_obj_set_style_bg_opa(k, LV_OPA_TRANSP, 0);
  lv_obj_set_style_bg_opa(k, LV_OPA_COVER, ST_DOING);
  transitions(k, 1);

  // done: the check (Icons foundation, icon size = the box; the box's border is drawn over its edge, like the docs SVG)
  if (lv_obj_t *ic = fw::icon(b, "check", fw::ic::icon, fw::st::on_label(t))) {
    lv_obj_add_flag(ic, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_align(ic, LV_ALIGN_CENTER, 0, 0);
    fw::icon_colour(ic, t.bg, ST_OFF | ST_DONE);
    lv_obj_set_style_opa(ic, LV_OPA_TRANSP, 0);
    lv_obj_set_style_opa(ic, LV_OPA_COVER, ST_DONE);
    transitions(ic, 1);
  }
  if (*label) {
    lv_obj_t *l = fw::text(r, fw::ty::body, label, t.text);
    lv_obj_set_height(l, fw::style(fw::ty::body).box);
    lv_obj_update_layout(l);  // the label's box rounded up to the grid (Label fit8), so the checkbox is on the grid
    lv_obj_set_width(l, (lv_obj_get_width(l) + fw::sp::GRID - 1) / fw::sp::GRID * fw::sp::GRID);
    lv_obj_set_style_text_color(l, t.text_dim, ST_DONE);
    lv_obj_set_style_text_color(l, fw::st::disabled_ink(t), ST_OFF);
    lv_obj_set_style_text_color(l, fw::st::disabled_ink(t), ST_OFF | ST_DONE);
    transitions(l, 2);
  }
  fw::shape(r, sig_of(p));
}
}  // namespace

FW_COMPONENT(checkbox, "Checkbox", "components", R"({
  "about": "A small rounded box: open, in progress (an accent dot) or done (filled with a check). Tapping cycles open and done unless it is read-only.",
  "frame": [400, 96],
  "icons": ["check@icon"],
  "props": [
    {"name": "check", "type": "select", "options": ["open", "doing", "done"], "default": "done"},
    {"name": "label", "type": "text", "default": "Screens: Home, Media, Light"},
    {"name": "interactive", "type": "bool", "default": true},
    {"name": "state", "type": "select", "options": ["default", "disabled"], "default": "default"}
  ],
  "variants": [
    {"label": "open", "props": {"check": "open", "label": "Planned"}},
    {"label": "doing", "props": {"check": "doing", "label": "In progress"}},
    {"label": "done", "props": {"check": "done", "label": "Shipped"}},
    {"label": "box only", "props": {"label": ""}},
    {"label": "disabled · done", "props": {"state": "disabled", "label": "Shipped"}},
    {"label": "disabled · open", "props": {"state": "disabled", "check": "open", "label": "Planned"}}
  ]
})") {
  lv_obj_t *r = lv_obj_create(parent);
  fill(r, p);
  set_check(r, check_of(p), false);
  if (!strcmp(p["state"] | "default", "disabled")) fw::st::set_off(r, true, false);
  fw::st::on_tap(r, [](lv_event_t *e) {
    if (!fw::st::tap_now(e)) return;
    auto *o = (lv_obj_t *) lv_event_get_current_target(e);
    const int c = check_now(o);
    if (lv_obj_has_state(o, ST_OFF)) { fw::st::blocked(o, NAMES[c]); return; }
    const int to = c == 2 ? 0 : 2;  // open -> done, doing -> done, done -> open (docs)
    set_check(o, to, true);
    fw::emit(o, fw::st::SWITCH_EVENT, NAMES[to]);
    fw::sound(o, to == 2 ? fw::st::ON_SOUND : fw::st::OFF_SOUND);
  }, nullptr);
}

FW_UPDATE(checkbox) {
  const int was = check_now(root);
  const bool off = lv_obj_has_state(root, ST_OFF);
  if (!fw::shape(root, sig_of(p))) {
    fw::refill(root);
    fill(root, p);
    set_check(root, was, false);
    if (off) fw::st::set_off(root, true, false);
  }
  set_check(root, check_of(p), true);
  const bool to_off = !strcmp(p["state"] | "default", "disabled");
  if (to_off != lv_obj_has_state(root, ST_OFF)) fw::st::set_off(root, to_off);
  return true;
}
