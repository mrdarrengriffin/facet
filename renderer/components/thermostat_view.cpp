// Thermostat view: the Thermostat screen's body (docs screens/thermostat.ts thermostatScreen; the factory spec
// renderer/specs/thermostat.json adds the header): the ring is the hero. The real Thermostat dial (fw::find) with the
// measured temperature big in its middle, the set point under it, the schedule's quiet line, and - / + square Icon
// buttons; under the ring the modes (up to three Buttons, the current one active) and a Presets button that opens a
// Bottom sheet of the presets (Picker rows: in use / tap to use).
//   centre   "heating" / "cooling" / "idle" / "off" (type label: accent / blue / text-mute), the measured temperature
//            (display, text; off = text-dim; "19.4°"), "set 21°" (title, text-dim), the quiet line (body, text-mute:
//            "17° from 22:30" or the preset), - / + (Icon button square minus / plus), sp::s5 apart, centred in the ring
//   row      lay::button_h tall at the content's foot: the modes left (Button secondary, the current active),
//            Presets right (Button secondary, the preset's name when one is in use)
// Feed (docs ThermostatData): props target, current, action (heating / cooling / idle), mode (heat / auto / off ...),
// modes ("heat,auto,off"), preset, presets ("home,away,eco,boost"), min / max / step, next (quiet line text): an HA
// climate.* entity's attributes, or the sample. Taps: the ring (drag, release), - / + (a step each; 800 ms for more
// taps, then one change), a mode, a preset row: each emits "climate" (target 21.5 | mode heat | preset eco) for the
// host, plus "target" / "mode" / "preset" (detail = the value) for the spec's climate actions, and shows it at once
// (held 3 s against updates, the docs rule). While the ring is dragged: "set to" + the set point live (the dial's
// "input").
// Update path: the dial through its own update (the knob sweeps, quietly), texts in place; the modes / presets row
// rebuilt when the list of modes changes. Contrast: "thermostat" (+ thermostat_dial, button, icon_button, picker_row).
#include <ctime>
// audit deps: bottom_sheet, button, icon_button, thermostat_dial
#include "fw.h"

namespace {
const char *mode_text(const std::string &m) {
  static const char *const T[][2] = {{"heat", "Heat"}, {"cool", "Cool"}, {"heat_cool", "Heat/Cool"}, {"auto", "Auto"},
                                     {"dry", "Dry"}, {"fan_only", "Fan"}, {"off", "Off"}};
  for (auto &r : T) if (m == r[0]) return r[1];
  return m.c_str();
}
std::string preset_text(const std::string &p) {
  std::string s = p;
  if (!s.empty()) s[0] = (char) toupper((unsigned char) s[0]);
  for (char &c : s) if (c == '_') c = ' ';
  return s;
}
std::string temp_text(double v, double step = -1) {  // 19.25 -> "19.3°"; a set point drops a trailing .0: "17°", "17.5°"
  if (std::isnan(v)) return "--";
  char b[16];
  const double r = std::round(v * 10) / 10;
  if (step < 0) snprintf(b, sizeof b, "%.1f\xC2\xB0", r);
  else if (step >= 1 || std::fabs(r - std::round(r)) < 1e-9) snprintf(b, sizeof b, "%ld\xC2\xB0", std::lround(r));
  else snprintf(b, sizeof b, "%.1f\xC2\xB0", r);
  return b;
}
std::vector<std::string> split(const char *s) {
  std::vector<std::string> out;
  std::string cur;
  for (const char *c = s; ; c++) {
    if (!*c || *c == ',') { while (!cur.empty() && cur[0] == ' ') cur.erase(0, 1); if (!cur.empty()) out.push_back(cur); cur.clear(); if (!*c) break; }
    else cur += *c;
  }
  return out;
}
struct TV {
  lv_obj_t *root = nullptr, *dial = nullptr, *cap = nullptr, *val = nullptr, *set = nullptr, *quiet = nullptr, *minus = nullptr, *plus = nullptr;
  lv_obj_t *modes = nullptr, *preset_btn = nullptr, *sheet = nullptr;
  std::vector<lv_obj_t *> mode_btns;
  std::vector<std::string> mode_ids, presets;
  std::string mode, action, preset, next, modes_key;
  double target = 21, current = 19.4, min = 5, max = 30, step = 0.5;
  double held = NAN;
  uint32_t hold_until = 0;
  lv_timer_t *pending = nullptr;
};
TV *of(lv_obj_t *o) { return (TV *) lv_obj_get_user_data(o); }
lv_obj_t *box(lv_obj_t *parent) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  return o;
}
lv_obj_t *build(const char *id, lv_obj_t *parent, ArduinoJson::JsonDocument &d) {
  const uint32_t n = lv_obj_get_child_count(parent);
  if (const fw::Component *c = fw::find(id)) c->build(parent, d.as<fw::Props>());
  return lv_obj_get_child_count(parent) > n ? lv_obj_get_child(parent, -1) : nullptr;
}
bool held(const TV *v) { return !std::isnan(v->held) && (int32_t) (v->hold_until - lv_tick_get()) > 0; }
double target_of(const TV *v) { return held(v) ? v->held : v->target; }
void hold(TV *v, double t) { v->held = t; v->hold_until = lv_tick_get() + 3000; }
void dial_props(const TV *v, ArduinoJson::JsonDocument &d) {
  const bool off = v->mode == "off";
  d["target"] = target_of(v);
  d["current"] = v->current;
  d["action"] = off ? "off" : v->action == "heating" || v->action == "cooling" ? v->action : "idle";
  d["min"] = v->min, d["max"] = v->max;
  char s[8];
  snprintf(s, sizeof s, "%g", v->step);
  d["step"] = s;
  d["opening"] = false;
}
void render(lv_obj_t *root) {
  TV *v = of(root);
  const fw::Theme &t = *fw::T;
  const bool off = v->mode == "off";
  const char *doing = off ? "off" : v->action == "heating" ? "heating" : v->action == "cooling" ? "cooling" : "idle";
  fw::set_text(v->cap, fw::ty::label, doing);
  lv_obj_set_style_text_color(v->cap, !strcmp(doing, "heating") ? t.accent_text : !strcmp(doing, "cooling") ? t.blue : t.text_mute, 0);
  fw::set_text(v->val, fw::ty::display, temp_text(v->current).c_str());
  lv_obj_set_style_text_color(v->val, off ? t.text_dim : t.text, 0);
  fw::set_text(v->set, fw::ty::title, off ? "" : ("set " + temp_text(target_of(v), v->step)).c_str());
  fw::set_text(v->quiet, fw::ty::body, !off && !v->next.empty() ? v->next.c_str() : (!v->preset.empty() && v->preset != "none" ? preset_text(v->preset).c_str() : ""));
  for (lv_obj_t *b : {v->minus, v->plus}) lv_obj_set_style_opa(b, off ? LV_OPA_TRANSP : LV_OPA_COVER, 0);
  for (size_t i = 0; i < v->mode_btns.size(); i++) {
    ArduinoJson::JsonDocument bp;
    bp["kind"] = "secondary", bp["label"] = mode_text(v->mode_ids[i]), bp["width"] = fw::lay::thermo_mode_w;
    bp["state"] = v->mode_ids[i] == v->mode ? "active" : "default";
    fw::update_child(fw::find("button"), v->mode_btns[i], bp.as<fw::Props>());
  }
  if (v->preset_btn) {
    ArduinoJson::JsonDocument bp;
    bp["kind"] = "secondary", bp["label"] = !v->preset.empty() && v->preset != "none" ? preset_text(v->preset).c_str() : "Presets";
    fw::update_child(fw::find("button"), v->preset_btn, bp.as<fw::Props>());
  }
  ArduinoJson::JsonDocument dp;
  dial_props(v, dp);
  fw::update_child(fw::find("thermostat_dial"), v->dial, dp.as<fw::Props>());
}
// a change for the host: "climate" <what> <value> (the panel mirrors it), and the spec's own event per kind (target /
// mode / preset, detail = the value) for its climate.set_temperature / set_hvac_mode / set_preset_mode actions
void climate(lv_obj_t *root, const char *what, const std::string &value) {
  fw::emit(root, "climate", (std::string(what) + " " + value).c_str());
  fw::emit(root, what, value.c_str());
}
void nudge(lv_obj_t *root, int dir) {
  TV *v = of(root);
  if (v->mode == "off") return;
  const double t = std::min(v->max, std::max(v->min, std::round((target_of(v) + dir * v->step) * 100) / 100));
  hold(v, t);
  render(root);
  if (v->pending) lv_timer_delete(v->pending);
  v->pending = lv_timer_create([](lv_timer_t *tm) {  // 800 ms for more taps, then one change (docs)
    auto *root = (lv_obj_t *) lv_timer_get_user_data(tm);
    TV *v = of(root);
    v->pending = nullptr;
    lv_timer_delete(tm);
    char b[16];
    snprintf(b, sizeof b, "%g", target_of(v));
    climate(root, "target", b);
  }, fw::mo::ms::nudge_wait, root);
  lv_timer_set_repeat_count(v->pending, 1);
}
void sheet_props(const TV *v, bool open, ArduinoJson::JsonDocument &d) {
  static const char *const ICON[][2] = {{"home", "person"}, {"away", "back"}, {"eco", "bolt"}, {"boost", "plus"}, {"sleep", "timer"}, {"comfort", "person"}};
  d["overlay"] = true;
  d["title"] = "preset";
  d["open"] = open ? "open" : "closed";
  JsonArray items = d["items"].to<JsonArray>();
  int sel = -1;
  for (size_t i = 0; i < v->presets.size(); i++) {
    JsonObject it = items.add<JsonObject>();
    const char *ic = "auto";
    for (auto &r : ICON) if (v->presets[i] == r[0]) ic = r[1];
    it["icon"] = ic;
    it["name"] = preset_text(v->presets[i]);
    it["status"] = v->presets[i] == v->preset ? "in use" : "tap to use";
    if (v->presets[i] == v->preset) sel = (int) i;
  }
  d["selected"] = sel < 0 ? 0 : sel;
}
void build_row(lv_obj_t *root) {  // the modes + Presets (again when the list of modes changes)
  TV *v = of(root);
  lv_obj_clean(v->modes);
  v->mode_btns.clear();
  lv_obj_t *left = box(v->modes);
  lv_obj_set_size(left, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(left, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(left, fw::sp::s3, 0);
  for (size_t i = 0; i < v->mode_ids.size() && i < 3; i++) {
    ArduinoJson::JsonDocument bp;
    bp["kind"] = "secondary", bp["label"] = mode_text(v->mode_ids[i]), bp["width"] = fw::lay::thermo_mode_w;
    lv_obj_t *b = build("button", left, bp);
    if (!b) continue;
    lv_obj_set_user_data(b, (void *) (intptr_t) i);
    lv_obj_add_event_cb(b, [](lv_event_t *e) {
      auto *root = (lv_obj_t *) lv_event_get_user_data(e);
      TV *v = of(root);
      const size_t i = (size_t) (intptr_t) lv_obj_get_user_data((lv_obj_t *) lv_event_get_current_target(e));
      v->mode = v->mode_ids[i];
      climate(root, "mode", v->mode);
      render(root);
    }, LV_EVENT_CLICKED, root);
    v->mode_btns.push_back(b);
  }
  v->preset_btn = nullptr;
  if (!v->presets.empty()) {
    ArduinoJson::JsonDocument bp;
    bp["kind"] = "secondary", bp["label"] = "Presets";
    v->preset_btn = build("button", v->modes, bp);
    if (v->preset_btn)
      lv_obj_add_event_cb(v->preset_btn, [](lv_event_t *e) {
        auto *root = (lv_obj_t *) lv_event_get_user_data(e);
        TV *v = of(root);
        if (!v->sheet) return;
        ArduinoJson::JsonDocument sd;
        sheet_props(v, true, sd);
        fw::update_child(fw::find("bottom_sheet"), v->sheet, sd.as<fw::Props>());
      }, LV_EVENT_CLICKED, root);
  }
}
void take(lv_obj_t *root, fw::Props p) {
  TV *v = of(root);
  v->target = p["target"] | 21.0;
  v->current = p["current"] | 19.4;
  v->action = p["action"] | "heating";
  v->mode = p["mode"] | "heat";
  v->preset = p["preset"] | "none";
  v->next = p["next"] | "";
  v->min = p["min"] | 5.0, v->max = p["max"] | 30.0;
  v->step = atof(p["step"] | "0.5");
  if (v->step <= 0) v->step = 0.5;
  const char *ms = p["modes"] | "heat,auto,off";
  v->mode_ids = split(ms);
  v->presets = split(p["presets"] | "home,away,eco,boost");
  const std::string key = std::string(ms) + "|" + (p["presets"] | "home,away,eco,boost");
  if (key != v->modes_key) { v->modes_key = key; build_row(root); }
  if (held(v) && std::fabs(v->held - v->target) < 1e-9) v->held = NAN;  // HA caught up
  render(root);
}
}  // namespace

FW_COMPONENT(thermostat_view, "Thermostat view", "components", R"({
  "about": "The Thermostat screen's body: the ring with the measured temperature in its middle, the set point, - and +, the modes and a Presets sheet.",
  "frame": [480, 400],
  "props": [
    {"name": "target", "type": "range", "min": 5, "max": 30, "step": 0.5, "default": 21},
    {"name": "current", "type": "range", "min": 5, "max": 30, "step": 0.1, "default": 19.4},
    {"name": "action", "type": "select", "options": ["heating", "cooling", "idle"], "default": "heating"},
    {"name": "mode", "type": "select", "options": ["heat", "auto", "off"], "default": "heat"},
    {"name": "preset", "type": "select", "options": ["none", "home", "away", "eco", "boost"], "default": "none"},
    {"name": "next", "type": "text", "default": "17° from 22:30"}
  ],
  "variants": [
    {"label": "heating to 21", "props": {}},
    {"label": "idle at 21", "props": {"current": 21, "action": "idle"}},
    {"label": "eco preset", "props": {"preset": "eco", "next": ""}},
    {"label": "off", "props": {"mode": "off", "action": "idle"}}
  ]
})") {
  const fw::Theme &t = *fw::T;
  auto *v = new TV;
  lv_obj_t *root = v->root = lv_obj_create(parent);
  lv_obj_remove_style_all(root);
  lv_obj_remove_flag(root, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_user_data(root, v);
  lv_obj_add_event_cb(root, [](lv_event_t *e) {
    auto *v = (TV *) lv_event_get_user_data(e);
    if (v->pending) lv_timer_delete(v->pending);
    delete v;
  }, LV_EVENT_DELETE, v);
  lv_obj_set_size(root, fw::lay::content_w, fw::lay::content_h);
  lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(root, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  const int S = fw::lay::thermo_scale;
  lv_obj_t *ring = box(root);
  lv_obj_set_size(ring, S, S);
  ArduinoJson::JsonDocument dp;
  dp["target"] = p["target"] | 21.0, dp["current"] = p["current"] | 19.4, dp["action"] = p["action"] | "heating";
  v->dial = build("thermostat_dial", ring, dp);
  fw::listen(ring, [](lv_obj_t *, const char *name, const char *detail, void *user) {
    auto *root = (lv_obj_t *) user;
    TV *v = of(root);
    if (!strcmp(name, "input")) {  // live while dragging: "set to" + the set point big, "now 19.4°" under it (docs)
      hold(v, atof(detail));
      fw::set_text(v->cap, fw::ty::label, "set to");
      fw::set_text(v->val, fw::ty::display, temp_text(v->held, v->step).c_str());
      fw::set_text(v->set, fw::ty::title, ("now " + temp_text(v->current)).c_str());
      return;
    }
    if (strcmp(name, "change")) return;  // a drag released on a new set point
    hold(v, atof(detail));
    climate(root, "target", detail);
    render(root);
  }, root);
  lv_obj_t *mid = box(ring);  // the centre: texts + - / +, the dial's free middle
  lv_obj_set_size(mid, S, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(mid, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(mid, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(mid, fw::sp::s2, 0);
  v->cap = fw::text(mid, fw::ty::label, "", t.text_mute);
  v->val = fw::text(mid, fw::ty::display, "--", t.text);
  v->set = fw::text(mid, fw::ty::title, "", t.text_dim);
  v->quiet = fw::text(mid, fw::ty::body, "", t.text_mute);
  lv_obj_t *pm = box(mid);
  lv_obj_set_size(pm, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(pm, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(pm, fw::sp::s9, 0);
  lv_obj_set_style_margin_top(pm, fw::sp::s3, 0);
  for (int k = 0; k < 2; k++) {
    ArduinoJson::JsonDocument bp;
    bp["style"] = "square", bp["icon"] = k ? "plus" : "minus";
    lv_obj_t *b = build("icon_button", pm, bp);
    (k ? v->plus : v->minus) = b;
    if (b) lv_obj_add_event_cb(b, k ? [](lv_event_t *e) { nudge((lv_obj_t *) lv_event_get_user_data(e), 1); }
                                     : [](lv_event_t *e) { nudge((lv_obj_t *) lv_event_get_user_data(e), -1); }, LV_EVENT_CLICKED, root);
  }
  lv_obj_center(mid);
  v->modes = box(root);
  lv_obj_set_size(v->modes, fw::lay::content_w, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(v->modes, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(v->modes, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  take(root, p);
  // the presets sheet: the view's own child (it goes with the view), stretched over the whole screen from the content
  // box's corner, so its scrim covers the header too (docs: the page's sheet)
  if (!v->presets.empty()) {
    ArduinoJson::JsonDocument sd;
    sheet_props(v, false, sd);
    v->sheet = build("bottom_sheet", root, sd);
    if (v->sheet) {
      lv_obj_set_pos(v->sheet, -fw::lay::inset, -fw::lay::top);
      lv_obj_set_size(v->sheet, fw::lay::screen_w, fw::lay::screen_h);
      lv_obj_add_flag(root, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    }
    if (v->sheet)
      lv_obj_add_event_cb(v->sheet, [](lv_event_t *e) {  // a preset picked
        auto *root = (lv_obj_t *) lv_event_get_user_data(e);
        TV *v = of(root);
        const int i = (int) (intptr_t) lv_event_get_param(e);
        if (i < 0 || i >= (int) v->presets.size()) return;
        v->preset = v->presets[(size_t) i];
        climate(root, "preset", v->preset);
        render(root);
      }, LV_EVENT_VALUE_CHANGED, root);
  }
}

FW_UPDATE(thermostat_view) {
  if (!of(root)) return false;
  take(root, p);
  return true;
}
