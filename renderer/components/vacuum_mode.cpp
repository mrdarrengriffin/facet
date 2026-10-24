// Vacuum mode: the robot vacuum's cleaning settings (the panel lab's Vacuum Mode row + mode button + mode sheet, board
// facet-52, apps/hub panel/vacuum/screen.ts): cleaning mode (Vacuum / Vac + mop / Mop), suction (Quiet .. Max+) and
// water (Low / Medium / High), applied at once like the Roborock app.
//   style row     the Rooms view's row: the real List row (fw::find) with the over line "mode", the title = what
//                 it will do ("Vac + mop · Turbo · High water": suction only when it vacuums, water only when it mops)
//                 and a chevron; a tap opens the sheet (the List row's tap + sound open)
//   style button  the Map view's short button at the bottom left (type label, caps, text-dim, touch::min tall,
//                 hairline line edge, rad::button, at most lay::mode_btn_max wide: "Vac + mop · Turbo · High"); its
//                 root grows (it is the spacer that pushes the action buttons right, as the lab's bar); shown only
//                 when prop view = map (the spec's "show" hides it while a run is on)
//   sheet         the Bottom sheet component (fw::find, title "mode") on the screen, one host row per setting: a
//                 hairline, the setting's name (type label, caps, text-dim), sp::s5, its Segmented (hug); Suction
//                 hides for Mop, Water for Vacuum. A pick = event "change" "mode <id>" / "fan <id>" / "mop <id>"
//                 (the spec's vacuum.control) and the texts follow at once; the Segmented's own tap sound plays
// Props: mode, fan, mop (the Roborock ids; HA's select states / fan_speed), style, view.
// Update path: texts in place (List row update), the sheet's choices move with the props. Contrast: section
// "vacuum_mode" (+ list_row, segmented, bottom_sheet).
// audit deps: list_row, segmented, bottom_sheet
#include "fw.h"
#include "spec.h"

namespace {
struct Opt { const char *id, *label; };
constexpr Opt MODES[] = {{"vacuum", "Vacuum"}, {"vac_and_mop", "Vac + mop"}, {"mop", "Mop"}};
constexpr Opt FANS[] = {{"quiet", "Quiet"}, {"balanced", "Balanced"}, {"turbo", "Turbo"}, {"max", "Max"}, {"max_plus", "Max+"}};
constexpr Opt MOPS[] = {{"low", "Low"}, {"medium", "Medium"}, {"high", "High"}};
struct Set { const char *key, *name; const Opt *opts; int n; };
constexpr Set SETS[] = {{"mode", "Cleaning", MODES, 3}, {"fan", "Suction", FANS, 5}, {"mop", "Water", MOPS, 3}};

struct VM {
  std::string mode, fan, mop, style, view;
  lv_obj_t *row = nullptr, *btn = nullptr, *btn_l = nullptr, *sheet = nullptr;
};
VM *of(lv_obj_t *o) { return (VM *) lv_obj_get_user_data(o); }
const char *label_of(const Set &s, const std::string &id) {
  for (int i = 0; i < s.n; i++)
    if (id == s.opts[i].id) return s.opts[i].label;
  return id.c_str();
}
std::string lower(std::string s) {
  for (char &c : s) c = (char) tolower((unsigned char) c);
  return s;
}
// "Vac + mop · Turbo · High water" (suction only when it vacuums, water only when it mops); short drops "water"
std::string line(const VM *c, bool short_) {
  std::string o = c->mode.empty() ? std::string("\xE2\x80\x94") : label_of(SETS[0], c->mode);
  if (c->mode != "mop" && !c->fan.empty()) o += std::string(" \xC2\xB7 ") + label_of(SETS[1], c->fan);
  if (c->mode != "vacuum" && !c->mop.empty()) o += std::string(" \xC2\xB7 ") + label_of(SETS[2], c->mop) + (short_ ? "" : " water");
  return o;
}
bool shows(const VM *c, int k) { return k == 0 || (k == 1 && c->mode != "mop") || (k == 2 && c->mode != "vacuum"); }
int rows(const VM *c) { int n = 0; for (int k = 0; k < 3; k++) n += shows(c, k); return n; }
int row_h() { return fw::stroke::hair + 2 * fw::sp::s7 + fw::style(fw::ty::label).box + fw::sp::s5 + fw::touch::min; }
lv_obj_t *build(const char *id, lv_obj_t *parent, ArduinoJson::JsonDocument &d) {
  const uint32_t n = lv_obj_get_child_count(parent);
  if (const fw::Component *c = fw::find(id)) c->build(parent, d.as<fw::Props>());
  return lv_obj_get_child_count(parent) > n ? lv_obj_get_child(parent, -1) : nullptr;
}
void sheet_props(const VM *c, bool open, ArduinoJson::JsonDocument &d) {
  d["overlay"] = true;
  d["title"] = "mode";
  d["open"] = open ? "open" : "closed";
  d["rows"] = rows(c);
  d["row_h"] = row_h();
}
void paint(lv_obj_t *root);
void open_sheet(lv_obj_t *root);
// the sheet's rows (the sheet asks with LV_EVENT_REFRESH, param = its list)
void fill_rows(lv_obj_t *root, lv_obj_t *list) {
  VM *c = of(root);
  const fw::Theme &t = *fw::T;
  for (int k = 0; k < 3; k++) {
    if (!shows(c, k)) continue;
    const Set &s = SETS[k];
    lv_obj_t *r = lv_obj_create(list);
    lv_obj_remove_style_all(r);
    lv_obj_remove_flag(r, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
    lv_obj_set_size(r, fw::lay::content_w, row_h());
    lv_obj_set_style_border_side(r, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_border_width(r, fw::stroke::hair, 0);
    lv_obj_set_style_border_color(r, t.line, 0);
    lv_obj_set_style_pad_top(r, fw::sp::s7, 0);
    lv_obj_set_flex_flow(r, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(r, fw::sp::s5, 0);
    lv_obj_t *l = fw::text(r, fw::ty::label, s.name, t.text_dim);
    lv_obj_set_height(l, fw::style(fw::ty::label).box);
    std::string opts;
    for (int i = 0; i < s.n; i++) opts += std::string(i ? ", " : "") + s.opts[i].label;
    const std::string &cur = k == 0 ? c->mode : k == 1 ? c->fan : c->mop;
    ArduinoJson::JsonDocument d;
    d["options"] = opts;
    d["selected"] = lower(label_of(s, cur));
    lv_obj_t *seg = build("segmented", r, d);
    if (!seg) continue;
    lv_obj_set_user_data(r, (void *) (intptr_t) k);
    fw::listen(r, [](lv_obj_t *from, const char *name, const char *detail, void *user) {  // stops here: the spec
      auto *root = (lv_obj_t *) user;                                                      // sees "mode <id>" only
      if (strcmp(name, "change")) return;
      lv_obj_t *r = from;
      while (r && !lv_obj_has_flag(r, LV_OBJ_FLAG_USER_3)) r = lv_obj_get_parent(r);
      if (!r) return;
      const int k = (int) (intptr_t) lv_obj_get_user_data(r);
      const Set &s = SETS[k];
      VM *c = of(root);
      for (int i = 0; i < s.n; i++) {
        if (lower(s.opts[i].label) != detail) continue;
        (k == 0 ? c->mode : k == 1 ? c->fan : c->mop) = s.opts[i].id;
        const std::string ev = std::string(s.key) + " " + s.opts[i].id;
        fw::emit(root, "change", ev.c_str());
        paint(root);
        // Suction / Water come and go with the mode: the rows again, still open. On the next tick: the Segmented that
        // sent this is one of those rows, and it still plays its tap after this returns (a refill now = use after free)
        if (k == 0 && c->sheet)
          lv_async_call([](void *r) { if (lv_obj_is_valid((lv_obj_t *) r)) open_sheet((lv_obj_t *) r); }, root);
        return;
      }
    }, root);
    lv_obj_add_flag(r, LV_OBJ_FLAG_USER_3);  // a settings row (found from its Segmented's event)
  }
}
void open_sheet(lv_obj_t *root) {
  VM *c = of(root);
  if (!c->sheet) return;
  ArduinoJson::JsonDocument d;
  sheet_props(c, true, d);
  fw::update_child(fw::find("bottom_sheet"), c->sheet, d.as<fw::Props>());
}
// texts + the button's visibility from the props
void paint(lv_obj_t *root) {
  VM *c = of(root);
  if (c->row) {
    ArduinoJson::JsonDocument d;
    d["over"] = "mode", d["title"] = line(c, false), d["trailing"] = "chevron", d["icon"] = "none";
    fw::update_child(fw::find("list_row"), c->row, d.as<fw::Props>());
  }
  if (c->btn) {
    const std::string s = line(c, true);
    if (fw::cased(fw::ty::label, s.c_str()) != lv_label_get_text(c->btn_l)) {
      fw::set_text(c->btn_l, fw::ty::label, s.c_str());
      lv_point_t sz;  // the box = the text + padding, rounded up to the grid (docs fit8), at most mode-btn-max
      lv_text_get_size(&sz, lv_label_get_text(c->btn_l), lv_obj_get_style_text_font(c->btn_l, LV_PART_MAIN),
                       lv_obj_get_style_text_letter_space(c->btn_l, LV_PART_MAIN), 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
      const int w = std::min<int>((int) fw::lay::mode_btn_max, (sz.x + 2 * fw::sp::s4 + fw::sp::GRID - 1) / fw::sp::GRID * fw::sp::GRID);
      lv_obj_set_width(c->btn, w);
      lv_obj_set_width(c->btn_l, w - 2 * fw::sp::s4);
    }
    const bool hide = c->view != "map";
    if (hide != lv_obj_has_flag(c->btn, LV_OBJ_FLAG_HIDDEN)) lv_obj_set_flag(c->btn, LV_OBJ_FLAG_HIDDEN, hide);
  }
}
void take(VM *c, fw::Props p) {
  c->mode = p["mode"] | "vac_and_mop";
  c->fan = p["fan"] | "turbo";
  c->mop = p["mop"] | "high";
  c->view = p["view"] | "map";
}
const lv_style_prop_t PRESS_PROPS[] = {LV_STYLE_BG_COLOR, LV_STYLE_BG_OPA, LV_STYLE_PROP_INV};
const lv_style_transition_dsc_t *press_tr(bool pressed) {
  static lv_style_transition_dsc_t d[2];
  static bool ready = false;
  if (!ready) { fw::st::press_transitions(d, PRESS_PROPS); ready = true; }
  return &d[pressed];
}
// the spec screen this is built in (its root hosts the sheet), else the stage
lv_obj_t *screen_of(lv_obj_t *o, lv_obj_t *fallback) {
  for (fw::spec::Screen *s : fw::spec::live())
    for (lv_obj_t *a = o; a; a = lv_obj_get_parent(a))
      if (a == s->root) return s->root;
  return fallback;
}
}  // namespace

FW_COMPONENT(vacuum_mode, "Vacuum mode", "components", R"({
  "about": "The robot vacuum's cleaning settings: a row (or a short button on the map) that opens a sheet with the cleaning mode, suction and water, applied at once.",
  "frame": [480, 0],
  "props": [
    {"name": "mode", "type": "select", "options": ["vacuum", "vac_and_mop", "mop"], "default": "vac_and_mop"},
    {"name": "fan", "type": "select", "options": ["quiet", "balanced", "turbo", "max", "max_plus"], "default": "turbo"},
    {"name": "mop", "type": "select", "options": ["low", "medium", "high"], "default": "high"},
    {"name": "style", "type": "select", "options": ["row", "button"], "default": "row"},
    {"name": "view", "type": "select", "options": ["rooms", "map"], "default": "map"}
  ],
  "variants": [
    {"label": "row", "props": {}},
    {"label": "row · vacuum only", "props": {"mode": "vacuum", "fan": "balanced"}},
    {"label": "button · map", "props": {"style": "button"}},
    {"label": "button · mop only", "props": {"style": "button", "mode": "mop", "mop": "medium"}}
  ]
})") {
  const fw::Theme &t = *fw::T;
  auto *c = new VM;
  take(c, p);
  c->style = p["style"] | "row";
  lv_obj_t *root = lv_obj_create(parent);
  lv_obj_remove_style_all(root);
  lv_obj_remove_flag(root, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_user_data(root, c);
  lv_obj_add_event_cb(root, [](lv_event_t *e) {
    auto *c = (VM *) lv_event_get_user_data(e);
    if (c->sheet) lv_obj_delete(c->sheet);  // it lives on the screen, not inside this (its own cb clears c->sheet)
    delete c;
  }, LV_EVENT_DELETE, c);
  if (c->style == "row") {
    lv_obj_set_size(root, LV_PCT(100), LV_SIZE_CONTENT);
    ArduinoJson::JsonDocument d;
    d["over"] = "mode", d["title"] = line(c, false), d["trailing"] = "chevron", d["icon"] = "none";
    c->row = build("list_row", root, d);
    if (c->row) lv_obj_set_width(c->row, LV_PCT(100));
  } else {
    lv_obj_set_size(root, LV_SIZE_CONTENT, fw::touch::min);  // grows in a Group row: the spacer before the action buttons
    lv_obj_set_flex_grow(root, 1);
    c->btn = lv_obj_create(root);
    lv_obj_remove_style_all(c->btn);
    lv_obj_remove_flag(c->btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(c->btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(c->btn, LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_set_height(c->btn, fw::touch::min);
    lv_obj_set_style_max_width(c->btn, fw::lay::mode_btn_max, 0);
    lv_obj_set_style_radius(c->btn, fw::rad::button, 0);
    lv_obj_set_style_border_width(c->btn, fw::stroke::hair, 0);
    lv_obj_set_style_border_color(c->btn, t.line, 0);
    lv_obj_set_style_bg_color(c->btn, fw::st::pressed_surface(t), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(c->btn, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_set_style_transition(c->btn, press_tr(false), 0);
    lv_obj_set_style_transition(c->btn, press_tr(true), LV_STATE_PRESSED);
    lv_obj_set_style_pad_hor(c->btn, fw::sp::s4 - fw::stroke::hair, 0);
    c->btn_l = fw::text(c->btn, fw::ty::label, "", t.text_dim);
    lv_label_set_long_mode(c->btn_l, LV_LABEL_LONG_DOT);
    lv_obj_set_style_max_width(c->btn_l, fw::lay::mode_btn_max - 2 * fw::sp::s4, 0);
    lv_obj_align(c->btn_l, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_add_event_cb(c->btn, [](lv_event_t *e) {
      auto *root = (lv_obj_t *) lv_event_get_user_data(e);
      auto *b = (lv_obj_t *) lv_event_get_current_target(e);
      fw::emit(b, "tap", "mode");
      fw::sound(b, "open");
      open_sheet(root);
    }, LV_EVENT_CLICKED, root);
  }
  if (c->row)  // the List row's tap opens the sheet (listened on the row: the root's own "change" goes on to the spec)
    fw::listen(c->row, [](lv_obj_t *, const char *name, const char *, void *user) {
      if (!strcmp(name, "tap")) open_sheet((lv_obj_t *) user);
    }, root);
  // the sheet on the screen (overlay), closed until a tap
  ArduinoJson::JsonDocument sd;
  sheet_props(c, false, sd);
  lv_obj_t *host = screen_of(parent, parent);
  c->sheet = build("bottom_sheet", host, sd);
  if (c->sheet) {
    lv_obj_add_event_cb(c->sheet, [](lv_event_t *e) { fill_rows((lv_obj_t *) lv_event_get_user_data(e), (lv_obj_t *) lv_event_get_param(e)); },
                        LV_EVENT_REFRESH, root);
    lv_obj_add_event_cb(c->sheet, [](lv_event_t *e) {  // the screen went first: forget it
      auto *root = (lv_obj_t *) lv_event_get_user_data(e);
      if (VM *c = of(root)) c->sheet = nullptr;
    }, LV_EVENT_DELETE, root);
  }
  paint(root);
}

FW_UPDATE(vacuum_mode) {
  VM *c = of(root);
  if (!c || c->style != (p["style"] | "row")) return false;
  take(c, p);
  paint(root);
  return true;
}
