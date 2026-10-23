// Card: the Facet Home card (docs: @facet/ui layout.ts card(), CSS .fs-card). One screen's summary on Home; a tap
// opens that screen. Fixed size from the layout tokens, two per row:
//   720: 320 x 280 (card-w x card-h), padding card-pad 24      480: 212 x 184, padding 16
//   surface fill, 1 px line border, radius card (0); children count from the outer edge (pad - hair, = docs)
// Layout (flex column, nothing hand-placed):
//   top row      screen icon (icon size, text-dim) + caps label (type label, text-dim) + status dot (accent, only when
//                the card is "accent"), the preview artwork top right at the icon size token "card" (72 / 48): a
//                weather icon, or a card preview (core/previews.h: agents, media, dial, ... lightning)
//   (grow)
//   value        type value (64 / 44 box, text; accent card = accent-text), one line, ellipsis
//     or text    type title, up to two lines bottom-aligned at the same place (a track name), text
//   gap          space 9 (32 / 20): value box bottom = card-value-bottom from the card edge (88 / 56, = docs)
//   sub          type body, one line, text-dim; tone ok / warn / error = that status colour
// Dot: docs size-dot 8 (720) / 6 (480) -> space 2 (8 / 4) for the 4 px grid.
// Pressed, Motion press / release colour transitions; the style is an option (Darren round 4), state token
// states.json pressed.style (fw::st::PRESS_STYLE, default "fill"; a theme may set it later, facet-130), prop "press"
// overrides it:
//   fill         press-tint fill + line-hi border
//   border       fill stays surface, 1 px accent border
//   border-fill  press-tint fill + 1 px accent border
// press-tint (Darren rounds 4-5) = a very faint, greyed accent tint over the surface, not the docs' grey surface-press:
// mix token in colour.json: the accent moved halfway to text-mute (components.card.press-desat 0.5), then 5 % over the
// surface (card.press-tint 0.05). On RGB565 a tint this faint lands on one 5/6-bit step per channel, so opacity alone
// barely changes it (0.03-0.05 give the same dark-theme ember step); desaturation is what makes it gentler.
// Unavailable (not in the docs; Facet disabled rule, like Button): the entity behind the card is gone. Icon, label and
// value text-mute, value shows an em dash, sub "Unavailable" in text-dim, no preview, no press look; a tap emits
// "blocked" + plays error.
// Tap = a real CLICKED: fw::emit "tap" (detail = the label) + fw::sound "open" (sound.json: going deeper, a card opens
// its screen). Contrast pairs: tokens/colour.json section "card".
#include "fw.h"
#include "previews.h"
// audit deps: card_previews

namespace {
constexpr lv_state_t ST_OFF = fw::st::OFF;  // unavailable (LVGL sends no events to LV_STATE_DISABLED; see Button)

const lv_style_prop_t TR_PROPS[] = {LV_STYLE_BG_COLOR, LV_STYLE_BORDER_COLOR, LV_STYLE_PROP_INV};
const lv_style_transition_dsc_t *tr(bool pressed) {
  static lv_style_transition_dsc_t d[2];
  static bool ready = false;
  if (!ready) { fw::st::press_transitions(d, TR_PROPS); ready = true; }  // states.json pressed motion
  return &d[pressed];
}

lv_obj_t *box(lv_obj_t *parent) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  return o;
}

lv_color_t tone(const char *t) {
  if (!strcmp(t, "ok")) return fw::T->ok;
  if (!strcmp(t, "warn")) return fw::T->warn;
  if (!strcmp(t, "error")) return fw::T->error;
  return fw::T->text_dim;
}
}  // namespace

namespace {
// the label (top row > label box > the label), looked up so a tap after an update reads the live one
lv_obj_t *label_of(lv_obj_t *c) {
  lv_obj_t *lab = lv_obj_get_child(lv_obj_get_child(c, 0), 0);
  for (uint32_t i = 0; lab && i < lv_obj_get_child_count(lab); i++)
    if (lv_obj_check_type(lv_obj_get_child(lab, (int32_t) i), &lv_label_class)) return lv_obj_get_child(lab, (int32_t) i);
  return c;  // never: a card always has its label
}
// the props without the press state: equal = only the press moves
std::string sig(fw::Props p) {
  ArduinoJson::JsonDocument rest;
  rest.set(p);
  if (strcmp(p["state"] | "default", "unavailable")) rest["state"] = "default";
  std::string s;
  serializeJson(rest, s);
  return s;
}
void fill(lv_obj_t *c, fw::Props p) {
  fw::shape(c, sig(p));
  const char *state = p["state"] | "default";
  const bool off = !strcmp(state, "unavailable");
  const bool accent = !off && (p["accent"] | false);
  const bool text_mode = !strcmp(p["mode"] | "value", "text");
  const fw::Theme &t = *fw::T;
  const lv_color_t dim = off ? fw::st::unavailable_ink(t) : t.text_dim;

  lv_obj_remove_style_all(c);
  lv_obj_add_flag(c, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(c, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(c, fw::lay::card_w, fw::lay::card_h);
  lv_obj_set_style_radius(c, fw::rad::card, 0);
  lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(c, fw::stroke::hair, 0);
  lv_obj_set_style_pad_all(c, fw::lay::card_pad - fw::stroke::hair, 0);
  lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
  // looks: default / pressed / unavailable (USER_1 outranks PRESSED: no press look)
  const lv_style_selector_t SEL[3] = {0, LV_STATE_PRESSED, ST_OFF};
  const char *press = p["press"] | fw::st::PRESS_STYLE;
  const bool p_fill = strcmp(press, "border") != 0, p_border = strcmp(press, "fill") != 0;
  // states.json pressed roles: tint = mix press-tint (accent tint over the surface at components.card.press-tint)
  const lv_color_t BG[3] = {t.surface, p_fill ? fw::st::pressed_tint(t) : t.surface, t.surface};
  const lv_color_t BD[3] = {t.line, p_border ? fw::st::pressed_tint_line(t) : fw::st::pressed_line(t), t.line};
  for (int i = 0; i < 3; i++) {
    lv_obj_set_style_bg_color(c, BG[i], SEL[i]);
    lv_obj_set_style_border_color(c, BD[i], SEL[i]);
    lv_obj_set_style_transition(c, tr(i == 1), SEL[i]);
  }

  // top row: icon + label + dot, preview at the right edge (top-aligned, so the label row sits on the padding line)
  lv_obj_t *top = box(c);
  lv_obj_set_size(top, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(top, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(top, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_t *lab = box(top);
  // fixed height (icon 16 / 24 = label box): a content height would follow the label's caps translate (15 at 480)
  lv_obj_set_size(lab, LV_SIZE_CONTENT, std::max<int>(fw::ic::icon, fw::style(fw::ty::label).box));
  lv_obj_set_flex_grow(lab, 1);
  lv_obj_set_flex_flow(lab, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(lab, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(lab, fw::sp::s2, 0);
  const char *ic = p["icon"] | "weather";
  if (strcmp(ic, "none")) fw::icon(lab, ("screen-" + std::string(ic)).c_str(), fw::ic::icon, dim);
  lv_obj_t *l = fw::text(lab, fw::ty::label, p["label"] | "Weather", dim);
  lv_obj_remove_flag(l, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_update_layout(l);  // the label box on the grid (Label fit8), so the dot after it lands on the grid (audit)
  lv_obj_set_width(l, (lv_obj_get_width(l) + fw::sp::GRID - 1) / fw::sp::GRID * fw::sp::GRID);
  // one line at the type box (16 / 24): fw::text sizes every content-height label to whole type boxes
  {  // the accent dot is always there, shown by its opacity, so an update fades it in / out (like a tap)
    lv_obj_t *dot = box(lab);
    lv_obj_set_size(dot, fw::sp::s2, fw::sp::s2);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(dot, t.accent, 0);
    lv_obj_set_style_bg_opa(dot, accent ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
  }
  const char *pv = p["preview"] | "none";
  if (!off && fw::is_preview(pv)) fw::card_preview(top, pv, p);  // the card artwork (components/card_previews.cpp)
  else if (!off && strcmp(pv, "none")) fw::icon(top, pv, fw::ic::card, t.text_dim);

  lv_obj_t *grow = box(c);
  lv_obj_set_width(grow, 0);
  lv_obj_set_flex_grow(grow, 1);

  const int inner = fw::lay::card_w - 2 * fw::lay::card_pad;
  if (text_mode && !off) {  // up to two lines of title, bottom-aligned (the grow above pushes it down)
    lv_obj_t *tx = fw::text(c, fw::ty::title, p["text"] | "", t.text, inner);
    lv_obj_remove_flag(tx, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_update_layout(tx);
    // fw::text sizes a wrapped label to whole type boxes (28 / 40 a line): at most 2 lines
    const int two = 2 * fw::style(fw::ty::title).box;
    if (lv_obj_get_height(tx) > two) {
      lv_label_set_long_mode(tx, LV_LABEL_LONG_MODE_DOTS);
      lv_obj_set_height(tx, two);
    }
  } else {
    // the value's line box (64 / 44) on the grid; the label inside it is pulled up by text_style to centre the caps
    // (translate -6 at 720), content centred in an on-grid box
    lv_obj_t *vb = box(c);
    lv_obj_set_size(vb, inner, fw::style(fw::ty::value).box);
    lv_obj_add_flag(vb, LV_OBJ_FLAG_OVERFLOW_VISIBLE);  // the pulled-up label must not clip
    lv_obj_t *v = fw::text(vb, fw::ty::value, off ? fw::st::PLACEHOLDER : (p["value"] | ""),
                           off ? fw::st::unavailable_ink(t) : accent ? t.accent_text : t.text);
    lv_obj_remove_flag(v, LV_OBJ_FLAG_CLICKABLE);
    lv_label_set_long_mode(v, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_width(v, inner);
    lv_obj_set_height(v, fw::style(fw::ty::value).box);  // set_long_mode resets the text-box height
  }

  lv_obj_t *gap = box(c);
  lv_obj_set_size(gap, 0, fw::sp::s9);  // value box bottom = card-value-bottom (88 / 56) from the card edge

  const char *sub = off ? fw::st::UNAVAILABLE_TEXT : (p["sub"] | "");
  lv_obj_t *s = fw::text(c, fw::ty::body, sub, off ? fw::st::unavailable_note(t) : tone(p["tone"] | "dim"));
  lv_obj_remove_flag(s, LV_OBJ_FLAG_CLICKABLE);
  lv_label_set_long_mode(s, LV_LABEL_LONG_MODE_DOTS);
  lv_obj_set_width(s, inner);
  lv_obj_set_height(s, fw::style(fw::ty::body).box);

  if (!strcmp(state, "pressed") != lv_obj_has_state(c, LV_STATE_PRESSED)) lv_obj_set_state(c, LV_STATE_PRESSED, !strcmp(state, "pressed"));
  if (off) lv_obj_add_state(c, ST_OFF);


}
}  // namespace

FW_COMPONENT(card, "Card", "components", R"({
  "about": "A tappable Home card: an icon and label, a value or short text, and a supporting line.",
  "frame": [244, 220],
  "icons": ["screen-weather@icon", "screen-media@icon", "screen-timer@icon", "screen-energy@icon", "screen-light@icon",
            "partly-day@card", "rain-light@card", "clear-night@card"],
  "props": [
    {"name": "label", "type": "text", "default": "Weather"},
    {"name": "icon", "type": "select", "options": ["none", "weather", "media", "timer", "energy", "light"], "default": "weather"},
    {"name": "preview", "type": "select", "options": ["none", "partly-day", "rain-light", "clear-night", "agents", "media", "dial", "switch", "clock", "timer", "github", "energy", "lightning"], "default": "partly-day"},
    {"name": "mode", "type": "select", "options": ["value", "text"], "default": "value"},
    {"name": "value", "type": "text", "default": "13°"},
    {"name": "text", "type": "text", "default": "Paper Lanterns on a Quiet Evening"},
    {"name": "sub", "type": "text", "default": "Dry for the next hour"},
    {"name": "tone", "type": "select", "options": ["dim", "ok", "warn", "error"], "default": "dim"},
    {"name": "accent", "type": "bool", "default": false},
    {"name": "state", "type": "select", "options": ["default", "pressed", "unavailable"], "default": "default"},
    {"name": "press", "type": "select", "options": ["fill", "border", "border-fill"], "default": "fill"}
  ],
  "variants": [
    {"label": "value · icon + preview", "props": {}},
    {"label": "pressed · fill", "props": {"state": "pressed", "press": "fill"}},
    {"label": "pressed · border", "props": {"state": "pressed", "press": "border"}},
    {"label": "pressed · border + fill", "props": {"state": "pressed", "press": "border-fill"}},
    {"label": "text · two lines", "props": {"label": "Media", "icon": "media", "preview": "none", "mode": "text", "sub": "The Quiet Hours"}},
    {"label": "accent · needs you", "props": {"label": "Timer", "icon": "timer", "preview": "none", "value": "04:32", "sub": "Pasta", "accent": true}},
    {"label": "sub tone · warn", "props": {"preview": "rain-light", "value": "9°", "sub": "Rain from 15:00", "tone": "warn"}},
    {"label": "preview · energy bars", "props": {"label": "Energy", "icon": "energy", "preview": "energy", "value": "£2.42", "sub": "Today so far"}},
    {"label": "no icon, no preview", "props": {"label": "Energy", "icon": "none", "preview": "none", "value": "£2.42", "sub": "Today so far"}},
    {"label": "unavailable", "props": {"label": "Lights", "icon": "light", "preview": "none", "value": "65%", "sub": "", "state": "unavailable"}}
  ]
})") {
  lv_obj_t *c = lv_obj_create(parent);
  fill(c, p);

  fw::st::on_tap(c, [](lv_event_t *e) {  // blocked on finger down, a real tap on CLICKED (states.json)
    if (!fw::st::tap_now(e)) return;
    auto *o = (lv_obj_t *) lv_event_get_current_target(e);
    const char *d = lv_label_get_text(label_of(o));  // looked up: an update may have made it again
    if (lv_obj_has_state(o, ST_OFF)) fw::st::blocked(o, d);
    else { fw::emit(o, "tap", d); fw::sound(o, "open"); }
  }, nullptr);
}

// Update path: pressed <-> default = the press fade (LVGL state transition); every other prop builds the contents again
// in the same card (fw::refill, snap); default <-> unavailable fades the new contents in (fw::st::fade_in_children).
// Silent.
FW_UPDATE(card) {
  const char *state = p["state"] | "default";
  const bool was_off = lv_obj_has_state(root, ST_OFF), off = !strcmp(state, "unavailable");
  if (fw::shape(root, sig(p)) && was_off == off) {
    const bool pr = !strcmp(state, "pressed");
    if (pr != lv_obj_has_state(root, LV_STATE_PRESSED)) lv_obj_set_state(root, LV_STATE_PRESSED, pr);
    return true;
  }
  fw::st::refill_fade(root, [&] { fw::refill(root); fill(root, p); });  // a new tone / the dot fade (like a tap)
  if (was_off != off) fw::st::fade_in_children(root);
  return true;
}
