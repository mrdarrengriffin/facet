// Back button: the Facet header back button (docs: Components > Buttons > Back button, @facet/ui backButton()): a
// square icon button with the back arrow and no label; the header puts the screen name next to it (lay::back_gap).
// Built by the Icon button component itself (fw::find("icon_button"), style square, internal role "back"), so look,
// press and disabled rules are the Icon button's: surface + 1 px line border, radius rad::square, text glyph back at
// ic::glyph (32 / 20); pressed = surface-press + line-hi (Motion press / release); disabled = text-mute glyph.
// Size: touch::back (48 / 32) = the header row (layout.json header row, checked at compile time) = touch::min, so it
// fits the 48 px header exactly and is a full touch target. Tap = event "back" + sound back (sound.json: the falling
// mirror of Open); disabled = "blocked" + error. Pairs: colour.json contrast section "icon_button" (shared).
#include "fw.h"

static_assert(fw::touch::back == fw::lay::row, "back button must fill the header row (space.json touch back = layout.json header row)");
static_assert(fw::touch::back >= fw::touch::min, "back button below the touch minimum");

FW_COMPONENT(back_button, "Back button", "components", R"({
  "about": "The square back arrow at the start of a screen's header.",
  "frame": [96, 96],
  "icons": ["back@glyph"],
  "props": [
    {"name": "state", "type": "select", "options": ["default", "pressed", "disabled"], "default": "default"}
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
  bp["style"] = "square";
  bp["role"] = "back";
  bp["state"] = p["state"] | "default";
  ib->build(parent, bp.as<fw::Props>());
}
