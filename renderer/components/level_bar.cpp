// Level bar: how full something is, as a straight bar, on the contract (renderer/CONTRACT.md, "contract": 2). A
// drawn strip (fx::strip, the Pen): its parent's width (C5), layout:level-bar-h tall (three space-2 tracks). The track
// (fg:line) space 2 thick in the middle third, round ends; the level from the left in its tone, round ends, at least
// one track-thickness long when known; an optional mark (a threshold: water below here) = a stroke:tick wide notch in
// fg:text-dim, the full height, so every edge is on the grid.
//   value  0..100 (-1 = unknown: no level)
//   mark   0..100, -1 = none
//   tone   accent (needs you) | steel (fine) | blue (too much) | off (fg:text-mute)
// A caller that wants a short bar gives it a box that wide (the Plant row: layout:level-bar-w).
// A new tone sweeps in from the left over the old one (Motion value: the new ink's run grows to the level), so a
// tone change animates like a tap instead of flicking (the legacy bar faded it).
// Static, not a control: no press, no events, no sound. States: the tones; n/a pressed, on, disabled. Update path: the
// level glides (Motion value, silent); a new tone sweeps in (above); a new mark repaints in place. Bleed: none. Inks are
// read at draw time (a theme switch repaints it). Contrast: tokens/colour.json section "level_bar".
#include <cmath>
#include "fw.h"
#include "fx.h"

namespace {
// shown: the level now (0-1000, -10 = none); was = the tone being swept over, sweep = how far the new tone has
// come (0-1000 of the level's run)
struct LB { int value = -1, mark = -1, shown = -10, sweep = 1000; std::string tone = "steel", was = "steel"; };
LB *of(lv_obj_t *o) { return (LB *) lv_obj_get_user_data(o); }
const char *ink(const std::string &t) { return t == "accent" ? "fg:accent" : t == "blue" ? "fg:blue" : t == "off" ? "fg:text-mute" : "fg:steel"; }
void draw(const fx::Pen &p) {
  const LB *b = of(p.obj);
  if (!b) return;
  const int T = fx::size("space:2"), x0 = -p.w / 2, y0 = -p.h / 2;
  p.rect(x0, y0 + T, p.w, T, "fg:line", T / 2);
  if (b->shown >= 0) {
    const int lw = std::max(T, (int) std::lround(p.w * b->shown / 1000.0));
    const int run = std::min(lw, p.w), nw = b->sweep >= 1000 ? run : (int) std::lround(run * b->sweep / 1000.0);
    if (nw < run) p.rect(x0, y0 + T, run, T, ink(b->was), T / 2);
    if (nw > 0) p.rect(x0, y0 + T, std::max(nw, std::min(T, run)), T, ink(b->tone), T / 2);
  }
  if (b->mark >= 0) {
    const int k = fx::size("stroke:tick");
    p.rect(x0 + (int) std::lround(p.w * b->mark / 100.0) - k / 2, y0, k, p.h, "fg:text-dim");
  }
}
void show(void *o, int32_t v) {
  auto *s = (lv_obj_t *) o;
  LB *b = of(s);
  if (!b || b->shown == v) return;
  b->shown = v;
  lv_obj_invalidate(s);
}
void sweep(void *o, int32_t v) {
  auto *s = (lv_obj_t *) o;
  LB *b = of(s);
  if (!b || b->sweep == v) return;
  b->sweep = v;
  lv_obj_invalidate(s);
}
void take(LB *b, fw::Props p) {
  const auto num = [&](const char *k, double d) {
    double v = p[k].is<const char *>() ? atof(p[k].as<const char *>()) : (p[k] | d);  // binds give doubles or strings
    return std::isfinite(v) ? (int) std::lround(v) : (int) d;
  };
  const int v = num("value", -1), m = num("mark", -1);
  b->value = v < 0 ? -1 : LV_CLAMP(0, v, 100);
  b->mark = m < 0 ? -1 : LV_CLAMP(0, m, 100);
  b->tone = p["tone"] | "steel";
}
}  // namespace

FW_COMPONENT(level_bar, "Level bar", "components", R"({
  "about": "How full something is as a straight bar, with an optional threshold mark: soil moisture, a tank, a battery.",
  "contract": 2,
  "parts": [],
  "min": ["layout:level-bar-w", "layout:level-bar-h"],
  "frame": [320, 0],
  "props": [
    {"name": "value", "about": "How full the bar is, from 0 to 100. -1 means the level is not known and shows an empty bar.", "type": "range", "min": -1, "max": 100, "step": 1, "default": 44},
    {"name": "mark", "about": "Where the threshold line sits, from 0 to 100, such as the point a plant needs water. -1 hides it.", "type": "range", "min": -1, "max": 100, "step": 1, "default": 30},
    {"name": "tone", "about": "The fill colour: accent, steel, blue, or off for a grey fill.", "type": "select", "options": ["accent", "steel", "blue", "off"], "default": "steel"}
  ],
  "variants": [
    {"label": "fine", "props": {}},
    {"label": "needs you", "props": {"value": 18, "tone": "accent"}},
    {"label": "too much", "props": {"value": 92, "mark": -1, "tone": "blue"}},
    {"label": "unknown", "props": {"value": -1, "tone": "off"}}
  ]
})") {
  auto *b = new LB;
  lv_obj_t *s = fx::strip(parent, draw, "fill-w");
  fx::name(s, "level_bar");
  for (const char *t : {"h:level-bar-h", "max-h:level-bar-h"}) fx::use(s, t);
  lv_obj_set_user_data(s, b);
  lv_obj_add_event_cb(s, [](lv_event_t *e) { delete (LB *) lv_event_get_user_data(e); }, LV_EVENT_DELETE, b);
  take(b, p);
  b->was = b->tone;
  b->shown = b->value < 0 ? -10 : b->value * 10;
}

FW_UPDATE(level_bar) {
  LB *b = of(root);
  if (!b) return false;
  const LB old = *b;
  take(b, p);
  if (old.mark != b->mark) lv_obj_invalidate(root);
  if (old.tone != b->tone) {  // the new tone sweeps in over the one shown now
    lv_anim_delete(root, (lv_anim_exec_xcb_t) sweep);
    b->was = old.sweep >= 500 ? old.tone : old.was;  // mid-sweep: over whichever covers more of the run
    b->sweep = 0;
    if (b->shown < 0) b->sweep = 1000;  // no level: nothing to sweep
    else fw::animate(root, fw::mo::value, {sweep, 0, 1000, fw::mo::VALUE});
    lv_obj_invalidate(root);
  }
  if (old.value != b->value) {
    lv_anim_delete(root, (lv_anim_exec_xcb_t) show);
    if (b->value < 0 || b->shown < 0) show(root, b->value < 0 ? -10 : b->value * 10);  // to / from unknown: at once
    else fw::animate(root, fw::mo::value, {show, b->shown, b->value * 10, fw::mo::VALUE});
  }
  return true;
}
