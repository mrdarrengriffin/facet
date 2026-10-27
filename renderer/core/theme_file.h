// theme_file.h: theme files (renderer/CONTRACT.md section 10, Appendix A) -> an immutable compiled theme, the Skin.
// Core only: components never include this (C44, lint `theme`); the core applies a Skin (fx::look), a component only
// names tokens and parts. No LVGL here: the parser and its guards are pure C++ (fuzzed natively under ASan).
//
//   load = read (strict JSON) -> phase 1: structure, names, ranges into a scratch model (every error collected, with
//   a path) -> phase 2: compile (presets, inheritance from Facet, state merge, lengths to px, palettes per mode x
//   accent) -> the static guards (contrast on RGB565, press feedback, geometry). Only a fully compiled, fully guarded
//   Skin is returned; anything else returns null and the errors. A Skin never changes once made, and the live one is
//   swapped by pointer (theme_apply), so a refused file can never be partly applied.
#pragma once
#include <stddef.h>
#include <stdint.h>
#include <memory>
#include <string>
#include <vector>

namespace fx {

struct ThemeError {
  std::string path, msg;  // "parts.card.pressed.edges[1].side", "\"diagonal\" is not a side (all, top, ...)"
  int line = 0;           // 1-based line in the file the error is at (0 = the file as a whole)
};

namespace skin {
// sizes of the generated schema (tools/gen_theme.mjs -> theme_gen.h); theme_file.cpp static_asserts they match, so a
// token added to the schema fails the build here until this header follows
constexpr int N_COLOURS = 90, N_ROLES = 5, N_SIZES = 193, N_TYPES = 7, N_FONT_ROLES = 4, N_PARTS = 21, N_STATES = 4,
              N_COMBOS = 1 << N_STATES, MAX_SETS = 8, MAX_EDGES = 128, MAX_BUTTONS = 8;
// states (tokens/theme.json, merge order): bit i of a combination = state i
enum State : uint8_t { ON = 1, FOCUS = 2, PRESSED = 4, DISABLED = 8 };
// an ink: a colour token, an accent role, or a fixed colour (both modes)
enum InkKind : uint8_t { INK_NONE, INK_TOKEN, INK_ROLE, INK_HEX };
struct Ink {
  uint32_t kind : 8, value : 24;  // InkKind; INK_TOKEN: colour index, INK_ROLE: accent role index, INK_HEX: 0xRRGGBB
  Ink() : kind(INK_NONE), value(0) {}
};
// a length: px at [1.5x, 1x]; cells > 0 = it was given in cells of stroke:pixel (snapped drawing, C29)
struct Len { int16_t px[2] = {0, 0}; int8_t cells = 0; uint8_t set = 0; };
enum Fill : uint8_t { FILL_SOLID, FILL_GRADIENT, FILL_CHECKER, FILL_BLOCKS };
struct Face { uint8_t fill = FILL_SOLID, down = 0, pad_[2] = {0, 0}; Ink a, b; Len run, gap; };
enum Side : uint8_t { SIDE_BOTTOM = 1, SIDE_TOP = 2, SIDE_LEFT = 4, SIDE_RIGHT = 8, SIDE_ALL = 15, SIDE_TL = 16, SIDE_BR = 32 };
struct Edge { uint8_t side = SIDE_ALL, dotted = 0, pad_[2] = {0, 0}; Len w, inset; Ink ink; };
struct Shadow { Len dx, dy; Ink ink; };
// key bits (tokens/theme.json keys, generated order): what a block sets; a key not set = the component's own look
enum Key : uint16_t { K_FACE = 1, K_RADIUS = 2, K_PAD = 4, K_EDGES = 8, K_SHADOW = 16, K_TEXT = 32, K_WEIGHT = 64,
                      K_INK = 128, K_SHAPE = 256, K_BUTTONS = 512, K_W = 1024, K_LANE = 2048, K_ARROWS = 4096 };
struct Block {
  uint16_t has = 0;           // Key bits
  uint8_t bold = 0, shape = 0;  // weight; knob / tick shape (index into the knob or tick shape list)
  Face face;
  Len radius, pad;
  Ink text, ink;
  Shadow shadow;
  uint16_t edge0 = 0;  // edges: Skin::edges[edge0 .. edge0 + n_edges)
  uint8_t n_edges = 0, pad_ = 0;
};
// the header's decorative caption buttons and the scrollbar's extras (base block only)
struct Buttons { uint8_t names[MAX_BUTTONS] = {}; uint8_t n = 0, pad_[3] = {0, 0, 0}; Len w, h, gap; };
struct Scrollbar { uint8_t arrows = 0, has_lane = 0, pad_[2] = {0, 0}; Len w; Face lane; };

struct Skin {
  char id[33], label[193];  // id: 1-32 of a-z 0-9 -; label: up to 48 characters (UTF-8)
  uint8_t modes;   // bit 0 dark, bit 1 light
  uint8_t n_sets;  // accent sets
  char set_id[MAX_SETS][33], set_label[MAX_SETS][193];
  uint32_t palette[2][MAX_SETS][N_COLOURS];  // 0xRRGGBB per [mode][accent set][colour token] (accent roles = the set's)
  int16_t size[2][N_SIZES];                  // every size token at [1.5x, 1x] (layout ones are Facet's, C18)
  int16_t type[2][N_TYPES];                  // the themeable type styles' px
  uint8_t font[N_FONT_ROLES];                // family index per role
  uint8_t corner, cap, knob, ticks, card, indicator, icons, snap;  // shape.* (index into each list; 0 = Facet's)
  Block state[N_PARTS][1 + N_STATES];  // each part's blocks as the theme says (presets in, lengths in px): [0] base, [1 + i] state i
  Edge edges[MAX_EDGES];
  uint16_t n_edges;
  Buttons buttons;
  Scrollbar scrollbar;
  uint32_t checks;  // guard checks this Skin passed

  int mode(int light) const { return (modes >> (light ? 1 : 0)) & 1 ? (light ? 1 : 0) : (light ? 0 : 1); }  // a one-mode theme keeps its mode
  int set(int i) const { return i >= 0 && i < n_sets ? i : 0; }                                             // an accent index out of range = the first set
  uint32_t rgb(int light, int set_i, int colour) const { return palette[mode(light)][set(set_i)][colour]; }
  uint32_t ink(const Ink &k, int light, int set_i) const;  // an ink's 0xRRGGBB (INK_NONE = 0)
  // a part's look in a combination of states (State bits): base <- on <- focus <- pressed <- disabled for the bits set.
  // The apply path attaches one per combination (LVGL takes each property from the matching style with the largest
  // state, so only the exact combination's block is always right); the Pen merges the same way at draw time.
  Block merged(int part, int states) const;
  uint64_t digest() const;                                  // FNV-1a over the whole Skin: equal digests = equal themes
};
}  // namespace skin
using Skin = skin::Skin;

// Load a theme file: the compiled Skin, or null with `err` filled (never both). `err` is cleared first.
std::shared_ptr<const Skin> theme_load(const char *json, size_t n, std::vector<ThemeError> &err);
// Facet itself (the generated defaults, no file read): always valid (tools/theme_check.mjs proves it at build time)
std::shared_ptr<const Skin> theme_facet();
// the built-in themes (themes/*.json, embedded at build time; facet first)
int theme_builtins();
const char *theme_builtin_id(int i);
const char *theme_builtin_json(int i);

// the live theme: theme_apply loads and, only if the load succeeds, swaps it in and bumps the generation (the apply
// path repaints when the generation changes: never by pointer identity, a freed Skin's address can be reused).
// A refused file leaves the live Skin and the generation exactly as they were. The slot is swapped atomically (a
// reader on another task sees the old Skin or the new one, never a torn one); loads run on the caller's task.
std::shared_ptr<const Skin> theme_live();
uint32_t theme_generation();
bool theme_apply(const char *json, size_t n, std::vector<ThemeError> &err);
void theme_reset();  // back to Facet (a new generation)

// One contrast check the guard ran (tools/contrast.mjs --theme, the workbench Guard panel). The guard and this list
// are one code path: theme_load refuses exactly the rows with pass == false.
struct ContrastRow {
  std::string where, fg, bg, kind;  // where = "card" (a declared pair's section) or "parts.card.pressed.face"
  int light = 0, set = 0;
  double exact = 0, panel = 0, min = 0;  // ratio on the exact values, on RGB565 values; the worse decides
  bool pass = true;
};
void theme_contrast(const Skin &s, std::vector<ContrastRow> &rows);

#ifndef ESP_PLATFORM
// Break-it switch (WASM and native tests only, never on a panel): when on, theme_apply puts a refused file's partial
// result live, as the prototype parser did. The never-partial-apply proof must then go red.
void theme_break_partial(bool on);
#endif
// the most memory the last theme_load held at once (scratch + the Skin it built), for the memory budget (S6)
size_t theme_peak_bytes();
}  // namespace fx
