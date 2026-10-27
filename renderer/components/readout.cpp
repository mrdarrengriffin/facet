// State readout: the Facet state readout (docs: Screen parts > State readout, @facet/ui readouts.ts readout() /
// readoutSlot() / readoutFrom()), on the contract (renderer/CONTRACT.md, "contract": 2): a Home item that reports a
// state and opens nothing (someone in a room, a door open, one temperature, who is home). The rule: a box is a button,
// no box is a readout: on the canvas, no fill, no press, no sound, no chevron, a tap does nothing. It fills its parent
// (meta min layout:card-min-w x pair-h); two share one Home card cell (the Cells layout's pair: equal halves, a
// hairline between them inset by the card padding).
//   one     padding readout-pad (card-pad from the top and sides, none below: the half is taller than its content),
//           a column card-pad apart:
//           the top row (at least the label box tall: the icon at icon size text-dim, the caps label (type label,
//           text-dim, its box on the grid), space 2 apart; the Status dot (working) after it, shown only when the
//           state matters (accent)), then the state (type title) and the detail (type body, text-dim, "...") centred on
//           one line, space 3 apart (= the docs' shared baseline: the body box sits half the difference up)
//   tones   text (default) · accent = it matters (accent-text + the dot) · warn = out of range · mute = offline
//           (the unavailable ink; the label row too)
// Live (a Home sensor card, screen model piece 2): prop label set = one readout from its own props (icon, label,
// state, detail, tone). Else the workbench pair: two docs readoutFrom() samples (a mock house) split like the Cells
// pair (fx::pair_split), the rule between in part:rule.
// States: the tones above; mute = fw::st::OFF on that readout (offline is unavailable). n/a: pressed, on, disabled (no tap), loading, unknown, empty, error (the binding maps those
// to mute). Update path: texts in place by a fade (fw::st::refill_fade with shared styles, like a tap would), silent.
// Bleed: none. Contrast: "readout".
#include "fw.h"
#include "fx.h"

namespace {
struct Ro { const char *id, *icon, *label, *state, *detail, *tone; };
constexpr Ro SAMPLES[] = {
    {"occupancy · on", "person", "Study", "Occupied", "for 12 min", "text"},
    {"occupancy · off", "person", "Study", "Clear", "for 3 h", "text"},
    {"door · open", "door", "Garden door", "Open", "for 4 min", "accent"},
    {"door · closed", "door", "Garden door", "Closed", "since Mon", "text"},
    {"window · open", "window", "Bedroom window", "Open", "just now", "accent"},
    {"temperature", "thermometer", "Hallway", "21.4°", "", "text"},
    {"temperature · cold", "thermometer", "Hallway", "16.2°", "too cold", "warn"},
    {"home · everyone", "house", "Home", "Everyone in", "", "text"},
    {"home · some", "house", "Home", "2 home", "Alex, Sam", "text"},
    {"moisture · wet", "alert", "Utility", "Wet", "for 2 min", "warn"},
    {"offline", "door", "Back door", "Offline", "since Mon", "mute"},
};
const Ro *find_ro(const char *id) {
  for (const Ro &r : SAMPLES) if (!strcmp(r.id, id)) return &r;
  return nullptr;
}
// one readout in o (a box that fills its parent)
void one(lv_obj_t *o, const Ro &r) {
  const bool mute = !strcmp(r.tone, "mute"), accent = !strcmp(r.tone, "accent");
  const char *dim = mute ? "fg:unavailable-ink" : "fg:text-dim";
  for (const char *t : {"fill", "readout-pad", "gap:card-pad"}) fx::use(o, t);
  lv_obj_t *top = fx::box(o, "row");
  for (const char *t : {"gap:2", "label-row"}) fx::use(top, t);
  fx::icon(top, r.icon, "icon", dim);
  fx::fit_grid(fx::text(top, r.label, "type:label", dim));  // the label box on the grid, so the dot after it is too
  if (const fw::Component *dc = fw::find("status_dot")) {  // the dot: always there, shown when it matters (fades)
    ArduinoJson::JsonDocument dp;
    dp["tone"] = "working";
    const uint32_t n = lv_obj_get_child_count(top);
    dc->build(top, dp.as<fw::Props>());
    if (!accent && lv_obj_get_child_count(top) > n) fx::use(lv_obj_get_child(top, -1), "transp");
  }
  lv_obj_t *line = fx::box(o, "row-mid");
  fx::use(line, "gap:3");
  fx::text(line, r.state, "type:title", mute ? "fg:unavailable-ink" : accent ? "fg:accent-text" : !strcmp(r.tone, "warn") ? "fg:warn" : "fg:text");
  if (*r.detail) fx::use(fx::text(line, r.detail, "type:body", "fg:text-dim"), "grow-w");
  if (mute) fx::state(o, fw::st::OFF, true);  // offline = unavailable: the unavailable ink is the disabled look (3:1)
}
void fill(lv_obj_t *s, fw::Props p) {
  if (*(p["label"] | "")) {  // one live readout (a Home sensor card): its own texts
    const Ro r{"live", p["icon"] | "thermometer", p["label"] | "", p["state"] | "", p["detail"] | "", p["tone"] | "text"};
    one(s, r);
    return;
  }
  const Ro *a = find_ro(p["top"] | "occupancy · on"), *b = find_ro(p["bottom"] | "door · open");
  if (!a || !b) {  // one sample alone: the whole box
    if (a || b) one(s, a ? *a : *b);
    return;
  }
  fx::use(s, "pair");  // two: the Cells pair (equal halves on the grid, the hairline centred in the space between)
  one(fx::box(s, "col"), *a);
  lv_obj_t *gap = fx::box(s, "centre");
  fx::use(gap, "grow");
  lv_obj_t *rule = fx::box(gap, "free");
  fx::use(rule, "pair-rule");
  lv_obj_t *ink = fx::box(rule, "free");
  for (const char *t : {"fill", "bg:line", "part:rule"}) fx::use(ink, t);
  one(fx::box(s, "col"), *b);
  for (int i : {0, 2}) fx::use(lv_obj_get_child(s, i), "max-h:pair-h");
  fx::pair_split(s);
}
}  // namespace

FW_COMPONENT(readout, "State readout", "components", R"({
  "about": "Reports a state and opens nothing: someone in a room, a door open, one temperature, who is home. No box, no press, no sound; two share a Home card cell.",
  "contract": 2,
  "parts": ["rule"],
  "min": ["layout:card-min-w", "layout:pair-h"],
  "frame": [352, 304],
  "icons": ["person@icon", "door@icon", "window@icon", "thermometer@icon", "house@icon", "alert@icon", "bolt@icon"],
  "props": [
    {"name": "top", "about": "For previews only: the made-up readout in the top half of the slot, such as someone in the room or a door open.", "type": "select", "options": ["occupancy · on", "occupancy · off", "door · open", "door · closed", "window · open", "temperature", "temperature · cold", "home · everyone", "home · some", "moisture · wet", "offline"], "default": "occupancy · on"},
    {"name": "label", "about": "The name of the thing it reports on. Leave empty to use the sample's own.", "type": "text", "default": ""},
    {"name": "icon", "about": "The icon before the name.", "type": "select", "options": ["person", "door", "window", "thermometer", "house", "alert", "bolt"], "default": "thermometer"},
    {"name": "state", "about": "The state in words, such as 21.4° or Open.", "type": "text", "default": "21.4°"},
    {"name": "detail", "about": "A quieter line after the state, such as how long ago it changed. Leave empty for none.", "type": "text", "default": ""},
    {"name": "tone", "about": "The colour of the state: text for normal, accent for something notable, warn for something to check, mute for quiet.", "type": "select", "options": ["text", "accent", "warn", "mute"], "default": "text"},
    {"name": "bottom", "about": "For previews only: the made-up readout in the bottom half of the slot, or none.", "type": "select", "options": ["none", "occupancy · on", "occupancy · off", "door · open", "door · closed", "window · open", "temperature", "temperature · cold", "home · everyone", "home · some", "moisture · wet", "offline"], "default": "door · open"}
  ],
  "variants": [
    {"label": "someone in · a door open", "props": {}},
    {"label": "temperature · who is home", "props": {"top": "temperature", "bottom": "home · some"}},
    {"label": "too cold · offline", "props": {"top": "temperature · cold", "bottom": "offline"}},
    {"label": "one alone", "props": {"top": "home · everyone", "bottom": "none"}},
    {"label": "live", "props": {"label": "Dishwasher", "icon": "bolt", "state": "0.8 W"}}
  ]
})") {
  lv_obj_t *s = lv_obj_create(parent);
  lv_obj_remove_style_all(s);
  lv_obj_remove_flag(s, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  fx::use(s, "fill");
  fx::name(s, "readout");
  fill(s, p);
}

FW_UPDATE(readout) {
  fw::st::refill_fade(root, [&] { fw::refill(root); fx::use(root, "fill"); fill(root, p); }, true, true);  // a new tone / the dot fade (like a tap)
  return true;
}
