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
// Large text (Settings > Text size = Large): title / body / label one step bigger in the same line boxes, so nothing
// moves. A build-time switch: fw::font(s) gives the large font while it is on. render() sets it from props "text_size"
// (large / standard), else the panel's setting (set_text_large_default); the device runtime from its setting.
void set_text_large(bool on);
bool text_large();
void set_text_large_default(bool on);
bool text_large_default();
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
// Turn icons (icons.json turn: the plane): the master's outline filled at any heading on the device (no resampled
// mask). fw::icon_turned(parent, "plane", px, track, T->text_dim) = a px square image turned clockwise by deg;
// icon_turned_mask = the same as a bare A8 mask (draw it with an image recolour) for a component's own draw event,
// freed with icon_mask_free. nullptr = not a turn icon.
lv_obj_t *icon_turned(lv_obj_t *parent, const char *name, int px, float deg, lv_color_t colour);
lv_draw_buf_t *icon_turned_mask(int index, int px, float deg);
void icon_mask_free(lv_draw_buf_t *mask);
int icon_index(const char *name);          // into ic::ICONS, -1 = unknown
bool icon_has(int index, ic::Size size);  // is this icon@size in this build (all sizes, or FW_ICONS=used)
int icon_bytes(int index, ic::Size size); // flash of one icon@size (its masks), 0 = not in this build
int icons_bytes();                        // flash of every icon in this build (masks + tables)
int icons_entries();                      // icon x size pairs in this build
bool icons_used_only();                   // built with FW_ICONS=used
// Animated weather icons (README "Icons" > "Animated weather"): fw::weather_icon(parent, "rain-light", ic::card,
// animate) = the same S x S box as fw::icon. animate + not reduced motion + the size has animated parts
// (icons.json animated) -> the master's parts (motion.json weather keyframes, one lv_anim clock, writes only on whole
// px / opacity steps); else exactly the static master (fw::icon). behind = the colour under the icon (T->bg, a card's
// T->surface): a moving cloud's fill is drawn in it so it hides what is behind the cloud. weather_icon_set = in place
// (same box): another condition swaps the parts, animate on / off; false = not a weather icon box.
lv_obj_t *weather_icon(lv_obj_t *parent, const char *name, ic::Size size, bool animate, lv_color_t ink, lv_color_t behind);
bool weather_icon_set(lv_obj_t *wx, const char *name, bool animate);
bool weather_icon_animated(lv_obj_t *wx);  // currently showing the moving parts (false = the static master)
const char *weather_icon_name(lv_obj_t *wx);  // the icon a weather icon box shows (nullptr = not one)
int weather_anim_bytes(int index, ic::Size size);  // flash of the animated parts of one icon@size (0 = none)
int weather_anim_parts(int index, ic::Size size);  // number of part masks
// Moon phase glyph (Facet moonIcon, drawn in code, README "Icons" > "Moon phase"): frac 0..1 through the synodic month
// (0 new, 0.25 first quarter, 0.5 full, 0.75 last quarter); disc moon-dark, lit part moon-lit, outline moon-edge
// (weather stroke: 2 px, 3 from 96); south = mirrored. A8 masks computed at the exact px on the device (lit part per
// phase, disc + outline once per size). moon_icon_set = a new phase redrawn in place.
lv_obj_t *moon_icon(lv_obj_t *parent, float frac, ic::Size size, bool south = false);
// the same drawn at any size in px (the Moon screen's hero, lay::moon_hero); masks made once per size
lv_obj_t *moon_icon_px(lv_obj_t *parent, float frac, int px, bool south = false);
bool moon_icon_set(lv_obj_t *moon, float frac, bool south = false);

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
// fades (Darren 2026-10-22, Guition): a device-level motion choice. Off = the screen change is instant (no Content
// rise) and the Bottom sheet's scrim snaps while its panel moves by translate only (no opacity), reduced mode too.
// Default: off in the 1x build (Guition, 480: large blended areas cost ~90 ms a frame there), on elsewhere; props
// "fades" (true / false) overrides it per display like reduced_motion (workbench, bench before / after).
bool fades(const lv_obj_t *o);
void set_fades(lv_display_t *d, bool on);
bool fades_default();
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
// Update path (Darren 2026-10-22, README "Update path"): every component takes new props IN PLACE and moves to them with
// its own animation, no rebuild. root = the one object build() made on its parent; p = the FULL new props (same shape
// build() gets; the component compares with what it shows). Return false when it can't (a structural prop changed:
// range, state, ...): the caller rebuilds. Updates are always silent (nobody touched it): fw::update() runs it inside
// fw::Quiet (emit / sound dropped) and the component makes its own animations silent too (no step ticks).
using Update = bool (*)(lv_obj_t *root, Props p);
struct Component {
  const char *id, *name, *section, *meta;  // meta = JSON: {"demo"?, "frame":[w,h] dp, "props":[...], "variants":[...]}
  Build build;
  Update update = nullptr;  // FW_UPDATE(id) { ... }; nullptr = no update path yet (always rebuilt)
};
int add(const Component &c);
int set_update(const char *id, Update u);  // FW_UPDATE registers through this
std::vector<Component> &reg();            // every registered component (registration order)
const Component *find(const char *id);    // nullptr = not in this build
void meta_json(std::string &out);         // [{id,name,section,meta}] as JSON
// clean `scr` (any LVGL screen) and build component `comp` on it with props JSON, theme light/dark + accent set
// index: the same call the workbench preview and the device firmware make. Returns "" or an error.
const char *render(lv_obj_t *scr, const char *comp, const char *props, bool light, int accent);
// the update path on a screen render() built: the same component, theme and accent -> its update(root, props) in place,
// silent. Returns "" (applied), "rebuild" (another component / theme / accent, no update path, or update() said no:
// call render()) or an error. reduced_motion in props is applied first. Device / HA side and WASM fw_update use this.
const char *update(lv_obj_t *scr, const char *comp, const char *props, bool light, int accent);
// a component holding another one (fw::find + build) updates it the same way: true = applied in place
inline bool update_child(const Component *c, lv_obj_t *root, Props p) { return c && c->update && root && c->update(root, p); }
// Refill (update path phase 2): a prop that changes a component's STRUCTURE (another kind, icon, layout, a part that
// comes or goes) builds its contents again inside the SAME root, so the host keeps its object and nothing flashes:
// fw::refill(root) deletes the children, the root's local styles, its running animations and its forced / state-style
// states (not PRESSED: a finger may be down), and keeps its user data, flags and event callbacks; the component's fill
// step then styles the root and makes the children again. A refill snaps (the new look at once, no fade); what moves
// (value, on, disabled / unavailable) animates through the component's own update and fw::st::set_state.
void refill(lv_obj_t *root);
// shape(root, sig): the component's structural props as one string (kind, icon, width, ...), kept on the root (lives
// and dies with it). Returns true when sig equals what was stored (the update can move in place) and stores sig.
// key: a component built on another's root (Picker row on the List row's) keeps its own string under its own key.
bool shape(lv_obj_t *root, const std::string &sig, const char *key = "");

// Demo state (Darren 2026-10-22): the hosts' demo reset animates a sample back to its starting props after an
// interaction. A demo whose replay ends where it started (a line drawing in again) says so, and the reset is skipped:
// fw::demo_away(obj, false) = this demo is at its preset, true = moved away (new readings, another value). Per screen:
// the screen is away while any of its demos is; demo_state(scr) = -1 nothing reported (hosts reset as before),
// 0 every reporting demo at its preset (skip the reset), 1 away. render() clears it; a deleted demo drops out.
void demo_away(lv_obj_t *demo, bool away);
int demo_state(lv_obj_t *scr);

// Events + sound (README "Events and sound"): a component reports what happened to the host, which decides what to
// do with it. fw::emit(obj, "tap", "primary Start") -> the workbench event log (device: ESP log); fw::sound(obj, "tap")
// -> the host plays that UI sound id (tokens/sound.json types: tap, error, toggle-on, ...) in the user's pack. The
// workbench plays it in the browser; the device has no audio yet (TODO: sound playback on the panels, logged for now).
using Sink = void (*)(lv_obj_t *obj, const char *kind, const char *name, const char *detail);  // kind = "event" | "sound"
void set_sink(Sink s);
void emit(lv_obj_t *obj, const char *name, const char *detail = "");
// a page listening to the real components inside `scope` (Icons page: the Animated / Static segmented, the phase
// slider): every emit from scope or below also calls cb (the nearest scope only; dropped when quiet). Gone with scope.
using Listener = void (*)(lv_obj_t *from, const char *name, const char *detail, void *user);
void listen(lv_obj_t *scope, Listener cb, void *user = nullptr);
// a live value for listeners only (not the event log / host): the Slider's "input" while the shown value moves
void notify(lv_obj_t *obj, const char *name, const char *detail = "");
void sound(lv_obj_t *obj, const char *id);
// a pitched sound: pos = where the value sits in its range (0..1), sent as the event detail ("0.420"); the host shifts
// the pitch by the pack's `track` semitones over the whole range (tokens/sound.json; 0 = not pitched). Step ticks use it.
void sound(lv_obj_t *obj, const char *id, float pos);
// silence (update path): while a Quiet lives, emit / sound / step ticks are dropped (nested ok). Synchronous only: a
// component's own animations started by an update must be silent themselves (e.g. the Dial's quiet sweep).
struct Quiet { Quiet(); ~Quiet(); };
bool quiet();
// Step ticks (Darren 2026-10-21), the one rule for every slider / dial / arc: call when the SHOWN value moves from
// step `from` to step `to` of `n` steps. Every step crossed counts; an audible "slider-tick" plays for a step only when
// the last audible tick is at least mo::ms::tick_min (motion.json "tick-min") ago, so a slow drag ticks on every single
// step and a fast one thins out to a steady patter, never more ticks than steps. A landmark step (ends of the range:
// tick_landmark()) plays "slider-landmark" instead and is never dropped: too soon after a tick, it waits out the
// interval (one lv_timer). One limiter for the whole display (one finger at a time).
// Pitch follows the value (Darren 2026-10-21): every tick and landmark carries its step's place in the range
// (step / n) as the sound position, so low values tick lower and high values higher (sound.json `track`).
void step_ticks(lv_obj_t *obj, int from, int to, int n);
// Same limiter, own pitch: pos(user, step) = the sound position 0..1 of each step (a chart scrub pitches its ticks
// by the line's height at the cursor, Darren 2026-10-22, instead of by step / n).
using TickPos = float (*)(void *user, int step);
void step_ticks(lv_obj_t *obj, int from, int to, int n, TickPos pos, void *user);
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
// In-place state changes for the update path: fw::st::set_state / set_off / swap (core/states.h)
#include "states.h"
// Atoms: the shared accent marks fw::focus_ring() + fw::corner_ticks() (core/atoms.h, after the state tokens they use)
#include "atoms.h"
// The mascot fw::mascot() (core/mascot.h; tokens in mascot_gen.h): moods, parts, keyframe motion
#include "mascot.h"

// FW_COMPONENT(id, "Name", "components", R"({...meta json...})") { ...build with `parent` and `p`... }
#define FW_COMPONENT(ID, NAME, SECTION, META)                                    \
  static void fw_build_##ID(lv_obj_t *parent, fw::Props p);                      \
  static int fw_reg_##ID = fw::add({#ID, NAME, SECTION, META, fw_build_##ID});   \
  static void fw_build_##ID(lv_obj_t *parent, fw::Props p)
// FW_UPDATE(id) { ...apply `p` to `root` in place, return true; false = rebuild... } after the FW_COMPONENT block
#define FW_UPDATE(ID)                                                            \
  static bool fw_update_##ID(lv_obj_t *root, fw::Props p);                      \
  static int fw_upd_##ID = fw::set_update(#ID, fw_update_##ID);                  \
  static bool fw_update_##ID(lv_obj_t *root, fw::Props p)
