// Print view: the 3D print screen's body (docs screens/print.ts printScreen; the factory spec renderer/specs/print.json
// adds the header "3D print"): the model fills up layer by layer (the real 3D print model, the content width x
// lay::print_model_h), and under it the words: the hero (hero, text: "45%"; a word when idle: "Done", "Paused" is still
// the %), and beside it the state with the end time (type label, text-dim: "printing · ends 16:20"), the job (title,
// text: "Lighthouse"), the layer and time left (body, text-dim: "layer 68 of 150 · 1 h 50 min left").
// Feed (docs PrintData): props state (printing / paused / preparing / done / idle), progress (0-100), task: a printer
// integration's sensors (Bambu: print_status, mc_percent, mc_remaining_time, layer_num), or the sample (the docs
// lighthouse, a 3 h 20 min job). The model is the lighthouse until a host sends a job's own grid. Static to touch;
// update path: the model re-renders on a new layer (snap: a new picture, the docs canvas), the words in place.
// Contrast: "print_model" + "text".
#include <ctime>
// audit deps: print_model
#include "fw.h"

namespace {
struct PV { lv_obj_t *model = nullptr, *hero = nullptr, *lbl = nullptr, *title = nullptr, *body = nullptr; std::string key; fw::ty::Id hero_style = fw::ty::hero; };
PV *of(lv_obj_t *o) { return (PV *) lv_obj_get_user_data(o); }
lv_obj_t *box(lv_obj_t *parent) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  return o;
}
void model_props(fw::Props p, ArduinoJson::JsonDocument &d) {
  const std::string st = p["state"] | "printing";
  d["progress"] = st == "done" ? 100 : st == "preparing" || st == "idle" ? 0 : (int) (p["progress"] | 45);
  d["width"] = (int) (fw::lay::content_w / fw::D);
  d["height"] = (int) (fw::lay::print_model_h / fw::D);
}
void words(lv_obj_t *root, fw::Props p) {
  PV *v = of(root);
  const std::string st = p["state"] | "printing";
  const int prog = st == "done" ? 100 : (int) (p["progress"] | 45);
  const bool active = st == "printing" || st == "paused" || st == "preparing";
  const int total = 200, left = st == "done" ? 0 : (int) std::lround((100 - prog) / 100.0 * total), layers = 150;
  char b[96];
  if (active) snprintf(b, sizeof b, "%d%%", prog);
  else snprintf(b, sizeof b, "%s", st == "done" ? "Done" : "Idle");
  // a word a size down (value: the hero's font carries digits only; docs .is-word)
  const fw::ty::Id hs = active ? fw::ty::hero : fw::ty::value;
  if (hs != v->hero_style) { fw::text_style(v->hero, hs); lv_obj_set_width(v->hero, fw::lay::print_hero_w); v->hero_style = hs; }
  fw::set_text(v->hero, hs, b);
  const time_t end = time(nullptr) + left * 60;
  struct tm tm;
  localtime_r(&end, &tm);
  const char *word = st == "preparing" ? "heating the bed" : st.c_str();
  if (active) snprintf(b, sizeof b, "%s \xC2\xB7 ends %02d:%02d", word, tm.tm_hour, tm.tm_min);
  else snprintf(b, sizeof b, "%s", word);
  fw::set_text(v->lbl, fw::ty::label, b);
  fw::set_text(v->title, fw::ty::title, p["task"] | "Lighthouse");
  if (active) {
    const int layer = (int) std::lround(prog / 100.0 * layers);
    if (left >= 60) snprintf(b, sizeof b, "layer %d of %d \xC2\xB7 %d h %02d min left", layer, layers, left / 60, left % 60);
    else snprintf(b, sizeof b, "layer %d of %d \xC2\xB7 %d min left", layer, layers, left);
  } else snprintf(b, sizeof b, "%d layers", layers);
  fw::set_text(v->body, fw::ty::body, b);
}
}  // namespace

FW_COMPONENT(print_view, "Print view", "components", R"({
  "about": "The 3D print screen's body: the model filling up layer by layer, how far it is, when it ends and how much is left.",
  "frame": [480, 400],
  "audit": {"snaps": {"progress": "a new layer is a new picture: the model re-renders, like the docs canvas"}},
  "props": [
    {"name": "state", "type": "select", "options": ["printing", "paused", "preparing", "done", "idle"], "default": "printing"},
    {"name": "progress", "type": "range", "min": 0, "max": 100, "step": 1, "default": 45},
    {"name": "task", "type": "text", "default": "Lighthouse"}
  ],
  "variants": [
    {"label": "printing · 45%", "props": {}},
    {"label": "nearly done", "props": {"progress": 92}},
    {"label": "done", "props": {"state": "done"}}
  ]
})") {
  const fw::Theme &t = *fw::T;
  auto *v = new PV;
  lv_obj_t *root = lv_obj_create(parent);
  lv_obj_remove_style_all(root);
  lv_obj_remove_flag(root, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_user_data(root, v);
  lv_obj_add_event_cb(root, [](lv_event_t *e) { delete (PV *) lv_event_get_user_data(e); }, LV_EVENT_DELETE, v);
  lv_obj_set_size(root, fw::lay::content_w, fw::lay::content_h);
  lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(root, fw::sp::s9, 0);
  ArduinoJson::JsonDocument mp;
  model_props(p, mp);
  serializeJson(mp, v->key);
  if (const fw::Component *c = fw::find("print_model")) { c->build(root, mp.as<fw::Props>()); v->model = lv_obj_get_child(root, -1); }
  lv_obj_t *row = box(root);
  lv_obj_set_size(row, fw::lay::content_w, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_set_style_pad_column(row, fw::lay::gutter, 0);
  v->hero = fw::text(row, fw::ty::hero, "--", t.text, fw::lay::print_hero_w);
  const int W = fw::lay::content_w - fw::lay::print_hero_w - fw::lay::gutter;
  lv_obj_t *col = box(row);
  lv_obj_set_size(col, W, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
  v->lbl = fw::text(col, fw::ty::label, "", t.text_dim, W);
  v->title = fw::text(col, fw::ty::title, "", t.text, W);
  v->body = fw::text(col, fw::ty::body, "", t.text_dim, W);
  words(root, p);
}

FW_UPDATE(print_view) {
  PV *v = of(root);
  if (!v) return false;
  ArduinoJson::JsonDocument mp;
  model_props(p, mp);
  std::string key;
  serializeJson(mp, key);
  if (key != v->key && v->model) { v->key = key; fw::update_child(fw::find("print_model"), v->model, mp.as<fw::Props>()); }
  words(root, p);
  return true;
}
