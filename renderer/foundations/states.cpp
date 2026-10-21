// Foundations > States: tokens/states.json (via the generated states_gen.h, fw::st::) drawn by the panel renderer. One
// section per state (pressed, disabled, unavailable, on, focus, rest and tap): the state's motion / event / sound, its
// colour roles (the real colour of this theme x accent set beside the token it names) and a row of the approved
// components in that state, built by their own build functions through the registry (fw::find, as List row builds
// Toggle): never drawn here. Then the rules in plain words, the round-1 audit table and the open questions. The
// samples are live: tap them and the Events panel (meta "events": true) shows what they send. Status (WIP / Done) = this file +
// tokens/states.json in git.
#include <cstdio>
#include "fw.h"

namespace {
using namespace fw;
using Doc = ArduinoJson::JsonDocument;

constexpr float W = 480;  // doc page width in dp (= the screen width)

lv_obj_t *box(lv_obj_t *parent) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
  return o;
}
lv_obj_t *column(lv_obj_t *parent, int gap, int w = LV_PCT(100)) {
  lv_obj_t *c = box(parent);
  lv_obj_set_size(c, w, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(c, gap, 0);
  return c;
}
lv_obj_t *row(lv_obj_t *parent, int gap) {
  lv_obj_t *r = box(parent);
  lv_obj_set_size(r, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(r, gap, 0);
  return r;
}
// samples wrap: a row of components, a gutter apart, a group apart between lines
lv_obj_t *wrap(lv_obj_t *parent) {
  lv_obj_t *r = box(parent);
  lv_obj_set_size(r, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_START);
  lv_obj_set_style_pad_column(r, lay::gutter, 0);
  lv_obj_set_style_pad_row(r, sp::s7, 0);
  return r;
}
lv_obj_t *entry(lv_obj_t *list, int gap = sp::s2) {
  lv_obj_t *e = column(list, gap);
  lv_obj_set_style_pad_top(e, sp::s5, 0);
  lv_obj_set_style_pad_bottom(e, sp::s5 - stroke::hair, 0);  // the hairline sits in the padding: entries stay on the grid
  lv_obj_set_style_border_side(e, LV_BORDER_SIDE_BOTTOM, 0);
  lv_obj_set_style_border_width(e, stroke::hair, 0);
  lv_obj_set_style_border_color(e, T->line, 0);
  return e;
}
int inner_w() { return px(W) - 2 * sp::s7; }
lv_obj_t *page(lv_obj_t *parent) {
  lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_t *l = column(parent, 0, px(W));
  lv_obj_set_style_pad_hor(l, sp::s7, 0);
  lv_obj_set_style_pad_bottom(l, sp::s7, 0);
  return l;
}
// page text: fw::text with its box rounded up to the 4 px grid (the 720 type boxes are 1.5x docs values, e.g. 21), so
// every block under it stays on the grid
int snap(int v) { return (v + 3) / 4 * 4; }
lv_obj_t *tx(lv_obj_t *parent, ty::Id s, const char *t, lv_color_t c, int wrap_w = 0) {
  lv_obj_t *l = fw::text(parent, s, t, c, wrap_w);
  lv_obj_update_layout(l);
  lv_obj_set_height(l, snap(lv_obj_get_height(l)));
  return l;
}
lv_obj_t *heading(lv_obj_t *l, const char *t) {
  lv_obj_t *c = column(l, 0);  // a group above, the caption box below
  lv_obj_set_style_pad_top(c, lay::group, 0);
  lv_obj_set_style_pad_bottom(c, sp::s5, 0);
  return tx(c, ty::c_caps_sm, t, T->text_dim);
}

// one real component in a sample: the component's own build, a caption under it
void sample(lv_obj_t *parent, const char *id, Doc &props, const char *caption) {
  const Component *c = find(id);
  lv_obj_t *s = column(parent, sp::s3, LV_SIZE_CONTENT);
  lv_obj_set_flex_align(s, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  if (c) c->build(s, props.as<Props>());
  tx(s, ty::c_meta, c ? caption : "component missing", T->text_dim);
  lv_obj_update_layout(s);  // the caption can be wider than the component: keep the next sample on the grid
  lv_obj_set_width(s, snap(lv_obj_get_width(s)));
}
// helpers: sample(parent, "button", {"kind", "secondary", "state", "pressed"}, "secondary")
void sample(lv_obj_t *parent, const char *id, std::initializer_list<std::pair<const char *, const char *>> kv, const char *caption) {
  Doc d;
  for (auto &p : kv) d[p.first] = p.second;
  sample(parent, id, d, caption);
}

const st::State &state(const char *id) {
  for (int i = 0; i < st::N_STATES; i++) if (!strcmp(st::STATES[i].id, id)) return st::STATES[i];
  return st::STATES[0];
}
// the state's spec line (motion / event / sound) and its colour roles with this theme's colours
void spec(lv_obj_t *l, const char *id) {
  const st::State &s = state(id);
  lv_obj_t *e = entry(l, sp::s3);
  char b[160];
  const struct { const char *k, *v; } kv[] = {{"Motion", s.motion}, {"Event", s.event}, {"Sound", s.sound}};
  for (auto &x : kv) {
    lv_obj_t *r = row(e, sp::s4);
    lv_obj_t *k = tx(r, ty::c_caps_sm, x.k, T->text_dim);
    lv_obj_set_width(k, sp::s10 * 2);
    snprintf(b, sizeof b, "%s", *x.v ? x.v : "none");
    tx(r, ty::c_body, b, *x.v ? T->text : T->text_dim);
  }
  for (int i = 0; i < st::N_ROLES; i++) {
    const st::Role &r = st::ROLES[i];
    if (strcmp(r.state, id)) continue;
    lv_obj_t *e2 = entry(l, sp::s1);
    lv_obj_t *h = row(e2, sp::s4);
    lv_obj_t *sw = box(h);
    lv_obj_set_size(sw, sp::s7, sp::s7);
    lv_obj_set_style_bg_color(sw, r.colour(*T), 0);
    lv_obj_set_style_bg_opa(sw, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(sw, stroke::hair, 0);
    lv_obj_set_style_border_color(sw, T->line, 0);
    snprintf(b, sizeof b, "%s_%s", r.state, r.role);
    for (char *p = b; *p; p++) if (*p == '-') *p = '_';
    tx(h, ty::c_caps_sm, b, T->text);
    snprintf(b, sizeof b, "%s%s", r.mix ? "mix " : "", r.token);
    tx(h, ty::c_meta, b, T->text_dim);
    tx(e2, ty::c_meta, r.use, T->text_dim, inner_w());
  }
}

void pressed(lv_obj_t *l) {
  spec(l, "pressed");
  heading(l, "Boxed controls");
  lv_obj_t *w = wrap(l);
  sample(w, "button", {{"state", "pressed"}}, "primary");
  sample(w, "button", {{"kind", "secondary"}, {"label", "Cancel"}, {"state", "pressed"}}, "secondary");
  sample(w, "button", {{"kind", "outline"}, {"label", "Save"}, {"state", "pressed"}}, "outline");
  sample(w, "button", {{"kind", "ghost"}, {"label", "More"}, {"state", "pressed"}}, "ghost");
  sample(w, "icon_button", {{"style", "play"}, {"state", "pressed"}}, "play");
  sample(w, "icon_button", {{"state", "pressed"}}, "round");
  sample(w, "back_button", {{"state", "pressed"}}, "back");
  heading(l, "Tinted areas");
  w = wrap(l);
  sample(w, "card", {{"state", "pressed"}}, "card · fill (default)");
  sample(w, "card", {{"state", "pressed"}, {"press", "border"}}, "card · border");
  sample(w, "list_row", {{"title", "Sounds"}, {"icon", "volume"}, {"trailing", "chevron"}, {"state", "pressed"}}, "list row");
  Doc sh;
  sh["title"] = "Notifications"; sh["count"] = 5; sh["tone"] = "error"; sh["right"] = "action"; sh["action"] = "Clear"; sh["state"] = "pressed";
  sample(w, "section_header", sh, "section header action");
}

void disabled(lv_obj_t *l) {
  spec(l, "disabled");
  heading(l, "Buttons");
  lv_obj_t *w = wrap(l);
  sample(w, "button", {{"state", "disabled"}}, "primary");
  sample(w, "button", {{"kind", "secondary"}, {"label", "Cancel"}, {"state", "disabled"}}, "secondary");
  sample(w, "button", {{"kind", "outline"}, {"label", "Save"}, {"state", "disabled"}}, "outline");
  sample(w, "button", {{"kind", "ghost"}, {"label", "More"}, {"state", "disabled"}}, "ghost");
  sample(w, "icon_button", {{"style", "play"}, {"state", "disabled"}}, "play");
  sample(w, "icon_button", {{"state", "disabled"}}, "round");
  sample(w, "back_button", {{"state", "disabled"}}, "back");
  heading(l, "Controls");
  w = wrap(l);
  sample(w, "toggle", {{"state", "disabled"}}, "toggle · off");
  Doc on;
  on["on"] = true; on["state"] = "disabled";
  sample(w, "toggle", on, "toggle · on");
  Doc sl;
  sl["state"] = "disabled"; sl["width"] = 192;
  sample(w, "slider", sl, "slider");
  heading(l, "Rows");
  w = wrap(l);
  Doc lr;
  lr["title"] = "Night dimming"; lr["icon"] = "power"; lr["sub"] = "Needs a clock source"; lr["trailing"] = "toggle"; lr["on"] = true; lr["state"] = "disabled";
  sample(w, "list_row", lr, "list row · toggle");
  Doc sh;
  sh["title"] = "Notifications"; sh["badge"] = false; sh["right"] = "action"; sh["action"] = "Clear"; sh["state"] = "disabled";
  sample(w, "section_header", sh, "section header action");
  heading(l, "Dial");
  w = wrap(l);
  sample(w, "dial", {{"state", "disabled"}}, "dial");
}

void unavailable(lv_obj_t *l) {
  spec(l, "unavailable");
  lv_obj_t *w = wrap(l);
  sample(w, "card", {{"label", "Lights"}, {"icon", "light"}, {"preview", "none"}, {"value", "65%"}, {"sub", ""}, {"state", "unavailable"}}, "card");
  sample(w, "list_row", {{"title", "Porch light"}, {"icon", "bulb"}, {"trailing", "toggle"}, {"state", "unavailable"}}, "list row");
  sample(w, "dial", {{"state", "unavailable"}}, "dial");
  heading(l, "Controls");
  w = wrap(l);
  sample(w, "toggle", {{"state", "unavailable"}}, "toggle");
  Doc sl;
  sl["state"] = "unavailable"; sl["width"] = 192;
  sample(w, "slider", sl, "slider");
  sample(w, "icon_button", {{"style", "play"}, {"state", "unavailable"}}, "play (no room for the word)");
}

void on(lv_obj_t *l) {
  spec(l, "on");
  heading(l, "Switches");
  lv_obj_t *w = wrap(l);
  sample(w, "toggle", {}, "toggle · off");
  Doc t;
  t["on"] = true;
  sample(w, "toggle", t, "toggle · on");
  Doc lr;
  lr["title"] = "Chime"; lr["icon"] = "speaker"; lr["trailing"] = "toggle"; lr["on"] = true;
  sample(w, "list_row", lr, "list row · toggle on");
  heading(l, "Active");
  w = wrap(l);
  sample(w, "button", {{"state", "active"}}, "primary");
  sample(w, "button", {{"kind", "secondary"}, {"label", "Cancel"}, {"state", "active"}}, "secondary");
  sample(w, "button", {{"kind", "outline"}, {"label", "Save"}, {"state", "active"}}, "outline");
  sample(w, "button", {{"kind", "ghost"}, {"label", "More"}, {"state", "active"}}, "ghost");
  sample(w, "icon_button", {{"style", "play"}, {"state", "active"}}, "play = pause");
  sample(w, "icon_button", {{"state", "active"}}, "round");
  sample(w, "icon_button", {{"style", "square"}, {"icon", "plus"}, {"state", "active"}}, "square");
  heading(l, "Dial on and off");
  w = wrap(l);
  Doc d;
  d["sweep"] = false;
  sample(w, "dial", d, "dial · on");
  Doc d2;
  d2["sweep"] = false; d2["on"] = false;
  sample(w, "dial", d2, "dial · off");
}

void focus(lv_obj_t *l) {
  spec(l, "focus");
  lv_obj_t *e = entry(l, sp::s1);
  char b[128];
  snprintf(b, sizeof b, "Ring %d px, %d px outside the box (stroke::focus, touch::focus_offset)", st::FOCUS_W, st::FOCUS_OFFSET);
  tx(e, ty::c_body, b, T->text);
  lv_obj_t *w = wrap(l);
  lv_obj_set_style_pad_top(w, sp::s5, 0);
  sample(w, "button", {{"state", "active"}}, "button primary · active");
  sample(w, "icon_button", {{"style", "play"}, {"state", "active"}}, "play · playing");
}

void tap(lv_obj_t *l) {
  spec(l, "rest");
  for (int i = 0; i < st::N_TAP_SOUNDS; i++) {
    const st::TapSound &s = st::TAP_SOUNDS[i];
    lv_obj_t *e = entry(l, sp::s1);
    lv_obj_t *h = row(e, sp::s4);
    lv_obj_t *k = tx(h, ty::c_caps_sm, s.id, T->text);
    lv_obj_set_width(k, sp::s10 * 2);
    tx(h, ty::c_meta, s.sound, T->accent_text);
    tx(e, ty::c_meta, s.use, T->text_dim, inner_w());
  }
  heading(l, "Tap them: the Events panel shows what each sends");
  lv_obj_t *w = wrap(l);
  sample(w, "button", {{"kind", "secondary"}, {"label", "Cancel"}}, "tap");
  sample(w, "back_button", {}, "back");
  sample(w, "toggle", {}, "toggle-on / off");
  Doc sl;
  sl["width"] = 192;
  sample(w, "slider", sl, "step ticks");
  sample(w, "list_row", {{"title", "Sounds"}, {"icon", "volume"}, {"trailing", "chevron"}}, "open");
  sample(w, "button", {{"state", "disabled"}}, "blocked + error");
}

void rules(lv_obj_t *l) {
  char b[16];
  for (int i = 0; i < st::N_RULES; i++) {
    lv_obj_t *e = entry(l, sp::s1);
    lv_obj_t *r = row(e, sp::s4);
    lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    snprintf(b, sizeof b, "%d", i + 1);
    lv_obj_t *n = tx(r, ty::c_caps_sm, b, T->accent_text);
    lv_obj_set_width(n, sp::s7);
    tx(r, ty::c_body, st::RULES[i], T->text, inner_w() - sp::s7 - sp::s4);
  }
  heading(l, "LVGL state bits");
  const struct { const char *k, *v; } bits[] = {
      {"PRESSED", "finger down (LVGL sets it; variants force it)"},
      {"CHECKED", "on / active"},
      {"USER_1", "disabled and unavailable (fw::st::OFF): events still arrive, so a blocked tap can play error; outranks PRESSED"},
  };
  for (auto &x : bits) {
    lv_obj_t *e = entry(l, sp::s1);
    tx(e, ty::c_caps_sm, x.k, T->text);
    tx(e, ty::c_meta, x.v, T->text_dim, inner_w());
  }
}

void audit(lv_obj_t *l) {
  char b[48];
  for (int i = 0; i < st::N_AUDIT; i++) {
    const st::AuditRow &a = st::AUDIT[i];
    lv_obj_t *e = entry(l, sp::s1);
    tx(e, ty::c_body, a.component, T->text);
    for (int c = 0; c < st::N_AUDIT_COLS; c++) {
      if (!strcmp(a.cells[c], "-")) continue;
      lv_obj_t *r = row(e, sp::s4);
      lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
      snprintf(b, sizeof b, "%s", st::AUDIT_COLS[c]);
      lv_obj_t *k = tx(r, ty::c_caps_sm, b, T->text_dim);
      lv_obj_set_width(k, sp::s10 * 2);
      tx(r, ty::c_meta, a.cells[c], T->text, inner_w() - sp::s10 * 2 - sp::s4);
    }
  }
}

void decisions(lv_obj_t *l) {
  char b[16];
  for (int i = 0; i < st::N_DECISIONS; i++) {
    lv_obj_t *e = entry(l, sp::s2);
    lv_obj_t *r = row(e, sp::s4);
    lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    snprintf(b, sizeof b, "%d", i + 1);
    lv_obj_t *n = tx(r, ty::c_caps_sm, b, T->accent_text);
    lv_obj_set_width(n, sp::s7);
    lv_obj_t *c = column(r, sp::s2, inner_w() - sp::s7 - sp::s4);
    tx(c, ty::c_body, st::DECISIONS[i].decision, T->text, inner_w() - sp::s7 - sp::s4);
    tx(c, ty::c_meta, st::DECISIONS[i].q, T->text_dim, inner_w() - sp::s7 - sp::s4);
  }
}
}  // namespace

FW_COMPONENT(states, "States", "foundations", R"({
  "about": "How every control looks and behaves when pressed, on, disabled or unavailable, with the event and sound each state makes.",
  "layout": "doc",
  "events": true,
  "frame": [480, 0],
  "props": [{"name": "section", "type": "select", "options": )" FW_STATES_SECTION_IDS R"(, "default": "rules"}],
  "variants": )" FW_STATES_SECTIONS R"(
})") {
  const char *s = p["section"] | "rules";
  lv_obj_t *l = page(parent);
  if (!strcmp(s, "pressed")) pressed(l);
  else if (!strcmp(s, "disabled")) disabled(l);
  else if (!strcmp(s, "unavailable")) unavailable(l);
  else if (!strcmp(s, "on")) on(l);
  else if (!strcmp(s, "focus")) focus(l);
  else if (!strcmp(s, "tap")) tap(l);
  else if (!strcmp(s, "audit")) audit(l);
  else if (!strcmp(s, "decisions")) decisions(l);
  else rules(l);
}
