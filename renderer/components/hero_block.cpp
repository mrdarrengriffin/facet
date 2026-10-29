// Hero block: the Facet hero block (docs: @facet/ui layout.ts heroBlock({label, role}) + set({n, label, line, accent}),
// CSS .fs-hero), on the contract (renderer/CONTRACT.md, "contract": 2). A screen's one hero: a big number with a caps
// label over one line to its right. Accent = it matters now. One per screen (design principles).
//   box      fills its parent's width (C5; in a Body that is the content width, = List row / Section header), as tall
//            as the number's type box (hero 96 / 64; role display 144 / 96): hero-row(-display), a row, top-aligned,
//            the gutter apart (16)
//   number   type hero (or display), text; accent = accent-text (docs .is-accent); its box on the grid (fx::fit_grid,
//            docs fit8), so the column after it starts on the grid. unit: after it (type title, text-dim, space 2
//            apart, its figures on the number's baseline: baseline:hero:title); the number + unit box is what fits the
//            grid. count: a number (an optional prefix such as £, then digits) counts up from 0 when shown and from
//            the shown value on an update (Motion value, the Big number's count, which this replaces: 2026-10-28);
//            its box is fitted at the final value first, so the count never moves the column; reduced = at once
//   column   hero-col (the free width): label (type label, caps, text) over the line (type body, text-dim, one line,
//            "..."), space 2 apart, centred on the hero type box ((96 - (24 + 8 + 32)) / 2 = 16 = the docs padding-top
//            16 at 720, 12 at 480; a derived style, no literal). With role display it stays on the hero box offset
//            too, so the label / line read at the same height whichever number style is used.
//            label_type title (the Calendar hero, docs .fs-cal-htitle): hero-col-title, the label in title type as
//            written (not caps), text ((96 - (40 + 8 + 32)) / 2 = 8 = the docs padding-top 8)
// States: default; accent; unavailable (fw::st::OFF; the hero shows a device's state): number = the placeholder em
// dash in unavailable-ink, no accent, line = "Unavailable" in unavailable-note. n/a: pressed, on, disabled, loading,
// unknown, empty, error (a read-out: no press, no events, no sound, docs). Update path: the contents again in the same
// root, colours fading like a tap (fw::st::refill_fade, shared styles), silent. Bleed: none.
// Contrast pairs: tokens/colour.json section "hero_block".
#include "fw.h"
#include "fx.h"
#include <cmath>
#include <cstdlib>
#include <string>

namespace {
// the count: kept on the root (a DELETE event's user data), so it lives through the update path's refill
struct Count { lv_obj_t *num = nullptr; std::string prefix, type; int dec = 0; double shown = 0, target = 0; bool on = false; };
void count_free(lv_event_t *e) { delete (Count *) lv_event_get_user_data(e); }
Count *count_of(lv_obj_t *h) {
  for (uint32_t i = 0, n = lv_obj_get_event_count(h); i < n; i++) {
    lv_event_dsc_t *d = lv_obj_get_event_dsc(h, i);
    if (lv_event_dsc_get_cb(d) == count_free) return (Count *) lv_event_dsc_get_user_data(d);
  }
  return nullptr;
}
// "£2.42" -> prefix "£", 2.42, 2 decimals; false = not a number (counted text stays as written)
bool parse(const char *s, std::string *prefix, double *v, int *dec) {
  const char *d = s;
  while (*d && !(*d >= '0' && *d <= '9') && !(*d == '-' && d[1] >= '0' && d[1] <= '9')) d++;
  if (!*d) return false;
  char *end = nullptr;
  const double x = strtod(d, &end);
  if (end == d || *end || !std::isfinite(x)) return false;
  const char *dot = strchr(d, '.');
  *prefix = std::string(s, d - s), *v = x, *dec = dot ? (int) (end - dot - 1) : 0;
  return true;
}
void count_show(void *o, int32_t k) {  // k = 0..1024 of the way from the shown value to the target
  Count *c = count_of((lv_obj_t *) o);
  if (!c || !c->num) return;
  const double v = c->shown + (c->target - c->shown) * k / 1024.0;
  char b[48];
  snprintf(b, sizeof b, "%s%.*f", c->prefix.c_str(), c->dec, v);
  if (strcmp(b, lv_label_get_text(c->num))) fx::set_text(c->num, b, c->type.c_str());
  if (k == 1024) c->shown = c->target;
}
void fill(lv_obj_t *h, fw::Props p, bool opening) {
  const bool off = !strcmp(p["state"] | "default", "unavailable");
  const bool accent = !off && (p["accent"] | false);
  const bool display = !strcmp(p["role"] | "hero", "display");
  fx::use(h, "box");  // again after an update's refill (it takes every style off the root): the full width
  // layout stack (the Timer's ring, Darren 2026-10-28): label over the number over the line, centred, in one column
  const bool stack = !strcmp(p["layout"] | "row", "stack");
  fx::use(h, stack ? "hero-stack" : display ? "hero-row-display" : "hero-row");
  const bool title0 = !strcmp(p["label_type"] | "label", "title");
  if (stack) fx::text(h, p["label"] | "", title0 ? "type:title" : "type:label", off ? "fg:unavailable-ink" : "fg:text-dim");
  const char *n = off ? fw::st::PLACEHOLDER : (p["n"] | "");
  // role value (2026-10-29): the smallest, for a hero inside a small box (the compact Timer ring's hole)
  const char *type = display ? "type:display" : !strcmp(p["role"] | "hero", "value") ? "type:value" : "type:hero";
  const char *unit = off ? "" : (p["unit"] | "");
  lv_obj_t *line = h;
  if (*unit) {  // number + unit in one box on the grid
    line = fx::box(h, "row-top");
    fx::use(line, "hug");
    fx::use(line, "gap:2");
  }
  lv_obj_t *num = fx::text(line, n, type, off ? "fg:unavailable-ink" : accent ? "fg:accent-text" : "fg:text");
  if (*unit) {
    fx::use(fx::text(line, unit, "type:title", "fg:text-dim"), display ? "baseline:display:title" : "baseline:hero:title");
    fx::fit_grid(line);
  } else fx::fit_grid(num);  // fit8: the box on the grid, the text at its left
  Count *c = count_of(h);
  if (p["count"] | false) {
    if (!c) { c = new Count; lv_obj_add_event_cb(h, count_free, LV_EVENT_DELETE, c); }
    std::string pre;
    double v;
    int dec;
    const bool was = c->on;
    c->on = !off && parse(n, &pre, &v, &dec);
    if (c->on) {
      c->num = num, c->prefix = pre, c->dec = dec, c->type = type;
      if (!was) c->shown = 0;  // shown for the first time (or a number again): from 0
      c->target = v;
      if (!opening && !was) c->shown = v;
      if (c->shown != c->target) { count_show(h, 0); fw::animate(h, fw::st::SWEEP, {count_show, 0, 1024, fw::mo::VALUE}); }
    }
  } else if (c) c->on = false;
  // label type: caps label (default) or title (the Calendar hero: the event's name as written, docs .fs-cal-htitle)
  const bool title = title0;
  if (stack) {  // the line under the number, centred (the label is above it)
    fx::text(h, off ? fw::st::UNAVAILABLE_TEXT : (p["line"] | ""), "type:body", off ? "fg:unavailable-note" : "fg:text-dim");
    if (off) fx::state(h, fw::st::OFF, true);
    return;
  }
  lv_obj_t *col = fx::box(h, "free");
  fx::use(col, title ? "hero-col-title" : "hero-col");
  fx::use(fx::text(col, p["label"] | "", title ? "type:title" : "type:label", off ? "fg:unavailable-ink" : "fg:text"), "fill-w");
  fx::use(fx::text(col, off ? fw::st::UNAVAILABLE_TEXT : (p["line"] | ""), "type:body", off ? "fg:unavailable-note" : "fg:text-dim"), "fill-w");
  if (off) fx::state(h, fw::st::OFF, true);  // the unavailable look (3:1)
}
}  // namespace

FW_COMPONENT(hero_block, "Hero block", "components", R"({
  "about": "A screen's one big number, with a label and one line beside it; accent when it matters now.",
  "contract": 2,
  "parts": [],
  "min": ["layout:card-min-w"],
  "frame": [480, 112],
  "props": [
    {"name": "n", "about": "The big number, such as how many things need you.", "type": "text", "default": "3"},
    {"name": "unit", "about": "A unit shown after the number, such as kWh or °C. Leave empty for none.", "type": "text", "default": ""},
    {"name": "count", "about": "Counts the number up from 0 when it first shows, and from the old value when it changes.", "type": "bool", "default": false},
    {"name": "label", "about": "The words next to the number that say what it counts.", "type": "text", "default": "need you"},
    {"name": "line", "about": "One quieter line with the detail, under the label.", "type": "text", "default": "2 reviews · 2 assigned"},
    {"name": "accent", "about": "Draws the number in the accent colour, for when it matters now.", "type": "bool", "default": true},
    {"name": "role", "about": "How big the number is: display is the largest, hero a step smaller for a screen's main figure, value smaller again for a hero inside a small space such as the compact Timer ring.", "type": "select", "options": ["hero", "display", "value"], "default": "hero"},
    {"name": "layout", "about": "row puts the label and line beside the number; stack puts the label above it and the line under it, centred, such as inside a ring.", "type": "select", "options": ["row", "stack"], "default": "row"},
    {"name": "label_type", "about": "How the words next to the number are written: label in small capitals, or title in larger plain text.", "type": "select", "options": ["label", "title"], "default": "label"},
    {"name": "state", "about": "default shows the number; unavailable shows a dash when the value cannot be read.", "type": "select", "options": ["default", "unavailable"], "default": "default"}
  ],
  "variants": [
    {"label": "accent · needs you", "props": {}},
    {"label": "quiet", "props": {"n": "12", "label": "open", "line": "3 drafts · 1 waiting on CI", "accent": false}},
    {"label": "money", "props": {"n": "£2.42", "label": "today", "line": "8.4 kWh · cheaper than yesterday", "accent": false}},
    {"label": "display role", "props": {"n": "21", "label": "inside", "line": "Feels like 19 · heating off", "accent": false, "role": "display"}},
    {"label": "title label · calendar", "props": {"n": "16:50", "label": "Pottery class", "line": "today · Larkfield studio · Family", "accent": false, "label_type": "title"}},
    {"label": "long line", "props": {"n": "7", "label": "strikes", "line": "nearest 14 km north-east, moving away at 30 km/h", "accent": true}},
    {"label": "unavailable", "props": {"n": "21", "label": "inside", "line": "", "accent": false, "state": "unavailable"}},
    {"label": "unit · counts up", "props": {"n": "8.69", "unit": "kWh", "label": "today · so far", "line": "cheaper than yesterday", "accent": false, "count": true}},
    {"label": "£ · counts up", "props": {"n": "£2.43", "label": "cost today", "line": "8.4 kWh", "accent": false, "count": true}}
  ],
  "opening": "With count on, showing it counts the number up from 0 (Motion value)."
})") {
  lv_obj_t *h = fx::box(parent, "free");
  fx::name(h, "hero_block");
  fill(h, p, p["opening"] | true);
}

// Update path: number, label, line, accent, role and unavailable in the same root (fw::refill), colours fading like a
// tap (shared styles), silent.
FW_UPDATE(hero_block) {
  if (Count *c = count_of(root)) {  // the count goes on from the value on screen (a count still running stops there)
    std::string pre;
    int dec;
    if (c->num && c->on) parse(lv_label_get_text(c->num), &pre, &c->shown, &dec);
    lv_anim_delete(root, count_show);
    c->num = nullptr;
  }
  fw::st::refill_fade(root, [&] { fw::refill(root); fill(root, p, false); }, true, true);
  return true;
}
