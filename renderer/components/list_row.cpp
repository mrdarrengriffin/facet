// List row: the Facet hairline row (docs: Settings rows .fs-set, picker rows .f-prow / pickerRow(), sheet rows), on the
// contract (renderer/CONTRACT.md, "contract": 2). Rows are not boxed: they sit on what is behind them and a hairline
// on top separates them (docs "rows separated by hairlines rather than boxes"); a stack of rows = a list.
//   size     fills its parent's width (C5; in a list Body that is inset to inset, as before); h:list-row-h tall (96 /
//            64); pad-x:4 (16 / 8) so the press fill does not run into the text; gap:4 between the parts
//   minimum  layout:card-min-w x layout:list-row-h. Steps: one (the text column takes the free width and ends in "...")
// Parts (a flex row, centred; nothing placed by hand):
//   icon     optional, the icon size (24 / 16), text-dim
//   text     list-text-* (the free width): an optional over line (type label, caps, text-dim), the title (type title,
//            text, one line, "...") and an optional sub (type body, text-dim) or under line (type label, caps, text-dim;
//            a choice's status, the Picker row's; sub wins). tone working = icon + under in accent-text. Its height is grown so that, centred in the
//            row, every box top lands on the 4 px grid (a derived style: core/fx_core.cpp list-text-*, list-slot)
//   end      a SLOT (meta "slots": "end", C8a): what the props put there (trailing value = type body text-dim in a
//            list-slot, chevron icon, value + chevron, or the Toggle component itself) and/or ONE component a spec puts
//            in it ("in": k, "slot": 0). Empty = hidden (no gap). Callers that append to the root (Picker row's radio,
//            Accent swatches) still land after it, as before.
// Playground: trailing "none" with a value typed in shows the value; "on" with trailing none shows a toggle; a value
// kind with no value shows an em dash (in text: a thin dash in text-dim measures under 4.5:1 at 1x).
// States (tokens per state, C17): default; pressed (fill pressed-tint fading in over nothing: fade-press-fill); on (the
// toggle's); disabled (fw::st::OFF: icon, title, sub, value, chevron disabled-ink, no press look, the toggle's own
// disabled look, a tap = "blocked" + error); unavailable (OFF too: icon + title disabled-ink, no sub, the end = the word
// Unavailable in unavailable-note). n/a: loading, unknown, empty, error (no data of its own: the binding maps those to
// unavailable); a stack with no items = its empty words.
// Tap = a real CLICKED: chevron rows emit "tap" (detail = title) + sound "open"; a toggle row hands the tap to its toggle
// (whole row = hit area); other rows emit "tap" + sound "tap". Bleed: none.
// Stack (list: stack): one row per item of items (core/items.h, the Grid's model). A row = this node's props with
// "$item" / "$item.key" filled; a prop that does not name $item comes from an object item's field of the same name, a
// plain-text item is the title. rows caps the list (a last "+N" row for the rest); no items = the empty words.
// Update: the same structure = texts in place, a toggle slides (its own update), pressed fades; another structure =
// the contents again (fw::refill, snap; disabled / unavailable in or out fades in). Silent.
// Contrast pairs: tokens/colour.json section "list_row" (+ toggle, card).
// Still legacy inside: the Toggle (built through fw::find).
#include "fw.h"
#include "fx.h"
#include "spec.h"
#include "items.h"

namespace {
constexpr lv_state_t ST_OFF = fw::st::OFF;  // disabled / unavailable (LVGL sends no events to LV_STATE_DISABLED)

enum Kind { K_NONE, K_VALUE, K_CHEVRON, K_VALUE_CHEVRON, K_TOGGLE };
Kind kind_of(const char *s) {
  if (!strcmp(s, "value")) return K_VALUE;
  if (!strcmp(s, "chevron")) return K_CHEVRON;
  if (!strcmp(s, "value-chevron")) return K_VALUE_CHEVRON;
  if (!strcmp(s, "toggle")) return K_TOGGLE;
  return K_NONE;
}

struct Row {
  Kind kind = K_NONE;
  lv_obj_t *toggle = nullptr;
  lv_obj_t *ti = nullptr, *su = nullptr, *val = nullptr, *ov = nullptr, *un = nullptr;  // title, sub, value, over, under labels (the update path sets their text)
  std::string title;
};

struct Spec {
  const char *title, *icon, *sub, *trailing, *value, *state;
  bool on, divider;
  const char *over = "", *under = "", *tone = "dim";
};
std::string caps(const char *s) {
  std::string o(s);
  for (char &c : o) c = (char) toupper((unsigned char) c);  // ASCII only; UTF-8 bytes pass through
  return o;
}

void row_free(lv_event_t *e) { delete (Row *) lv_event_get_user_data(e); }
Row *row_of(lv_obj_t *r) {  // the Row rides on the root's DELETE event (the root's own user data is the caller's)
  for (uint32_t i = 0, n = lv_obj_get_event_count(r); i < n; i++) {
    lv_event_dsc_t *d = lv_obj_get_event_dsc(r, i);
    if (lv_event_dsc_get_cb(d) == row_free) return (Row *) lv_event_dsc_get_user_data(d);
  }
  return nullptr;
}
Kind effective(const Spec &s) {
  const bool unavail = !strcmp(s.state, "unavailable");
  Kind kind = unavail ? K_NONE : kind_of(s.trailing);
  if (!unavail && kind == K_NONE && s.on) kind = K_TOGGLE;            // playground: on implies a toggle
  else if (!unavail && kind == K_NONE && s.value[0]) kind = K_VALUE;  // a typed value implies a value
  return kind;
}

// one line of the text column: its type, its ink (+ disabled-ink when OFF), one line ending in "..."
lv_obj_t *line(lv_obj_t *col, const char *txt, const char *type, const char *fg) {
  lv_obj_t *l = fx::text(col, txt, type, fg);
  fx::use(l, "fill-w");
  fx::use(l, "fg:disabled-ink", ST_OFF);
  lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_DOTS);
  return l;
}
// a word in the end slot, in a body slot on the grid
lv_obj_t *word(lv_obj_t *end, const char *txt, const char *fg) {
  lv_obj_t *b = fx::box(end, "free");
  fx::use(b, "list-slot");
  lv_obj_t *l = fx::text(b, txt, "type:body", fg);
  fx::use(l, "fg:disabled-ink", ST_OFF);
  return l;
}
void show_end(lv_obj_t *en) {
  if (lv_obj_get_child_count(en)) lv_obj_remove_flag(en, LV_OBJ_FLAG_HIDDEN); else lv_obj_add_flag(en, LV_OBJ_FLAG_HIDDEN);
}

// the row's look and contents on r (build: a new root; update: the same root after fw::refill)
void fill(lv_obj_t *r, const Spec &s, Row *d) {
  const bool unavail = !strcmp(s.state, "unavailable"), off = unavail || !strcmp(s.state, "disabled");
  const Kind kind = effective(s);

  for (const char *t : {"box", "row-mid", "gap:4", "pad-x:4", "tall:list-row-h", "bg:pressed-tint", "bg-none", "fade-release-fill"}) fx::use(r, t);
  if (s.divider) { fx::use(r, "line:line"); fx::use(r, "rule-top"); }
  fx::use(r, "bg:pressed-tint", LV_STATE_PRESSED);
  fx::use(r, "fade-press-fill", LV_STATE_PRESSED);
  fx::use(r, "bg-none", ST_OFF);                      // no press look when disabled / unavailable
  fx::use(r, "bg-none", ST_OFF | LV_STATE_PRESSED);
  fx::use(r, "part:list-row");  // the theme's list row (C46), after the row's own state tokens
  lv_obj_add_flag(r, LV_OBJ_FLAG_CLICKABLE);

  const bool working = !strcmp(s.tone, "working");
  if (strcmp(s.icon, "none")) fx::icon_use(fx::icon(r, s.icon, "icon", working ? "fg:accent-text" : "fg:text-dim"), "ink:disabled-ink", ST_OFF);

  const bool sub = !unavail && s.sub[0], over = s.over[0], under = !unavail && !sub && s.under[0];
  lv_obj_t *col = fx::box(r, "free");  // an under line is a label, like an over line: the same column height
  fx::use(col, over ? (sub || under ? "list-text-ots" : "list-text-ot") : (sub ? "list-text-ts" : under ? "list-text-ot" : "list-text-t"));
  d->ov = over ? line(col, caps(s.over).c_str(), "type:label", "fg:text-dim") : nullptr;
  d->ti = line(col, s.title, "type:title", "fg:text");
  d->su = sub ? line(col, s.sub, "type:body", "fg:text-dim") : nullptr;
  d->un = under ? line(col, caps(s.under).c_str(), "type:label", working ? "fg:accent-text" : "fg:text-dim") : nullptr;

  lv_obj_t *en = fx::box(r, "free");  // the end slot (meta "slots": end)
  fx::slot(en, "end");
  for (const char *t : {"row-end", "gap:4"}) fx::use(en, t);
  lv_obj_add_flag(en, fw::spec::SLOT);
  lv_obj_add_event_cb(en, [](lv_event_t *e) { show_end((lv_obj_t *) lv_event_get_current_target(e)); }, LV_EVENT_CHILD_CHANGED, nullptr);

  d->kind = kind;
  d->title = s.title;
  d->toggle = nullptr;
  d->val = nullptr;
  if (unavail) word(en, fw::st::UNAVAILABLE_TEXT, "fg:unavailable-note");
  if (kind == K_VALUE || kind == K_VALUE_CHEVRON) d->val = word(en, s.value[0] ? s.value : fw::st::PLACEHOLDER, s.value[0] ? "fg:text-dim" : "fg:text");
  if (kind == K_CHEVRON || kind == K_VALUE_CHEVRON) fx::icon_use(fx::icon(en, "chevron", "icon", "fg:text-dim"), "ink:disabled-ink", ST_OFF);
  if (kind == K_TOGGLE) {
    if (const fw::Component *tc = fw::find("toggle")) {  // the Toggle component itself, not a copy
      ArduinoJson::JsonDocument tp;
      tp["on"] = s.on;
      tp["state"] = off ? "disabled" : "default";
      tc->build(en, tp.as<fw::Props>());
      d->toggle = lv_obj_get_child(en, -1);
      lv_obj_remove_flag(d->toggle, LV_OBJ_FLAG_CLICKABLE);  // the row is the hit area and hands the tap on
    }
  }
  show_end(en);
  // the row's press reaches its texts and icons (the theme's pressed text ink, C46); the end Toggle keeps its own states
  struct T { static void trickle(lv_obj_t *o, lv_obj_t *skip) {
    if (o == skip || !lv_obj_get_child_count(o)) return;
    lv_obj_add_flag(o, LV_OBJ_FLAG_STATE_TRICKLE);
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) trickle(lv_obj_get_child(o, (int32_t) i), skip);
  } };
  T::trickle(r, d->toggle);

  if (!strcmp(s.state, "pressed")) lv_obj_add_state(r, LV_STATE_PRESSED);
  if (off) fx::state(r, ST_OFF, true);
}

// the root (made once): the Row + the tap handler
lv_obj_t *root(lv_obj_t *parent) {
  lv_obj_t *r = lv_obj_create(parent);  // styles: fill() (an update refills the same root)
  lv_obj_remove_style_all(r);
  lv_obj_remove_flag(r, LV_OBJ_FLAG_SCROLLABLE);
  fx::name(r, "list_row");
  auto *d = new Row;
  lv_obj_add_event_cb(r, row_free, LV_EVENT_DELETE, d);
  fw::st::on_tap(r, [](lv_event_t *e) {  // blocked on finger down, a real tap on CLICKED (states.json)
    if (!fw::st::tap_now(e)) return;
    auto *o = (lv_obj_t *) lv_event_get_current_target(e);
    auto *d = (Row *) lv_event_get_user_data(e);
    if (lv_obj_has_state(o, ST_OFF)) { fw::st::blocked(o, d->title.c_str()); return; }
    if (d->toggle) { lv_obj_send_event(d->toggle, LV_EVENT_CLICKED, nullptr); return; }
    fw::emit(o, "tap", d->title.c_str());
    fw::sound(o, d->kind == K_CHEVRON || d->kind == K_VALUE_CHEVRON ? "open" : "tap");
  }, d);
  return r;
}

Spec spec_of(fw::Props p) {
  return {p["title"] | "", p["icon"] | "bulb", p["sub"] | "", p["trailing"] | "none", p["value"] | "",
          p["state"] | "default", p["on"] | false, p["divider"] | true, p["over"] | "", p["under"] | "", p["tone"] | "dim"};
}
std::string stack_sig(fw::Props p);  // below: a stack's row count
// the props that need the contents again: all but a pressed state and a toggle's on (those move in place)
std::string sig(fw::Props p) {
  ArduinoJson::JsonDocument r;
  if (!strcmp(p["list"] | "single", "stack")) return "stack|" + stack_sig(p);
  r.set(p);
  if (!strcmp(p["state"] | "default", "pressed")) r["state"] = "default";
  if (effective(spec_of(p)) == K_TOGGLE) { r.remove("on"); r["trailing"] = "toggle"; }
  // texts move in place (a light's "Off" -> "On · 64%" must not rebuild the row: its Toggle would snap); only whether
  // there is a sub / a value is structure
  r["title"] = "";
  r["sub"] = (p["sub"] | "")[0] ? "sub" : "";
  r["over"] = (p["over"] | "")[0] ? "over" : "";
  r["under"] = (p["under"] | "")[0] ? "under" : "";
  if (r["value"].is<const char *>()) r["value"] = (p["value"] | "")[0] ? "value" : "";
  std::string s;
  serializeJson(r, s);
  return s;
}
constexpr int LIST_MAX = 24;  // list_items: rows a stack makes at most
const char *const ROW_FIELDS[] = {"title", "icon", "sub", "over", "under", "tone", "trailing", "value", "on", "state"};
const char *const ROW_SKIP[] = {"list", "items", "rows", "empty", "_items"};
// the stack's rows: the props of each item that fits, then a "+N" row for the rest
std::vector<ArduinoJson::JsonDocument> stack_rows(fw::Props p) {
  std::vector<ArduinoJson::JsonDocument> out;
  const std::vector<std::string> all = fw::items::parse(p["items"] | "");
  const int rows = p["rows"] | 0, cap = rows > 0 ? std::min(rows, LIST_MAX) : LIST_MAX;
  const int n = (int) all.size() > cap ? cap - 1 : (int) all.size();
  for (int i = 0; i < n; i++) {
    out.emplace_back();
    ArduinoJson::JsonDocument &d = out.back();
    fw::items::row_props(p, all[i], ROW_FIELDS, 10, ROW_SKIP, 5, d);
    if (d["on"].is<const char *>()) d["on"] = !strcmp(d["on"] | "", "on") || !strcmp(d["on"] | "", "true");  // an HA state
  }
  if ((int) all.size() > n) {
    out.emplace_back();
    ArduinoJson::JsonDocument &m = out.back();
    m["title"] = "+" + std::to_string(all.size() - n);
    m["icon"] = "none";
  }
  return out;
}
// the stack's signature part: its row count (texts and toggles of the same rows move in place)
std::string stack_sig(fw::Props p) { return std::to_string(stack_rows(p).size()) + "|" + (p["empty"] | ""); }
void empty_row(lv_obj_t *r, const char *txt) {
  lv_obj_t *b = fx::box(r, "row-mid");
  for (const char *t : {"gap:4", "pad-x:4", "tall:list-row-h"}) fx::use(b, t);
  word(b, txt, "fg:text-dim");
}
void fill_props(lv_obj_t *r, fw::Props p) {
  fw::shape(r, sig(p));
  if (!strcmp(p["list"] | "single", "stack")) {  // a list: rows touch, the hairline on top of each = the divider
    for (const char *t : {"box", "col"}) fx::use(r, t);
    lv_obj_remove_flag(r, LV_OBJ_FLAG_CLICKABLE);
    std::vector<ArduinoJson::JsonDocument> rows = stack_rows(p);
    if (rows.empty()) empty_row(r, p["empty"] | "Nothing here");
    for (ArduinoJson::JsonDocument &d : rows) fill_props(root(r), d.as<fw::Props>());
    return;
  }
  fill(r, spec_of(p), row_of(r));
}
}  // namespace

FW_COMPONENT(list_row, "List row", "components", R"({
  "about": "A full-width row in a list or settings page, with an optional icon, a title and sub line, and a value, chevron or toggle at the end. A screen can put one more component in the end slot.",
  "contract": 2,
  "parts": ["list-row"],
  "min": ["layout:card-min-w", "layout:list-row-h"],
  "slots": [
    {"name": "end", "count": 1, "about": "One component at the right end, after the value, chevron or toggle the props put there (in + slot 0).", "accepts": ["toggle", "status_chip", "count_badge", "icon_button", "button", "label"], "max": 1}
  ],
  "audit": {"base": {"trailing": "toggle"}},
  "frame": [448, 0],
  "icons": ["bulb@icon", "speaker@icon", "tv@icon", "volume@icon", "power@icon", "house@icon", "chevron@icon"],
  "props": [
    {"name": "title", "about": "The main words of the row.", "type": "text", "default": "Lights"},
    {"name": "icon", "about": "The icon at the start of the row, or none.", "type": "select", "options": ["none", "bulb", "speaker", "tv", "volume", "power", "house"], "default": "bulb"},
    {"name": "sub", "about": "A quieter second line under the title. Leave empty for none.", "type": "text", "default": ""},
    {"name": "under", "about": "A small capitals line under the title, such as a speaker's status in a choose-one list. A sub line takes its place.", "type": "text", "default": ""},
    {"name": "tone", "about": "The icon and under line colour: dim for normal, working for something going on now, such as playing.", "type": "select", "options": ["dim", "working"], "default": "dim"},
    {"name": "over", "about": "A small line above the title, for extra detail such as a flight's height and distance. Leave empty for none.", "type": "text", "default": ""},
    {"name": "trailing", "about": "What sits at the right end: none, a value, a chevron to show it opens more, a value and a chevron, or an on/off switch.", "type": "select", "options": ["none", "value", "chevron", "value-chevron", "toggle"], "default": "none"},
    {"name": "value", "about": "The value shown at the right end when there is one, such as 42%.", "type": "text", "default": ""},
    {"name": "on", "about": "Whether the switch at the right end is on.", "type": "bool", "default": false},
    {"name": "state", "about": "How the row looks: default ready to tap, pressed while held, disabled when it cannot be used, or unavailable when its device cannot be reached.", "type": "select", "options": ["default", "pressed", "disabled", "unavailable"], "default": "default"},
    {"name": "divider", "about": "Draws a thin line under the row to separate it from the next.", "type": "bool", "default": true},
    {"name": "list", "about": "single is one row on its own; stack is a list of rows touching, with lines between them, one row per item of items.", "type": "select", "options": ["single", "stack"], "default": "single"},
    {"name": "items", "about": "With list stack: the rows, a comma list or a JSON list, usually bound to an entity attribute. Each row is this node's props with $item (or $item.field) filled in; a plain item is the row's title, and an object item's title (or name), icon, sub, over, under, tone, trailing, value, on and state fields fill the props that do not name $item. No items shows the empty words, never sample rows.", "type": "text", "default": ""},
    {"name": "rows", "about": "With list stack: at most this many rows; 0 for up to 24. A longer list ends with a +N row for the rest.", "type": "range", "min": 0, "max": 24, "step": 1, "default": 0},
    {"name": "empty", "about": "With list stack: the words shown when items is empty.", "type": "text", "default": "Nothing here"}
  ],
  "variants": [
    {"label": "icon + title", "props": {}},
    {"label": "title + sub", "props": {"title": "Living room", "icon": "none", "sub": "3 lights on"}},
    {"label": "over + title", "props": {"title": "Brussels \u2192 Nottingham", "icon": "none", "over": "B763 \u00b7 21,000 ft \u00b7 22 km S"}},
    {"label": "icon + title + sub", "props": {"title": "Kitchen", "icon": "bulb", "sub": "Off since 18:40"}},
    {"label": "trailing value", "props": {"title": "Brightness", "icon": "none", "trailing": "value", "value": "80%"}},
    {"label": "trailing chevron", "props": {"title": "Sounds", "icon": "volume", "trailing": "chevron"}},
    {"label": "value + chevron", "props": {"title": "Theme", "icon": "none", "trailing": "value-chevron", "value": "Dark"}},
    {"label": "toggle · off", "props": {"title": "Chime", "icon": "speaker", "trailing": "toggle"}},
    {"label": "toggle · on", "props": {"title": "Chime", "icon": "speaker", "sub": "Plays on every tap", "trailing": "toggle", "on": true}},
    {"label": "pressed", "props": {"title": "Sounds", "icon": "volume", "trailing": "chevron", "state": "pressed"}},
    {"label": "disabled · toggle", "props": {"title": "Night dimming", "icon": "power", "sub": "Needs a clock source", "trailing": "toggle", "on": true, "state": "disabled"}},
    {"label": "disabled · value", "props": {"title": "Brightness", "icon": "none", "trailing": "value-chevron", "value": "80%", "state": "disabled"}},
    {"label": "unavailable", "props": {"title": "Porch light", "icon": "bulb", "trailing": "toggle", "state": "unavailable"}},
    {"label": "stack · dividers", "props": {"list": "stack", "items": "[{\"title\":\"Lights\",\"icon\":\"bulb\",\"trailing\":\"value-chevron\",\"value\":\"3 on\"},{\"title\":\"Chime\",\"icon\":\"speaker\",\"sub\":\"Plays on every tap\",\"trailing\":\"toggle\",\"on\":true},{\"title\":\"Volume\",\"icon\":\"volume\",\"trailing\":\"value\",\"value\":\"60%\"},{\"title\":\"Night dimming\",\"icon\":\"power\",\"trailing\":\"toggle\",\"state\":\"disabled\"},{\"title\":\"Porch light\",\"icon\":\"house\",\"trailing\":\"chevron\",\"state\":\"unavailable\"}]"}},
    {"label": "stack · plain items · cap", "props": {"list": "stack", "icon": "none", "trailing": "chevron", "items": "Kitchen,Hallway,Study,Bedroom,Garden", "rows": 3}},
    {"label": "stack · empty", "props": {"list": "stack", "empty": "No rooms yet"}}
  ]
})") {
  fill_props(root(parent), p);
}

// Update path: default <-> pressed = the press fade in place, a toggle row's on = the Toggle's own update (the knob
// slides, silent); any other prop (title, icon, trailing, value, tone, disabled / unavailable, stack) = the contents
// again in the same root (fw::refill; colours that change fade like a tap; disabled / unavailable in or out fades them
// in). Silent.
FW_UPDATE(list_row) {
  Row *d = row_of(root);
  if (!d) return false;
  const bool pr = !strcmp(p["state"] | "default", "pressed");
  if (fw::shape(root, sig(p))) {
    if (!strcmp(p["list"] | "single", "stack")) {  // the same rows: each one's own update (texts in place, toggles slide)
      std::vector<ArduinoJson::JsonDocument> rows = stack_rows(p);
      for (uint32_t i = 0; i < rows.size() && i < lv_obj_get_child_count(root); i++)
        fw::update_child(fw::find("list_row"), lv_obj_get_child(root, i), rows[i].as<fw::Props>());
      return true;
    }
    if (pr != lv_obj_has_state(root, LV_STATE_PRESSED)) lv_obj_set_state(root, LV_STATE_PRESSED, pr);
    const Spec sp = spec_of(p);
    auto put = [](lv_obj_t *l, const char *type, const char *t) { if (l) fx::set_text(l, t, type); };
    put(d->ti, "type:title", sp.title);
    d->title = sp.title;
    put(d->su, "type:body", sp.sub);
    put(d->ov, "type:label", caps(sp.over).c_str());
    put(d->un, "type:label", caps(sp.under).c_str());
    put(d->val, "type:body", sp.value[0] ? sp.value : fw::st::PLACEHOLDER);  // empty <-> a value is structure (sig)
    if (d->toggle) {
      ArduinoJson::JsonDocument tp;
      tp["on"] = p["on"] | false;
      tp["state"] = "default";
      fw::update_child(fw::find("toggle"), d->toggle, tp.as<fw::Props>());
    }
    return true;
  }
  const bool was_off = lv_obj_has_state(root, ST_OFF);
  fw::st::refill_fade(root, [&] { fw::refill(root); fill_props(root, p); }, true, true);  // a new tone fades like a tap (shared styles too)
  if (pr != lv_obj_has_state(root, LV_STATE_PRESSED)) lv_obj_set_state(root, LV_STATE_PRESSED, pr);
  if (was_off != lv_obj_has_state(root, ST_OFF)) fw::st::fade_in_children(root);
  return true;
}
