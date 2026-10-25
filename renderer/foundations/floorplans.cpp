// Foundations > Floor plans: tokens/floorplan.json (via floorplan_gen.h fw::fp::) = how a floor plan is drawn: the
// rules in plain words with a sample plan, then the same sample at each state, every plan drawn by the real Iso floor
// plan component (the sample house is made up). Density: the device setting (Waveshare 720 = 1.5x, Guition 480 = 1x)
// redraws every plan at that density. For people (no token names on the page).
// Generated: the floor plan generator (core/floorplan_auto.cpp) run live on a public test map (Valetudo's Dreame D9,
// tests/floorplan/README.md, embedded by build.sh as fp_fixture_gen.h): the map's rooms as traced, then the cleaned plan
// with the robot, its path and the dock through the generator's transform, and each cleanup step in words.
// Status = this file + tokens/floorplan.json (WIP until approved).
#include <string>
#include "fw.h"
#include "floorplan_gen.h"
#include "floorplan_auto.h"
#include "fp_fixture_gen.h"

namespace {
lv_obj_t *column(lv_obj_t *parent, int gap) {
  lv_obj_t *c = lv_obj_create(parent);
  lv_obj_remove_style_all(c);
  lv_obj_remove_flag(c, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_size(c, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(c, gap, 0);
  return c;
}
// the sample house through the real component (plan = its made-up default house)
void plan(lv_obj_t *parent, const char *state, const char *robot, const char *trail, const char *picked = "", const char *here = "") {
  const fw::Component *c = fw::find("iso_floorplan");
  if (!c) return;
  ArduinoJson::JsonDocument d;
  d["dock"] = "0,0.5,0.14,0.8,0.36", d["state"] = state, d["robot"] = robot, d["trail"] = trail;
  d["picked"] = picked, d["here"] = here, d["transform"] = "";
  c->build(parent, d.as<fw::Props>());
}
constexpr const char *LAST_RUN = "0.6,0.7 3.1,0.7 3.1,1.3 0.6,1.3 0.6,1.9 3.1,1.9 3.1,2.5 0.6,2.5";
constexpr const char *LIVE_RUN = "0.5,3.6 3.4,3.6 3.4,4.2 0.5,4.2 0.5,4.8 1.4,4.8";
void caption(lv_obj_t *parent, const char *title, const char *about) {
  const fw::Theme &t = *fw::T;
  lv_obj_t *c = column(parent, fw::sp::s1);
  fw::text(c, fw::ty::c_label, title, t.text);
  fw::text(c, fw::ty::c_body, about, t.text_dim, fw::lay::content_w);
}
void rules(lv_obj_t *page) {
  plan(page, "docked", "1.4,4.8", LAST_RUN, "Dining:2");
  for (const fw::fp::Rule &r : fw::fp::RULES) caption(page, r.title, r.about);
}
void states(lv_obj_t *page) {
  struct S { const char *state, *robot, *trail, *picked, *here, *title, *about; };
  static const S ALL[] = {
    {"docked", "1.4,4.8", LAST_RUN, "", "", "Docked", "On the dock, the last run's path left faintly behind it. Only rooms it can clean carry a name."},
    {"docked", "1.4,4.8", LAST_RUN, "Dining:2,Hall:3", "", "Rooms picked", "Tap a room to pick it for the next run: it fills with the accent's soft tint and gets an accent edge."},
    {"cleaning", "1.4,4.8", LIVE_RUN, "", "Lounge", "Cleaning", "A run is on: the robot glides across the plan with its turret in the accent, the room it's in is edged, the live path in blue. Rooms can't be picked until it's done."},
    {"paused", "1.4,4.8", LIVE_RUN, "", "Lounge", "Paused", "Stopped mid-run: the robot stays where it is and the path keeps its last point; the turret goes back to steel."},
  };
  for (const S &s : ALL) {
    lv_obj_t *c = column(page, fw::sp::s3);
    plan(c, s.state, s.robot, s.trail, s.picked, s.here);
    caption(c, s.title, s.about);
  }
}
// the generator on the embedded test map, once (the report is plain data: plan strings, steps, cuts)
const ArduinoJson::JsonDocument &report() {
  static ArduinoJson::JsonDocument rep;
  static bool done = false;
  if (!done) deserializeJson(rep, fw::fpa::run(R"({"adapter": "valetudo_json"})", FP_FIXTURE, nullptr, 0, 0)), done = true;
  return rep;
}
void generated(lv_obj_t *page) {
  const ArduinoJson::JsonDocument &r = report();
  char b[400];
  snprintf(b, sizeof b, "A plan can come straight from the robot's own map: every room it knows, traced, straightened into walls on a %.0f cm grid with a gap between rooms, and anything odd cut and reported. This is a public test map from a Dreame D9.", fw::fp::gen::GRID * 100);
  caption(page, "From the vacuum's map", b);
  if (!r["error"].isNull()) { caption(page, "Couldn't read the map", r["error"] | ""); return; }
  const fw::Component *c = fw::find("iso_floorplan");
  if (!c) return;
  lv_obj_t *col = column(page, fw::sp::s3);
  ArduinoJson::JsonDocument d;
  const float h = fw::fp::gen::DOCK / 2;
  auto dock = [&](const char *key) {
    if (!r[key].is<ArduinoJson::JsonArrayConst>()) return std::string("0,0,0,0,0");
    const float x = r[key][0], z = r[key][1];
    snprintf(b, sizeof b, "%.2f,%.2f,%.2f,%.2f,%.2f", x - h, z - h, x + h, z + h, fw::fp::gen::DOCK);
    return std::string(b);
  };
  d["plan"] = r["plan_raw"], d["state"] = "paused", d["robot"] = r["robot_src"], d["trail"] = "", d["picked"] = "", d["here"] = "";
  d["transform"] = r["transform_raw"], d["dock"] = dock("dock_raw");
  c->build(col, d.as<fw::Props>());
  caption(col, "As the map draws it", r["steps"][0]["detail"] | "");
  col = column(page, fw::sp::s3);
  d.clear();
  d["plan"] = r["plan"], d["state"] = "paused", d["robot"] = r["robot_src"], d["trail"] = r["trail"], d["picked"] = "", d["here"] = "";
  d["transform"] = r["transform"], d["dock"] = dock("dock");
  c->build(col, d.as<fw::Props>());
  caption(col, "Cleaned up", "The same rooms after the steps below. The robot, its path and the dock land in place through the plan's own transform from the vacuum's coordinates.");
  for (ArduinoJson::JsonVariantConst s : r["steps"].as<ArduinoJson::JsonArrayConst>()) caption(page, s["name"] | "", s["detail"] | "");
}
}  // namespace

FW_COMPONENT(floorplans, "Floor plans", "foundations", R"({
  "about": "How a floor plan is drawn: your rooms in isometric, as raised tiles with a low back wall, and the robot moving on it. The robot vacuum's map is the first user.",
  "layout": "doc",
  "frame": [480, 0],
  "props": [
    {"name": "section", "about": "Which part of the page to show: rules for how plans are drawn, states for the robot's states, generated for a plan made from room data.", "type": "select", "options": ["rules", "states", "generated"], "default": "rules"}
  ],
  "variants": [
    {"label": "Rules", "about": "What every floor plan follows, with the sample house (made up) drawn by the real component.", "props": {"section": "rules"}},
    {"label": "States", "about": "The same house at each state: docked, rooms picked, cleaning, paused. Switch the device to see it at 1.5x (720) and 1x (480).", "props": {"section": "states"}},
    {"label": "Generated", "about": "A plan made by the floor plan generator from a public vacuum map (a Dreame D9 test map), before and after the cleanup, with each step.", "props": {"section": "generated"}}
  ]
})") {
  const char *s = p["section"] | "rules";
  lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_t *page = column(parent, fw::sp::s5);
  lv_obj_set_style_pad_hor(page, fw::lay::inset, 0);
  if (!strcmp(s, "states")) states(page);
  else if (!strcmp(s, "generated")) generated(page);
  else rules(page);
}
