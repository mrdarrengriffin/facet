// Settings screen (docs: Screens > Settings, @facet/ui screens/settings.ts, CSS .fs-set*): the first full screen, every
// personalisation setting from the ONE schema tokens/settings.json (fw::set::, tools/gen_settings.mjs), so the screen,
// the panel's Home Assistant entities, the ESPHome page and the docs never drift. Composed only of real components:
//   header   the Header component (back + "Settings" + the screen chrome: the live clock) in the Page's head
//   body     Screen > Page (fx::page) > Header + list Body (C1-C3), real components only; per schema group a Section
//            header (size label) over its rows, groups a group apart (above:group, the spec renderer's rule)
//   rows     by the setting's control:
//            segmented / swatches  a read-out List row (title, act none) with the Segmented / Accent swatches in its
//                                  end slot
//            toggle                a List row with trailing toggle (the whole row is the hit area)
//            sheet / presets       a List row with value + chevron; a tap opens the Bottom sheet
//            slider                a read-out List row (the name, the value in words) over the Slider component;
//                                  the words follow the knob while it moves
//   API      after the API group's switch: List row "API keys" (value: how many keys) opens Settings > API keys
//            (screens/agent_keys.cpp)
//   sheet    the Bottom sheet component in overlay mode, the screen's last child (its single overlay root)
// Values come from the props (one per setting = the playground; missing = the live value, fw::dev::setting). Inside the
// Device (live) view, Theme / Accent restyle that device; the workbench's own theme and accent stay global settings.
// Sheet rows open the ONE Bottom sheet, moved in place to the setting's options (its `items`); a pick updates the row's
// value; Sound pack stays open after a pick (docs). Every control's change (a pick, a Segmented / swatch / toggle /
// slider) goes through fw::dev::set (core/device.h "settings": the ONE path the screen, HA and the workbench share),
// which applies it and emits "setting" (detail id=value). Missing props = the panel's live values, so the device
// runtime builds this screen with {} and a restyle shows what is set; a change from HA moves the controls in place.
// Update path: a prop change moves every control in place (FW_UPDATE(settings) at the end), no rebuild.
// Bench: prop sheet = "loop" opens and closes the sheet forever over this screen (the Guition sheet speed test).
#include "fw.h"
#include "fx.h"
#include "settings_gen.h"
#include "device.h"
#include "spec.h"
#include "keys.h"

namespace {
using Doc = ArduinoJson::JsonDocument;
using ArduinoJson::JsonArray;
using ArduinoJson::JsonObject;

lv_obj_t *build(const char *id, lv_obj_t *parent, Doc &props) {
  const fw::Component *c = fw::find(id);
  if (!c) return nullptr;
  const uint32_t before = lv_obj_get_child_count(parent);
  c->build(parent, props.as<fw::Props>());
  return lv_obj_get_child_count(parent) > before ? lv_obj_get_child(parent, -1) : nullptr;
}
// a List row's end slot (shown once it holds something), where its trailing control goes (C8a)
lv_obj_t *end_of(lv_obj_t *row) {
  lv_obj_t *e = fw::spec::slot_of(row, 0);
  if (!e) return row;
  lv_obj_remove_flag(e, LV_OBJ_FLAG_HIDDEN);
  return e;
}
// a group's Section header (size label); not the first: a group apart (the spec renderer's rule)
void group(lv_obj_t *body, const char *title, bool first) {
  Doc sh;
  sh["title"] = title, sh["size"] = "label", sh["badge"] = false, sh["right"] = "none";
  lv_obj_t *h = build("section_header", body, sh);
  if (h && !first) fx::use(h, "above:group");
}

// ---- values (as @facet/settings format())
std::string num(double v) {
  char b[24];
  snprintf(b, sizeof b, "%g", v);
  return b;
}
std::string format_number(const fw::set::Setting &s, double v) {
  if (v == 0 && s.zero[0]) return s.zero;
  if (!strcmp(s.unit, "s") && v >= 60 && std::fmod(v, 60) == 0) return num(v / 60) + " min";
  if (!strcmp(s.unit, "min") && v >= 60 && std::fmod(v, 60) == 0) return num(v / 60) + " h";
  return !strcmp(s.unit, "%") ? num(v) + "%" : num(v) + " " + s.unit;
}
const fw::set::Option *option(const fw::set::Setting &s, const char *id) {
  for (int i = 0; i < s.n_options; i++)
    if (!strcmp(s.options[i].id, id)) return &s.options[i];
  return nullptr;
}
// a missing prop = the panel's value now (fw::dev settings: the schema default until one is set)
const char *live(const fw::set::Setting &s) {
  const char *v = fw::dev::setting(s.id);
  return v ? v : s.def;
}
// the prop as a string (select ids, presets "60"), else the live value
std::string str_of(const fw::set::Setting &s, fw::Props p) {
  ArduinoJson::JsonVariantConst v = p[s.id];
  if (v.is<const char *>()) return v.as<const char *>();
  if (v.is<double>()) return num(v.as<double>());
  return live(s);
}
double num_of(const fw::set::Setting &s, fw::Props p) {
  ArduinoJson::JsonVariantConst v = p[s.id];
  if (v.is<double>()) return v.as<double>();
  if (v.is<const char *>()) return atof(v.as<const char *>());
  return atof(live(s));
}
bool bool_of(const fw::set::Setting &s, fw::Props p) {
  ArduinoJson::JsonVariantConst v = p[s.id];
  return v.is<bool>() ? v.as<bool>() : !strcmp(live(s), "true");
}
std::string lower(std::string t) {
  for (char &c : t) c = (char) tolower((unsigned char) c);
  return t;
}

// ---- the screen
struct Screen {
  lv_obj_t *root = nullptr, *sheet = nullptr;
  bool loop = false;
  int first_sheet = -1;                     // the first sheet / presets setting: the one a build (and an update) shows
  const fw::set::Setting *sheet_for = nullptr;  // the setting the sheet shows now, and its row (the value shows the pick)
  lv_obj_t *sheet_row = nullptr;
  lv_obj_t *ctrl[fw::set::N_SETTINGS] = {};  // per setting: the component the update path moves (Segmented, Accent
                                             // swatches, List row, Slider)
  lv_obj_t *words[fw::set::N_SETTINGS] = {};  // per slider setting: the read-out List row over it (its value in words)
  std::string values[fw::set::N_SETTINGS];  // per setting: its value (option id, "true" / "false", the number)
};
int index_of(const fw::set::Setting &s) { return (int) (&s - fw::set::SETTINGS); }

// the value of setting s from the props (missing = the schema default), as a string
std::string value_of(const fw::set::Setting &s, fw::Props p) {
  if (s.kind == fw::set::SWITCH) return bool_of(s, p) ? "true" : "false";
  if (s.kind == fw::set::NUMBER) return num(num_of(s, p));
  return str_of(s, p);
}
std::string shown(const fw::set::Setting &s, const std::string &v) {
  if (s.kind == fw::set::SELECT) {
    const fw::set::Option *o = option(s, v.c_str());
    return o ? o->label : v;
  }
  return format_number(s, atof(v.c_str()));
}

// a control in the column changed its value (Segmented / Accent swatches / Slider "change", a toggle row's
// "toggle"): the setting it belongs to takes it through the ONE set() path (fw::dev::set, device.h "settings"), which
// keeps it, applies it (theme / accent restyle the device) and emits "setting" id=value. Everything else goes on up
// to the next listener (the device runtime: the Header's "back"). Slider "input" (live while dragging) is not a value.
bool inside(lv_obj_t *o, lv_obj_t *in) {
  for (; o; o = lv_obj_get_parent(o))
    if (o == in) return true;
  return false;
}
void on_control(lv_obj_t *from, const char *name, const char *detail, void *user) {
  auto *scr = (Screen *) user;
  const bool change = !strcmp(name, "change"), flip = !strcmp(name, fw::st::SWITCH_EVENT);
  if (change || flip)
    for (int k = 0; k < fw::set::N_SETTINGS; k++) {
      lv_obj_t *c = scr->ctrl[k];
      if (!c || !inside(from, c)) continue;
      const fw::set::Setting &s = fw::set::SETTINGS[k];
      std::string v = detail;
      if (flip) v = !strcmp(detail, "on") ? "true" : "false";
      else if (s.control == fw::set::SEGMENTED)  // Segmented ids = labels lower-cased
        for (int i = 0; i < s.n_options; i++)
          if (lower(s.options[i].label) == detail) v = s.options[i].id;
      scr->values[k] = v;
      fw::dev::set(s.id, v.c_str(), fw::dev::SCREEN, c);
      return;
    }
  if (lv_obj_t *up = lv_obj_get_parent(scr->root)) fw::notify(up, name, detail);
}

// the Settings row that opens Settings > API keys: its value = how many keys
void keys_props(Doc &d) {
  const size_t n = fw::keys::list().size();
  const std::string v = !n ? std::string("No keys") : n == 1 ? std::string("1 key") : std::to_string(n) + " keys";
  d["title"] = "API keys", d["icon"] = "none", d["trailing"] = "value-chevron", d["value"] = v.c_str();
}
// the props of setting s's component for value v: ONE place, so a build and the update path can't drift
void ctrl_props(const fw::set::Setting &s, const std::string &v, Doc &d) {
  using namespace fw::set;
  switch (s.control) {
    case SWATCHES:
      d["selected"] = v.c_str();
      return;
    case SEGMENTED: {
      std::string labels, sel;
      for (int i = 0; i < s.n_options; i++) {
        if (i) labels += ", ";
        labels += s.options[i].label;
        if (v == s.options[i].id) sel = lower(s.options[i].label);  // Segmented ids = labels lower-cased
      }
      d["options"] = labels.c_str();
      d["selected"] = sel.c_str();
      return;
    }
    case TOGGLE:
    case SHEET:
    case PRESETS:
      d["title"] = s.label;
      d["icon"] = "none";
      d["trailing"] = s.control == TOGGLE ? "toggle" : "value-chevron";
      d["value"] = s.control == TOGGLE ? "" : shown(s, v).c_str();
      d["on"] = s.control == TOGGLE && v == "true";
      return;
    case SLIDER:
      d["min"] = s.min;
      d["max"] = s.max;
      d["step"] = s.step;
      d["value"] = atof(v.c_str());
      return;
  }
}
// a slider setting's read-out row: its name and the value in words
void words_props(const fw::set::Setting &s, const std::string &v, Doc &d) {
  d["title"] = s.label, d["icon"] = "none", d["trailing"] = "value", d["act"] = "none";
  d["value"] = format_number(s, atof(v.c_str()));
}
const char *ctrl_id(const fw::set::Setting &s) {
  using namespace fw::set;
  return s.control == SWATCHES ? "accent_swatches" : s.control == SEGMENTED ? "segmented" : s.control == SLIDER ? "slider" : "list_row";
}

// the icon on a sheet's rows (docs SHEET_ICON; idleAfter = timer, not shipped at icon size: check)
const char *sheet_icon(const fw::set::Setting &s) {
  if (!strcmp(s.id, "soundPack")) return "volume";
  if (!strcmp(s.id, "screenOff")) return "power";
  return "check";
}
// the Bottom sheet's props for setting s (its options, title, the value ringed)
void sheet_props(Screen *scr, const fw::set::Setting &s, bool open, Doc &d) {
  const std::string &v = scr->values[index_of(s)];
  d["overlay"] = true;
  d["title"] = lower(s.label).c_str();
  d["open"] = open ? "open" : "closed";
  d["loop"] = scr->loop;
  d["stay"] = !strcmp(s.id, "soundPack");  // a sound pack: stay open, so the next one can be heard straight after
  JsonArray items = d["items"].to<JsonArray>();
  const int n = s.kind == fw::set::SELECT ? s.n_options : s.n_presets;
  int sel = 0;
  for (int i = 0; i < n; i++) {
    JsonObject it = items.add<JsonObject>();
    it["icon"] = sheet_icon(s);
    if (s.kind == fw::set::SELECT) {
      it["name"] = s.options[i].label;
      it["status"] = s.options[i].hint;
      if (v == s.options[i].id) sel = i;
    } else {
      it["name"] = format_number(s, s.presets[i]).c_str();
      if (std::fabs(atof(v.c_str()) - s.presets[i]) < 1e-9) sel = i;
    }
  }
  d["selected"] = sel;
}

// the one Bottom sheet over the screen (overlay = its single root), made once; a row tap moves it to that setting's
// options in place (Bottom sheet update path: rows again, then sheet-open)
void make_sheet(Screen *scr, bool open) {
  if (scr->first_sheet < 0) return;
  const fw::set::Setting &s = fw::set::SETTINGS[scr->first_sheet];
  scr->sheet_for = &s;
  scr->sheet_row = scr->ctrl[scr->first_sheet];
  Doc d;
  sheet_props(scr, s, open, d);
  scr->sheet = build("bottom_sheet", scr->root, d);
  if (!scr->sheet) return;
  lv_obj_add_event_cb(scr->sheet, [](lv_event_t *e) {  // a pick: remember it, the row shows it, report the setting
    auto *scr = (Screen *) lv_event_get_user_data(e);
    const fw::set::Setting &s = *scr->sheet_for;
    const int i = (int) (intptr_t) lv_event_get_param(e);
    std::string &v = scr->values[index_of(s)];
    v = s.kind == fw::set::SELECT ? (i < s.n_options ? s.options[i].id : v) : (i < s.n_presets ? num(s.presets[i]) : v);
    if (scr->sheet_row) {
      Doc rp;
      ctrl_props(s, v, rp);
      fw::update_child(fw::find("list_row"), scr->sheet_row, rp.as<fw::Props>());
    }
    fw::dev::set(s.id, v.c_str(), fw::dev::SCREEN, scr->sheet);  // keeps + applies it, emits "setting" id=value
  }, LV_EVENT_VALUE_CHANGED, scr);
}
void open_sheet(Screen *scr, const fw::set::Setting &s, lv_obj_t *row) {
  if (!scr->sheet) return;
  scr->sheet_for = &s;
  scr->sheet_row = row;
  Doc d;
  sheet_props(scr, s, true, d);
  fw::update_child(fw::find("bottom_sheet"), scr->sheet, d.as<fw::Props>());
}

// the row of setting s in the column: its component (kept in scr->ctrl for the update path)
void row_for(Screen *scr, lv_obj_t *col, const fw::set::Setting &s) {
  using namespace fw::set;
  const int k = index_of(s);
  const std::string &v = scr->values[k];
  Doc d;
  ctrl_props(s, v, d);
  switch (s.control) {
    case SEGMENTED:
    case SWATCHES: {  // a List row (title only, not a tap target) + the control appended as its trailing part
      Doc rd;
      rd["title"] = s.label;
      rd["icon"] = "none";
      rd["trailing"] = "none";
      rd["act"] = "none";  // the control is the target, not the row
      lv_obj_t *r = build("list_row", col, rd);
      if (!r) return;
      scr->ctrl[k] = build(ctrl_id(s), end_of(r), d);
      return;
    }
    case TOGGLE:
      scr->ctrl[k] = build("list_row", col, d);
      return;
    case SHEET:
    case PRESETS: {
      lv_obj_t *r = build("list_row", col, d);
      scr->ctrl[k] = r;
      if (!r) return;
      if (scr->first_sheet < 0) scr->first_sheet = k;
      lv_obj_set_user_data(r, (void *) &s);
      lv_obj_add_event_cb(r, [](lv_event_t *e) {  // after the List row's own tap (event + sound open)
        auto *scr = (Screen *) lv_event_get_user_data(e);
        auto *row = (lv_obj_t *) lv_event_get_current_target(e);
        if (lv_obj_has_state(row, fw::st::OFF)) return;
        open_sheet(scr, *(const fw::set::Setting *) lv_obj_get_user_data(row), row);
      }, LV_EVENT_CLICKED, scr);
      return;
    }
    case SLIDER: {  // a read-out List row (the name, the value in words) over the Slider
      Doc wd;
      words_props(s, v, wd);
      scr->words[k] = build("list_row", col, wd);
      lv_obj_t *sl = scr->ctrl[k] = build("slider", col, d);
      if (!sl || !scr->words[k]) return;
      // the words follow the knob: the Slider's live "input" (a finger, or the update path's quiet sweep)
      fw::listen(sl, [](lv_obj_t *from, const char *name, const char *detail, void *user) {
        auto *scr = (Screen *) user;
        // "change" goes to the settings path from the Slider itself (re-sent from its parent it matched no control:
        // the glass Volume never applied)
        if (strcmp(name, "input")) { on_control(from, name, detail, scr); return; }
        for (int k = 0; k < fw::set::N_SETTINGS; k++)
          if (scr->ctrl[k] == from && scr->words[k]) {
            Doc wd;
            words_props(fw::set::SETTINGS[k], detail, wd);
            fw::update_child(fw::find("list_row"), scr->words[k], wd.as<fw::Props>());
            return;
          }
      }, scr);
      return;
    }
  }
}

std::string meta_json() {
  std::string props = fw::set::PROPS_JSON;  // one per setting (generated), then the screen's own
  props.pop_back();
  props += R"(,{"name":"sheet","about":"For previews only: shows the picker sheet closed, open, or opening and closing over and over to check its motion.","type":"select","options":["closed","open","loop"],"default":"closed"}])";
  return std::string(R"({
  "about": "Every personalisation setting of the panel, grouped, from the one settings schema: inline controls, toggles, sliders and pickers in a bottom sheet.",
  "frame": [480, 480],
  "icons": ["back@glyph", "screen-settings@icon", "chevron@icon", "close@glyph", "volume@icon", "power@icon", "check@icon"],
  "props": )") + props + R"J(,
  "variants": [
    {"label": "default values", "props": {}},
    {"label": "changed values", "props": {"theme": "dark", "accent": "teal", "sounds": false, "volume": 35, "idleAfter": "300", "speedUnit": "kn"}},
    {"label": "sheet open", "props": {"sheet": "open"}},
    {"label": "sheet bench (open / close loop)", "props": {"sheet": "loop"}},
    {"label": "reduced motion", "props": {"sheet": "open", "reduced_motion": true}}
  ]
})J";
}
const std::string META = meta_json();

void build_settings(lv_obj_t *parent, fw::Props p) {
  auto *scr = new Screen;
  scr->root = fx::box(parent, "fill");
  lv_obj_set_user_data(scr->root, scr);
  lv_obj_add_event_cb(scr->root, [](lv_event_t *e) { delete (Screen *) lv_event_get_user_data(e); }, LV_EVENT_DELETE, scr);
  for (int i = 0; i < fw::set::N_SETTINGS; i++) scr->values[i] = value_of(fw::set::SETTINGS[i], p);
  fw::listen(scr->root, on_control, scr);

  Doc hd;  // the Header (+ its screen chrome: the live clock; the settings button is Home's only)
  hd["label"] = "Settings";
  const fx::Page fp = fx::page(scr->root, "list");  // Screen > Page > Header + list Body (C1-C3)
  build("header", fp.head, hd);
  lv_obj_t *c = fp.body;

  // per group: a Section header (size label) over its rows; groups a group apart
  {  // "panel": Screens (Settings > Screens: which screens this panel has, their sources and options) + Home Assistant
    // (Settings > Home Assistant: the panel's own link to HA, screens/ha_link.cpp)
    group(c, "panel", true);
    Doc deck;
    deserializeJson(deck, fw::spec::deck_json());
    int on = 0, all = 0;
    for (ArduinoJson::JsonVariantConst x : deck["pages"].as<ArduinoJson::JsonArrayConst>())
      if (strcmp(x["kind"] | "", "home")) on++;  // the deck's pages (not the Homes) + the page kinds it has none of
    all = on + (int) fw::spec::off_kinds().size();
    Doc rd;
    rd["title"] = "Screens", rd["icon"] = "none", rd["trailing"] = "value-chevron";
    rd["value"] = (std::to_string(on) + " of " + std::to_string(all) + " on").c_str();
    if (lv_obj_t *r = build("list_row", c, rd))
      lv_obj_add_event_cb(r, [](lv_event_t *e) {  // after the List row's own tap (event tap + sound open)
        auto *o = (lv_obj_t *) lv_event_get_current_target(e);
        if (!lv_obj_has_state(o, fw::st::OFF)) fw::emit(o, "navigate", "screens");
      }, LV_EVENT_CLICKED, nullptr);
    Doc hr;  // Home Assistant: the panel's own link (screens/ha_link.cpp)
    namespace hl = fw::dev::ha_link;
    hr["title"] = "Home Assistant", hr["icon"] = "none", hr["trailing"] = "value-chevron";
    // linked = an address and a token, whether or not the entity list has been read yet: "Linked, <host>"
    std::string host = hl::url();
    if (size_t a = host.find("://"); a != std::string::npos) host.erase(0, a + 3);
    host = host.substr(0, host.find_first_of(":/"));
    hr["value"] = !hl::ready() ? std::string("Not set up") : hl::status() == hl::FAIL ? std::string("Not reached") : "Linked, " + host;
    if (lv_obj_t *r = build("list_row", c, hr))
      lv_obj_add_event_cb(r, [](lv_event_t *e) {
        auto *o = (lv_obj_t *) lv_event_get_current_target(e);
        if (!lv_obj_has_state(o, fw::st::OFF)) fw::emit(o, "navigate", "ha_link");
      }, LV_EVENT_CLICKED, nullptr);
  }
  for (int g = 0; g < fw::set::N_GROUPS; g++) {
    group(c, fw::set::GROUPS[g].label, false);
    for (int i = 0; i < fw::set::N_SETTINGS; i++)
      if (!strcmp(fw::set::SETTINGS[i].group, fw::set::GROUPS[g].id) && strcmp(fw::set::SETTINGS[i].id, "idleAfter"))
        row_for(scr, c, fw::set::SETTINGS[i]);  // Clock after: the screensaver is not built yet (no knob without an effect)
    if (strcmp(fw::set::GROUPS[g].id, "agents")) continue;
    Doc ar;  // API keys: the keys scripts, automations and agents use (screens/agent_keys.cpp, core/keys.h)
    keys_props(ar);
    if (lv_obj_t *r = build("list_row", c, ar)) {
      lv_obj_add_event_cb(r, [](lv_event_t *e) {
        auto *o = (lv_obj_t *) lv_event_get_current_target(e);
        if (!lv_obj_has_state(o, fw::st::OFF)) fw::emit(o, "navigate", "agent_keys");
      }, LV_EVENT_CLICKED, nullptr);
      // the count follows the keys (a key allowed on the glass while Settings shows, one revoked by an agent)
      fx::every(r, 250, [r, rev = fw::keys::rev()]() mutable {
        if (rev == fw::keys::rev()) return;
        rev = fw::keys::rev();
        Doc k;
        keys_props(k);
        fw::update_child(fw::find("list_row"), r, k.as<fw::Props>());
      });
    }
  }

  const char *sheet = p["sheet"] | "closed";
  scr->loop = !strcmp(sheet, "loop");
  make_sheet(scr, strcmp(sheet, "closed"));
}
const int reg = fw::add({"settings", "Settings", "screens", META.c_str(), build_settings});
}  // namespace

// The update path (README "Update path"): new values move every control in place with its own animation (Segmented /
// Accent swatches slide to the choice, Toggles slide, Sliders sweep with the value label following, picker rows
// show the new value), the sheet opens / closes / changes its list in place; nothing is built again. Silent (core
// fw::Quiet + the components' quiet animations). Only the bench `loop` makes the sheet again (its timer).
FW_UPDATE(settings) {
  auto *scr = (Screen *) lv_obj_get_user_data(root);
  if (!scr || scr->root != root) return false;
  for (int i = 0; i < fw::set::N_SETTINGS; i++) {
    const fw::set::Setting &s = fw::set::SETTINGS[i];
    scr->values[i] = value_of(s, p);
    if (!scr->ctrl[i]) continue;
    Doc d;
    ctrl_props(s, scr->values[i], d);
    fw::update_child(fw::find(ctrl_id(s)), scr->ctrl[i], d.as<fw::Props>());
    if (scr->words[i]) {
      Doc wd;
      words_props(s, scr->values[i], wd);
      fw::update_child(fw::find("list_row"), scr->words[i], wd.as<fw::Props>());
    }
  }
  if (!p["sheet"].is<const char *>()) return true;  // no sheet prop (the device runtime's value moves): the sheet stays
  const char *sheet = p["sheet"] | "closed";
  const bool loop = !strcmp(sheet, "loop"), open = strcmp(sheet, "closed");
  if (loop != scr->loop || !scr->sheet) {
    if (scr->sheet) lv_obj_delete(scr->sheet);  // (no DELETE hook on it: the root's DELETE frees scr first)
    scr->sheet = nullptr;
    scr->loop = loop;
    make_sheet(scr, open);
    return true;
  }
  if (scr->first_sheet >= 0) {  // back to what a build shows: the first sheet setting's list, open or closed
    const fw::set::Setting &s = fw::set::SETTINGS[scr->first_sheet];
    scr->sheet_for = &s;
    scr->sheet_row = scr->ctrl[scr->first_sheet];
    Doc d;
    sheet_props(scr, s, open, d);
    fw::update_child(fw::find("bottom_sheet"), scr->sheet, d.as<fw::Props>());
  }
  return true;
}
