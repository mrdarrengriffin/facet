// Plant gauge: one plant's soil moisture as the screen's hero (Plant monitor). As wide as its parent:
//   caption  type label (caps): "needs water" accent-text (under dry); "moist", "too wet" (over wet), "offline" in
//            text-dim (the bar carries the tone)
//   value    type hero (96 / 64, the Numbers rule for percent: % in the same style, no space): "18%", an em dash
//            when unknown
//   line     type body, text-dim: "Fern · water below 30%"
//   bar      sp::s5 under the line, the Level bar component (fw::find) the full width: the moisture, the dry
//            threshold as its mark; accent under dry, steel between, blue over wet, off unknown
// Feed: name, moisture (0-100; a soil moisture sensor's state, or the HA plant entity's moisture), dry / wet (the
// plant's limits, default 30 / 80). Static, not a control. Update path: texts in place, the bar glides (its own
// update), tone fades. Contrast: section "plant_gauge" (+ level_bar).
// audit deps: level_bar
#include "fw.h"

namespace {
// a text only when it changes (an update with the same props redraws nothing)
void put(lv_obj_t *l, fw::ty::Id ty, const char *t) {
  if (fw::cased(ty, t) != lv_label_get_text(l)) fw::set_text(l, ty, t);
}
struct PG { lv_obj_t *cap, *val, *line, *bar; std::string tone; };
PG *of(lv_obj_t *o) { return (PG *) lv_obj_get_user_data(o); }
struct Read { int m; const char *cap, *tone; bool dry; };
Read read(fw::Props p) {
  const double v = p["moisture"].is<double>() ? p["moisture"].as<double>() : p["moisture"].is<const char *>() ? strtod(p["moisture"] | "", nullptr) : -1;
  const bool known = p["moisture"].is<double>() || (p["moisture"].is<const char *>() && *(p["moisture"] | "") && isdigit((unsigned char) *(p["moisture"] | "0")));
  const int m = known ? (int) std::lround(LV_CLAMP(0.0, v, 100.0)) : -1, dry = p["dry"] | 30, wet = p["wet"] | 80;
  if (m < 0) return {m, "offline", "off", false};
  if (m < dry) return {m, "needs water", "accent", true};
  if (m > wet) return {m, "too wet", "blue", false};
  return {m, "moist", "steel", false};
}
void bar_props(fw::Props p, const Read &r, ArduinoJson::JsonDocument &d) {
  d["value"] = r.m, d["mark"] = p["dry"] | 30, d["tone"] = r.tone;
}
void texts(PG *g, fw::Props p, const Read &r) {
  char b[16];
  if (r.m < 0) snprintf(b, sizeof b, "\xE2\x80\x94");
  else snprintf(b, sizeof b, "%d%%", r.m);
  put(g->cap, fw::ty::label, r.cap);
  put(g->val, fw::ty::hero, b);
  std::string line = p["name"] | "Fern";
  line += " \xC2\xB7 water below " + std::to_string((int) (p["dry"] | 30)) + "%";
  put(g->line, fw::ty::body, line.c_str());
}
void colours(PG *g, const Read &r) {
  const fw::Theme &t = *fw::T;
  lv_obj_set_style_text_color(g->cap, r.dry ? t.accent_text : t.text_dim, 0);
  lv_obj_set_style_text_color(g->val, r.m < 0 ? t.text_dim : t.text, 0);
}
}  // namespace

FW_COMPONENT(plant_gauge, "Plant gauge", "components", R"({
  "about": "One plant's soil moisture as a screen's hero: whether it needs water, the percentage and a bar with its dry line.",
  "frame": [480, 0],
  "props": [
    {"name": "name", "about": "The plant's name.", "type": "text", "default": "Fern"},
    {"name": "moisture", "about": "The soil moisture now, from 0 to 100 per cent. -1 means it cannot be read.", "type": "range", "min": -1, "max": 100, "step": 1, "default": 18},
    {"name": "dry", "about": "The moisture below which the plant needs water. The bar shows it as a line.", "type": "range", "min": 0, "max": 100, "step": 1, "default": 30},
    {"name": "wet", "about": "The moisture above which the soil is too wet.", "type": "range", "min": 0, "max": 100, "step": 1, "default": 80}
  ],
  "variants": [
    {"label": "needs water", "props": {}},
    {"label": "moist", "props": {"name": "Monstera", "moisture": 46}},
    {"label": "too wet", "props": {"name": "Basil", "moisture": 91}},
    {"label": "offline", "props": {"moisture": -1}}
  ]
})") {
  const fw::Theme &t = *fw::T;
  auto *g = new PG;
  lv_obj_t *r = lv_obj_create(parent);
  lv_obj_remove_style_all(r);
  lv_obj_remove_flag(r, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_size(r, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(r, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(r, fw::sp::s2, 0);
  lv_obj_set_user_data(r, g);
  lv_obj_add_event_cb(r, [](lv_event_t *e) { delete (PG *) lv_event_get_user_data(e); }, LV_EVENT_DELETE, g);
  g->cap = fw::text(r, fw::ty::label, "", t.text_dim);
  g->val = fw::text(r, fw::ty::hero, "", t.text);
  g->line = fw::text(r, fw::ty::body, "", t.text_dim);
  for (auto [l, ty] : {std::pair{g->cap, fw::ty::label}, std::pair{g->val, fw::ty::hero}, std::pair{g->line, fw::ty::body}}) {
    lv_obj_set_size(l, LV_PCT(100), fw::style(ty).box);
    lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
  }
  const Read rd = read(p);
  texts(g, p, rd);
  colours(g, rd);
  g->tone = rd.tone;
  ArduinoJson::JsonDocument bp;
  bar_props(p, rd, bp);
  lv_obj_update_layout(r);
  const uint32_t n = lv_obj_get_child_count(r);
  if (const fw::Component *c = fw::find("level_bar")) c->build(r, bp.as<fw::Props>());
  g->bar = lv_obj_get_child_count(r) > n ? lv_obj_get_child(r, -1) : nullptr;
  if (g->bar) lv_obj_set_style_margin_top(g->bar, fw::sp::s5 - fw::sp::s2, 0);
}

FW_UPDATE(plant_gauge) {
  PG *g = of(root);
  if (!g) return false;
  const Read rd = read(p);
  texts(g, p, rd);
  if (g->tone != rd.tone) { g->tone = rd.tone; fw::st::restyle(root, [&] { colours(g, rd); }); }
  ArduinoJson::JsonDocument bp;
  bar_props(p, rd, bp);
  if (g->bar) fw::update_child(fw::find("level_bar"), g->bar, bp.as<fw::Props>());
  return true;
}
