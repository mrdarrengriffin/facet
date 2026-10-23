// State readout: the Facet state readout (docs: Screen parts > State readout, @facet/ui readouts.ts readout() /
// readoutSlot() / readoutFrom()): a Home item that reports a state and opens nothing (someone in a room, a door open,
// one temperature, who is home). The rule: a box is a button, no box is a readout: on the canvas, no fill, no press,
// no sound, no chevron, a tap does nothing. Two share one Home card slot (lay::card_w x card_h), sp::s2 apart, one
// hairline between them inset by the card padding.
//   one     card_w x (card_h - s2) / 2, padding lay::card_pad top and sides: a top row (lay row of the label box: the
//           icon at ic::icon text-dim, the caps label (screen type label, text-dim), sp::s2 apart; the working Status
//           dot after it only when the state matters (accent)), then lay::card_pad below it the state (type title) and
//           the detail (type body, text-dim, ellipsis) on one baseline, sp::s3 apart
//   tones   text (default) · accent = it matters (accent-text + the dot) · warn = out of range · mute = offline
//           (text-mute, the unavailable ink; the label row too)
// The samples are the docs readoutFrom() wordings (a mock house): occupancy, door, temperature, who is home, offline.
// Update path: texts in place, a tone fades (fw::st::refill_fade, like a tap would), silent. Contrast: "readout".
#include "fw.h"

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
lv_obj_t *box(lv_obj_t *parent) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  return o;
}
void one(lv_obj_t *slot, const Ro &r, int h) {
  const fw::Theme &t = *fw::T;
  const bool mute = !strcmp(r.tone, "mute"), accent = !strcmp(r.tone, "accent");
  const lv_color_t dim = mute ? fw::st::unavailable_ink(t) : t.text_dim;
  lv_obj_t *o = box(slot);
  lv_obj_set_size(o, LV_PCT(100), h);
  lv_obj_set_style_pad_top(o, fw::lay::card_pad, 0);
  lv_obj_set_style_pad_hor(o, fw::lay::card_pad, 0);
  lv_obj_set_flex_flow(o, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(o, fw::lay::card_pad, 0);
  lv_obj_t *top = box(o);
  lv_obj_set_size(top, LV_PCT(100), fw::style(fw::ty::label).box);
  lv_obj_set_flex_flow(top, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(top, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(top, fw::sp::s2, 0);
  fw::icon(top, r.icon, fw::ic::icon, dim);
  lv_obj_t *l = fw::text(top, fw::ty::label, r.label, dim);
  lv_obj_set_height(l, fw::style(fw::ty::label).box);
  lv_obj_update_layout(l);  // the label box on the grid (Label fit8), so the dot after it lands on the grid
  lv_obj_set_width(l, (lv_obj_get_width(l) + fw::sp::GRID - 1) / fw::sp::GRID * fw::sp::GRID);
  if (const fw::Component *dc = fw::find("status_dot")) {  // the dot: always there, shown when it matters (fades)
    ArduinoJson::JsonDocument dp;
    dp["tone"] = "working";
    dc->build(top, dp.as<fw::Props>());
    lv_obj_t *dot = lv_obj_get_child(top, -1);
    lv_obj_set_style_bg_opa(dot, accent ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
  }
  lv_obj_t *line = box(o);
  lv_obj_set_size(line, LV_PCT(100), fw::style(fw::ty::title).box);
  lv_obj_set_flex_flow(line, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(line, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
  lv_obj_set_style_pad_column(line, fw::sp::s3, 0);
  const lv_color_t sc = mute ? fw::st::unavailable_ink(t) : accent ? t.accent_text : !strcmp(r.tone, "warn") ? t.warn : t.text;
  lv_obj_t *st = fw::text(line, fw::ty::title, r.state, sc);
  lv_obj_set_height(st, fw::style(fw::ty::title).box);
  if (*r.detail) {  // on the state's baseline: the body box's caps bottom meets the title's
    lv_obj_t *d = fw::text(line, fw::ty::body, r.detail, t.text_dim);
    lv_obj_set_height(d, fw::style(fw::ty::body).box);
    lv_label_set_long_mode(d, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_flex_grow(d, 1);
    lv_obj_set_style_margin_bottom(d, (fw::style(fw::ty::title).box - fw::style(fw::ty::body).box) / 2, 0);
  }
}
void fill(lv_obj_t *s, fw::Props p) {
  lv_obj_remove_style_all(s);
  lv_obj_remove_flag(s, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_size(s, fw::lay::card_w, fw::lay::card_h);
  lv_obj_set_flex_flow(s, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(s, fw::sp::s2, 0);
  const Ro *a = find_ro(p["top"] | "occupancy · on"), *b = find_ro(p["bottom"] | "door · open");
  const int h = (fw::lay::card_h - fw::sp::s2) / 2;
  if (a) one(s, *a, b ? h : fw::lay::card_h);
  if (a && b) {  // the hairline between, inset like the card text, centred in the gap
    lv_obj_t *hl = box(s);
    lv_obj_add_flag(hl, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_set_size(hl, fw::lay::card_w - 2 * fw::lay::card_pad, fw::stroke::hair);
    lv_obj_set_pos(hl, fw::lay::card_pad, h + fw::sp::s2 / 2);
    lv_obj_set_style_bg_color(hl, fw::T->line, 0);
    lv_obj_set_style_bg_opa(hl, LV_OPA_COVER, 0);
  }
  if (b) one(s, *b, a ? h : fw::lay::card_h);
}
}  // namespace

FW_COMPONENT(readout, "State readout", "components", R"({
  "about": "Reports a state and opens nothing: someone in a room, a door open, one temperature, who is home. No box, no press, no sound; two share a Home card slot.",
  "frame": [352, 304],
  "icons": ["person@icon", "door@icon", "window@icon", "thermometer@icon", "house@icon", "alert@icon"],
  "props": [
    {"name": "top", "type": "select", "options": ["occupancy · on", "occupancy · off", "door · open", "door · closed", "window · open", "temperature", "temperature · cold", "home · everyone", "home · some", "moisture · wet", "offline"], "default": "occupancy · on"},
    {"name": "bottom", "type": "select", "options": ["none", "occupancy · on", "occupancy · off", "door · open", "door · closed", "window · open", "temperature", "temperature · cold", "home · everyone", "home · some", "moisture · wet", "offline"], "default": "door · open"}
  ],
  "variants": [
    {"label": "someone in · a door open", "props": {}},
    {"label": "temperature · who is home", "props": {"top": "temperature", "bottom": "home · some"}},
    {"label": "too cold · offline", "props": {"top": "temperature · cold", "bottom": "offline"}},
    {"label": "one alone", "props": {"top": "home · everyone", "bottom": "none"}}
  ]
})") {
  lv_obj_t *s = lv_obj_create(parent);
  fill(s, p);
}

FW_UPDATE(readout) {
  fw::st::refill_fade(root, [&] { fw::refill(root); fill(root, p); });  // a new tone / the dot fade (like a tap)
  return true;
}
