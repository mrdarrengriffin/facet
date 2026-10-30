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
// States: default, disabled, unavailable = the Slider's (its own looks: the word Unavailable on the track); the label
// and value stay. Steps: none (fixed boxes, the slider takes the rest). Update path: the Slider's own update (the
// finger wins), the words in place, silent. Contrast: text, slider.
#include "fw.h"
#include "fx.h"

namespace {
void slider_props(fw::Props p, ArduinoJson::JsonDocument &d) {
  d["value"] = p["value"] | 40, d["state"] = p["state"] | "default";
}
void fill(lv_obj_t *root, fw::Props p) {
  for (const char *t : {"box", "row", "fill-w"}) fx::use(root, t);
  lv_obj_t *l = fx::text(root, p["label"] | "vol", "type:label", "fg:text-dim");
  fx::use(l, "w:m-vol-label");
  ArduinoJson::JsonDocument d;
  slider_props(p, d);
  fw::find("slider")->build(root, d.as<fw::Props>());
  fx::use(lv_obj_get_child(root, 1), "grow");
  lv_obj_t *vb = fx::box(root, "free");
  for (const char *t : {"wrap-end", "w:m-vol-label"}) fx::use(vb, t);  // the value at the box's right end
  fx::text(vb, p["text"] | "--", "type:label", "fg:text");
}
}  // namespace

FW_COMPONENT(slider_row, "Slider row", "components", R"({
  "about": "A slider with its name before it and its value after it, such as the volume on the Media screen.",
  "contract": 2,
  "parts": [],
  "frame": [656, 64],
  "props": [
    {"name": "label", "about": "The name before the slider, in small capitals, such as vol.", "type": "text", "default": "vol"},
    {"name": "value", "about": "Where the slider is, from 0 to 100.", "type": "range", "min": 0, "max": 100, "step": 1, "default": 42},
    {"name": "text", "about": "The value shown after the slider, such as 42 (-- when it is not known).", "type": "text", "default": "42"},
    {"name": "state", "about": "default can be dragged; disabled cannot be changed; unavailable shows the device cannot be reached.", "type": "select", "options": ["default", "disabled", "unavailable"], "default": "default"}
  ],
  "variants": [
    {"label": "volume · 42", "props": {}},
    {"label": "volume · 100", "props": {"value": 100, "text": "100"}},
    {"label": "unavailable", "props": {"state": "unavailable", "text": "--"}}
  ]
})") {
  lv_obj_t *root = lv_obj_create(parent);
  lv_obj_remove_style_all(root);
  lv_obj_remove_flag(root, (lv_obj_flag_t) (LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE));
  fx::name(root, "slider_row");
  fill(root, p);
}

FW_UPDATE(slider_row) {
  lv_obj_t *l = lv_obj_get_child(root, 0), *s = lv_obj_get_child(root, 1), *v = lv_obj_get_child(root, 2);
  if (!l || !s || !v) return false;
  ArduinoJson::JsonDocument d;
  slider_props(p, d);
  fw::update_child(fw::find("slider"), s, d.as<fw::Props>());
  fx::set_text(l, p["label"] | "vol", "type:label");
  fx::set_text(lv_obj_get_child(v, 0), p["text"] | "--", "type:label");
  return true;
}
