// Facet fonts for this density, generated from tokens/type.json (tools/gen_fonts.py -> facet_fonts*.h, README "Type
// tokens"), in their own translation unit so a component change never recompiles them.
#include <cmath>
#include "fw.h"
#ifdef FW_1X
#include "facet_fonts_1x.h"
#else
#include "facet_fonts.h"
#endif

// Large text (Settings > Text size, tokens/type.json "large"): the styles that have one use their bigger font in the
// same line box while it is on (set per build: fw::render props "text_size", the device runtime's setting)
namespace {
bool large_now = false, large_dflt = false;
}
void fw::set_text_large(bool on) { large_now = on; }
bool fw::text_large() { return large_now; }
void fw::set_text_large_default(bool on) { large_dflt = on; }
bool fw::text_large_default() { return large_dflt; }
const lv_font_t *fw::font(ty::Id s) {
  if (s < 0 || s >= ty::N) return LV_FONT_DEFAULT;
  if (large_now && gen::LARGE_FONT[s]) return gen::LARGE_FONT[s]->get();
  return gen::STYLE_FONT[s]->get();
}

// nearest full-text font in px to `dp` (sans or mono): for quick labels that are not a type style (prefer fw::text)
const lv_font_t *fw::font(float dp, bool mono) {
  const float want = dp * D;
  const gen::FontInfo *best = nullptr;
  for (auto &f : gen::FONTS)
    if (f.mono == mono && !f.tabular && (!best || std::fabs(f.size - want) < std::fabs(best->size - want))) best = &f;
  return best ? best->f->get() : LV_FONT_DEFAULT;
}

int fw::n_fonts() { return gen::N_FONTS; }
int fw::font_bytes() { return gen::FONT_BYTES; }
fw::FontRef fw::font_ref(int i) {
  if (i < 0 || i >= gen::N_FONTS) return {};
  const gen::FontInfo &f = gen::FONTS[i];
  return {f.id, f.family, f.weight, f.size, f.mono, f.tabular, f.glyphs, f.bytes, f.f->get()};
}
int fw::style_font_index(ty::Id s) {
  for (int i = 0; i < gen::N_FONTS; i++)
    if (gen::FONTS[i].f == gen::STYLE_FONT[s]) return i;
  return -1;
}
uint32_t fw::font_glyph(int i, int g) {
  if (i < 0 || i >= gen::N_FONTS || g < 0 || g >= gen::FONTS[i].glyphs) return 0;
  return gen::FONTS[i].f->glyphs[g].cp;
}
