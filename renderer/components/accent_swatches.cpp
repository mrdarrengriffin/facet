// Accent swatches: the Facet accent picker (docs: @facet/ui settings.ts accentSwatches(), CSS .f-sws / .f-sw; the
// Settings screen's Accent row, settings.json control "swatches"). One round target per accent set in
// tokens/colour.json (tokens::ACCENTS, all of them, in order), each with a dot in that set's accent for the theme in
// use; exactly one is chosen. A radiogroup, like the Segmented: pick one of a few, in place.
//   swatch   lay::swatch (48 / 40; docs 48), round, no fill. Chosen (st::ON = LVGL CHECKED): a stroke::edge (2 px)
//            ring in text on the swatch edge (docs border 2px text). Ring = the LVGL border of the round box (AA like
//            every LVGL radius: clean at both densities, same as the Radio ring), not the stroke atom: a static
//            circle needs no mask per swatch. The ring fades in / out with Motion toggle (st::SWITCH)
//   dot      lay::swatch_dot (32 / 24; docs 32), centred: (swatch - dot) / 2 = 8 at both, 6 clear of the ring.
//            Colour = ACCENTS[set] role accent of the current theme (light / dark), never a hard-coded colour
//   row      flex row, wraps when narrow (it never is: 5 x 48 + 4 x 8 = 272 at 720, 5 x 40 + 4 x 4 = 216 at 480),
//            gap sp::s2 (8 / 4; docs gap 8)
//   pressed  the dot goes to its set's accent-press (states.json pressed.accent: an accent fill pressed), Motion
//            press / release. Not a surface-press plate behind it: accent fails 3:1 on surface-press in light
//   disabled st::OFF on every swatch (Button rule): dots in their accent-dim (st::disabled_accent, = Toggle track),
//            the chosen ring text-mute (st::disabled_ink), no press look; finger down = blocked + error
//   unavailable  the word Unavailable (body, text-dim) in place of the control, touch::min tall, finger down = blocked
//            + error (= Segmented / Toggle)
// Tap: a real CLICKED on a swatch chooses it: event "change" (detail = the set id, e.g. "teal"), or "tap" (same
// detail) on the one already chosen; sound tap either way (= Segmented / Radio). The control owns the choice; the
// workbench demo reset puts it back to `selected`. It does not change the theme itself: the host does that on change.
// Props: selected = a set id or "current" (the accent in use, the workbench Accent setting), state, context alone |
// settings-row (the real List row, title Accent, with the swatches as its trailing part: docs Settings .fs-set).
// Contrast pairs: tokens/colour.json section "accent_swatches".
#include "fw.h"

namespace {
constexpr lv_state_t ST_ON = fw::st::ON, ST_OFF = fw::st::OFF;

const lv_style_prop_t RING_PROPS[] = {LV_STYLE_BORDER_OPA, LV_STYLE_BORDER_COLOR, LV_STYLE_PROP_INV};
const lv_style_prop_t DOT_PROPS[] = {LV_STYLE_BG_COLOR, LV_STYLE_PROP_INV};
const lv_style_transition_dsc_t *ring_tr() {
  static lv_style_transition_dsc_t d;
  static bool ready = false;
  if (!ready) { fw::mo::transition(&d, fw::st::SWITCH, RING_PROPS); ready = true; }  // states.json on: Motion toggle
  return &d;
}
const lv_style_transition_dsc_t *dot_tr(bool press) {
  static lv_style_transition_dsc_t d[2];
  static bool ready = false;
  if (!ready) { fw::st::press_transitions(d, DOT_PROPS); ready = true; }  // states.json pressed motion
  return &d[press];
}

// a role of accent set `set` for theme t (tokens::ROLES names: accent, accent-press, accent-dim, ...)
lv_color_t role(const fw::Theme &t, int set, const char *name) {
  for (int r = 0; r < fw::tokens::N_ROLES; r++)
    if (!strcmp(fw::tokens::ROLES[r], name))
      return lv_color_hex(t.light ? fw::tokens::ACCENTS[set].light[r] : fw::tokens::ACCENTS[set].dark[r]);
  return t.text;  // unreachable: gen_colour.mjs fails the build on a missing role
}

struct Sw {
  int chosen = 0;
};

void choose(lv_obj_t *row, int i) {
  for (uint32_t k = 0; k < lv_obj_get_child_count(row); k++) {
    lv_obj_t *s = lv_obj_get_child(row, k);
    if ((int) k == i) lv_obj_add_state(s, ST_ON);
    else lv_obj_remove_state(s, ST_ON);
  }
  ((Sw *) lv_obj_get_user_data(row))->chosen = i;
}

lv_obj_t *unavailable(lv_obj_t *parent) {  // states.json unavailable: the word in place of the control (= Segmented)
  const fw::Theme &t = *fw::T;
  lv_obj_t *u = lv_obj_create(parent);
  lv_obj_remove_style_all(u);
  lv_obj_add_flag(u, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(u, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(u, LV_SIZE_CONTENT, fw::touch::min);
  lv_obj_t *l = fw::text(u, fw::ty::body, fw::st::UNAVAILABLE_TEXT, fw::st::unavailable_note(t));
  lv_obj_remove_flag(l, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_center(l);
  lv_obj_update_layout(l);
  lv_obj_set_width(u, (lv_obj_get_width(l) + fw::sp::GRID - 1) / fw::sp::GRID * fw::sp::GRID);  // on the grid
  lv_obj_add_state(u, ST_OFF);
  fw::st::on_tap(u, [](lv_event_t *e) {
    if (fw::st::tap_now(e)) fw::st::blocked((lv_obj_t *) lv_event_get_current_target(e), "unavailable");
  }, nullptr);
  return u;
}

lv_obj_t *swatches(lv_obj_t *parent, int chosen, bool off) {
  const fw::Theme &t = *fw::T;
  const int D = fw::lay::swatch, dot = fw::lay::swatch_dot;
  static_assert((fw::lay::swatch - fw::lay::swatch_dot) % (2 * fw::sp::GRID) == 0, "the dot must centre on the grid");
  static_assert(fw::lay::swatch - fw::lay::swatch_dot >= 4 * fw::stroke::edge, "the ring needs clear space round the dot");

  auto *d = new Sw;
  d->chosen = chosen;
  lv_obj_t *row = lv_obj_create(parent);
  lv_obj_remove_style_all(row);
  lv_obj_remove_flag(row, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_style_pad_column(row, fw::sp::s2, 0);
  lv_obj_set_style_pad_row(row, fw::sp::s2, 0);
  lv_obj_set_user_data(row, d);
  lv_obj_add_event_cb(row, [](lv_event_t *e) { delete (Sw *) lv_event_get_user_data(e); }, LV_EVENT_DELETE, d);

  for (int i = 0; i < fw::tokens::N_ACCENTS; i++) {
    lv_obj_t *s = lv_obj_create(row);
    lv_obj_remove_style_all(s);
    lv_obj_add_flag(s, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_STATE_TRICKLE));
    lv_obj_remove_flag(s, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(s, D, D);
    lv_obj_set_ext_click_area(s, std::max(0, (fw::touch::min - D) / 2));
    lv_obj_set_style_radius(s, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(s, fw::stroke::edge, 0);
    lv_obj_set_style_border_color(s, t.text, 0);  // docs: the chosen ring in text
    lv_obj_set_style_border_color(s, fw::st::disabled_ink(t), ST_OFF);
    lv_obj_set_style_border_opa(s, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_opa(s, LV_OPA_COVER, ST_ON);
    lv_obj_set_style_border_opa(s, LV_OPA_COVER, ST_ON | ST_OFF);
    lv_obj_set_style_border_color(s, fw::st::disabled_ink(t), ST_ON | ST_OFF);
    lv_obj_set_style_transition(s, ring_tr(), 0);
    lv_obj_set_style_transition(s, ring_tr(), ST_ON);

    lv_obj_t *k = lv_obj_create(s);
    lv_obj_remove_style_all(k);
    lv_obj_remove_flag(k, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
    lv_obj_set_size(k, dot, dot);
    lv_obj_align(k, LV_ALIGN_CENTER, 0, 0);  // inside the border: (D - 2 edge - dot) / 2 + edge = (D - dot) / 2
    lv_obj_set_style_radius(k, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(k, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(k, role(t, i, "accent"), 0);
    lv_obj_set_style_bg_color(k, role(t, i, "accent-press"), LV_STATE_PRESSED);  // st::pressed_accent, per set
    lv_obj_set_style_bg_color(k, role(t, i, "accent-dim"), ST_OFF);              // st::disabled_accent, per set
    lv_obj_set_style_bg_color(k, role(t, i, "accent-dim"), ST_OFF | LV_STATE_PRESSED);  // no press look
    lv_obj_set_style_transition(k, dot_tr(false), 0);
    lv_obj_set_style_transition(k, dot_tr(true), LV_STATE_PRESSED);

    if (off) lv_obj_add_state(s, ST_OFF);
    fw::st::on_tap(s, [](lv_event_t *e) {  // blocked on finger down, a real tap on CLICKED (states.json)
      if (!fw::st::tap_now(e)) return;
      auto *o = (lv_obj_t *) lv_event_get_current_target(e);
      lv_obj_t *r = lv_obj_get_parent(o);
      auto *d = (Sw *) lv_obj_get_user_data(r);
      const int i = (int) lv_obj_get_index(o);
      if (lv_obj_has_state(o, ST_OFF)) { fw::st::blocked(o, fw::tokens::ACCENTS[d->chosen].id); return; }
      if (i != d->chosen) {
        choose(r, i);
        fw::emit(o, "change", fw::tokens::ACCENTS[i].id);
      } else fw::emit(o, fw::st::TAP_EVENT, fw::tokens::ACCENTS[i].id);  // the chosen one again: nothing changes
      fw::sound(o, fw::st::TAP_SOUND);
    }, nullptr);
  }
  choose(row, chosen);
  return row;
}
}  // namespace

namespace {
// the root = a content-sized wrapper (no look of its own) holding the swatch row, the word, or the List row with the
// swatches as its trailing part, so every prop keeps one root for the update path
std::string sig(fw::Props p) {
  return std::string(p["context"] | "alone") + "|" + (strcmp(p["state"] | "default", "unavailable") ? "" : "u");
}
lv_obj_t *swatch_row(lv_obj_t *w, fw::Props p) {  // the swatches' row, or nullptr (unavailable)
  if (!strcmp(p["state"] | "default", "unavailable")) return nullptr;
  lv_obj_t *c = lv_obj_get_child(w, 0);
  if (c && !strcmp(p["context"] | "alone", "settings-row")) c = lv_obj_get_child(c, -1);
  return c;
}
int chosen_of(fw::Props p) {
  int chosen = fw::T->accent_set;
  const char *sel = p["selected"] | "current";
  for (int i = 0; i < fw::tokens::N_ACCENTS; i++)
    if (!strcmp(fw::tokens::ACCENTS[i].id, sel)) chosen = i;
  return chosen;
}
void fill(lv_obj_t *w, fw::Props p) {
  lv_obj_remove_style_all(w);
  lv_obj_remove_flag(w, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  // a settings row = the List row, which fills its parent's width (contract C5): the wrapper takes that width too
  lv_obj_set_size(w, strcmp(p["context"] | "alone", "settings-row") ? LV_SIZE_CONTENT : LV_PCT(100), LV_SIZE_CONTENT);
  fw::shape(w, sig(p));
  const char *state = p["state"] | "default";
  const bool unavail = !strcmp(state, "unavailable"), off = !strcmp(state, "disabled");
  int chosen = fw::T->accent_set;
  const char *sel = p["selected"] | "current";
  for (int i = 0; i < fw::tokens::N_ACCENTS; i++)
    if (!strcmp(fw::tokens::ACCENTS[i].id, sel)) chosen = i;

  if (!strcmp(p["context"] | "alone", "settings-row")) {
    // the List row component itself (title Accent, no trailing of its own), the swatches appended as its trailing
    // part; the row is not a tap target (docs .fs-set with an inline control), its swatches are. Unavailable: the
    // List row shows the word itself
    const fw::Component *c = fw::find("list_row");
    if (!c) return;
    ArduinoJson::JsonDocument rp;
    rp["title"] = "Accent";
    rp["icon"] = "none";
    rp["trailing"] = "none";
    rp["state"] = state;
    const uint32_t before = lv_obj_get_child_count(w);
    c->build(w, rp.as<fw::Props>());
    if (lv_obj_get_child_count(w) == before) return;
    lv_obj_t *row = lv_obj_get_child(w, -1);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_CLICKABLE);
    if (!unavail) swatches(row, chosen, off);
    return;
  }
  if (unavail) unavailable(w);
  else swatches(w, chosen, off);
}
}  // namespace

FW_COMPONENT(accent_swatches, "Accent swatches", "components", R"({
  "about": "Pick the accent colour: one dot per accent set in the theme in use, the chosen one ringed.",
  "frame": [480, 96],
  "props": [
    {"name": "selected", "about": "Which accent colour is ringed as the chosen one. current follows the colour the panel uses now; the others pick that accent set by name.", "type": "select", "options": ["current", "ember", "rose", "violet", "teal", "mono"], "default": "current"},
    {"name": "state", "about": "How the swatches look: default can be tapped; disabled is dimmed and ignores taps; unavailable shows the colour cannot be read right now.", "type": "select", "options": ["default", "disabled", "unavailable"], "default": "default"},
    {"name": "context", "about": "Where the swatches sit: alone on their own, or settings-row tucked into the right end of a row on a settings page.", "type": "select", "options": ["alone", "settings-row"], "default": "alone"}
  ],
  "variants": [
    {"label": "default · the accent in use", "props": {}},
    {"label": "teal chosen", "props": {"selected": "teal"}},
    {"label": "disabled", "props": {"selected": "violet", "state": "disabled"}},
    {"label": "unavailable", "props": {"state": "unavailable"}},
    {"label": "settings row", "props": {"context": "settings-row"}},
    {"label": "settings row · disabled", "props": {"context": "settings-row", "state": "disabled"}}
  ]
})") {
  fill(lv_obj_create(parent), p);
}

// Update path: another accent = the ring moves (the swatches' ON state, Motion toggle transition), default <->
// disabled fades in place (fw::st::set_state OFF); context or unavailable in / out = the contents again (fw::refill,
// snap; unavailable fades the new contents in). Silent.
FW_UPDATE(accent_swatches) {
  if (!fw::shape(root, sig(p))) {
    fw::refill(root);
    fill(root, p);
    fw::st::fade_in_children(root);
    return true;
  }
  lv_obj_t *row = swatch_row(root, p);
  if (!row || !lv_obj_get_user_data(row)) return true;  // unavailable: the word only
  const int c = chosen_of(p);
  const bool off = !strcmp(p["state"] | "default", "disabled");
  for (uint32_t i = 0; i < lv_obj_get_child_count(row); i++) {
    lv_obj_t *sw = lv_obj_get_child(row, (int32_t) i);
    fw::st::set_state(sw, ST_ON, (int) i == c, false);  // the ring's own transition (Motion toggle) moves it
    fw::st::set_state(sw, ST_OFF, off);
  }
  ((Sw *) lv_obj_get_user_data(row))->chosen = c;
  return true;
}
