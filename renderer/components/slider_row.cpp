// Slider row: a Slider with its name before it and its value after it, on the contract (renderer/CONTRACT.md,
// "contract": 2). The Media screen's volume (docs screens/media.ts .fs-vol: "vol", the slider, the value); any level
// a screen sets live (a fan speed, a brightness) the same way.
//   row      a row (cross-centred, the gutter apart), fills its parent's width, touch:min tall (the Slider's)
//   label    type:label caps, fg:text-dim, in a w:m-vol-label box (docs m-vol-label-w), so the track starts at the same
//            place whatever the word
//   slider   the real Slider (fw::find("slider"), C8b), growing into the room between the two boxes; its events are
//            this node's: "change" (released) and the live "input" while a finger moves it (core/spec.h maps them)
//   value    type:label, fg:text, right-aligned in a w:m-vol-label box ("--" = not known); bound (the screen's feed puts
//            the finger's value while it moves)
// States: default, disabled, unavailable, missing = the Slider's (its own looks: the word Unavailable on the track); the label
// and value stay. Steps: none (fixed boxes, the slider takes the rest). Update path: the Slider's own update (the
// finger wins), the words in place, silent. Contrast: text, slider.
#include "fw.h"
#include "fx.h"

namespace {
// the value after the slider: text, or a number bound to it (a light's brightness %, fmt pct: "50"); none = "--"
std::string text_of(fw::Props p) { return p["text"].is<int>() ? std::to_string(p["text"].as<int>()) : std::string(p["text"] | "--"); }
void slider_props(fw::Props p, ArduinoJson::JsonDocument &d) {
  d["value"] = p["value"] | 40, d["state"] = p["state"] | "default";
}
void fill(lv_obj_t *root, fw::Props p) {
  if (!strcmp(p["look"] | "line", "bar")) {  // look bar (#200): the Slider's bar fills the row; the name and the value on it
    fx::use(root, "fill");
    ArduinoJson::JsonDocument d;
    slider_props(p, d);
    d["look"] = "bar";
    fw::find("slider")->build(root, d.as<fw::Props>());
    lv_obj_t *o = fx::box(root, "row");  // on top of the bar, out of the way of a press (not clickable: the bar takes it)
    for (const char *t : {"fill", "pad-x:4"}) fx::use(o, t);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t *l = fx::text(o, p["label"] | "vol", "type:label", "fg:text", "none");
    fx::use(l, "grow");
    lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
    fx::text(o, text_of(p).c_str(), "type:label", "fg:text", "none");
    return;
  }
  for (const char *t : {"box", "row", "fill-w"}) fx::use(root, t);
  lv_obj_t *l = fx::text(root, p["label"] | "vol", "type:label", "fg:text-dim", "none");
  if (p["fit"] | false) {  // the label as wide as its words, at most strip-name (a Home Slider strip: a light's name)
    for (const char *t : {"hug", "max-w:strip-name"}) fx::use(l, t);
    lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
  } else fx::use(l, "w:m-vol-label");
  ArduinoJson::JsonDocument d;
  slider_props(p, d);
  fw::find("slider")->build(root, d.as<fw::Props>());
  fx::use(lv_obj_get_child(root, 1), "grow");
  lv_obj_t *vb = fx::box(root, "free");
  for (const char *t : {"wrap-end", "w:m-vol-label", "label-row"}) fx::use(vb, t);  // the value at the box's right end, the label's box tall
  fx::text(vb, text_of(p).c_str(), "type:label", "fg:text", "none");
}
}  // namespace

FW_COMPONENT(slider_row, "Slider row", "components", R"({
  "about": "A slider with its name before it and its value after it, such as the volume on the Media screen.",
  "sizes": ["2xhalf", "3xhalf"],
  "contract": 2,
  "parts": [],
  "frame": [656, 64],
  "props": [
    {"name": "label", "about": "The name before the slider, in small capitals, such as vol.", "type": "text", "default": "vol"},
    {"name": "value", "about": "Where the slider is, from 0 to 100.", "type": "range", "min": 0, "max": 100, "step": 1, "default": 42},
    {"name": "look", "about": "line is the name, a thin slider and the value in a row; bar makes the whole row the slider, a bar that fills up to the value with the name and the value on it (a light on Home).", "type": "select", "options": ["line", "bar"], "default": "line"},
    {"name": "fit", "about": "Makes the name before the slider as wide as its words (at most layout strip-name, then ...) instead of the short fixed width, for a longer name such as a light's on Home.", "type": "bool", "default": false},
    {"name": "text", "about": "The value shown after the slider, such as 42 (-- when it is not known).", "type": "text", "default": "42"},
    {"name": "state", "debug": true, "about": "default can be dragged; disabled cannot be changed; unavailable shows the device has no state now; missing shows Home Assistant has no such device.", "type": "select", "options": ["default", "disabled", "unavailable", "missing"], "default": "default"}
  ],
  "variants": [
    {"label": "volume · 42", "props": {}},
    {"label": "volume · 100", "props": {"value": 100, "text": "100"}},
    {"label": "bar · a light on Home", "props": {"look": "bar", "label": "Sofa lamp", "value": 71, "text": "71"}},
    {"label": "unavailable", "props": {"state": "unavailable", "text": "--"}},
    {"label": "missing", "props": {"state": "missing", "text": "--"}}
  ]
})") {
  lv_obj_t *root = lv_obj_create(parent);
  lv_obj_remove_style_all(root);
  lv_obj_remove_flag(root, (lv_obj_flag_t) (LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE));
  fx::name(root, "slider_row");
  fill(root, p);
}

FW_UPDATE(slider_row) {
  const bool bar = !strcmp(p["look"] | "line", "bar");
  if (bar != (lv_obj_get_child_count(root) == 2)) return false;  // another look: built again
  if (bar) {  // the bar, then the row on it: name, value
    lv_obj_t *s = lv_obj_get_child(root, 0), *o = lv_obj_get_child(root, 1);
    ArduinoJson::JsonDocument d;
    slider_props(p, d);
    d["look"] = "bar";
    fw::update_child(fw::find("slider"), s, d.as<fw::Props>());
    fx::set_text(lv_obj_get_child(o, 0), p["label"] | "vol", "type:label");
    fx::set_text(lv_obj_get_child(o, 1), text_of(p).c_str(), "type:label");
    return true;
  }
  lv_obj_t *l = lv_obj_get_child(root, 0), *s = lv_obj_get_child(root, 1), *v = lv_obj_get_child(root, 2);
  if (!l || !s || !v) return false;
  ArduinoJson::JsonDocument d;
  slider_props(p, d);
  fw::update_child(fw::find("slider"), s, d.as<fw::Props>());
  fx::set_text(l, p["label"] | "vol", "type:label");
  fx::set_text(lv_obj_get_child(v, 0), text_of(p).c_str(), "type:label");
  return true;
}
