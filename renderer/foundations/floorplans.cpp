// Foundations > Floor plans: tokens/floorplan.json (via floorplan_gen.h fw::fp::) = how a floor plan is drawn: the
// rules in plain words with a sample plan, then the same sample at each state, every plan drawn by the real Iso floor
// plan component (the sample house is made up). Density: the device setting (Waveshare 720 = 1.5x, Guition 480 = 1x)
// redraws every plan at that density. For people (no token names on the page).
// Status = this file + tokens/floorplan.json (WIP until approved).
#include "fw.h"
#include "floorplan_gen.h"

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
}  // namespace

FW_COMPONENT(floorplans, "Floor plans", "foundations", R"({
  "about": "How a floor plan is drawn: your rooms in isometric, as raised tiles with a low back wall, and the robot moving on it. The robot vacuum's map is the first user.",
  "layout": "doc",
  "frame": [480, 0],
  "props": [
    {"name": "section", "type": "select", "options": ["rules", "states"], "default": "rules"}
  ],
  "variants": [
    {"label": "Rules", "about": "What every floor plan follows, with the sample house (made up) drawn by the real component.", "props": {"section": "rules"}},
    {"label": "States", "about": "The same house at each state: docked, rooms picked, cleaning, paused. Switch the device to see it at 1.5x (720) and 1x (480).", "props": {"section": "states"}}
  ]
})") {
  const char *s = p["section"] | "rules";
  lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_t *page = column(parent, fw::sp::s5);
  lv_obj_set_style_pad_hor(page, fw::lay::inset, 0);
  if (!strcmp(s, "states")) states(page);
  else rules(page);
}
