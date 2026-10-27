// Back button: the Facet header back button (docs: Components > Buttons > Back button, @facet/ui backButton()). A thin
// wrapper (Header round 3, 2026-10-27): it IS the Icon button (components/icon_button.cpp, contract v2) in style square
// with the back icon and its internal role "back", so look, press, disabled, size (layout:ibtn-square = touch:min = the
// header row) and the update path are the Icon button's. The wrapper adds what only Back does: tap = event "back" +
// sound back (the role); hold (LVGL long press) = event "home" + sound back (the device goes straight Home, facet-188;
// the tap after the hold is swallowed); disabled = "blocked" + error. The Header builds it (fixed, first, shown only
// when the screen has somewhere to go back to). Kept as its own id: screens, specs and the catalogue name it.
// Steps / bleed: none. States: default, pressed, disabled (the Icon button's). Pairs: colour.json "icon_button".
#include "fw.h"


namespace {
void props(fw::Props p, ArduinoJson::JsonDocument &bp) {  // the Icon button's props
  bp["style"] = "square";
  bp["icon"] = "back";
  bp["role"] = "back";
  bp["state"] = p["state"] | "default";
}
}  // namespace

FW_COMPONENT(back_button, "Back button", "components", R"({
  "about": "The square back arrow at the start of a screen's header.",
  "contract": 2,
  "min": ["layout:ibtn-square", "layout:ibtn-square"],
  "frame": [96, 96],
  "icons": ["back@glyph"],
  "props": [
    {"name": "state", "about": "How the button looks: default ready to tap, pressed while a finger is on it, or disabled when there is nowhere to go back to.", "type": "select", "options": ["default", "pressed", "disabled"], "default": "default"}
  ],
  "variants": [
    {"label": "default", "props": {}},
    {"label": "pressed", "props": {"state": "pressed"}},
    {"label": "disabled", "props": {"state": "disabled"}}
  ]
})") {
  const fw::Component *ib = fw::find("icon_button");
  if (!ib) return;
  ArduinoJson::JsonDocument bp;
  props(p, bp);
  ib->build(parent, bp.as<fw::Props>());
  lv_obj_t *b = lv_obj_get_child(parent, -1);  // the Icon button just built
  if (!b) return;
  lv_obj_add_event_cb(b, [](lv_event_t *e) {
    auto *o = (lv_obj_t *) lv_event_get_current_target(e);
    if (lv_obj_has_state(o, fw::st::OFF)) return;  // disabled: the tap's own blocked feedback
    if (lv_indev_t *in = lv_indev_active()) lv_indev_wait_release(in);  // no tap after the hold
    fw::emit(o, "home");
    fw::sound(o, "back");
  }, LV_EVENT_LONG_PRESSED, nullptr);
}

// Update path: the Icon button's own (the state fades in place), silent.
FW_UPDATE(back_button) {
  ArduinoJson::JsonDocument bp;
  props(p, bp);
  return fw::update_child(fw::find("icon_button"), root, bp.as<fw::Props>());
}
