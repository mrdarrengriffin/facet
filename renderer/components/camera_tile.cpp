// Camera tile: one camera's latest still and what it saw (Security cameras; Frigate-style detections). Stills only,
// never video (Darren 2026-10-23).
//   still    card-w x lay::camera_h (16:9 on the grid), surface, rad::card: the camera's snapshot. Until the panel
//            fetches stills (HA camera_proxy / Frigate latest.jpg, only while the screen shows) a quiet "no still
//            yet" (type label, text-dim) sits in the middle: a plain stand-in, never a fake photo
//   detect   Frigate-style: the object's box (prop box "x,y,w,h" in % of the still) as a stroke::edge accent outline
//            with its label on an accent tab at its top left (type label, on-accent: "PERSON 92%")
//   caption  sp::s3 under the still: the camera's name (type title, text, ellipsis) and the still's time at the right
//            (type body, text-dim)
//   state    default | unavailable: the name text-dim, "offline" in the still
//   size     tile (card-w, the Cameras grid) | full (the content width x lay::camera_full_h: the camera's own screen)
// Tap (Darren 2026-10-24): a tile opens the camera's own screen (event "open" "camera?{entity, name, time, detect,
// score, box}" + sound open, the spec's nav.open); full size, a tap closes it (event "tap" + sound back, the spec's
// nav.back). Slid off before letting go = nothing. Offline: a tap = blocked + error. Update path: texts in place,
// the box moves / shows / hides with the props (snap: a new still is a new picture).
// Contrast: section "camera_tile".
#include "fw.h"

namespace {
// a text only when it changes (an update with the same props redraws nothing)
void put(lv_obj_t *l, fw::ty::Id ty, const char *t) {
  if (fw::cased(ty, t) != lv_label_get_text(l)) fw::set_text(l, ty, t);
}
struct CT { lv_obj_t *still, *note, *box, *tab, *tab_l, *name, *time; bool full = false; std::string params; };
CT *of(lv_obj_t *o) { return (CT *) lv_obj_get_user_data(o); }
lv_obj_t *mk(lv_obj_t *parent) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  return o;
}
int snap(int v) { return v / fw::sp::GRID * fw::sp::GRID; }
int still_w(const CT *c) { return c->full ? fw::lay::content_w : fw::lay::card_w; }
int still_h(const CT *c) { return c->full ? fw::lay::camera_full_h : fw::lay::camera_h; }
void take(CT *c, fw::Props p) {
  {  // what opens the camera's own screen: the tile's props as the camera spec's parameters
    ArduinoJson::JsonDocument d;
    for (const char *k : {"entity", "name", "time", "detect", "box"}) if ((p[k] | "")[0]) d[k] = p[k] | "";
    if ((p["score"] | 0) > 0) d["score"] = std::to_string((int) (p["score"] | 0));
    c->params.clear();
    serializeJson(d, c->params);
  }
  const fw::Theme &t = *fw::T;
  const bool off = !strcmp(p["state"] | "default", "unavailable");
  if (off != lv_obj_has_state(lv_obj_get_parent(c->still), fw::st::OFF)) fw::st::set_off(lv_obj_get_parent(c->still), off, false);
  put(c->name, fw::ty::title, p["name"] | "Front door");
  put(c->time, fw::ty::body, off ? "" : (p["time"] | ""));
  const lv_color_t nc = off ? t.text_dim : t.text;
  if (!lv_color_eq(lv_obj_get_style_text_color(c->name, LV_PART_MAIN), nc)) lv_obj_set_style_text_color(c->name, nc, 0);
  put(c->note, fw::ty::label, off ? "offline" : "no still yet");
  const std::string det = p["detect"] | "";
  int x = 0, y = 0, w = 0, h = 0;
  if (off || det.empty() || sscanf(p["box"] | "", "%d,%d,%d,%d", &x, &y, &w, &h) != 4 || w <= 0 || h <= 0) {
    for (lv_obj_t *o : {c->box, c->tab}) if (!lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN)) lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
    return;
  }
  for (lv_obj_t *o : {c->box, c->tab}) if (lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN)) lv_obj_remove_flag(o, LV_OBJ_FLAG_HIDDEN);
  const int W = still_w(c), H = still_h(c);
  const int bx = snap(W * LV_CLAMP(0, x, 100) / 100), by = snap(H * LV_CLAMP(0, y, 100) / 100);
  const int bw = std::max(fw::touch::min, snap(W * LV_CLAMP(0, w, 100) / 100)), bh = std::max(fw::touch::min, snap(H * LV_CLAMP(0, h, 100) / 100));
  const int px = std::max(0, std::min(bx, W - bw)), py = std::max(0, std::min(by, H - bh));
  lv_obj_set_pos(c->box, px, py);
  lv_obj_set_size(c->box, std::min(bw, W), std::min(bh, H));
  std::string tab = det;
  const int score = p["score"].is<const char *>() ? atoi(p["score"] | "0") : (p["score"] | 0);
  if (score > 0) tab += " " + std::to_string(score) + "%";
  put(c->tab_l, fw::ty::label, tab.c_str());
  lv_point_t sz;  // the tab hugs its label, rounded up to the grid, kept inside the still
  lv_text_get_size(&sz, lv_label_get_text(c->tab_l), lv_obj_get_style_text_font(c->tab_l, LV_PART_MAIN),
                   lv_obj_get_style_text_letter_space(c->tab_l, LV_PART_MAIN), 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
  const int tw = std::min<int>(W, (sz.x + 2 * fw::sp::s2 + fw::sp::GRID - 1) / fw::sp::GRID * fw::sp::GRID);
  lv_obj_set_size(c->tab, tw, fw::lay::chip_h);
  lv_obj_set_pos(c->tab, std::min(px, W - tw), py);
}
}  // namespace

FW_COMPONENT(camera_tile, "Camera tile", "components", R"({
  "about": "One camera's latest still with what it saw: the object's box and label, the camera's name and the time.",
  "frame": [320, 0],
  "props": [
    {"name": "name", "about": "The camera's name, shown along the bottom of the picture.", "type": "text", "default": "Front door"},
    {"name": "time", "about": "When the picture was taken, shown next to the name.", "type": "text", "default": "19:28"},
    {"name": "detect", "about": "What the camera saw, such as person or car, shown as a label on the box around it. Leave empty when nothing was seen.", "type": "text", "default": "person"},
    {"name": "score", "about": "How sure the camera is about what it saw, from 0 to 100 per cent.", "type": "range", "min": 0, "max": 100, "step": 1, "default": 92},
    {"name": "box", "about": "Where the box around what was seen sits on the picture: left, top, width and height as percentages, separated by commas.", "type": "text", "default": "40,20,24,64"},
    {"name": "state", "about": "default shows the picture; unavailable shows that the camera cannot be reached.", "type": "select", "options": ["default", "unavailable"], "default": "default"},
    {"name": "size", "about": "tile is a small picture for a grid of cameras; full fills the screen with one camera.", "type": "select", "options": ["tile", "full"], "default": "tile"},
    {"name": "entity", "about": "Which camera to show, by its Home Assistant entity id. Leave empty to use the sample picture.", "type": "text", "default": ""}
  ],
  "variants": [
    {"label": "person", "props": {}},
    {"label": "nothing seen", "props": {"name": "Garden", "detect": "", "time": "19:02"}},
    {"label": "car", "props": {"name": "Driveway", "detect": "car", "score": 81, "box": "8,44,52,44", "time": "18:47"}},
    {"label": "offline", "props": {"name": "Garage", "state": "unavailable"}},
    {"label": "full size", "props": {"size": "full"}}
  ]
})") {
  const fw::Theme &t = *fw::T;
  auto *c = new CT;
  c->full = !strcmp(p["size"] | "tile", "full");
  const int W = still_w(c), H = still_h(c);
  lv_obj_t *r = mk(parent);
  lv_obj_set_user_data(r, c);
  lv_obj_add_event_cb(r, [](lv_event_t *e) { delete (CT *) lv_event_get_user_data(e); }, LV_EVENT_DELETE, c);
  lv_obj_set_size(r, W, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(r, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(r, fw::sp::s3, 0);
  c->still = mk(r);
  lv_obj_set_size(c->still, W, H);
  lv_obj_set_style_radius(c->still, fw::rad::card, 0);
  lv_obj_set_style_bg_color(c->still, t.surface, 0);
  lv_obj_set_style_bg_opa(c->still, LV_OPA_COVER, 0);
  c->note = fw::text(c->still, fw::ty::label, "", t.text_dim);
  lv_obj_center(c->note);
  c->box = mk(c->still);
  lv_obj_set_style_border_width(c->box, fw::stroke::edge, 0);
  lv_obj_set_style_border_color(c->box, t.accent, 0);
  fw::qa::free(c->box, "the detection box: placed where the object is in the still");
  c->tab = mk(c->still);  // beside the box, not in it: a tab wider than a small box still shows whole
  lv_obj_set_style_bg_color(c->tab, t.accent, 0);
  lv_obj_set_style_bg_opa(c->tab, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_hor(c->tab, fw::sp::s2, 0);
  c->tab_l = fw::text(c->tab, fw::ty::label, "", t.on_accent);
  lv_obj_center(c->tab_l);
  lv_obj_set_style_clip_corner(c->still, true, 0);
  lv_obj_t *cap = mk(r);
  lv_obj_set_size(cap, W, fw::style(fw::ty::title).box);
  lv_obj_set_flex_flow(cap, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(cap, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(cap, fw::sp::s3, 0);
  c->name = fw::text(cap, fw::ty::title, "", t.text);
  lv_obj_set_flex_grow(c->name, 1);
  lv_label_set_long_mode(c->name, LV_LABEL_LONG_DOT);
  c->time = fw::text(cap, fw::ty::body, "", t.text_dim);
  lv_obj_add_flag(r, LV_OBJ_FLAG_CLICKABLE);  // tile: opens the camera; full: closes it (Back)
  lv_obj_remove_flag(r, LV_OBJ_FLAG_PRESS_LOCK);  // slid off before letting go = nothing
  fw::st::on_tap(r, [](lv_event_t *e) {
    if (!fw::st::tap_now(e)) return;
    auto *r = (lv_obj_t *) lv_event_get_current_target(e);
    CT *c = of(r);
    if (lv_obj_has_state(r, fw::st::OFF)) { fw::st::blocked(r, "offline"); return; }
    if (c->full) { fw::emit(r, "tap", "close"); fw::sound(r, "back"); return; }
    fw::emit(r, "open", ("camera?" + c->params).c_str());
    fw::sound(r, "open");
  }, nullptr);
  take(c, p);
}

FW_UPDATE(camera_tile) {
  CT *c = of(root);
  if (!c || c->full != !strcmp(p["size"] | "tile", "full")) return false;
  take(c, p);
  return true;
}
