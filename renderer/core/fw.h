// Facet WASM workbench: the tiny API a component sees (README.md).
// A component = one .cpp in ../components/ with one FW_COMPONENT(...) block. Sizes are written in dp (1x) and turned
// into device pixels with fw::px() (Waveshare/Shelly build = 1.5x, Guition build = 1x). Colours only from fw::T
// (tokens/colour.json; never a hex in a component).
#pragma once
#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>
#include "lvgl.h"
#include "ArduinoJson.h"
#include "colour_gen.h"  // fw::Theme + fw::tokens:: (generated from tokens/colour.json)
#include "type_gen.h"    // fw::ty:: styles (generated from tokens/type.json)
#include "space_gen.h"   // fw::sp:: lay:: rad:: stroke:: touch:: (generated from tokens/space.json), device px
#include "layout_gen.h"  // fw::lay:: screen geometry + fw::lyt:: devices / size classes (generated from tokens/layout.json), device px
#include "icons_gen.h"   // fw::ic:: icon sizes + the icon table (generated from tokens/icons.json)
#include "motion_gen.h"  // fw::mo:: durations, easings, patterns (generated from tokens/motion.json)
#include "mascot_gen.h"  // fw::mas:: mascot geometry, moods, motion tracks (generated from tokens/mascot.json)

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
// snapped to the 4 px grid (Darren 2026-10-21: no odd or off-grid geometry at either density); 0 stays 0, minimum 4
inline int px(float dp) {
  const int v = (int) std::lround(dp * D);
  return v == 0 ? 0 : v > 0 ? std::max(4, (v + 2) / 4 * 4) : -std::max(4, (-v + 2) / 4 * 4);
}
// Spacing, layout, radius, strokes, touch targets (tokens/space.json, README "Space tokens"): already device px for
// this density (fw::sp::s3, fw::lay::inset, fw::rad::button, fw::stroke::hair, fw::touch::min): never wrap them in px().
// px() stays the dp mapping for one-off geometry the tokens do not cover (page frames, sample sizes).

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

// Icons (tokens/icons.json, README "Icons"): pre-rasterised A8 masks at the exact pixel size of each icon size token
// for this density (never scaled on the device), recoloured here. fw::icon(parent, "plus", ic::icon, T->text) = an
// ic::PX[size] square box; ink parts take `colour`, a screen icon's accent part T->accent, weather parts their own
// tokens (text-dim, accent, blue, steel; the cloud stroke = ink). Not in this build (name or size) -> nullptr.
lv_obj_t *icon(lv_obj_t *parent, const char *name, ic::Size size, lv_color_t colour);
// recolour an icon's ink parts for a state selector (e.g. LV_STATE_PRESSED), optional transition (press fades)
void icon_colour(lv_obj_t *icon, lv_color_t colour, lv_style_selector_t sel = 0, const lv_style_transition_dsc_t *tr = nullptr);
int icon_index(const char *name);          // into ic::ICONS, -1 = unknown
bool icon_has(int index, ic::Size size);  // is this icon@size in this build (all sizes, or FW_ICONS=used)
int icon_bytes(int index, ic::Size size); // flash of one icon@size (its masks), 0 = not in this build
int icons_bytes();                        // flash of every icon in this build (masks + tables)
int icons_entries();                      // icon x size pairs in this build
bool icons_used_only();                   // built with FW_ICONS=used

// Motion (tokens/motion.json, README "Motion tokens"): never a raw duration or easing in a component. Named patterns
// fw::mo::press, sheet_open, screen, value, ... = duration + easing + offset (device px) + reduced-motion mode
// (fw::mo::PATTERNS). Reduced motion is one setting per display (device: global; workbench: per stage, set by
// fw::render from the props key "reduced_motion"): nothing moves, opacity fades stay short, colours stay, loops stop.
namespace mo {
inline const Pattern &pat(Id p) { return PATTERNS[p]; }
inline lv_anim_path_cb_t path(Id p) { return EASINGS[PATTERNS[p].easing].path; }
bool reduced(const lv_obj_t *o);                // reduced motion for o's display
void set_reduced(lv_display_t *d, bool on);     // per display (render() sets it for the stage it builds on)
void set_reduced_default(bool on);              // the panel's setting: every display without its own
bool reduced_default();
enum Channel { MOVE, OPA, VALUE };              // what an animation changes: position, opacity, a shown value
uint32_t duration(const lv_obj_t *o, Id p, Channel ch);  // the pattern's ms for o, 0 = instant (reduced motion)
// a style transition for a colour pattern (press / release) on the given props (LV_STYLE_PROP_INV terminated); one
// static descriptor per call site: static lv_style_transition_dsc_t d; mo::transition(&d, mo::press, PROPS);
void transition(lv_style_transition_dsc_t *d, Id p, const lv_style_prop_t *props);
}  // namespace mo
// one animation of obj (the anim's var) with a pattern's duration + easing; reduced motion collapses it (exec(to) now,
// done called). loop = there and back forever (pulse). Restarts an animation already running on the same obj + exec.
struct Anim {
  lv_anim_exec_xcb_t exec;
  int32_t from, to;
  mo::Channel ch = mo::MOVE;
  uint32_t delay = 0;
  bool loop = false;
  lv_anim_completed_cb_t done = nullptr;
  void *user = nullptr;
};
void animate(lv_obj_t *o, mo::Id p, const Anim &a);
// appear (in) / leave: opacity to 100 / 0 % and translate_y from / to dir x the pattern's offset (+1 = below, -1 =
// above, 0 = in place), from wherever it is now. Reduced: opacity only (pattern mode). done after the opacity anim.
void appear(lv_obj_t *o, mo::Id p, bool in, int dir = 1, uint32_t delay = 0, lv_anim_completed_cb_t done = nullptr, void *user = nullptr);
// the screen pattern (Facet "Content rise", = the old lab's): call after cutting to the new screen. items[0] = the
// header, then the content blocks; blocks are grouped into rows by their top (side by side = one row), each row fades
// in and rises by the offset, header first, each row a stagger later (max-steps); home = drops in from above (going
// back Home). Reduced: instant.
void rise(lv_obj_t *const *items, int n, bool home);

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

// Events + sound (README "Events and sound"): a component reports what happened to the host, which decides what to
// do with it. fw::emit(obj, "tap", "primary Start") -> the workbench event log (device: ESP log); fw::sound(obj, "tap")
// -> the host plays that UI sound id (tokens/sound.json types: tap, error, toggle-on, ...) in the user's pack. The
// workbench plays it in the browser; the device has no audio yet (TODO: sound playback on the panels, logged for now).
using Sink = void (*)(lv_obj_t *obj, const char *kind, const char *name, const char *detail);  // kind = "event" | "sound"
void set_sink(Sink s);
void emit(lv_obj_t *obj, const char *name, const char *detail = "");
void sound(lv_obj_t *obj, const char *id);
// a pitched sound: pos = where the value sits in its range (0..1), sent as the event detail ("0.420"); the host shifts
// the pitch by the pack's `track` semitones over the whole range (tokens/sound.json; 0 = not pitched). Step ticks use it.
void sound(lv_obj_t *obj, const char *id, float pos);
// Step ticks (Darren 2026-10-21), the one rule for every slider / dial / arc: call when the SHOWN value moves from
// step `from` to step `to` of `n` steps. Every step crossed counts; an audible "slider-tick" plays for a step only when
// the last audible tick is at least mo::ms::tick_min (motion.json "tick-min") ago, so a slow drag ticks on every single
// step and a fast one thins out to a steady patter, never more ticks than steps. A landmark step (ends of the range:
// tick_landmark()) plays "slider-landmark" instead and is never dropped: too soon after a tick, it waits out the
// interval (one lv_timer). One limiter for the whole display (one finger at a time).
// Pitch follows the value (Darren 2026-10-21): every tick and landmark carries its step's place in the range
// (step / n) as the sound position, so low values tick lower and high values higher (sound.json `track`).
void step_ticks(lv_obj_t *obj, int from, int to, int n);
bool tick_landmark(int step, int n);  // 0 and n today; add 50 % etc. in fw_core.cpp LANDMARKS

// Bleed (States, Darren 2026-10-21): let o's marks draw up to `px` outside its box (a focus ring at the focus offset,
// an overhanging press fill). LVGL clips every child to each ancestor's box, so o and every ancestor up to the screen
// get LV_OBJ_FLAG_OVERFLOW_VISIBLE AND an ext draw size of at least px (OVERFLOW_VISIBLE alone draws 0 px outside: the
// clip is the ancestor's box grown by ITS ext draw size). Call once o is in its final parent.
void bleed(lv_obj_t *o, int px);

// apply a forced state from props ("state": "pressed" | "disabled" | ...) so variants can show it without a touch
inline void force_state(lv_obj_t *o, Props p) {
  const char *s = p["state"] | "";
  if (!strcmp(s, "pressed")) lv_obj_add_state(o, LV_STATE_PRESSED);
  if (!strcmp(s, "disabled")) lv_obj_add_state(o, LV_STATE_DISABLED);
  if (!strcmp(s, "checked")) lv_obj_add_state(o, LV_STATE_CHECKED);
}

}  // namespace fw

// Interaction states (tokens/states.json, README "State tokens"): fw::st::OFF / ON / PRESSED, colour roles per state
// (fw::st::disabled_ink(t), pressed_tint(t), ...), press / switch motion, blocked() = event + error.
// Components take every state choice from here, never pick a colour token for a state themselves.
#include "states_gen.h"
// Atoms: the shared accent marks fw::focus_ring() + fw::corner_ticks() (core/atoms.h, after the state tokens they use)
#include "atoms.h"
// The mascot fw::mascot() (core/mascot.h; tokens in mascot_gen.h): moods, parts, keyframe motion
#include "mascot.h"

// FW_COMPONENT(id, "Name", "components", R"({...meta json...})") { ...build with `parent` and `p`... }
#define FW_COMPONENT(ID, NAME, SECTION, META)                                    \
  static void fw_build_##ID(lv_obj_t *parent, fw::Props p);                      \
  static int fw_reg_##ID = fw::add({#ID, NAME, SECTION, META, fw_build_##ID});   \
  static void fw_build_##ID(lv_obj_t *parent, fw::Props p)
