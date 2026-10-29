// Lightning view: the Lightning screen's body (docs screens/lightning.ts lightningParts; the factory spec
// renderer/specs/lightning.json adds the header): the nearest strike
// of the last 15 minutes as the hero ("Quiet" when there is none), three Stats (strikes in 15 minutes, the trend, the
// last strike) and the Lightning radar beside them.
//   box      the content box; a row: the left column (lay::lx_col wide, the docs 240) and the radar (a real Lightning
//            radar, size 256 dp = lay::radar), both centred on the content height
//   column   "nearest" (type label, text-dim), the number (hero, text; accent-text while a strike is within 25 km; the
//            word "Quiet" a size down, value), "km NW" (body, text-dim, the distance in the Units setting), then sp::s9
//            and three real Stats: last 15 min ("12 strikes", text; none = dim), trend ("48 km/h" approaching = warn,
//            moving away = ok, none = "--" dim), last strike ("2 min ago", dim)
// Data: core/lightning.h (fw::lx). The demo (prop demo, default on: the workbench page) plays the docs storm at 20x
// like the radar's own demo, and moves the radar through its update path, so new strikes ripple there; a pinned time
// (demo off, now) shows one moment (variants, tests). A panel feeds real strikes into the same fw::lx::Data.
// Update path: the texts and the Stats in place, the radar through its own. Static to touch. Contrast: "radar",
// "stat", "text".
#include <ctime>
// audit deps: lightning_radar, stat
#include "fw.h"
#include "lightning.h"

namespace {
constexpr double DEMO_SPEED = 20;  // the radar's: one clock tick = 20 s of storm
struct LV {
  lv_obj_t *num = nullptr, *unit = nullptr, *stats[3] = {}, *radar = nullptr;
  std::string stat_key[3], num_s, unit_s;
  lv_timer_t *demo = nullptr;
  double now = 0;
  bool quiet = false, word = false;
  fw::units::Distance du = fw::units::Distance::km;
};
LV *of(lv_obj_t *o) { return (LV *) lv_obj_get_user_data(o); }
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
std::string ago(double t, double now) {
  const long m = std::max(0L, std::lround((now - t) / 60000));
  char b[24];
  if (m < 1) return "just now";
  if (m < 60) snprintf(b, sizeof b, "%ld min ago", m);
  else snprintf(b, sizeof b, "%ld h ago", std::lround(m / 60.0));
  return b;
}
void stat_props(const fw::lx::Data &d, double now, int i, ArduinoJson::JsonDocument &p) {
  char b[32];
  if (i == 0) {
    p["label"] = "last 15 min";
    snprintf(b, sizeof b, "%d strike%s", d.m15, d.m15 == 1 ? "" : "s");
    p["value"] = b;
    p["tone"] = d.m15 ? "text" : "dim";
  } else if (i == 1) {
    if (!d.trend) { p["label"] = "trend"; p["value"] = "--"; p["tone"] = "dim"; }
    else {
      p["label"] = d.trend > 0 ? "approaching" : "moving away";
      p["value"] = fw::units::speed_kmh(std::abs(d.kmh), fw::units::Speed::kmh);
      p["tone"] = d.trend > 0 ? "warn" : "ok";
    }
  } else {
    p["label"] = "last strike";
    p["value"] = d.strikes.empty() ? "none" : ago(d.strikes.front().t, now).c_str();
    p["tone"] = "dim";
  }
}
void show(lv_obj_t *root, bool radar_too) {
  LV *v = of(root);
  const fw::Theme &t = *fw::T;
  const fw::lx::Data d = fw::lx::sample(v->now, v->quiet);
  const bool word = !d.has_nearest;
  if (word != v->word) { fw::text_style(v->num, word ? fw::ty::value : fw::ty::hero); v->word = word; }
  char b[48];
  if (d.has_nearest) snprintf(b, sizeof b, "%ld", std::lround(fw::units::from_km(d.nearest.d, v->du)));
  const std::string num_s = word ? "Quiet" : b;
  if (num_s != v->num_s) { v->num_s = num_s; fw::set_text(v->num, word ? fw::ty::value : fw::ty::hero, num_s.c_str()); }
  const lv_color_t nc = d.has_nearest && d.nearest.d <= 25 ? t.accent_text : t.text;
  if (!lv_color_eq(lv_obj_get_style_text_color(v->num, LV_PART_MAIN), nc)) lv_obj_set_style_text_color(v->num, nc, 0);
  if (d.has_nearest) snprintf(b, sizeof b, "%s %s", v->du == fw::units::Distance::mi ? "mi" : "km", fw::lx::compass(d.nearest.az));
  else snprintf(b, sizeof b, "nothing within %s", fw::units::distance(d.radius, v->du).c_str());
  if (v->unit_s != b) { v->unit_s = b; fw::set_text(v->unit, fw::ty::body, b); }
  for (int i = 0; i < 3; i++) {  // only a Stat whose words changed (a demo tick must stay under LVGL's 32 dirty areas)
    ArduinoJson::JsonDocument p;
    stat_props(d, v->now, i, p);
    std::string key;
    serializeJson(p, key);
    if (key == v->stat_key[i]) continue;
    v->stat_key[i] = key;
    fw::update_child(fw::find("stat"), v->stats[i], p.as<fw::Props>());
  }
  if (radar_too && v->radar) {
    ArduinoJson::JsonDocument p;
    p["demo"] = false;
    p["now"] = v->now;
    p["quiet"] = v->quiet;
    p["unit"] = v->du == fw::units::Distance::mi ? "mi" : "km";
    fw::update_child(fw::find("lightning_radar"), v->radar, p.as<fw::Props>());
  }
}
}  // namespace

FW_COMPONENT(lightning_view, "Lightning view", "components", R"({
  "about": "The Lightning screen's body: the nearest strike of the last 15 minutes as the hero, three stats and the radar.",
  "frame": [480, 400],
  "motion": "demo: strikes arrive and ripple on the radar; the nearest, the counts and the last strike follow",
  "motion_off": "demo",
  "props": [
    {"name": "demo", "about": "For previews only: plays a made-up storm so strikes appear.", "type": "bool", "default": true},
    {"name": "quiet", "about": "For previews only: shows a quiet sky with no recent strikes.", "type": "bool", "default": false},
    {"name": "unit", "about": "Whether distances are shown in kilometres or miles.", "type": "select", "options": ["km", "mi"], "default": "km"},
    {"name": "size", "about": "screen fills the Lightning screen; weather fits it inside the Weather screen's lightning view.", "type": "select", "options": ["screen", "weather"], "default": "screen"}
  ],
  "variants": [
    {"label": "a storm, 9 km away", "props": {"demo": false, "now": 1792944000000}},
    {"label": "it has passed", "props": {"demo": false, "now": 1792946520000}},
    {"label": "quiet", "props": {"demo": false, "now": 1792944000000, "quiet": true}},
    {"label": "miles", "props": {"demo": false, "now": 1792944000000, "unit": "mi"}}
  ]
})") {
  const fw::Theme &t = *fw::T;
  auto *v = new LV;
  lv_obj_t *root = lv_obj_create(parent);
  lv_obj_remove_style_all(root);
  lv_obj_remove_flag(root, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_user_data(root, v);
  lv_obj_add_event_cb(root, [](lv_event_t *e) {
    auto *v = (LV *) lv_event_get_user_data(e);
    if (v->demo) lv_timer_delete(v->demo);
    delete v;
  }, LV_EVENT_DELETE, v);
  // size "weather" = the Weather screen's Lightning tab: its own height under the tabs
  lv_obj_set_size(root, fw::lay::content_w, !strcmp(p["size"] | "screen", "weather") ? LV_SIZE_CONTENT : fw::lay::content_h);
  lv_obj_set_flex_flow(root, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(root, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  const bool demo = p["demo"] | true;
  const double fixed = p["now"] | 0.0;
  v->now = fixed > 0 ? fixed : (double) time(nullptr) * 1000;
  if (demo) v->now = std::floor(v->now / 3600000.0) * 3600000.0 + 0.3 * 3600000.0;  // the storm over the area
  v->quiet = p["quiet"] | false;
  v->du = fw::units::distance_of(p["unit"] | "km");
  lv_obj_t *col = box(root);
  lv_obj_set_size(col, fw::lay::lx_col, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
  lv_obj_t *k = fw::text(col, fw::ty::label, "nearest", t.text_dim);
  lv_obj_set_style_margin_bottom(k, fw::sp::s2, 0);
  v->num = fw::text(col, fw::ty::hero, "--", t.text);
  v->unit = fw::text(col, fw::ty::body, "", t.text_dim, fw::lay::lx_col);
  lv_obj_set_style_margin_bottom(v->unit, fw::sp::s9, 0);
  lv_obj_t *stats = box(col);
  lv_obj_set_size(stats, fw::lay::lx_col, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(stats, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(stats, fw::sp::s5, 0);
  const fw::lx::Data d = fw::lx::sample(v->now, v->quiet);
  for (int i = 0; i < 3; i++) {
    ArduinoJson::JsonDocument sp;
    stat_props(d, v->now, i, sp);
    v->stats[i] = build("stat", stats, sp);
    // a value's first glyph may overhang left of its box (the j of "just now" at the content edge): let it show
    if (v->stats[i])
      for (uint32_t c = 0; c < lv_obj_get_child_count(v->stats[i]); c++) fw::bleed(lv_obj_get_child(v->stats[i], c), fw::sp::s1);
  }
  ArduinoJson::JsonDocument rp;
  rp["demo"] = false;
  rp["now"] = v->now;
  rp["quiet"] = v->quiet;
  rp["unit"] = p["unit"] | "km";
  rp["size"] = 256;
  v->radar = build("lightning_radar", root, rp);
  show(root, false);
  if (demo)
    v->demo = lv_timer_create([](lv_timer_t *tm) {
      auto *root = (lv_obj_t *) lv_timer_get_user_data(tm);
      of(root)->now += fw::mo::ms::clock_tick * DEMO_SPEED;
      show(root, true);
    }, fw::mo::ms::clock_tick, root);
}

FW_UPDATE(lightning_view) {
  LV *v = of(root);
  if (!v || (bool) (p["demo"] | true) != (v->demo != nullptr)) return false;
  v->quiet = p["quiet"] | false;
  v->du = fw::units::distance_of(p["unit"] | "km");
  if (!v->demo) { const double f = p["now"] | 0.0; v->now = f > 0 ? f : (double) time(nullptr) * 1000; }
  show(root, true);
  return true;
}
