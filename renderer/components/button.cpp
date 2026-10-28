// Button: the Facet text button (docs: Components > Buttons > Button, @facet/ui button()), on the contract
// (renderer/CONTRACT.md, "contract": 2). One height, four kinds, five states:
//   size     a fixed touch control from tokens ("button", core/fx_core.cpp): touch:button-h tall (64 / 44), its
//            label's width on the 4 px grid (fx::fit_grid) + space 7 each side (the hair border on every kind inside),
//            icon to label space 2, radius:button. width "fill" = its parent's width, the label centred (a legacy
//            caller may size the root itself; a number > 0 = hug, kept for old specs)
//   minimum  touch:button-h x touch:button-h. Steps: one
//   primary    accent fill, on-accent label; pressed = pressed-accent; active = the focus ring outside (fx::focus_ring)
//   secondary  surface + hair line border; pressed = pressed-surface + pressed-line; active = on-line + corner ticks
//   outline    hair accent border, accent-text label; pressed = pressed-outline fill + pressed-ink label; active =
//              filled on-fill (active + pressed = pressed-accent)
//   ghost      label only (text-dim); pressed = pressed-surface + pressed-ink; active = on-ink + a short on-mark underline
// disabled (fw::st::OFF): disabled-ink label (primary: disabled-accent fill + disabled-on-fill label; outline:
// disabled-accent border), no press look; a tap plays Error. Every look is a colour style per LVGL state (C17):
// CHECKED = active, CHECKED | PRESSED outranks PRESSED, OFF wins. Label and icon follow the root's state (trickle).
// Press feedback = Motion press / release as style transitions ("fade-press-all", 80 ms ease-out; reduced motion
// keeps them: colour only). Tap = "tap" + sound tap (disabled: "blocked" + error on finger down). Bleed: the focus
// ring (decor, C14). Contrast pairs: tokens/colour.json section "button".
// hold (2026-10-28, was the Hold button): a confirm step (open the door's latch). No tap; a press fills the button with
// accent-dim from the left (core fx::hold: drawn on the Button, no extra object) over hold_time (Motion hold-confirm);
// let go early = it drains (Motion release), nothing happens; full = sound confirm + event "confirm" (detail = value,
// else the label), and letting go after is not a tap. hold_label = the words while held (silent). Disabled = blocked +
// error. Kinds: secondary, outline, ghost (their pressed label is text over accent-dim, contrast section "button");
// a primary hold button is drawn as secondary (on-accent over accent-dim fails in dark).
// Still legacy inside: the secondary kind's corner ticks (the core atom fw::corner_ticks).
#include "fw.h"
#include "fx.h"
#include "atoms.h"

namespace {
constexpr lv_state_t ST_OFF = fw::st::OFF;
const lv_style_selector_t SEL[5] = {0, LV_STATE_PRESSED, LV_STATE_CHECKED, LV_STATE_CHECKED | LV_STATE_PRESSED, ST_OFF};
const bool PRESSED_SEL[5] = {false, true, false, true, false};
// per kind and state (SEL order): fill ("" = none), border ("" = a clear hair), label / icon ink
struct Look { const char *bg, *bd, *fg; };
struct Kind { const char *name; Look l[5]; };
const Kind KINDS[] = {
  {"primary", {{"bg:accent", "", "fg:on-accent"}, {"bg:pressed-accent", "", "fg:on-accent"}, {"bg:accent", "", "fg:on-accent"},
               {"bg:pressed-accent", "", "fg:on-accent"}, {"bg:disabled-accent", "", "fg:disabled-on-fill"}}},
  {"secondary", {{"bg:surface", "border:line", "fg:text"}, {"bg:pressed-surface", "border:pressed-line", "fg:text"},
                 {"bg:surface", "border:on-line", "fg:text"}, {"bg:pressed-surface", "border:on-line", "fg:text"},
                 {"bg:surface", "border:line", "fg:disabled-ink"}}},
  {"outline", {{"", "border:accent", "fg:accent-text"}, {"bg:pressed-outline", "border:accent", "fg:pressed-ink"},
               {"bg:on-fill", "border:on-fill", "fg:on-label"}, {"bg:pressed-accent", "border:pressed-accent", "fg:on-label"},
               {"", "border:disabled-accent", "fg:disabled-ink"}}},
  {"ghost", {{"", "", "fg:text-dim"}, {"bg:pressed-surface", "", "fg:pressed-ink"}, {"", "", "fg:on-ink"},
             {"bg:pressed-surface", "", "fg:on-ink"}, {"", "", "fg:disabled-ink"}}},
};
const Kind &kind_of(const char *k) {
  for (const Kind &x : KINDS) if (!strcmp(x.name, k)) return x;
  return KINDS[0];
}
std::string ink(const char *fg) { return std::string("ink:") + (strchr(fg, ':') + 1); }
bool hold_on(fw::Props p) { return p["hold"] | false; }
const char *kind_name(fw::Props p) {  // a primary hold button is drawn as secondary (contrast: see the top)
  const char *k = p["kind"] | "primary";
  return hold_on(p) && !strcmp(k, "primary") ? "secondary" : k;
}
// the hold's words (behaviour state on the root: rides on its DELETE event, like core/sheet.cpp)
struct Held { std::string label, held, value; };
void held_free(lv_event_t *e) { delete (Held *) lv_event_get_user_data(e); }
Held *held_of(lv_obj_t *b) {
  for (uint32_t i = 0, n = lv_obj_get_event_count(b); i < n; i++) {
    lv_event_dsc_t *d = lv_obj_get_event_dsc(b, i);
    if (lv_event_dsc_get_cb(d) == held_free) return (Held *) lv_event_dsc_get_user_data(d);
  }
  Held *h = new Held;
  lv_obj_add_event_cb(b, held_free, LV_EVENT_DELETE, h);
  return h;
}

// structural props (a change = the contents again, fw::refill); state + label move in place
std::string sig(fw::Props p) {
  return std::string(kind_name(p)) + "|" + (p["icon"] | "none") + "|" + (p["width"].is<const char *>() ? p["width"] | "hug" : "hug");
}
lv_obj_t *label_of(lv_obj_t *b) {
  for (int i = (int) lv_obj_get_child_count(b) - 1; i >= 0; i--)
    if (lv_obj_check_type(lv_obj_get_child(b, i), &lv_label_class)) return lv_obj_get_child(b, i);
  return nullptr;
}
// the forced state from props; animate = the state looks fade (fw::st::set_state; a forced press shows at once)
void set_state(lv_obj_t *b, const char *state, bool animate) {
  const bool pressed = !strcmp(state, "pressed") || !strcmp(state, "active-pressed");
  const bool active = !strcmp(state, "active") || !strcmp(state, "active-pressed");
  if (pressed != lv_obj_has_state(b, LV_STATE_PRESSED)) lv_obj_set_state(b, LV_STATE_PRESSED, pressed);
  fw::st::set_state(b, LV_STATE_CHECKED, active, animate);
  fw::st::set_state(b, ST_OFF, !strcmp(state, "disabled"), animate);
}
void set_words(lv_obj_t *b, const char *txt) {
  if (lv_obj_t *l = label_of(b)) { fx::set_text(l, txt, "type:c-label"); fx::fit_grid(l); }  // a hug button re-fits its grid width
}
// phase 0 held (the hold words), 1 let go early, 2 full (confirm)
void on_hold(lv_obj_t *b, int phase) {
  Held *h = held_of(b);
  set_words(b, (phase ? h->label : h->held).c_str());
  if (phase != 2) return;
  fw::sound(b, "confirm");
  fw::emit(b, "confirm", (h->value.empty() ? h->label : h->value).c_str());
}
void take_hold(lv_obj_t *b, fw::Props p) {
  Held *h = held_of(b);
  h->label = p["label"] | "Start";
  h->held = p["hold_label"] | "";
  if (h->held.empty()) h->held = h->label;
  h->value = p["value"] | "";
  fx::hold(b, hold_on(p) ? (int) (p["hold_time"] | (double) fw::mo::ms::hold_confirm) : 0, on_hold);
}
void fill(lv_obj_t *b, fw::Props p) {
  const char *kn = kind_name(p);
  const Kind &k = kind_of(kn);
  fw::shape(b, sig(p));
  lv_obj_add_flag(b, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_STATE_TRICKLE));
  lv_obj_remove_flag(b, LV_OBJ_FLAG_SCROLLABLE);
  fx::use(b, "button");
  fx::use(b, "radius:button");
  if (!strcmp(p["size"] | "normal", "small")) fx::use(b, "button-sm");  // a row's action (Section header): touch:min tall
  if (p["width"].is<const char *>() && !strcmp(p["width"] | "", "fill")) fx::use(b, "button-fill");
  // the parts first, then the state, then the state looks + their fades: building never fades (C34)
  const char *ic = p["icon"] | "none";
  lv_obj_t *o = strcmp(ic, "none") ? fx::icon(b, ic, "icon", k.l[0].fg) : nullptr;  // before the label, its ink = the label's
  lv_obj_t *l = fx::text(b, p["label"] | "Start", "type:c-label", k.l[0].fg);
  fx::use(l, "text-mid");  // the text centred in its grid box (the rounding slack splits both sides)
  fx::fit_grid(l);
  lv_obj_t *u = nullptr;
  if (!strcmp(kn, "ghost")) {  // the ghost underline (active)
    u = fx::box(b, "free");
    lv_obj_add_flag(u, LV_OBJ_FLAG_IGNORE_LAYOUT);
    fx::use(u, "btn-underline");
  }
  set_state(b, p["state"] | "default", false);

  fx::use(b, "border-clear");
  for (int i = 0; i < 5; i++) {
    if (k.l[i].bg[0]) fx::use(b, k.l[i].bg, SEL[i]); else fx::use(b, "bg-none", SEL[i]);
    if (k.l[i].bd[0]) fx::use(b, k.l[i].bd, SEL[i]); else fx::use(b, "border-clear", SEL[i]);
    if (i) { fx::use(l, k.l[i].fg, SEL[i]); fx::icon_use(o, ink(k.l[i].fg).c_str(), SEL[i]); }
    const char *f = PRESSED_SEL[i] ? "fade-press-all" : "fade-release-all";
    fx::use(b, f, SEL[i]); fx::use(l, f, SEL[i]); fx::icon_use(o, f, SEL[i]);
  }
  if (!strcmp(kn, "primary")) fx::focus_ring(b);  // active: the focus ring (decor, C14)
  if (u) fx::use(u, "bg:on-mark", LV_STATE_CHECKED);
  if (!strcmp(kn, "secondary")) fw::corner_ticks(b, fw::Corners::all);  // active: corner ticks (core atom)
  if (!strcmp(kn, "secondary")) fx::use(b, "part:control");  // the theme's control (C46), last; outline / ghost keep their own look
  if (!strcmp(kn, "primary")) fx::use(b, "part:button-primary");
  if (!strcmp(kn, "outline")) fx::use(b, "part:button-outline");  // the theme's outline button (C46), last (unset = the accent edge)  // the theme's primary button (C46), last (Win98: the grey control look)
  take_hold(b, p);
}
}  // namespace

FW_COMPONENT(button, "Button", "components", R"({
  "about": "A text button in four kinds (primary, secondary, outline, ghost), with an optional icon.",
  "contract": 2,
  "parts": ["control", "button-primary", "button-outline"],
  "min": ["touch:button-h", "touch:button-h"],
  "audit": {"snaps": {"kind": "another kind of button is another button: structure"}},
  "frame": [240, 112],
  "icons": ["plus@icon", "check@icon", "close@icon"],
  "props": [
    {"name": "kind", "about": "How strong the button looks: primary is filled in the accent for the main action, secondary is a quieter fill, outline is just an edge, ghost is text only for the least important action.", "type": "select", "options": ["primary", "secondary", "outline", "ghost"], "default": "primary"},
    {"name": "label", "about": "The words on the button.", "type": "text", "default": "Start"},
    {"name": "state", "about": "How the button looks: default ready to tap, pressed while held, active when it is the chosen one, active-pressed while the chosen one is held, or disabled when it cannot be used.", "type": "select", "options": ["default", "pressed", "active", "active-pressed", "disabled"], "default": "default"},
    {"name": "icon", "about": "An icon shown before the words, or none for words only.", "type": "select", "options": ["none", "plus", "check", "close"], "default": "none"},
    {"name": "size", "about": "normal is the full button height; small is the smallest touch height with less padding, for an action inside a row such as a section header's See all.", "type": "select", "options": ["normal", "small"], "default": "normal"},
    {"name": "width", "about": "hug fits the button to its words; fill stretches it across the space it is in, with the words centred.", "type": "select", "options": ["hug", "fill"], "default": "hug"},
    {"name": "hold", "about": "Hold to confirm, for an action that must not happen by accident such as opening a door: a tap does nothing; the button fills while held and acts only when full.", "type": "bool", "default": false},
    {"name": "hold_label", "about": "The words shown while the button is held, such as Opening. Empty keeps the label.", "type": "text", "default": ""},
    {"name": "hold_time", "about": "How long a hold takes to confirm, in milliseconds.", "type": "range", "min": 400, "max": 4000, "step": 100, "default": 1200},
    {"name": "value", "about": "What a completed hold sends, such as open. Empty sends the label.", "type": "text", "default": ""}
  ],
  "variants": [
    {"label": "primary · default", "props": {}},
    {"label": "primary · pressed", "props": {"state": "pressed"}},
    {"label": "ghost · small", "props": {"kind": "ghost", "size": "small", "label": "See all"}},
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
    {"label": "primary · fill", "props": {"width": "fill"}},
    {"label": "secondary · hold", "props": {"kind": "secondary", "label": "Hold to open", "hold_label": "Keep holding", "value": "open", "hold": true, "width": "fill"}},
    {"label": "secondary · hold · disabled", "props": {"kind": "secondary", "label": "Hold to open", "hold": true, "width": "fill", "state": "disabled"}}
  ]
})") {
  lv_obj_t *b = lv_obj_create(parent);
  lv_obj_remove_style_all(b);
  fx::name(b, "button");
  fill(b, p);

  // a real tap (LVGL drops CLICKED after a scroll, like the docs' "moved > 8 px stays silent"); a hold button has no
  // tap of its own (fx::hold acts), disabled still says so
  fw::st::on_tap(b, [](lv_event_t *e) {  // blocked on finger down, a real tap on CLICKED (states.json)
    if (!fw::st::tap_now(e)) return;
    auto *o = (lv_obj_t *) lv_event_get_current_target(e);
    const char *d = lv_label_get_text(label_of(o));  // looked up: an update may have made the label again
    if (lv_obj_has_state(o, ST_OFF)) fw::st::blocked(o, d);
    else if (!fx::holds(o)) { fw::emit(o, "tap", d); fw::sound(o, "tap"); }
  }, nullptr);
}

// Update path: state (default / pressed / active / disabled) fades in place through fw::st::set_state, a new label
// text is set in place (a hug button re-fits its grid width); kind, icon or width = the contents again (fw::refill,
// snap); hold, hold_label, hold_time, value in place (hold on / off from primary = another kind: the contents again). Silent.
FW_UPDATE(button) {
  if (!fw::shape(root, sig(p))) {
    fx::hold(root, 0, nullptr);  // no fill left over from a hold the refill cut short
    fw::refill(root);
    fill(root, p);
    return true;
  }
  set_words(root, p["label"] | "Start");
  set_state(root, p["state"] | "default", true);
  take_hold(root, p);
  return true;
}
