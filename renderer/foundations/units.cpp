// Foundations > Units: how Facet writes distances and speeds (core/units.h; docs @facet/ui units.ts). One setting each
// (Settings > Units: Distance km | miles, Speed mph | km/h | knots, tokens/settings.json) changes every number a screen
// shows. The page is for people (the foundation page rule): the two real Segmented controls drive real Stats in
// place, the way the Settings screen will. Status (WIP / Done) = this file + core/units.h in git.
#include <cstdio>
#include "fw.h"
#include "units.h"

namespace {
using namespace fw;
using Doc = ArduinoJson::JsonDocument;
constexpr float W = 480;

lv_obj_t *box(lv_obj_t *parent) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, (lv_obj_flag_t) (LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE));
  return o;
}
lv_obj_t *column(lv_obj_t *parent, int gap, int w = LV_PCT(100)) {
  lv_obj_t *c = box(parent);
  lv_obj_set_size(c, w, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(c, gap, 0);
  return c;
}
lv_obj_t *entry(lv_obj_t *list, int gap = sp::s3) {
  lv_obj_t *e = column(list, gap);
  lv_obj_set_style_pad_ver(e, sp::s5, 0);
  lv_obj_set_style_border_side(e, LV_BORDER_SIDE_BOTTOM, 0);
  lv_obj_set_style_border_width(e, stroke::hair, 0);
  lv_obj_set_style_border_color(e, T->line, 0);
  return e;
}
int inner_w() { return px(W) - 2 * sp::s7; }
void spec(lv_obj_t *e, const char *title, const char *about) {
  text(e, ty::c_caps_sm, title, T->text);
  text(e, ty::c_meta, about, T->text_dim, inner_w());
}
lv_obj_t *comp(lv_obj_t *parent, const char *id, Doc &d) {
  const Component *c = find(id);
  if (!c) return nullptr;
  const uint32_t n = lv_obj_get_child_count(parent);
  c->build(parent, d.as<Props>());
  return lv_obj_get_child_count(parent) > n ? lv_obj_get_child(parent, -1) : nullptr;
}

// the samples: what a screen would show (Overhead, Lightning, Weather), in the chosen units
struct Sample { const char *label; double v; bool speed_kt, speed_kmh; };
constexpr Sample SAMPLES[] = {{"nearest strike", 12.4, false, false}, {"aircraft overhead", 4.3, false, false},
                              {"aircraft speed", 452, true, false},  {"wind", 28, false, true}};
struct Page { lv_obj_t *stats[4] = {}; units::Distance d = units::Distance::km; units::Speed s = units::Speed::mph; };
std::string value_of(const Page *pg, const Sample &x) {
  return x.speed_kt ? units::speed_kt(x.v, pg->s) : x.speed_kmh ? units::speed_kmh(x.v, pg->s) : units::distance(x.v, pg->d);
}
void refresh(Page *pg) {
  for (int i = 0; i < 4; i++) {
    if (!pg->stats[i]) continue;
    Doc d;
    d["label"] = SAMPLES[i].label, d["value"] = value_of(pg, SAMPLES[i]), d["tone"] = "text";
    update_child(find("stat"), pg->stats[i], d.as<Props>());
  }
}
void on_choice(lv_obj_t *, const char *name, const char *detail, void *user) {
  if (strcmp(name, "change")) return;
  auto *pg = (Page *) user;
  // the Segmented's ids are its words: km / miles, mph / km/h / knots
  if (!strcmp(detail, "km")) pg->d = units::Distance::km;
  else if (!strncmp(detail, "mi", 2)) pg->d = units::Distance::mi;
  else pg->s = !strncmp(detail, "kn", 2) ? units::Speed::kn : !strncmp(detail, "km", 2) ? units::Speed::kmh : units::Speed::mph;
  refresh(pg);
  demo_away(pg->stats[0], false);  // a viewing choice, not a demo to reset
}

void settings(lv_obj_t *l) {
  auto *pg = new Page;
  lv_obj_t *e = entry(l, sp::s5);
  lv_obj_add_event_cb(e, [](lv_event_t *ev) { delete (Page *) lv_event_get_user_data(ev); }, LV_EVENT_DELETE, pg);
  spec(e, "One setting each", "Distance in kilometres or miles, speed in miles per hour, kilometres per hour or knots. Change one here: every distance and speed on every screen follows it.");
  {
    Doc d;
    d["options"] = "km, Miles";
    d["selected"] = "km";
    comp(e, "segmented", d);
    Doc s;
    s["options"] = "mph, km/h, Knots";
    s["selected"] = "mph";
    comp(e, "segmented", s);
    listen(e, on_choice, pg);
  }
  lv_obj_t *grid = box(e);
  lv_obj_set_size(grid, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_style_pad_row(grid, sp::s5, 0);
  lv_obj_set_style_pad_column(grid, lay::gutter, 0);
  for (int i = 0; i < 4; i++) {
    Doc d;
    d["label"] = SAMPLES[i].label, d["value"] = value_of(pg, SAMPLES[i]), d["tone"] = "text";
    pg->stats[i] = comp(grid, "stat", d);
  }
}
void rules(lv_obj_t *l) {
  lv_obj_t *e = entry(l);
  spec(e, "How numbers are written", "Distances keep one decimal under 10 (4.3 km) and none above (12 km). Speeds are whole numbers with their unit after a space (452 kn, 28 mph). A reading that is not there shows two dashes.");
  lv_obj_t *e2 = entry(l);
  spec(e2, "Where they show", "Aircraft overhead and their speed, the nearest lightning strike and the storm's approach, wind on the Weather screen, and the rings on the Overhead and Lightning radars.");
}
}  // namespace

FW_COMPONENT(units, "Units", "foundations", R"({
  "about": "Distances and speeds follow one setting each: kilometres or miles, mph, km/h or knots.",
  "layout": "doc",
  "frame": [480, 0],
  "props": [{"name": "section", "type": "select", "options": ["settings", "rules"], "default": "settings"}],
  "variants": [
    {"label": "Distance and speed", "about": "Pick a unit: the numbers below change in place.", "props": {"section": "settings"}},
    {"label": "How they are written", "props": {"section": "rules"}}
  ]
})") {
  lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_t *l = column(parent, 0, px(W));
  lv_obj_set_style_pad_hor(l, sp::s7, 0);
  if (!strcmp(p["section"] | "settings", "rules")) rules(l);
  else settings(l);
}
