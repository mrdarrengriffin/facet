// Icon button: the Facet icon-only button (docs: Components > Buttons > Icon button, @facet/ui iconButton()). Three
// styles, each its own size (layout tokens in space.json, glyph = an icon size token the manifest ships, never scaled):
//   play    lay::ibtn_play (96 / 64) round, accent fill, on-accent glyph ic::feature (48 / 32); pressed = accent-press;
//           active (= playing) = the pause glyph + a stroke::focus accent ring touch::focus_offset outside (docs
//           focus-offset-play 8 / 4 = the same values). The play style always shows play / pause (only they ship at
//           ic::feature); a tap toggles active: event "toggle" play / pause + sound toggle-on / toggle-off.
//   round   lay::ibtn_round (80 / 52) round, surface + 1 px line border, text glyph ic::glyph (32 / 20); pressed =
//           surface-press + line-hi; active = line-hi border + accent-text glyph
//   square  lay::ibtn_square (48 / 32 = touch::min) radius rad::square, look as round (active too: line-hi + accent-text,
//           Darren 2026-10-21)
// disabled (USER_1, Button rule): play = accent-dim fill + text-dim glyph (as Button primary; docs text-mute fails
// 3:1 there), round / square = text-mute glyph; no press look, no ring; finger down = "blocked" + sound error.
// Unavailable (states.json): the same disabled look (an icon button has no room for the word), blocked detail
// "unavailable".
// Same press / sound / state model as Button: CHECKED = active, Motion press / release colour transitions, sounds only
// on a real tap (CLICKED). Tap (round / square) = event "tap" (detail = icon) + sound tap.
// Internal prop `role` (not in the playground): "back" = the Back button (components/back_button.cpp builds this
// component): side touch::back, event "back" + sound back. "settings" = the Settings button of the screen chrome
// (components/screen_chrome.cpp): square, side lay::chrome_btn, the screen-settings icon at ic::icon (docs Home
// settingsButton(): screenIcon('settings', 24) in the 48 square), tap = event "navigate" (detail settings) + sound open;
// "shortcut" = the same square holding another screen (a Home's header actions, e.g. the Timer): internal props icon
// (a screen icon at ic::icon, "screen-timer") and to (the page id), tap = event "navigate" (detail = to) + sound open.
// active = "you are in Settings" (the square's active border; the icon keeps its Icons-page look, see glyph()). Hit area: at least touch::min each way (ext click area when
// the drawn box is smaller; no style is today). Pairs: colour.json contrast section "icon_button".
#include "fw.h"

namespace {
constexpr lv_state_t ST_OFF = fw::st::OFF;  // disabled (LVGL sends no events to LV_STATE_DISABLED; see Button)

struct Look { lv_color_t bg; lv_opa_t bd_opa; lv_color_t bd; lv_color_t fg; };
struct Style { Look normal, pressed, active, active_pressed, off; };

// state colours = the roles of tokens/states.json (fw::st::<state>_<role>), resting looks = colour tokens
Style style_of(bool play, bool square) {
  using namespace fw::st;
  const fw::Theme &t = *fw::T;
  const lv_opa_t C = LV_OPA_COVER, N = LV_OPA_TRANSP;
  if (play)
    return {{t.accent, N, t.accent, t.on_accent}, {pressed_accent(t), N, t.accent, t.on_accent},
            {t.accent, N, t.accent, t.on_accent}, {pressed_accent(t), N, t.accent, t.on_accent},
            {disabled_accent(t), N, t.accent, disabled_on_fill(t)}};
  return {{t.surface, C, t.line, t.text}, {pressed_surface(t), C, pressed_line(t), t.text},
          {t.surface, C, on_line(t), on_ink(t)}, {pressed_surface(t), C, pressed_line(t), on_ink(t)},  // square too (Darren 2026-10-21)
          {t.surface, C, t.line, disabled_ink(t)}};
}

const lv_style_prop_t TR_PROPS[] = {LV_STYLE_BG_COLOR, LV_STYLE_BORDER_COLOR, LV_STYLE_IMAGE_RECOLOR, LV_STYLE_PROP_INV};
const lv_style_transition_dsc_t *tr(bool pressed) {  // LVGL runs the transition of the state being entered
  static lv_style_transition_dsc_t d[2];
  static bool ready = false;
  if (!ready) { fw::st::press_transitions(d, TR_PROPS); ready = true; }  // states.json pressed motion
  return &d[pressed];
}
const lv_style_selector_t SEL[5] = {0, LV_STATE_PRESSED, LV_STATE_CHECKED, LV_STATE_CHECKED | LV_STATE_PRESSED, ST_OFF};
const bool PRESSED_SEL[5] = {false, true, false, true, false};
const Look &look(const Style &s, int i) {
  const Look *l[5] = {&s.normal, &s.pressed, &s.active, &s.active_pressed, &s.off};
  return *l[i];
}

// a multi-part icon (a screen icon's accent part, weather colours): it looks exactly as on the Icons page in every
// state (Darren 2026-10-22: never a flat recolour). Its own parts keep their colours (fw::icon); the ink follows the
// button's rest / pressed / disabled ink, but active keeps the resting ink: the active accent-text ink would merge
// with the accent part into one flat colour, so active shows by the border alone
bool multi_part(lv_obj_t *icon) {
  for (uint32_t i = 0; icon && i < lv_obj_get_child_count(icon); i++)
    if ((uintptr_t) lv_obj_get_user_data(lv_obj_get_child(icon, (int32_t) i)) != fw::ic::R_ink) return true;
  return false;
}
lv_obj_t *glyph(lv_obj_t *b, const char *name, fw::ic::Size size, const Style &s) {
  lv_obj_t *o = fw::icon(b, name, size, s.normal.fg);
  const bool multi = multi_part(o);
  for (int i = 0; o && i < 5; i++) {
    const int k = multi && i == 2 ? 0 : multi && i == 3 ? 1 : i;  // active (+ pressed) -> rest (+ pressed) ink
    fw::icon_colour(o, look(s, k).fg, SEL[i], tr(PRESSED_SEL[i]));
  }
  if (o) lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
  return o;
}

enum Role : uint8_t { R_TAP, R_PLAY, R_BACK, R_SETTINGS, R_SHORTCUT };
struct Data { Role role; char icon[24]; char to[16]; };
}  // namespace

namespace {
// structural props (a change = the contents again, fw::refill); the state moves in place
std::string sig(fw::Props p) {
  return std::string(p["style"] | "round") + "|" + (p["icon"] | "next") + "|" + (p["role"] | "");
}
// the forced state from props; animate = the state looks fade (fw::st::set_state; a forced press shows at once).
// unavailable: states.json, no room for the word, so the disabled look
void set_state(lv_obj_t *b, const char *state, bool animate) {
  const bool pressed = !strcmp(state, "pressed") || !strcmp(state, "active-pressed");
  const bool active = !strcmp(state, "active") || !strcmp(state, "active-pressed");
  if (pressed != lv_obj_has_state(b, LV_STATE_PRESSED)) lv_obj_set_state(b, LV_STATE_PRESSED, pressed);
  fw::st::set_state(b, LV_STATE_CHECKED, active, animate);
  fw::st::set_state(b, ST_OFF, !strcmp(state, "unavailable") || !strcmp(state, "disabled"), animate);
}
// what a tap reports (user data, made once in build)
void data(lv_obj_t *b, fw::Props p) {
  auto *d = (Data *) lv_obj_get_user_data(b);
  if (!d) return;
  const bool back = !strcmp(p["role"] | "", "back"), set = !strcmp(p["role"] | "", "settings"), sc = !strcmp(p["role"] | "", "shortcut");
  const bool play = !back && !set && !sc && !strcmp(p["style"] | "round", "play");
  const char *ic = back ? "back" : set ? "settings" : (p["icon"] | "next");
  d->role = back ? R_BACK : set ? R_SETTINGS : sc ? R_SHORTCUT : play ? R_PLAY : R_TAP;
  snprintf(d->to, sizeof d->to, "%s", p["to"] | "");
  snprintf(d->icon, sizeof d->icon, "%s", !strcmp(p["state"] | "default", "unavailable") ? "unavailable" : play && !back ? "play" : ic);
}
void fill(lv_obj_t *b, fw::Props p) {
  const char *st = p["style"] | "round";
  const char *state = p["state"] | "default";
  const char *role = p["role"] | "";
  const bool sc = !strcmp(role, "shortcut"), set = !strcmp(role, "settings") || sc;  // a shortcut = the settings square
  const bool back = !strcmp(role, "back") || set;  // all: the header's squares
  const bool play = !strcmp(st, "play"), square = back || !strcmp(st, "square");
  const char *ic = sc ? (p["icon"] | "screen-timer") : set ? "screen-settings" : back ? "back" : (p["icon"] | "next");
  const Style s = style_of(play && !back, square);
  const int side = set ? fw::lay::chrome_btn : back ? fw::touch::back : play ? fw::lay::ibtn_play : square ? fw::lay::ibtn_square : fw::lay::ibtn_round;

  lv_obj_remove_style_all(b);
  fw::shape(b, sig(p));
  lv_obj_add_flag(b, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_STATE_TRICKLE));
  lv_obj_remove_flag(b, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(b, side, side);
  if (side < fw::touch::min) lv_obj_set_ext_click_area(b, (fw::touch::min - side + 1) / 2);
  lv_obj_set_style_radius(b, square ? fw::rad::square : fw::rad::round, 0);
  lv_obj_set_style_border_width(b, fw::stroke::hair, 0);  // 1 px border on every style (transparent on play)
  lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
  for (int i = 0; i < 5; i++) {
    const Look &l = look(s, i);
    lv_obj_set_style_bg_color(b, l.bg, SEL[i]);
    lv_obj_set_style_border_color(b, l.bd, SEL[i]);
    lv_obj_set_style_border_opa(b, l.bd_opa, SEL[i]);
    lv_obj_set_style_transition(b, tr(PRESSED_SEL[i]), SEL[i]);
  }
  if (play && !back) {  // active: the focus ring outside the box (states.json focus, Button primary's ring)
    fw::focus_ring(b);  // the focus ring atom (core/atoms.h): shown while CHECKED, hidden when disabled / unavailable
  }

  // glyph centred; play shows play, active (CHECKED, trickled) swaps to pause by opacity (no rebuild on a tap)
  if (play && !back) {
    lv_obj_t *pl = glyph(b, "play", fw::ic::feature, s), *pa = glyph(b, "pause", fw::ic::feature, s);
    if (pl) { lv_obj_center(pl); lv_obj_set_style_opa(pl, LV_OPA_TRANSP, LV_STATE_CHECKED); lv_obj_set_style_opa(pl, LV_OPA_COVER, ST_OFF); }
    if (pa) { lv_obj_center(pa); lv_obj_set_style_opa(pa, LV_OPA_TRANSP, 0); lv_obj_set_style_opa(pa, LV_OPA_COVER, LV_STATE_CHECKED); lv_obj_set_style_opa(pa, LV_OPA_TRANSP, ST_OFF); }
  } else if (lv_obj_t *g = glyph(b, ic, set ? fw::ic::icon : fw::ic::glyph, s)) {
    lv_obj_center(g);
    if (multi_part(g)) {  // pressed = the press tint (Card / List row fill): the accent part keeps 3:1 on it
      lv_obj_set_style_bg_color(b, fw::st::pressed_tint(*fw::T), LV_STATE_PRESSED);
      lv_obj_set_style_bg_color(b, fw::st::pressed_tint(*fw::T), LV_STATE_CHECKED | LV_STATE_PRESSED);
    }
  }

  set_state(b, state, false);
  data(b, p);


}
}  // namespace

FW_COMPONENT(icon_button, "Icon button", "components", R"({
  "about": "A button that is only an icon: a round play/pause button, or round and square icon buttons.",
  "frame": [160, 128],
  "icons": ["play@feature", "pause@feature", "play@glyph", "next@glyph", "prev@glyph", "plus@glyph", "close@glyph", "check@glyph", "back@glyph"],
  "props": [
    {"name": "style", "type": "select", "options": ["play", "round", "square"], "default": "round"},
    {"name": "icon", "type": "select", "options": ["next", "prev", "play", "plus", "close", "check", "back"], "default": "next"},
    {"name": "state", "type": "select", "options": ["default", "pressed", "active", "active-pressed", "disabled", "unavailable"], "default": "default"}
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
    {"label": "round · unavailable", "props": {"state": "unavailable"}}
  ]
})") {
  lv_obj_t *b = lv_obj_create(parent);
  auto *d = new Data{R_TAP, {}};
  lv_obj_set_user_data(b, d);
  fill(b, p);
  fw::st::on_tap(b, [](lv_event_t *e) {  // blocked on finger down, a real tap on CLICKED (states.json)
    if (!fw::st::tap_now(e)) return;
    auto *o = (lv_obj_t *) lv_event_get_current_target(e);
    auto *d = (Data *) lv_event_get_user_data(e);
    if (lv_obj_has_state(o, ST_OFF)) { fw::st::blocked(o, d->icon); return; }
    if (d->role == R_BACK) { fw::emit(o, "back"); fw::sound(o, "back"); return; }
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
