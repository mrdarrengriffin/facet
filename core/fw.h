// Facet WASM workbench: the tiny API a component sees (README.md).
// A component = one .cpp in ../components/ with one FW_COMPONENT(...) block. Sizes are written in dp (1x) and turned
// into device pixels with fw::px() (Waveshare/Shelly build = 1.5x, Guition build = 1x). Colours only from fw::T.
#pragma once
#include <cmath>
#include <cstring>
#include "lvgl.h"
#include "ArduinoJson.h"

namespace fw {

// Facet colour tokens (@facet/tokens color.*), both themes
struct Theme {
  bool light;
  lv_color_t bg, surface, surface_press, line, line_hi, text, text_dim, text_mute, accent, accent_press, accent_dim,
      on_accent, blue, ok, warn, error;
};
extern const Theme *T;  // the theme of the stage being built

#ifdef FW_1X
constexpr float D = 1.0f;
#else
constexpr float D = 1.5f;
#endif
inline int px(float dp) { return (int) std::lround(dp * D); }

// nearest Facet font to `dp` (sans, or mono); fonts are the panel's generated sets for this density
const lv_font_t *font(float dp, bool mono = false);

using Props = ArduinoJson::JsonObjectConst;
using Build = void (*)(lv_obj_t *parent, Props p);
struct Component {
  const char *id, *name, *section, *meta;  // meta = JSON: {"demo"?, "frame":[w,h] dp, "props":[...], "variants":[...]}
  Build build;
};
int add(const Component &c);

// apply a forced state from props ("state": "pressed" | "disabled" | ...) so variants can show it without a touch
inline void force_state(lv_obj_t *o, Props p) {
  const char *s = p["state"] | "";
  if (!strcmp(s, "pressed")) lv_obj_add_state(o, LV_STATE_PRESSED);
  if (!strcmp(s, "disabled")) lv_obj_add_state(o, LV_STATE_DISABLED);
  if (!strcmp(s, "checked")) lv_obj_add_state(o, LV_STATE_CHECKED);
}

}  // namespace fw

// FW_COMPONENT(id, "Name", "components", R"({...meta json...})") { ...build with `parent` and `p`... }
#define FW_COMPONENT(ID, NAME, SECTION, META)                                    \
  static void fw_build_##ID(lv_obj_t *parent, fw::Props p);                      \
  static int fw_reg_##ID = fw::add({#ID, NAME, SECTION, META, fw_build_##ID});   \
  static void fw_build_##ID(lv_obj_t *parent, fw::Props p)
