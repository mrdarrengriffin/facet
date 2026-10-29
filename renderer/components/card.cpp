// Card: the Facet Home card (docs: @facet/ui layout.ts card(), CSS .fs-card). One screen's summary on Home; a tap
// opens that screen. CONTRACT v2 (renderer/CONTRACT.md, release piece 2, 2026-10-26): built only from token names
// through core/fx.h. It has NO width or height of its own: it fills its parent (the Home grid cell gives it its size;
// Darren 2026-10-26: no predefined width, so no size prop). It DECLARES a minimum (meta "min": layout:card-min-w x
// card-min-h) and scales between that and any size above it in two steps, each picked from its own content box:
//   full      (the approved card, 320 x 280 / 212 x 184 at the docs size): top row icon + caps label + accent dot, the
//             preview artwork top right (icon size card 72 / 48); (grow); value (type value) or up to two lines of
//             text (type title, bottom-aligned); card-gap (space 9: value bottom = 88 / 56 from the edge); sub (body)
//   compact   when the full card does not fit its height: no preview, value / text one line in type title, space 2
//             before the sub. card-min-h is exactly what the compact step needs (a test proves the sum).
//   A label too long for its row takes the row's free width and ends in "..." (the dot then sits at the row's end).
// Below the minimum the bounds proof fails by design. Every text is one line with "...", except text mode (two).
// Look: bg:surface, border:line (hair), radius:card, padding card-pad (layout:card-pad less the hair, children count
// from the outer edge = docs). Accent card: value fg:accent-text + the dot (w:dot, bg:accent). Sub tone ok / warn /
// error = fg:<that status>, else text-dim.
// Pressed: Motion press / release fades (fade-press / fade-release); the style (states.json pressed.style, prop
// "press"): fill = bg:pressed-tint + border:pressed-line; border = surface + border:pressed-tint-line; border-fill =
// both tints. press-tint = the accent greyed halfway to text-mute, 5 % over the surface (tokens/colour.json).
// Unavailable (Facet disabled rule): icon, label and value in the unavailable ink, value an em dash, sub "Unavailable"
// (unavailable-note), no preview, no press look (USER_1 outranks PRESSED); a tap emits "blocked" + plays error.
// Tap = a real CLICKED: fw::emit "tap" (detail = the label) + fw::sound "open". Contrast pairs: colour.json "card".
// The preview artwork (components/card_previews.cpp) is still legacy (holds its colours): its own migration piece.
#include "fw.h"
#include "fx.h"
#include "previews.h"
#include <map>
// audit deps: card_previews

namespace {
constexpr lv_state_t ST_OFF = fw::st::OFF;  // unavailable (LVGL sends no events to LV_STATE_DISABLED; see Button)
struct Card {
  int step = 0;             // 0 full, 1 compact
  bool long_label = false;  // the label takes the row's free width and ends in "..."
  int w = -1, h = -1;       // the content size the step was picked at
  bool queued = false;
  std::string props;
  lv_obj_t *label = nullptr;
};
std::map<lv_obj_t *, Card> cards;

const char *tone(const char *t) {
  return !strcmp(t, "ok") ? "fg:ok" : !strcmp(t, "warn") ? "fg:warn" : !strcmp(t, "error") ? "fg:error" : "fg:text-dim";
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
lv_obj_t *line(lv_obj_t *parent, const char *s, const char *type, const char *fg) {  // one line, full width, "..."
  lv_obj_t *o = fx::text(parent, s, type, fg);
  fx::use(o, "fill-w");
  lv_label_set_long_mode(o, LV_LABEL_LONG_MODE_DOTS);
  return o;
}

void fill(lv_obj_t *c, fw::Props p) {
  Card &k = cards[c];
  fw::shape(c, sig(p));
  k.props.clear();
  serializeJson(p, k.props);
  const char *state = p["state"] | "default";
  const bool off = !strcmp(state, "unavailable");
  const bool accent = !off && (p["accent"].is<const char *>() ? !strcmp(p["accent"] | "", "on") : (p["accent"] | false));  // a feed bind gives "on" / "off"
  const bool text_mode = !strcmp(p["mode"] | "value", "text");
  const bool full = k.step == 0;
  const char *dim = off ? "fg:unavailable-ink" : "fg:text-dim";

  lv_obj_remove_style_all(c);
  lv_obj_add_flag(c, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(c, LV_OBJ_FLAG_SCROLLABLE);
  for (const char *s : {"fill", "col", "radius:card", "card-pad", "bg:surface", "border:line", "fade-release"}) fx::use(c, s);
  const char *press = p["press"] | fw::st::PRESS_STYLE;
  const bool p_fill = strcmp(press, "border") != 0, p_border = strcmp(press, "fill") != 0;
  fx::use(c, p_fill ? "bg:pressed-tint" : "bg:surface", LV_STATE_PRESSED);
  fx::use(c, p_border ? "border:pressed-tint-line" : "border:pressed-line", LV_STATE_PRESSED);
  fx::use(c, "fade-press", LV_STATE_PRESSED);
  fx::use(c, "bg:surface", ST_OFF);
  fx::use(c, "border:line", ST_OFF);
  fx::use(c, "part:card");  // the theme's card (C46), after the card's own state tokens
  fx::use(c, "card-move");  // a flush card head (C45): the card keeps only what its edges need (empty in Facet)

  // top row: icon + label + dot, the preview at the right edge (top-aligned: the label row sits on the padding line)
  lv_obj_t *top = fx::box(c, "row-top");
  fx::use(top, "part:card-head");  // the theme's card head (C46): Win98 = the title bar
  fx::use(top, "card-head-move");
  lv_obj_t *lab = fx::box(top, "row");
  fx::use(lab, "gap:2");
  fx::use(lab, "cell");
  fx::use(lab, "card-row");
  const char *ic = p["icon"] | "weather";
  if (strcmp(ic, "none")) fx::icon(lab, ("screen-" + std::string(ic)).c_str(), "icon", dim);
  k.label = fx::text(lab, p["label"] | "Weather", "type:label", dim);
  lv_label_set_long_mode(k.label, LV_LABEL_LONG_MODE_DOTS);
  fx::fit_grid(k.label);  // the dot after it on the grid (docs fit8)
  if (k.long_label) fx::use(k.label, "cell");
  if (accent) {
    lv_obj_t *dot = fx::box(lab, "free");
    for (const char *s : {"w:dot", "h:dot", "radius:round", "bg:accent"}) fx::use(dot, s);
  }
  const char *pv = p["preview"] | "none";
  if (!off && full && !strcmp(pv, "bins")) {  // the real Bins art at card size (C8b; contract, unlike the legacy previews)
    ArduinoJson::JsonDocument bd;
    bd["colours"] = p["colours"] | "black", bd["size"] = "card";
    fw::find("bins_art")->build(top, bd.as<fw::Props>());
  } else if (!off && full && fw::is_preview(pv)) fw::card_preview(top, pv, p);  // the card artwork (legacy, components/card_previews.cpp)
  else if (!off && full && strcmp(pv, "none")) fx::icon(top, pv, "card", "fg:text-dim");

  // the body: everything under the head (Facet: no padding of its own, so it is the card's content box as before)
  lv_obj_t *body = fx::box(c, "col");
  fx::use(body, "grow");
  fx::use(body, "card-body-move");
  lv_obj_t *grow = fx::box(body, "free");
  fx::use(grow, "grow");

  if (text_mode && !off) {  // up to two lines of title, bottom-aligned (the grow above pushes it down); compact: one
    lv_obj_t *tx = line(body, p["text"] | "", "type:title", "fg:text");
    if (full) fx::use(tx, "card-text");
  } else {
    line(body, off ? fw::st::PLACEHOLDER : (p["value"] | ""), full ? "type:value" : "type:title",
         off ? "fg:unavailable-ink" : accent ? "fg:accent-text" : "fg:text");
  }
  fx::use(fx::box(body, "free"), full ? "card-gap" : "card-gap-s");
  line(body, off ? fw::st::UNAVAILABLE_TEXT : (p["sub"] | ""), "type:body", off ? "fg:unavailable-note" : tone(p["tone"] | "dim"));

  if (!strcmp(state, "pressed") != lv_obj_has_state(c, LV_STATE_PRESSED)) lv_obj_set_state(c, LV_STATE_PRESSED, !strcmp(state, "pressed"));
  if (off) lv_obj_add_state(c, ST_OFF);
}

void rebuild(lv_obj_t *c) {
  ArduinoJson::JsonDocument d;
  deserializeJson(d, cards[c].props);
  fw::refill(c);
  fill(c, d.as<ArduinoJson::JsonObjectConst>());
}
bool outside(lv_obj_t *o, const lv_area_t &box) {
  lv_area_t a;
  lv_obj_get_coords(o, &a);
  if (lv_obj_check_type(o, &lv_label_class)) {  // the type's text-box translate (caps centred) is not layout (= the proof)
    const int32_t ty = lv_obj_get_style_translate_y(o, LV_PART_MAIN);
    a.y1 -= ty; a.y2 -= ty;
  }
  return a.x1 < box.x1 || a.y1 < box.y1 || a.x2 > box.x2 || a.y2 > box.y2;
}
// after a layout: does this step fit its own content box? if not, the next step down (never below compact: under the
// declared minimum the bounds proof reports it)
void check(void *v) {
  lv_obj_t *c = (lv_obj_t *) v;
  auto it = cards.find(c);
  if (it == cards.end()) return;
  Card &k = it->second;
  k.queued = false;
  lv_obj_update_layout(c);
  lv_area_t cb, lb, bb;
  lv_obj_get_content_coords(c, &cb);
  lv_obj_t *top = lv_obj_get_child(c, 0), *lab = top ? lv_obj_get_child(top, 0) : nullptr, *body = lv_obj_get_child(c, 1);
  bool tall = false, wide = false;
  for (uint32_t i = 0; i < lv_obj_get_child_count(c); i++) if (outside(lv_obj_get_child(c, (int32_t) i), cb)) tall = true;
  if (body) {  // the body's lines inside its own content box (the head and the body share the card's)
    lv_obj_get_content_coords(body, &bb);
    for (uint32_t i = 0; i < lv_obj_get_child_count(body); i++) if (outside(lv_obj_get_child(body, (int32_t) i), bb)) tall = true;
  }
  if (lab) {
    lv_obj_get_content_coords(lab, &lb);
    for (uint32_t i = 0; i < lv_obj_get_child_count(lab); i++) if (outside(lv_obj_get_child(lab, (int32_t) i), lb)) wide = true;
    if (outside(lab, cb)) wide = true;
  }
  if (wide && !k.long_label) { k.long_label = true; rebuild(c); }
  if (tall && k.step == 0) { k.step = 1; rebuild(c); }
}
void queue(lv_obj_t *c) {
  Card &k = cards[c];
  if (!k.queued) { k.queued = true; lv_async_call(check, c); }
}
void on_size(lv_event_t *e) {  // a new parent size: start again from the full card if it grew, then step down as needed
  lv_obj_t *c = (lv_obj_t *) lv_event_get_current_target(e);
  Card &k = cards[c];
  const int w = lv_obj_get_content_width(c), h = lv_obj_get_content_height(c);
  if (w == k.w && h == k.h) return;
  const bool grew = w > k.w || h > k.h;
  k.w = w; k.h = h;
  if (grew && (k.step || k.long_label)) { k.step = 0; k.long_label = false; rebuild(c); }
  queue(c);
}
void on_delete(lv_event_t *e) {
  lv_obj_t *c = (lv_obj_t *) lv_event_get_current_target(e);
  lv_async_call_cancel(check, c);
  cards.erase(c);
}
}  // namespace

FW_COMPONENT(card, "Card", "components", R"({
  "about": "A tappable Home card: an icon and label, a value or short text, and a supporting line.",
  "contract": 2,
  "parts": ["card", "card-head"],
  "frame": [256, 184],
  "min": ["layout:card-min-w", "layout:card-min-h"],
  "icons": ["screen-weather@icon", "screen-media@icon", "screen-timer@icon", "screen-energy@icon", "screen-light@icon",
            "screen-thermostat@icon", "screen-covers@icon", "screen-calendar@icon", "screen-clock@icon", "screen-switch@icon",
            "partly-day@card", "rain-light@card", "clear-night@card"],
  "props": [
    {"name": "label", "about": "The card's name, shown next to its icon at the top.", "type": "text", "default": "Weather"},
    {"name": "icon", "about": "The small icon before the name, or none.", "type": "select", "options": ["none", "weather", "media", "timer", "energy", "light", "thermostat", "covers", "calendar", "clock", "switch", "agents", "github", "moon", "lightning", "overhead", "bins", "print", "home"], "default": "weather"},
    {"name": "preview", "about": "The small picture at the top right of the card, such as the weather now or the record that is playing, or none.", "type": "select", "options": ["none", "partly-day", "rain-light", "clear-night", "agents", "media", "dial", "switch", "clock", "timer", "github", "energy", "lightning", "bins"], "default": "partly-day"},
    {"name": "colours", "about": "With the bins preview: the bins' colours, separated by commas, the first in front.", "type": "text", "default": "black"},
    {"name": "mode", "about": "What the middle of the card shows: value is a big short value such as 13°; text is a line of words such as the song playing.", "type": "select", "options": ["value", "text"], "default": "value"},
    {"name": "value", "about": "The big value shown in the middle when the mode is value.", "type": "text", "default": "13°"},
    {"name": "text", "about": "The words shown in the middle when the mode is text, such as a song title.", "type": "text", "default": "Paper Lanterns on a Quiet Evening"},
    {"name": "sub", "about": "The quieter supporting line at the bottom of the card.", "type": "text", "default": "Dry for the next hour"},
    {"name": "tone", "about": "The colour of the supporting line: dim for normal, ok for good news, warn for something to watch, error for a problem.", "type": "select", "options": ["dim", "ok", "warn", "error"], "default": "dim"},
    {"name": "accent", "about": "Edges the card in the accent colour to show it needs you now, such as a timer that is running.", "type": "bool", "default": false},
    {"name": "state", "about": "How the card looks: default ready to tap, pressed while held, or unavailable when its device cannot be reached.", "type": "select", "options": ["default", "pressed", "unavailable"], "default": "default"},
    {"name": "press", "about": "How the card shows it is being pressed: fill darkens the whole card, border lights its edge, border-fill does both.", "type": "select", "options": ["fill", "border", "border-fill"], "default": "fill"}
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
    {"label": "long name, big number", "props": {"label": "Living room ceiling light above the table", "icon": "light", "preview": "none", "value": "1234567890 kWh", "sub": "A very long supporting line that cannot fit"}},
    {"label": "unavailable", "props": {"label": "Lights", "icon": "light", "preview": "none", "value": "65%", "sub": "", "state": "unavailable"}}
  ]
})") {
  lv_obj_t *c = lv_obj_create(parent);
  fx::name(c, "card");
  fill(c, p);
  lv_obj_add_event_cb(c, on_size, LV_EVENT_SIZE_CHANGED, nullptr);
  lv_obj_add_event_cb(c, on_delete, LV_EVENT_DELETE, nullptr);
  queue(c);
  fw::st::on_tap(c, [](lv_event_t *e) {  // blocked on finger down, a real tap on CLICKED (states.json)
    if (!fw::st::tap_now(e)) return;
    auto *o = (lv_obj_t *) lv_event_get_current_target(e);
    lv_obj_t *l = cards[o].label;
    const char *d = l ? lv_label_get_text(l) : "";
    if (lv_obj_has_state(o, ST_OFF)) fw::st::blocked(o, d);
    else { fw::emit(o, "tap", d); fw::sound(o, "open"); }
  }, nullptr);
}

// Update path: pressed <-> default = the press fade (LVGL state transition); every other prop builds the contents again
// in the same card at the same step (fw::refill), then the step is checked again; default <-> unavailable fades the new
// contents in (fw::st::fade_in_children). Silent.
FW_UPDATE(card) {
  const char *state = p["state"] | "default";
  const bool was_off = lv_obj_has_state(root, ST_OFF), off = !strcmp(state, "unavailable");
  if (fw::shape(root, sig(p)) && was_off == off) {
    const bool pr = !strcmp(state, "pressed");
    if (pr != lv_obj_has_state(root, LV_STATE_PRESSED)) lv_obj_set_state(root, LV_STATE_PRESSED, pr);
    return true;
  }
  cards[root].long_label = false;  // new words: measure the label again
  fw::st::refill_fade(root, [&] { fw::refill(root); fill(root, p); }, true, true);  // shared: the token styles fade too
  if (was_off != off) fw::st::fade_in_children(root);
  queue(root);
  return true;
}
