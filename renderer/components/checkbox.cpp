// Checkbox: the Facet checkbox (docs: Components > Pickers > Checkbox, @facet/ui checkbox.ts, CSS .f-checkbox), on the
// contract (renderer/CONTRACT.md, "contract": 2): a small rounded box that is open, in progress (an accent dot) or done
// (filled accent with a check), with an optional label. Read-only unless interactive: a tap cycles open -> done and
// doing -> done, done -> open (docs).
//   box     checkbox (layout:checkbox 24 / 20 square = the check icon at icon / glyph size, so the check fills it; radius
//           chip; a stroke:edge border)
//   open    border text-dim (a control boundary, ui 3:1), no fill
//   doing   border on-mark + a checkbox-dot (layout:checkbox-dot 8 / 4) dot in on-mark, centred on the grid
//   done    fill + border on-fill, the check icon in on-label
//   label   type body, text, its box rounded up to the grid (fx::fit_grid); done = text-dim + struck through (a ticked
//           list item); gap:2 after the box, the box centred on the label's line box
//   minimum the box (+ the label, which does not shrink). Steps: one
// States: open / doing / done (st::ON = done, USER_2 = doing); disabled (fw::st::OFF: box, fill, label disabled-ink, the
// check in bg on the muted fill; a tap = "blocked" + error on finger down). n/a: pressed (no press look: the tick is
// the feedback), unavailable, loading, unknown, empty, error. A read-only checkbox takes no touch at all (docs
// aria-readonly). Themed through the live palette and radius:chip (no part: the theme engine has no checkbox part yet).
// Tap (interactive): event "toggle" (detail = the new state) + sound toggle-on (to done) / toggle-off (to open); the
// look switches with Motion toggle (fade-switch: colours and opacities fade, nothing moves). Hit area: at least touch::min
// tall. Update path: the state and disabled fade in place; label / interactive refill (snap). Bleed: none.
// Contrast pairs: tokens/colour.json section "checkbox".
#include "fw.h"
#include "fx.h"

namespace {
constexpr lv_state_t ST_DONE = fw::st::ON, ST_DOING = LV_STATE_USER_2, ST_OFF = fw::st::OFF;
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
// the Motion toggle fade on every state the part changes in
void fades(lv_obj_t *o) {
  for (lv_state_t st : {(lv_state_t) 0, ST_DONE, ST_DOING, ST_OFF}) fx::use(o, "fade-switch", st);
}

void fill(lv_obj_t *r, fw::Props p) {
  const bool live = p["interactive"] | true;
  const char *label = p["label"] | "";

  for (const char *t : {"box", "row-mid", "gap:2"}) fx::use(r, t);
  fx::name(r, "checkbox");
  lv_obj_set_flag(r, LV_OBJ_FLAG_CLICKABLE, live);
  lv_obj_add_flag(r, LV_OBJ_FLAG_STATE_TRICKLE);  // the states reach the box, dot, check and label

  lv_obj_t *b = fx::box(r, "free");  // the box
  lv_obj_add_flag(b, LV_OBJ_FLAG_STATE_TRICKLE);
  fx::use(b, "border:text-dim");
  fx::use(b, "border:on-mark", ST_DOING);
  fx::use(b, "border:on-fill", ST_DONE);
  fx::use(b, "bg:on-fill", ST_DONE);
  for (lv_state_t st : {ST_OFF, (lv_state_t) (ST_OFF | ST_DOING), (lv_state_t) (ST_OFF | ST_DONE)}) fx::use(b, "border:disabled-ink", st);
  fx::use(b, "bg:disabled-ink", ST_OFF | ST_DONE);
  // the box's size, radius and edge width last at each state: a border:<c> token draws a hairline
  for (lv_state_t st : {(lv_state_t) 0, ST_DOING, ST_DONE, ST_OFF, (lv_state_t) (ST_OFF | ST_DOING), (lv_state_t) (ST_OFF | ST_DONE)})
    fx::use(b, "checkbox", st);
  fades(b);

  lv_obj_t *k = fx::box(b, "free");  // doing: the dot
  fx::use(k, "checkbox-dot");
  fx::use(k, "transp");
  fx::use(k, "bg:on-mark");
  fx::use(k, "opaque", ST_DOING);
  fx::use(k, "bg:disabled-ink", ST_OFF | ST_DOING);
  fades(k);

  // done: the check (Icons foundation, icon size = the box; the box's border is drawn over its edge, like the docs SVG)
  if (lv_obj_t *ic = fx::icon(b, "check", "checkbox", "fg:on-label")) {
    lv_obj_add_flag(ic, LV_OBJ_FLAG_IGNORE_LAYOUT);
    fx::use(ic, "over-cell");
    fx::use(ic, "transp");
    fx::use(ic, "opaque", ST_DONE);
    fx::icon_use(ic, "ink:bg", ST_OFF | ST_DONE);
    fades(ic);
  }
  if (*label) {
    lv_obj_t *l = fx::text(r, label, "type:body", "fg:text");
    fx::fit_grid(l);  // the label's box rounded up to the grid, so the checkbox is on the grid
    fx::use(l, "fg:text-dim", ST_DONE);
    fx::use(l, "strike", ST_DONE);  // ticked = struck through (a done list item)
    fx::use(l, "fg:disabled-ink", ST_OFF);
    fx::use(l, "fg:disabled-ink", ST_OFF | ST_DONE);
    fades(l);
  }
  fx::hit(r);
  fw::shape(r, sig_of(p));
}
}  // namespace

FW_COMPONENT(checkbox, "Checkbox", "components", R"({
  "about": "A small rounded box: open, in progress (an accent dot) or done (filled with a check). Tapping cycles open and done unless it is read-only.",
  "contract": 2,
  "parts": [],
  "min": ["layout:checkbox"],
  "frame": [400, 96],
  "icons": ["check@icon"],
  "props": [
    {"name": "check", "about": "Where the item is: open not started, doing in progress with an accent dot, or done filled with a check mark.", "type": "select", "options": ["open", "doing", "done"], "default": "done"},
    {"name": "label", "about": "The words next to the box.", "type": "text", "default": "Screens: Home, Media, Light"},
    {"name": "interactive", "about": "Lets a tap move the box between open and done. Off makes it read-only.", "type": "bool", "default": true},
    {"name": "state", "about": "default can be used; disabled is greyed out and ignores taps.", "type": "select", "options": ["default", "disabled"], "default": "default"}
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
  lv_obj_remove_style_all(r);
  lv_obj_remove_flag(r, LV_OBJ_FLAG_SCROLLABLE);
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
