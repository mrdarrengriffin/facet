// Settings screen (docs: Screens > Settings, @facet/ui screens/settings.ts, CSS .fs-set*): the first full screen, every
// personalisation setting from the ONE schema tokens/settings.json (fw::set::, tools/gen_settings.mjs), so the screen,
// the panel's Home Assistant entities, the ESPHome page and the docs never drift. Composed only of real components:
//   header   the Header component (back + "Settings" + the screen chrome: the live clock) at (inset, inset)
//   content  from lay::top to the screen bottom, one scrolling column (bottom padding inset); per schema group:
//            its label (docs .fs-set-glabel = the screen type label, caps, as the header's: Darren 2026-10-22, the
//            component caps-sm was too small) then its rows; label to rows = section-gap, rows to the next label = group
//   rows     by the setting's control:
//            segmented / swatches  a List row (title, no trailing, not a tap target) + the Segmented / Accent swatches
//                                  component appended as its trailing part (docs .fs-set with an inline control)
//            toggle                a List row with trailing toggle (the whole row is the hit area)
//            sheet / presets       a List row with value + chevron; a tap opens the Bottom sheet
//            slider                a slider row, layout token set-slider-row-h (144 / 96): hairline, title + value on
//                                  top (title / body type), the Slider component full width under them; the value
//                                  follows the knob while it moves
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
#include "settings_gen.h"
#include "device.h"
#include "spec.h"

namespace {
using Doc = ArduinoJson::JsonDocument;
using ArduinoJson::JsonArray;
using ArduinoJson::JsonObject;

lv_obj_t *box(lv_obj_t *parent) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  return o;
}
lv_obj_t *build(const char *id, lv_obj_t *parent, Doc &props) {
  const fw::Component *c = fw::find(id);
  if (!c) return nullptr;
  const uint32_t before = lv_obj_get_child_count(parent);
  c->build(parent, props.as<fw::Props>());
  return lv_obj_get_child_count(parent) > before ? lv_obj_get_child(parent, -1) : nullptr;
}
lv_obj_t *spacer(lv_obj_t *parent, int h) {
  lv_obj_t *s = box(parent);
  lv_obj_set_size(s, 1, h);
  return s;
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
struct SliderRow {
  const fw::set::Setting *s;
  lv_obj_t *value, *slider;
};
struct Screen {
  lv_obj_t *root = nullptr, *sheet = nullptr;
  bool loop = false;
  int first_sheet = -1;                     // the first sheet / presets setting: the one a build (and an update) shows
  const fw::set::Setting *sheet_for = nullptr;  // the setting the sheet shows now, and its row (the value shows the pick)
  lv_obj_t *sheet_row = nullptr;
  lv_obj_t *ctrl[fw::set::N_SETTINGS] = {};  // per setting: the component the update path moves (Segmented, Accent
                                             // swatches, List row, Slider)
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
      // track + knob = the row's content width (width prop = the track in dp)
      d["width"] = (int) std::lround((fw::lay::content_w - 2 * fw::sp::s4 - fw::touch::min / 2) / fw::D);
      return;
  }
}
const char *ctrl_id(const fw::set::Setting &s) {
  using namespace fw::set;
  return s.control == SWATCHES ? "accent_swatches" : s.control == SEGMENTED ? "segmented" : s.control == SLIDER ? "slider" : "list_row";
}

// the icon on a sheet's rows (docs SHEET_ICON; clockFace / idleAfter = timer, not shipped at icon size: check)
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
  const fw::Theme &t = *fw::T;
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
      lv_obj_t *r = build("list_row", col, rd);
      if (!r) return;
      lv_obj_remove_flag(r, LV_OBJ_FLAG_CLICKABLE);  // the control is the target, not the row
      scr->ctrl[k] = build(ctrl_id(s), r, d);
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
    case SLIDER: {
      lv_obj_t *r = box(col);
      lv_obj_set_size(r, fw::lay::content_w, fw::lay::set_slider_row_h);
      lv_obj_set_style_border_side(r, LV_BORDER_SIDE_TOP, 0);  // the List row hairline
      lv_obj_set_style_border_width(r, fw::stroke::hair, 0);
      lv_obj_set_style_border_color(r, t.line, 0);
      lv_obj_set_style_pad_top(r, fw::sp::s7 - fw::stroke::hair, 0);  // the border counts as content offset
      lv_obj_set_style_pad_hor(r, fw::sp::s4, 0);                   // = List row side padding
      lv_obj_set_flex_flow(r, LV_FLEX_FLOW_COLUMN);
      lv_obj_set_style_pad_row(r, fw::sp::s5, 0);
      lv_obj_t *top = box(r);
      lv_obj_set_size(top, LV_PCT(100), fw::style(fw::ty::title).box);
      lv_obj_set_flex_flow(top, LV_FLEX_FLOW_ROW);
      lv_obj_set_flex_align(top, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
      lv_obj_t *ti = fw::text(top, fw::ty::title, s.label, t.text);
      lv_obj_set_height(ti, fw::style(fw::ty::title).box);
      lv_obj_set_flex_grow(ti, 1);
      auto *sr = new SliderRow{&s, nullptr, nullptr};
      sr->value = fw::text(top, fw::ty::body, format_number(s, atof(v.c_str())).c_str(), t.text_dim);
      lv_obj_set_height(sr->value, fw::style(fw::ty::body).box);
      sr->slider = scr->ctrl[k] = build("slider", r, d);
      lv_obj_add_event_cb(r, [](lv_event_t *e) { delete (SliderRow *) lv_event_get_user_data(e); }, LV_EVENT_DELETE, sr);
      // the value follows the knob: the fill (track child 0) resizes with every drawn position (a finger, or the
      // update path's quiet sweep)
      if (sr->slider && lv_obj_get_child_count(sr->slider) > 0) {
        lv_obj_t *track = lv_obj_get_child(sr->slider, 0);
        if (lv_obj_t *fill = lv_obj_get_child(track, 0))
          lv_obj_add_event_cb(fill, [](lv_event_t *e) {
            auto *sr = (SliderRow *) lv_event_get_user_data(e);
            lv_obj_t *fill = (lv_obj_t *) lv_event_get_current_target(e);
            const int w = lv_obj_get_width(lv_obj_get_parent(fill));
            if (w <= 0) return;
            const fw::set::Setting &s = *sr->s;
            const double n = (s.max - s.min) / s.step;
            const double v = s.min + std::lround((double) lv_obj_get_width(fill) / w * n) * s.step;
            const std::string txt = format_number(s, v);
            if (strcmp(lv_label_get_text(sr->value), txt.c_str())) fw::set_text(sr->value, fw::ty::body, txt.c_str());
          }, LV_EVENT_SIZE_CHANGED, sr);
      }
      return;
    }
  }
}

std::string meta_json() {
  std::string props = fw::set::PROPS_JSON;  // one per setting (generated), then the screen's own
  props.pop_back();
  props += R"(,{"name":"sheet","type":"select","options":["closed","open","loop"],"default":"closed"}])";
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
  const fw::Theme &t = *fw::T;
  auto *scr = new Screen;
  scr->root = box(parent);
  lv_obj_set_user_data(scr->root, scr);
  lv_obj_set_size(scr->root, fw::lay::screen_w, fw::lay::screen_h);
  lv_obj_set_style_bg_color(scr->root, t.bg, 0);
  lv_obj_set_style_bg_opa(scr->root, LV_OPA_COVER, 0);
  lv_obj_add_event_cb(scr->root, [](lv_event_t *e) { delete (Screen *) lv_event_get_user_data(e); }, LV_EVENT_DELETE, scr);
  for (int i = 0; i < fw::set::N_SETTINGS; i++) scr->values[i] = value_of(fw::set::SETTINGS[i], p);
  fw::listen(scr->root, on_control, scr);

  Doc hd;  // the Header (+ its screen chrome: the live clock; the settings button is Home's only)
  hd["label"] = "Settings";
  if (lv_obj_t *h = build("header", scr->root, hd)) lv_obj_set_pos(h, fw::lay::inset, fw::lay::inset);

  lv_obj_t *c = box(scr->root);
  lv_obj_add_flag(c, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(c, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(c, LV_SCROLLBAR_MODE_OFF);  // the Facet scroll track is its own part (later)
  lv_obj_set_pos(c, fw::lay::inset, fw::lay::top);
  lv_obj_set_size(c, fw::lay::content_w, fw::lay::screen_h - fw::lay::top);
  lv_obj_set_style_pad_bottom(c, fw::lay::inset, 0);
  lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);

  // per group: its label (docs .fs-set-glabel: the screen type label, caps, the header's own style; text-dim, docs
  // text-mute is disabled-only here) at its line box, section-gap above its rows; groups a group apart
  {  // "panel": Screens (Settings > Screens: which screens this panel has, their sources and options)
    lv_obj_t *gl = fw::text(c, fw::ty::label, "panel", t.text_dim);
    lv_obj_remove_flag(gl, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_height(gl, fw::style(fw::ty::label).box);
    spacer(c, fw::lay::section_gap);
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
    spacer(c, fw::lay::group);
  }
  for (int g = 0; g < fw::set::N_GROUPS; g++) {
    if (g) spacer(c, fw::lay::group);
    lv_obj_t *gl = fw::text(c, fw::ty::label, fw::set::GROUPS[g].label, t.text_dim);
    lv_obj_remove_flag(gl, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_height(gl, fw::style(fw::ty::label).box);  // fw::text leaves default labels at content height
    spacer(c, fw::lay::section_gap);
    for (int i = 0; i < fw::set::N_SETTINGS; i++)
      if (!strcmp(fw::set::SETTINGS[i].group, fw::set::GROUPS[g].id)) row_for(scr, c, fw::set::SETTINGS[i]);
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
