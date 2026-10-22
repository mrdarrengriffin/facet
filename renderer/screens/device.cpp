// Device (live): the panel running as on the wall (core/device.h): it starts on Home and every tap is handled inside the
// renderer (the settings button and Home's cards open their screen with the Content rise, Back goes back), the same
// C++ the panel firmware runs (POST /fw/show {"id":"device"}). Workbench: /wasm/screens/device at the selected size.
#include "fw.h"
#include "device.h"

namespace {
const char *META = R"({
  "about": "The panel running live: starts on Home; tap the settings button or a card to open a screen, Back to return. Every tap is handled inside the renderer, as on the device.",
  "frame": [480, 480],
  "icons": ["screen-settings@icon", "back@glyph"],
  "props": [
    {"name": "start", "type": "select", "options": ["home", "settings"], "default": "home"}
  ],
  "variants": []
})";
void build_device(lv_obj_t *parent, fw::Props p) { fw::dev::mount(parent, p["start"] | "home"); }
const int reg = fw::add({"device", "Device (live)", "screens", META, build_device});
}  // namespace
