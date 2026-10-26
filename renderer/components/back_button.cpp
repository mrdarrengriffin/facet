// Back button: the Facet header back button (docs: Components > Buttons > Back button, @facet/ui backButton()): a
// square icon button with the back arrow and no label; the header puts the screen name next to it (lay::back_gap).
// Built by the Icon button component itself (fw::find("icon_button"), style square, internal role "back"), so look,
// press and disabled rules are the Icon button's: surface + 1 px line border, radius rad::square, text glyph back at
// ic::glyph (32 / 20); pressed = surface-press + line-hi (Motion press / release); disabled = text-mute glyph.
// Size: touch::back (48 / 32) = the header row (layout.json header row, checked at compile time) = touch::min, so it
// fits the 48 px header exactly and is a full touch target. Tap = event "back" + sound back (sound.json: the falling
// mirror of Open); hold (LVGL long press) = event "home" + sound back (the device goes straight Home, facet-188; the
// tap after the hold is swallowed); disabled = "blocked" + error. Pairs: colour.json contrast section "icon_button" (shared).
#include "fw.h"

static_assert(fw::touch::back == fw::lay::row, "back button must fill the header row (space.json touch back = layout.json header row)");
static_assert(fw::touch::back >= fw::touch::min, "back button below the touch minimum");

namespace {
void props(fw::Props p, ArduinoJson::JsonDocument &bp) {  // the Icon button's props
  bp["style"] = "square";
  bp["role"] = "back";
  bp["state"] = p["state"] | "default";
}
}  // namespace

FW_COMPONENT(back_button, "Back button", "components", R"({
  "about": "The square back arrow at the start of a screen's header.",
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
