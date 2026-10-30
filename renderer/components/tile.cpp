// Tile: the Facet agent tile (docs: @facet/ui layout.ts tile(agent), CSS .fs-tile, Agents screen: 3 across). One agent:
// the mascot in its mood, a two-line title, and a status row (the state word in its tone left, the age or detail right).
//   box      lay::tile_w x tile_h (208 x 208 / 136 x 136: 3 tiles + 2 gutters = the content width at both densities),
//            surface fill, 1 px line border, radius rad::tile (0), padding lay::tile_pad - hair (children count from the
//            outer edge, = docs 23 + 1); flex column, nothing hand-placed
//   mascot   the core mascot runtime (fw::mascot, Mascots foundation; the mascot_mood component comes with Mascots
//            round 2), body top-left on the padding line; mood = docs moodFor(kind, status): error / failed in the
//            status = error, working, needs, idle = sleeping, done = done when it finished <= 2 h ago (else idle).
//            Colour = the mood's own token (mascot.json), which is the docs tile colour per kind (working accent, needs
//            warn, idle text-mute, done steel), so the tile passes no override
//   (grow)
//   title    screen type body, text, up to two lines in a box of 2 x the body box (64 / 40), text at its top (docs
//            line-clamp 2, height 64), ellipsis on the 2nd line
//   gap      space 5 (16 / 12; docs 16 = the title box bottom 64 from the edge: pad 24 + status 24 + 16)
//   status   screen type label (caps), one line: state word left (working text: calm, the mascot carries the accent; needs warn, idle / done
//            text-dim), detail right (text-dim; trailing " ctx" / " ago" dropped like the docs). The docs use text-mute
//            for idle / done and the detail: text-mute is for disabled only here (contrast rule), so text-dim
// Pressed (Motion press / release): the Card press (states.json pressed.style, prop "press" overrides): fill = mix
// press-tint + line-hi border, border = 1 px accent border, border-fill = both. Unavailable (states.json; the agent's
// source is gone): mascot sleeping in st::unavailable_ink (text-mute), title text-mute, status = "Unavailable"
// (text-dim), no press look; tap = "blocked" + error on finger down. The docs tile is static; a tap here = "tap"
// (detail = the title) + sound "open" (a tile opens its agent, like a Card opens its screen).
// No corner ticks (Darren 2026-10-21: cards / tiles get none until the screens). Contrast pairs: tokens/colour.json
// section "tile" (+ mascot).
#include "fw.h"
#include <string>

namespace {
constexpr lv_state_t ST_OFF = fw::st::OFF;

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

lv_obj_t *line(lv_obj_t *parent, fw::ty::Id s, const char *txt, lv_color_t c) {
  lv_obj_t *l = fw::text(parent, s, txt, c);
  lv_obj_remove_flag(l, LV_OBJ_FLAG_CLICKABLE);
  lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_DOTS);
  return l;
}

// docs mascot.ts moodFor(kind, status): core fw::mascot_mood_for (core/mascot.h), shared with the Mascot mood page
fw::mas::Mood mood_for(const char *kind, const std::string &status) { return fw::mascot_mood_for(kind, status.c_str()); }

// a states.json colour role's token name (fw::st::ROLES), for parts that take a token name (the mascot)
const char *role_token(const char *state, const char *role) {
  for (const fw::st::Role &r : fw::st::ROLES)
    if (!strcmp(r.state, state) && !strcmp(r.role, role)) return r.token;
  return nullptr;
}

lv_color_t state_ink(const char *kind) {
  const fw::Theme &t = *fw::T;
  if (!strcmp(kind, "working")) return t.text;  // calm: busy is normal, only needs-you is coloured (the mascot carries the accent)
  if (!strcmp(kind, "needs")) return t.warn;
  return t.text_dim;
}
}  // namespace

namespace {
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
  const fw::Theme &t = *fw::T;
  const char *state = p["state"] | "default";
  const bool off = !strcmp(state, "unavailable");
  const char *kind = p["kind"] | "working";
  const std::string status = p["status"] | "";

  lv_obj_remove_style_all(c);
  fw::shape(c, sig(p));
  lv_obj_add_flag(c, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(c, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(c, fw::lay::tile_w, fw::lay::tile_h);
  lv_obj_set_style_radius(c, fw::rad::tile, 0);
  lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(c, fw::stroke::hair, 0);
  lv_obj_set_style_pad_all(c, fw::lay::tile_pad - fw::stroke::hair, 0);
  lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
  // looks: default / pressed / unavailable (USER_1 outranks PRESSED: no press look); the Card press rules
  const lv_style_selector_t SEL[3] = {0, LV_STATE_PRESSED, ST_OFF};
  const char *press = p["press"] | fw::st::PRESS_STYLE;
  const bool p_fill = strcmp(press, "border") != 0, p_border = strcmp(press, "fill") != 0;
  const lv_color_t BG[3] = {t.surface, p_fill ? fw::st::pressed_tint(t) : t.surface, t.surface};
  const lv_color_t BD[3] = {t.line, p_border ? fw::st::pressed_tint_line(t) : fw::st::pressed_line(t), t.line};
  for (int i = 0; i < 3; i++) {
    lv_obj_set_style_bg_color(c, BG[i], SEL[i]);
    lv_obj_set_style_border_color(c, BD[i], SEL[i]);
    lv_obj_set_style_transition(c, tr(i == 1), SEL[i]);
  }

  // mascot: its box is the body; "!", z and dots draw outside it (fw::mascot bleeds them, inside the tile padding)
  const bool pressed = !strcmp(state, "pressed");
  const fw::mas::Mood mood = off ? fw::mas::sleeping : mood_for(kind, status);
  fw::MascotOpts mo;
  if (off) mo.colour = role_token("unavailable", "ink");  // st::unavailable_ink, by name (the mascot takes a token name)
  else if (pressed && mood == fw::mas::sleeping) mo.colour = role_token("disabled", "on-fill");
  lv_obj_t *m = fw::mascot(c, mood, mo);
  // The sleeping body (text-mute) is 2.9:1 on the press-tint fill in dark (contrast pair tile, ui 3:1): while the
  // tile is pressed it brightens to text-dim (build: the press handler, flag USER_3 = this tile's mascot sleeps)
  lv_obj_set_flag(c, LV_OBJ_FLAG_USER_3, !off && mood == fw::mas::sleeping);
  (void) m;

  lv_obj_t *grow = box(c);
  lv_obj_set_width(grow, 1);
  lv_obj_set_flex_grow(grow, 1);

  const int inner = fw::lay::tile_w - 2 * fw::lay::tile_pad;
  const int body = fw::style(fw::ty::body).box;
  lv_obj_t *ti = fw::text(c, fw::ty::body, p["title"] | "", off ? fw::st::unavailable_ink(t) : t.text, inner);
  lv_obj_remove_flag(ti, LV_OBJ_FLAG_CLICKABLE);
  lv_label_set_long_mode(ti, LV_LABEL_LONG_MODE_DOTS);  // two lines, ellipsis on the 2nd (docs line-clamp 2)
  lv_obj_set_size(ti, inner, 2 * body);

  lv_obj_t *gap = box(c);
  lv_obj_set_size(gap, 1, fw::sp::s5);

  lv_obj_t *row = box(c);
  lv_obj_set_size(row, inner, fw::style(fw::ty::label).box);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_set_style_pad_column(row, fw::sp::s2, 0);
  if (off) {
    lv_obj_t *u = line(row, fw::ty::label, fw::st::UNAVAILABLE_TEXT, fw::st::unavailable_note(t));
    lv_obj_set_size(u, inner, fw::style(fw::ty::label).box);
  } else {
    std::string word = status, detail;
    const size_t dot = status.find(" \xC2\xB7 ");  // "state · detail" (docs split on ' · ')
    if (dot != std::string::npos) { word = status.substr(0, dot); detail = status.substr(dot + 4); }
    for (const char *suf : {" ctx", " ago"}) {
      const size_t n = strlen(suf);
      if (detail.size() > n && detail.compare(detail.size() - n, n, suf) == 0) detail.resize(detail.size() - n);
    }
    lv_obj_t *k = line(row, fw::ty::label, word.c_str(), state_ink(kind));
    lv_obj_set_width(k, 0);  // the word takes the free space and ellipsises; the detail keeps its width
    lv_obj_set_flex_grow(k, 1);
    lv_obj_set_height(k, fw::style(fw::ty::label).box);
    if (!detail.empty()) {
      lv_obj_t *v = line(row, fw::ty::label, detail.c_str(), t.text_dim);
      lv_label_set_long_mode(v, LV_LABEL_LONG_MODE_CLIP);
      lv_obj_update_layout(v);
      const int g = fw::sp::GRID, w = lv_obj_get_width(v);
      lv_obj_set_width(v, (w + g - 1) / g * g);  // fit8 (docs): the box on the grid, so the word's end stays on it
      lv_obj_set_style_text_align(v, LV_TEXT_ALIGN_RIGHT, 0);  // the text ends on the content edge
      lv_obj_set_height(v, fw::style(fw::ty::label).box);
    }
  }

  if (pressed != lv_obj_has_state(c, LV_STATE_PRESSED)) lv_obj_set_state(c, LV_STATE_PRESSED, pressed);
  if (off) lv_obj_add_state(c, ST_OFF);


}
}  // namespace

FW_COMPONENT(tile, "Tile", "components", R"({
  "about": "An agent at a glance: the mascot in its mood, a two-line title and a status row.",
  "frame": [160, 160],
  "props": [
    {"name": "title", "about": "The agent's name, over up to two lines.", "type": "text", "default": "Photo backup"},
    {"name": "kind", "about": "What the agent is doing: working, needs you, done or idle. The mascot's mood follows it.", "type": "select", "options": ["working", "needs", "done", "idle"], "default": "working"},
    {"name": "status", "about": "The status line at the bottom, such as how far it has got.", "type": "text", "default": "working · 42% ctx"},
    {"name": "state", "debug": true, "about": "How the tile looks: default ready to tap, pressed while held, or unavailable when the agent cannot be reached.", "type": "select", "options": ["default", "pressed", "unavailable"], "default": "default"},
    {"name": "press", "about": "How the tile shows it is being pressed: fill darkens the whole tile, border lights its edge, border-fill does both.", "type": "select", "options": ["fill", "border", "border-fill"], "default": "fill"}
  ],
  "variants": [
    {"label": "working", "props": {}},
    {"label": "needs you", "props": {"title": "Review the garden lights PR", "kind": "needs", "status": "needs you · 12m"}},
    {"label": "done · recent", "props": {"title": "Nightly media scan", "kind": "done", "status": "done · 40m ago"}},
    {"label": "done · earlier = idle", "props": {"title": "Calendar sync", "kind": "done", "status": "done · 5h ago"}},
    {"label": "idle · sleeping", "props": {"title": "Energy report", "kind": "idle", "status": "idle · 2d"}},
    {"label": "error", "props": {"title": "Firmware build", "kind": "done", "status": "failed · 3m ago"}},
    {"label": "pressed", "props": {"state": "pressed"}},
    {"label": "unavailable", "props": {"title": "Doorbell watcher", "kind": "idle", "status": "idle · 1h", "state": "unavailable"}}
  ]
})") {
  lv_obj_t *c = lv_obj_create(parent);
  fill(c, p);
  // a sleeping mascot brightens while the tile is pressed (the states.json rule for a muted mark on a fill, disabled
  // on-fill); only the colour changes, the mascot keeps sleeping. The mascot = child 0, looked up (updates make it again)
  lv_obj_add_event_cb(c, [](lv_event_t *e) {
    const lv_event_code_t code = lv_event_get_code(e);
    auto *c = (lv_obj_t *) lv_event_get_current_target(e);
    if (code != LV_EVENT_PRESSED && code != LV_EVENT_RELEASED && code != LV_EVENT_PRESS_LOST) return;
    if (!lv_obj_has_flag(c, LV_OBJ_FLAG_USER_3)) return;
    fw::mascot_set(lv_obj_get_child(c, 0), fw::mas::sleeping, code == LV_EVENT_PRESSED ? role_token("disabled", "on-fill") : nullptr);
  }, LV_EVENT_ALL, nullptr);

  fw::st::on_tap(c, [](lv_event_t *e) {  // blocked on finger down, a real tap on CLICKED (states.json)
    if (!fw::st::tap_now(e)) return;
    auto *o = (lv_obj_t *) lv_event_get_current_target(e);
    const char *d = lv_label_get_text(lv_obj_get_child(o, 2));  // the title (mascot, grow, title), looked up: updates make it again
    if (lv_obj_has_state(o, ST_OFF)) fw::st::blocked(o, d);
    else { fw::emit(o, "tap", d); fw::sound(o, "open"); }
  }, nullptr);
}

// Update path: pressed <-> default = the press fade (LVGL state transition); every other prop builds the contents again
// in the same tile (fw::refill, snap: the mascot starts its mood again); default <-> unavailable fades the new
// contents in (fw::st::fade_in_children). Silent.
FW_UPDATE(tile) {
  const char *state = p["state"] | "default";
  const bool was_off = lv_obj_has_state(root, ST_OFF), off = !strcmp(state, "unavailable");
  if (fw::shape(root, sig(p)) && was_off == off) {
    const bool pr = !strcmp(state, "pressed");
    if (pr != lv_obj_has_state(root, LV_STATE_PRESSED)) {
      lv_obj_set_state(root, LV_STATE_PRESSED, pr);
      if (lv_obj_has_flag(root, LV_OBJ_FLAG_USER_3))
        fw::mascot_set(lv_obj_get_child(root, 0), fw::mas::sleeping, pr ? role_token("disabled", "on-fill") : nullptr);
    }
    return true;
  }
  fw::refill(root);
  fill(root, p);
  if (was_off != off) fw::st::fade_in_children(root);
  return true;
}
