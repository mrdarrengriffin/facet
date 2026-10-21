// Facet WASM workbench: the tiny API a component sees (README.md).
// A component = one .cpp in ../components/ with one FW_COMPONENT(...) block. Sizes are written in dp (1x) and turned
// into device pixels with fw::px() (Waveshare/Shelly build = 1.5x, Guition build = 1x). Colours only from fw::T
// (tokens/colour.json; never a hex in a component).
#pragma once
#include <cmath>
#include <cstring>
#include <string>
#include <vector>
#include "lvgl.h"
#include "ArduinoJson.h"
#include "colour_gen.h"  // fw::Theme + fw::tokens:: (generated from tokens/colour.json)
#include "type_gen.h"    // fw::ty:: styles (generated from tokens/type.json)

namespace fw {

// Facet colour tokens: generated from tokens/colour.json (build.sh -> colour_gen.h): fw::Theme has one lv_color_t per
// token (name with - -> _, e.g. T->text_dim), both themes, the five accent roles taken from the selected accent set
extern const Theme *T;  // the theme of the stage being built
// any theme (e.g. to show dark and light side by side); accent_set = index into tokens::ACCENTS, <0 = default
const Theme *theme(bool light, int accent_set = -1);

#ifdef FW_1X
constexpr float D = 1.0f;
#else
constexpr float D = 1.5f;
#endif
inline int px(float dp) { return (int) std::lround(dp * D); }

// Type (tokens/type.json, README "Type tokens"): styles fw::ty::display, ty::body, ty::c_meta, ... (- -> _), values in
// device px for this density (fw::ty::STYLES[s]). Fonts are generated per density with only the glyphs the styles use.
const lv_font_t *font(ty::Id s);
inline const ty::Style &style(ty::Id s) { return ty::STYLES[s]; }
// a label in type style `s`: font, letter spacing, case (upper styles are upper-cased here), height = the style's line
// box with the cap height centred in it (the Facet text-box rule). wrap_w > 0 = a paragraph wrapped at that width, lines
// on the box pitch. fw::set_text keeps the style's case.
lv_obj_t *text(lv_obj_t *parent, ty::Id s, const char *txt, lv_color_t c, int wrap_w = 0);
void set_text(lv_obj_t *label, ty::Id s, const char *txt);
void text_style(lv_obj_t *label, ty::Id s);  // apply style s to an existing label (then sets its box)
std::string cased(ty::Id s, const char *txt);
// nearest full-text font to `dp` (sans, or mono) for one-off labels outside the type scale; prefer fw::text
const lv_font_t *font(float dp, bool mono = false);
// the fonts of this build (Type page: coverage, memory)
struct FontRef { const char *id = nullptr, *family = nullptr; int weight = 0, size = 0; bool mono = false, tabular = false; int glyphs = 0, bytes = 0; const lv_font_t *font = nullptr; };
int n_fonts();
int font_bytes();  // flash: bitmaps + glyph tables of every font
FontRef font_ref(int i);
int style_font_index(ty::Id s);
uint32_t font_glyph(int font, int g);  // g-th code point of a font (sorted), 0 = none

using Props = ArduinoJson::JsonObjectConst;
using Build = void (*)(lv_obj_t *parent, Props p);
struct Component {
  const char *id, *name, *section, *meta;  // meta = JSON: {"demo"?, "frame":[w,h] dp, "props":[...], "variants":[...]}
  Build build;
};
int add(const Component &c);
std::vector<Component> &reg();            // every registered component (registration order)
const Component *find(const char *id);    // nullptr = not in this build
void meta_json(std::string &out);         // [{id,name,section,meta}] as JSON
// clean `scr` (any LVGL screen) and build component `comp` on it with props JSON, theme light/dark + accent set
// index: the same call the workbench preview and the device firmware make. Returns "" or an error.
const char *render(lv_obj_t *scr, const char *comp, const char *props, bool light, int accent);

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
