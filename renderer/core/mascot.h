// The mascot (tokens/mascot.json -> mascot_gen.h fw::mas::, README "Mascot tokens"; docs @facet/ui mascot.ts): a
// body with two eyes; the mood shows only in small geometry and motion. Every part is a tiny LVGL object (body, eye
// group, eyes, brows, "!", z lines, dots) and every motion is a keyframe track on its x, y, height, opacity or
// rotation, so a frame redraws a few dozen pixels. Included at the end of fw.h. The Mascots foundation page
// (foundations/mascot.cpp) and the mascot_mood component use it.
#pragma once
#include "fw.h"  // self-contained: ESPHome includes every component header on its own (alphabetical), fw.h includes this one at its end

namespace fw {

struct MascotOpts {
  const char *colour = nullptr;  // a colour.json token name instead of the mood's (a tile's own colour), nullptr = the mood's
  int delay = -1;                // ms into the desynced loops; -1 = n x mas::INSTANCE_OFFSET_MS (n = live mascots before it)
  int scale = 1;                 // every part and amplitude x scale (the foundation page's 4x view), 1 on screens
  uint32_t only = 0;             // play only these patterns (bit per mas::Pat) instead of the mood's (foundation page)
};
// a mascot in `mood` (its box = the body, mas::G.body_w x body_h; "!", z and dots draw outside it via fw::bleed, so
// create it in its final parent). Arriving as done plays the hop. Reduced motion: loops stop on their rest frame.
lv_obj_t *mascot(lv_obj_t *parent, mas::Mood mood, const MascotOpts &o = {});
// change the mood (and colour override); becoming done plays the hop
void mascot_set(lv_obj_t *m, mas::Mood mood, const char *colour = nullptr);
// restart its patterns from the start of their loop (+ the hop when done)
void mascot_replay(lv_obj_t *m);
mas::Mood mascot_mood(const char *id);  // by name; unknown = idle
int mascot_reach(int scale = 1);        // px the extras + motion reach outside the body box (the bleed)

}  // namespace fw
