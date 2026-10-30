// Vacuum mode: the robot vacuum's cleaning settings (the panel lab's Vacuum Mode row + mode sheet, board facet-52,
// apps/hub panel/vacuum/screen.ts), on the contract (renderer/CONTRACT.md, "contract": 2): cleaning mode (Vacuum /
// Vac + mop / Mop), suction (Quiet .. Max+) and water (Low / Medium / High), applied at once like the Roborock app.
// Made of the real components (C8b), nothing drawn here:
//   row     the List row (fw::find("list_row")), its parent's width: the over line "mode", the title = what it will do
//           ("Vac + mop · Turbo · High water": suction only when it vacuums, water only when it mops) and a chevron;
//           a tap opens the sheet (the List row's tap + sound open)
//   sheet   the Bottom sheet (fw::find("bottom_sheet"), title "mode", its list filled here: the panel as tall as what
//           is in it) over the screen, one block per setting: a hairline on top (line:line, rule-top), the setting's
//           name (type:label, caps, fg:text-dim, space 7 above), its Segmented (hug, space 5 above), space 7 under.
//           Suction hides for Mop, Water for Vacuum. A pick = event "change" "mode <id>" / "fan <id>" / "mop <id>"
//           (the spec's vacuum.control) and the texts follow at once; the Segmented's own tap sound plays
// The Map view's short mode button (the old style "button") left with the Map view (2026-10-30: the floor plan is not
// on the contract yet). Props: mode, fan, mop (the Roborock ids; HA's select states / fan_speed).
// States: the List row's (n/a here: pressed / disabled / unavailable are its own). Update path: texts in place (the
// List row's update), the sheet's choices move with the props. Bleed: none. Contrast: section "vacuum_mode" (+
// list_row, segmented, bottom_sheet).
#include "fw.h"
#include "fx.h"
#include "sheet.h"
#include "spec.h"

namespace {
struct Opt { const char *id, *label; };
constexpr Opt MODES[] = {{"vacuum", "Vacuum"}, {"vac_and_mop", "Vac + mop"}, {"mop", "Mop"}};
constexpr Opt FANS[] = {{"quiet", "Quiet"}, {"balanced", "Balanced"}, {"turbo", "Turbo"}, {"max", "Max"}, {"max_plus", "Max+"}};
constexpr Opt MOPS[] = {{"low", "Low"}, {"medium", "Medium"}, {"high", "High"}};
struct Set { const char *key, *name; const Opt *opts; int n; };
constexpr Set SETS[] = {{"mode", "Cleaning", MODES, 3}, {"fan", "Suction", FANS, 5}, {"mop", "Water", MOPS, 3}};

struct VM {
  std::string mode, fan, mop;
  lv_obj_t *row = nullptr, *sheet = nullptr;
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
// "Vac + mop · Turbo · High water" (suction only when it vacuums, water only when it mops)
std::string line(const VM *c) {
  std::string o = c->mode.empty() ? std::string("\xE2\x80\x94") : label_of(SETS[0], c->mode);
  if (c->mode != "mop" && !c->fan.empty()) o += std::string(" \xC2\xB7 ") + label_of(SETS[1], c->fan);
  if (c->mode != "vacuum" && !c->mop.empty()) o += std::string(" \xC2\xB7 ") + label_of(SETS[2], c->mop) + " water";
  return o;
}
bool shows(const VM *c, int k) { return k == 0 || (k == 1 && c->mode != "mop") || (k == 2 && c->mode != "vacuum"); }
lv_obj_t *build(const char *id, lv_obj_t *parent, ArduinoJson::JsonDocument &d) {
  const uint32_t n = lv_obj_get_child_count(parent);
  if (const fw::Component *c = fw::find(id)) c->build(parent, d.as<fw::Props>());
  return lv_obj_get_child_count(parent) > n ? lv_obj_get_child(parent, -1) : nullptr;
}
lv_obj_t *list_of(lv_obj_t *ov) { return ov ? lv_obj_get_child(fw::sheet::panel(ov), 1) : nullptr; }  // head, list
void paint(lv_obj_t *root);
// the sheet's blocks again (at build, and when the mode changes: Suction / Water come and go); the panel refits
void fill_rows(lv_obj_t *root) {
  VM *c = of(root);
  lv_obj_t *list = list_of(c->sheet);
  if (!list) return;
  lv_obj_clean(list);
  for (int k = 0; k < 3; k++) {
    if (!shows(c, k)) continue;
    const Set &s = SETS[k];
    lv_obj_t *r = fx::box(list, "col");
    for (const char *t : {"line:line", "rule-top"}) fx::use(r, t);
    fx::use(fx::text(r, s.name, "type:label", "fg:text-dim"), "above:7");
    std::string opts;
    for (int i = 0; i < s.n; i++) opts += std::string(i ? ", " : "") + s.opts[i].label;
    const std::string &cur = k == 0 ? c->mode : k == 1 ? c->fan : c->mop;
    ArduinoJson::JsonDocument d;
    d["options"] = opts;
    d["selected"] = lower(label_of(s, cur));
    lv_obj_t *seg = build("segmented", r, d);
    if (!seg) continue;
    fx::use(seg, "above:5");
    fx::use(fx::box(r, "free"), "above:7");  // space 7 under the Segmented (the next hairline)
    lv_obj_set_user_data(r, (void *) (intptr_t) k);
    lv_obj_add_flag(r, LV_OBJ_FLAG_USER_3);  // a settings block (found from its Segmented's event)
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
        // Suction / Water come and go with the mode: the blocks again, still open. On the next tick: the Segmented
        // that sent this is in one of them, and it still plays its tap after this returns (a clean now = use after free)
        if (k == 0 && c->sheet)
          lv_async_call([](void *r) { if (lv_obj_is_valid((lv_obj_t *) r)) fill_rows((lv_obj_t *) r); }, root);
        return;
      }
    }, root);
  }
}
void open_sheet(lv_obj_t *root) {
  VM *c = of(root);
  if (!c->sheet) return;
  ArduinoJson::JsonDocument d;
  d["_overlay"] = true, d["title"] = "mode", d["open"] = "open";
  fw::update_child(fw::find("bottom_sheet"), c->sheet, d.as<fw::Props>());
}
void paint(lv_obj_t *root) {  // the row's texts from the props
  VM *c = of(root);
  if (!c->row) return;
  ArduinoJson::JsonDocument d;
  d["over"] = "mode", d["title"] = line(c), d["trailing"] = "chevron", d["icon"] = "none";
  fw::update_child(fw::find("list_row"), c->row, d.as<fw::Props>());
}
void take(VM *c, fw::Props p) {
  c->mode = p["mode"] | "vac_and_mop";
  c->fan = p["fan"] | "turbo";
  c->mop = p["mop"] | "high";
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
  "about": "The robot vacuum's cleaning settings: a row that opens a sheet with the cleaning mode, suction and water, applied at once.",
  "contract": 2,
  "parts": [],
  "frame": [480, 0],
  "props": [
    {"name": "mode", "about": "What the robot does: vacuum only, vacuum and mop together, or mop only.", "type": "select", "options": ["vacuum", "vac_and_mop", "mop"], "default": "vac_and_mop"},
    {"name": "fan", "about": "How hard it sucks: quiet, balanced, turbo, max or max plus.", "type": "select", "options": ["quiet", "balanced", "turbo", "max", "max_plus"], "default": "turbo"},
    {"name": "mop", "about": "How much water the mop uses: low, medium or high.", "type": "select", "options": ["low", "medium", "high"], "default": "high"}
  ],
  "variants": [
    {"label": "row", "props": {}},
    {"label": "row · vacuum only", "props": {"mode": "vacuum", "fan": "balanced"}}
  ]
})") {
  auto *c = new VM;
  take(c, p);
  lv_obj_t *root = fx::box(parent, "col");
  fx::name(root, "vacuum_mode");
  lv_obj_set_user_data(root, c);
  lv_obj_add_event_cb(root, [](lv_event_t *e) {
    auto *c = (VM *) lv_event_get_user_data(e);
    if (c->sheet) lv_obj_delete(c->sheet);  // it lives on the screen, not inside this (its own cb clears c->sheet)
    delete c;
  }, LV_EVENT_DELETE, c);
  ArduinoJson::JsonDocument d;
  d["over"] = "mode", d["title"] = line(c), d["trailing"] = "chevron", d["icon"] = "none";
  c->row = build("list_row", root, d);
  if (c->row)  // the List row's tap opens the sheet (listened on the row: the root's own "change" goes on to the spec)
    fw::listen(c->row, [](lv_obj_t *, const char *name, const char *, void *user) {
      if (!strcmp(name, "tap")) open_sheet((lv_obj_t *) user);
    }, root);
  // the sheet on the screen (over the page), closed until a tap; its list is filled here
  ArduinoJson::JsonDocument sd;
  sd["_overlay"] = true, sd["title"] = "mode", sd["open"] = "closed";
  c->sheet = build("bottom_sheet", screen_of(parent, parent), sd);
  if (c->sheet) {
    lv_obj_add_event_cb(c->sheet, [](lv_event_t *e) {  // the screen went first: forget it
      auto *root = (lv_obj_t *) lv_event_get_user_data(e);
      if (VM *c = of(root)) c->sheet = nullptr;
    }, LV_EVENT_DELETE, root);
    fill_rows(root);
  }
}

FW_UPDATE(vacuum_mode) {
  VM *c = of(root);
  if (!c) return false;
  const std::string was = c->mode;
  take(c, p);
  paint(root);
  if (c->mode != was && c->sheet && !fw::sheet::is_open(c->sheet)) fill_rows(root);  // closed: the next open shows it
  return true;
}
