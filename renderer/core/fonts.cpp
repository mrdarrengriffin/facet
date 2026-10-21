// The panel's generated Facet fonts for this density ($FW_PANEL/include/fonts_gen*.h, see README),
// in their own translation unit so a component change never recompiles them. fw::font(dp) = nearest size.
#include <cmath>
#include "fw.h"
#ifdef FW_1X
#include "fonts_gen_1x.h"
#else
#include "fonts_gen.h"
#endif
using namespace panel::gen;

namespace {
struct F { int px; bool mono; FacetFont *f; };
#ifdef FW_1X
F all[] = {{12, false, &F_sans_12m}, {13, false, &F_sans_13m}, {16, false, &F_sans_16m}, {17, false, &F_sans_17m},
           {20, false, &F_sans_20m}, {21, false, &F_sans_21m}, {28, false, &F_sans_28}, {44, false, &F_sans_44l},
           {64, false, &F_sans_64l}, {96, false, &F_sans_96l}, {12, true, &F_mono_12m}, {13, true, &F_mono_13b},
           {15, true, &F_mono_15b}};
#else
F all[] = {{16, false, &F_sans_16}, {20, false, &F_sans_20}, {22, false, &F_sans_22}, {26, false, &F_sans_26},
           {28, false, &F_sans_28}, {30, false, &F_sans_30}, {32, false, &F_sans_32}, {40, false, &F_sans_40l},
           {64, false, &F_sans_64e}, {96, false, &F_sans_96e}, {144, false, &F_sans_144e}, {12, true, &F_mono_12m},
           {14, true, &F_mono_14}, {18, true, &F_mono_18}, {22, true, &F_mono_22m}, {24, true, &F_mono_24m},
           {26, true, &F_mono_26l}};
#endif
}  // namespace

const lv_font_t *fw::font(float dp, bool mono) {
  const float want = dp * D;
  F *best = nullptr;
  for (auto &f : all)
    if (f.mono == mono && (!best || std::fabs(f.px - want) < std::fabs(best->px - want))) best = &f;
  return best ? best->f->get() : LV_FONT_DEFAULT;
}
