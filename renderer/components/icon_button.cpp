// Icon button: the Facet icon-only button (docs: Components > Buttons > Icon button, @facet/ui iconButton()). CONTRACT
// v2 (renderer/CONTRACT.md, Header round 3, 2026-10-27): token names only through core/fx.h. Three styles, each its own
// size (square:<layout token> = exactly that square, a touch target; C7 note below):
//   play    layout:ibtn-play (96 / 64) round, bg:accent, glyph "feature" (48 / 32) in on-accent; pressed =
//           pressed-accent; active (= playing) = the pause glyph + outline:focus-ring (stroke focus, touch focus-offset
//           outside the box: decor, C14). The play style always shows play / pause; a tap toggles active: event
//           "toggle" play / pause + sound toggle-on / toggle-off.
//   round   layout:ibtn-round (80 / 52) round, bg:surface + border:line, glyph "glyph" (32 / 20) in text; pressed =
//           pressed-surface + pressed-line; active = on-line border + on-ink glyph
//   square  layout:ibtn-square (48 / 32 = touch:min) radius:square, look as round (active too, Darren 2026-10-21)
// A screen icon (icon "settings" = screen-settings, any "screen-*") sits at the icon size (24 / 16) in the square
// (docs Home settingsButton()). A multi-part icon (an accent part) looks exactly as on the Icons page in every state:
// its ink follows rest / pressed / disabled, active shows by the border alone (the active ink would merge with the
// accent part), pressed = bg:pressed-tint (the accent part keeps 3:1 on it).
// disabled (fw::st::OFF): play = disabled-accent fill + disabled-on-fill glyph; round / square = disabled-ink glyph; no
// press look, no ring; finger down = "blocked" + sound error. Unavailable = the same look (no room for the word),
// blocked detail "unavailable".
// Taps: fw::st::on_tap; round / square = event "tap" (detail = icon) + sound tap. Internal prop `role` (not in the
// playground): "back" (components/back_button.cpp: the Header's Back) = event "back" + sound back; "settings" = event
// "navigate" settings + sound open (Home's settings Icon button in the Header's end slot); "shortcut" = navigate `to`
// + sound open (a Home's header shortcut, icon screen-<kind>); "close" = icon close, event "close" detail "close" +
// sound back (the Bottom sheet's Close).
// C7 note: a button is a fixed touch target by design: its root is exactly its style's square (a layout token through
// a shared style, so fx::tune and the Layout lab move it), never a width from its parent, never a call.
// Steps: none. Bleed: none. States: default, pressed, active, active-pressed, disabled, unavailable; n/a: loading,
// unknown, empty, error (no data of its own). Update path: the state in place (LVGL state transitions, silent);
// style / icon / role = the contents again (fw::refill). Pairs: colour.json contrast section "icon_button".
#include "fw.h"
#include "fx.h"

namespace {
constexpr lv_state_t ST_OFF = fw::st::OFF;  // disabled (LVGL sends no events to LV_STATE_DISABLED; see Button)
constexpr lv_state_t P = LV_STATE_PRESSED, C = LV_STATE_CHECKED;

enum Role : uint8_t { R_TAP, R_PLAY, R_BACK, R_SETTINGS, R_SHORTCUT, R_CLOSE };
struct Data { Role role; char icon[24]; char to[16]; };

std::string sig(fw::Props p) {
  return std::string(p["style"] | "round") + "|" + (p["icon"] | "next") + "|" + (p["role"] | "");
}
void set_state(lv_obj_t *b, const char *state, bool animate) {
  const bool pressed = !strcmp(state, "pressed") || !strcmp(state, "active-pressed");
  const bool active = !strcmp(state, "active") || !strcmp(state, "active-pressed");
  if (pressed != lv_obj_has_state(b, P)) lv_obj_set_state(b, P, pressed);
  fw::st::set_state(b, C, active, animate);
  fw::st::set_state(b, ST_OFF, !strcmp(state, "unavailable") || !strcmp(state, "disabled"), animate);
}
const char *role_of(fw::Props p) { return p["role"] | ""; }
std::string icon_of(fw::Props p) {  // the icon this button shows
  const std::string r = role_of(p), ic = p["icon"] | "next";
  if (r == "back") return "back";
  if (r == "close") return "close";
  if (r == "settings" || ic == "settings") return "screen-settings";
  if (r == "shortcut") return ic.rfind("screen-", 0) == 0 ? ic : "screen-timer";
  return ic;
}
void data(lv_obj_t *b, fw::Props p) {
  auto *d = (Data *) lv_obj_get_user_data(b);
  if (!d) return;
  const std::string r = role_of(p);
  const bool play = r.empty() && !strcmp(p["style"] | "round", "play");
  d->role = r == "back" ? R_BACK : r == "settings" ? R_SETTINGS : r == "shortcut" ? R_SHORTCUT : r == "close" ? R_CLOSE : play ? R_PLAY : R_TAP;
  snprintf(d->to, sizeof d->to, "%s", p["to"] | "");
  const std::string ic = icon_of(p);
  snprintf(d->icon, sizeof d->icon, "%s", !strcmp(p["state"] | "default", "unavailable") ? "unavailable" : play ? "play"
           : ic == "screen-settings" ? "settings" : ic.c_str());
}
// the glyph: an icon centred in the button (play and pause on top of each other), its ink per state
lv_obj_t *glyph(lv_obj_t *b, const char *name, const char *size, bool play) {
  lv_obj_t *g = fx::icon(b, name, size, play ? "fg:on-accent" : "fg:text");
  if (!g) return nullptr;
  fx::use(g, "over-cell");
  lv_obj_remove_flag(g, LV_OBJ_FLAG_CLICKABLE);
  const bool multi = fx::icon_multi(g);
  fx::icon_use(g, "fade-release-ink");
  fx::icon_use(g, "fade-press-ink", P);
  if (!play && !multi) { fx::icon_use(g, "ink:on-ink", C); fx::icon_use(g, "ink:text", C | P); }
  fx::icon_use(g, play ? "ink:disabled-on-fill" : "ink:disabled-ink", ST_OFF);
  return g;
}
void fill(lv_obj_t *b, fw::Props p) {
  const std::string r = role_of(p), ic = icon_of(p);
  const bool play = r.empty() && !strcmp(p["style"] | "round", "play");
  const bool square = !r.empty() || !strcmp(p["style"] | "round", "square");
  const char *side = play ? "ibtn-play" : square ? "ibtn-square" : "ibtn-round";

  lv_obj_remove_style_all(b);
  fw::shape(b, sig(p));
  lv_obj_add_flag(b, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_STATE_TRICKLE));
  lv_obj_remove_flag(b, LV_OBJ_FLAG_SCROLLABLE);
  for (const char *t : {"hug", "over", "fade-release"}) fx::use(b, t);
  fx::use(b, (std::string("square:") + side).c_str());  // exactly the style's square (layout token)
  fx::use(b, square ? "radius:square" : "radius:round");
  fx::use(b, "fade-press", P);
  if (play) {
    fx::use(b, "bg:accent");
    fx::use(b, "bg:pressed-accent", P);
    fx::focus_ring(b);  // active: the focus ring outside the box (decor, C14), hidden while disabled
    fx::use(b, "bg:disabled-accent", ST_OFF);
  } else {
    fx::use(b, "bg:surface");
    fx::use(b, "border:line");
    fx::use(b, "bg:pressed-surface", P);
    fx::use(b, "border:pressed-line", P);
    fx::use(b, "border:on-line", C);
    fx::use(b, "bg:pressed-surface", C | P);
    fx::use(b, "border:pressed-line", C | P);
    fx::use(b, "bg:surface", ST_OFF);
    fx::use(b, "border:line", ST_OFF);
  }
  if (play) {  // play shows play, active (CHECKED, trickled) swaps to pause by opacity (no rebuild on a tap)
    lv_obj_t *pl = glyph(b, "play", "feature", true), *pa = glyph(b, "pause", "feature", true);
    for (lv_obj_t *o : {pl, pa}) if (o) lv_obj_add_flag(o, LV_OBJ_FLAG_IGNORE_LAYOUT);  // on top of each other ("over-cell")
    if (pl) { fx::use(pl, "transp", C); fx::use(pl, "opaque", ST_OFF); }
    if (pa) { fx::use(pa, "transp"); fx::use(pa, "opaque", C); fx::use(pa, "transp", ST_OFF); }
  } else if (lv_obj_t *g = glyph(b, ic.c_str(), ic.rfind("screen-", 0) == 0 ? "icon" : "glyph", false)) {
    if (fx::icon_multi(g)) { fx::use(b, "bg:pressed-tint", P); fx::use(b, "bg:pressed-tint", C | P); }
  }
  if (!play) fx::use(b, "part:control");  // the theme's control (C46), last; play is the accent action (its own fill)
  set_state(b, p["state"] | "default", false);
  data(b, p);
}
}  // namespace

FW_COMPONENT(icon_button, "Icon button", "components", R"({
  "about": "A button that is only an icon: a round play/pause button, or round and square icon buttons.",
  "contract": 2,
  "parts": ["control"],
  "frame": [160, 128],
  "min": ["layout:ibtn-square", "layout:ibtn-square"],
  "icons": ["play@feature", "pause@feature", "play@glyph", "next@glyph", "prev@glyph", "plus@glyph", "close@glyph", "check@glyph", "back@glyph", "screen-settings@icon", "screen-timer@icon"],
  "props": [
    {"name": "style", "about": "The button's shape: play is the large round play and pause button, round is a smaller circle, square has rounded corners.", "type": "select", "options": ["play", "round", "square"], "default": "round"},
    {"name": "icon", "about": "The icon on the button, such as next, previous, play or close.", "type": "select", "options": ["next", "prev", "play", "plus", "close", "check", "back", "settings"], "default": "next"},
    {"name": "state", "about": "How the button looks: default ready to tap, pressed while held, active when it is switched on, active-pressed while the active one is held, disabled when it cannot be used, or unavailable when its device cannot be reached.", "type": "select", "options": ["default", "pressed", "active", "active-pressed", "disabled", "unavailable"], "default": "default"}
  ],
  "variants": [
    {"label": "play · default", "props": {"style": "play"}},
    {"label": "play · pressed", "props": {"style": "play", "state": "pressed"}},
    {"label": "play · active = pause", "props": {"style": "play", "state": "active"}},
    {"label": "play · active + pressed", "props": {"style": "play", "state": "active-pressed"}},
    {"label": "play · disabled", "props": {"style": "play", "state": "disabled"}},
    {"label": "round · default", "props": {}},
    {"label": "round · pressed", "props": {"state": "pressed"}},
    {"label": "round · active", "props": {"state": "active"}},
    {"label": "round · active + pressed", "props": {"state": "active-pressed"}},
    {"label": "round · disabled", "props": {"state": "disabled"}},
    {"label": "round · prev", "props": {"icon": "prev"}},
    {"label": "square · default", "props": {"style": "square", "icon": "plus"}},
    {"label": "square · pressed", "props": {"style": "square", "icon": "plus", "state": "pressed"}},
    {"label": "square · active", "props": {"style": "square", "icon": "plus", "state": "active"}},
    {"label": "square · active + pressed", "props": {"style": "square", "icon": "plus", "state": "active-pressed"}},
    {"label": "square · disabled", "props": {"style": "square", "icon": "plus", "state": "disabled"}},
    {"label": "square · close", "props": {"style": "square", "icon": "close"}},
    {"label": "play · unavailable", "props": {"style": "play", "state": "unavailable"}},
    {"label": "round · unavailable", "props": {"state": "unavailable"}},
    {"label": "square · settings", "props": {"style": "square", "icon": "settings"}},
    {"label": "square · settings · pressed", "props": {"style": "square", "icon": "settings", "state": "pressed"}},
    {"label": "square · back", "props": {"style": "square", "icon": "back"}}
  ]
})") {
  lv_obj_t *b = lv_obj_create(parent);
  fx::name(b, "icon_button");
  auto *d = new Data{R_TAP, {}, {}};
  lv_obj_set_user_data(b, d);
  fill(b, p);
  fw::st::on_tap(b, [](lv_event_t *e) {  // blocked on finger down, a real tap on CLICKED (states.json)
    if (!fw::st::tap_now(e)) return;
    auto *o = (lv_obj_t *) lv_event_get_current_target(e);
    auto *d = (Data *) lv_event_get_user_data(e);
    if (lv_obj_has_state(o, ST_OFF)) { fw::st::blocked(o, d->icon); return; }
    if (d->role == R_BACK) { fw::emit(o, "back"); fw::sound(o, "back"); return; }
    if (d->role == R_CLOSE) { fw::emit(o, "close", "close"); fw::sound(o, "back"); return; }
    if (d->role == R_SETTINGS) { fw::emit(o, "navigate", "settings"); fw::sound(o, "open"); return; }
    if (d->role == R_SHORTCUT) { fw::emit(o, "navigate", d->to); fw::sound(o, "open"); return; }
    if (d->role == R_PLAY) {
      const bool on = !lv_obj_has_state(o, LV_STATE_CHECKED);
      if (on) lv_obj_add_state(o, LV_STATE_CHECKED); else lv_obj_remove_state(o, LV_STATE_CHECKED);
      fw::emit(o, fw::st::SWITCH_EVENT, on ? "play" : "pause");
      fw::sound(o, on ? fw::st::ON_SOUND : fw::st::OFF_SOUND);
      return;
    }
    fw::emit(o, "tap", d->icon);
    fw::sound(o, "tap");
  }, d);
  lv_obj_add_event_cb(b, [](lv_event_t *e) { delete (Data *) lv_event_get_user_data(e); }, LV_EVENT_DELETE, d);
}

// Update path: the state (default / pressed / active / disabled / unavailable) fades in place through
// fw::st::set_state (play: the play / pause glyph swap follows CHECKED); style, icon or role = the contents again
// (fw::refill, snap). Silent.
FW_UPDATE(icon_button) {
  if (!lv_obj_get_user_data(root)) return false;
  if (!fw::shape(root, sig(p))) {
    fw::refill(root);
    fill(root, p);
    return true;
  }
  set_state(root, p["state"] | "default", true);
  data(root, p);
  return true;
}
