// Weather icon: a Facet weather icon (docs @facet/icons weatherIcon(id, { size, animated }), weather.css), on the
// contract (renderer/CONTRACT.md, "contract": 2). The Icons foundation's master through fx::weather: static = the
// master (shared ink styles), animated = the master's parts moving by the motion.json weather keyframes (sun rays
// turn, clouds drift, drops / snow / hail fall and fade, the bolt flashes, fog / wind lines slide, stars twinkle);
// reduced motion = the static master. Animated parts ship at icons.json animated sizes (feature / card / hero); other
// sizes are always static. Icon sizes are the Icons foundation's shipped sizes (bitmaps, like fx::icon), not sliders.
// The words that go with it (the Weather screen, docs screens/weather.ts; layout picks how):
//   icon     the icon alone, its own size (hug); on card: a Card-coloured surface (bg:surface, radius:card, pad:5)
//   column   one of the next hours (docs .fs-hour): label over the icon (type:label, fg:text-dim; accent = fg:accent-text,
//            the hour now), value under it (type:body, fg:text), gap:2, centred across the parent's width (a Grid cell)
//   row      one of the next days (docs daysView), a List row's height (h:list-row-h, pad-x:4, gap:5; divider = the
//            List row's hairline on top, line:line + rule-top): the day (type:title, fg:text, w:wx-day-name), the icon,
//            line (type:body, fg:text-dim, the free width, "..."), low (type:body, fg:text-dim), value (type:title)
//   hero     the weather now (docs .fs-w-now): value (type:display, fg:text) at the start of a row (hero-row-display:
//            the display type box tall, = the hero icon) with the icon at its end, then label (type:headline, fg:text) and line (type:body, fg:text-dim), gap:3; fills the width
// Feed: on the Weather screen every word binds to panel.weather (core/weather_source.cpp: icon, temp, condition, line;
// hours / days items). Static: no touch, no sound. Update path: the icon / motion swap in place (same box), the words in
// place; a new layout, size, on, accent or set of words = refilled (fw::st::refill_fade, silent).
// States: default only (n/a: pressed, on, off, disabled: a picture, no press; loading / unavailable / empty / error =
// the feed's words: "--°", "Loading", "Offline"). Contrast: section "weather" (text, text-dim, accent-text on bg; line).
#include "fw.h"
#include "fx.h"

namespace {
bool flag(fw::Props p, const char *k, bool def) {  // a bind gives "on" / "off"
  return p[k].is<const char *>() ? !strcmp(p[k].as<const char *>(), "on") : (p[k] | def);
}
const char *layout_of(fw::Props p) { return p["layout"] | "icon"; }
std::string sig(fw::Props p) {
  const char *L = layout_of(p);
  std::string s = std::string(L) + "|" + (p["size"] | "card") + "|" + (p["on"] | "canvas");
  if (!strcmp(L, "column")) s += std::string("|") + (*(p["label"] | "") ? "l" : "-") + (*(p["value"] | "") ? "v" : "-") + (flag(p, "accent", false) ? "a" : "-");
  if (!strcmp(L, "row")) s += flag(p, "divider", false) ? "|d" : "|-";
  return s;
}
lv_obj_t *wx(lv_obj_t *parent, fw::Props p, const char *size) {
  lv_obj_t *w = fx::weather(parent, p["icon"] | "partly-day", size, p["animated"] | true);
  return w ? w : fx::weather(parent, "cloudy", size, false);  // an unknown name: the cloud (the docs' fallback)
}
void words(lv_obj_t *parent, const char *s, const char *type, const char *fg, const char *width) {
  fx::use(fx::text(parent, s, type, fg), width);
}
void fill(lv_obj_t *root, fw::Props p) {
  fw::shape(root, sig(p));
  const char *L = layout_of(p);
  if (!strcmp(L, "column")) {
    for (const char *t : {"box", "col-centre", "fill-w", "gap:2"}) fx::use(root, t);
    if (*(p["label"] | "")) fx::text(root, p["label"] | "", "type:label", flag(p, "accent", false) ? "fg:accent-text" : "fg:text-dim");
    wx(root, p, p["size"] | "glyph");
    if (*(p["value"] | "")) fx::text(root, p["value"] | "", "type:body", "fg:text");
  } else if (!strcmp(L, "row")) {
    for (const char *t : {"box", "row-mid", "fill-w", "h:list-row-h", "pad-x:4", "gap:5"}) fx::use(root, t);
    if (flag(p, "divider", false)) { fx::use(root, "line:line"); fx::use(root, "rule-top"); }
    words(root, p["label"] | "", "type:title", "fg:text", "w:wx-day-name");
    wx(root, p, p["size"] | "feature");
    words(root, p["line"] | "", "type:body", "fg:text-dim", "grow-w");
    fx::text(root, p["low"] | "", "type:body", "fg:text-dim");
    fx::text(root, p["value"] | "", "type:title", "fg:text");
  } else if (!strcmp(L, "hero")) {
    for (const char *t : {"box", "col", "fill-w", "gap:3"}) fx::use(root, t);
    lv_obj_t *top = fx::box(root, "free");
    fx::use(top, "hero-row-display");  // the display type box tall (= the hero icon), as the Hero block's number row
    fx::text(top, p["value"] | "", "type:display", "fg:text");
    fx::use(fx::box(top, "free"), "grow");
    wx(top, p, p["size"] | "hero");
    words(root, p["label"] | "", "type:headline", "fg:text", "fill-w");
    words(root, p["line"] | "", "type:body", "fg:text-dim", "fill-w");
  } else {
    for (const char *t : {"box", "centre", "hug"}) fx::use(root, t);
    if (!strcmp(p["on"] | "canvas", "card")) for (const char *t : {"bg:surface", "radius:card", "pad:5"}) fx::use(root, t);
    wx(root, p, p["size"] | "card");
  }
}
// the icon box and the word labels in build order (the same order fill() made them)
void parts(lv_obj_t *root, lv_obj_t **icon, std::vector<lv_obj_t *> &texts) {
  std::vector<lv_obj_t *> todo{root};
  for (size_t k = 0; k < todo.size(); k++)
    for (uint32_t i = 0; i < lv_obj_get_child_count(todo[k]); i++) {
      lv_obj_t *c = lv_obj_get_child(todo[k], (int32_t) i);
      if (lv_obj_check_type(c, &lv_label_class)) texts.push_back(c);
      else if (fw::weather_icon_name(c)) *icon = c;
      else if (k == 0) todo.push_back(c);  // the hero's top row
    }
}
}  // namespace

FW_COMPONENT(weather_icon, "Weather icon", "components", R"({
  "about": "A weather icon from the Icons foundation; animated, its parts move gently (rays turn, clouds drift, rain falls). Reduced motion shows it still. With its words it is one of the next hours, one of the next days, or the weather now at the top of the Weather screen.",
  "contract": 2,
  "parts": [],
  "frame": [480, 280],
  "props": [
    {"name": "icon", "about": "Which weather to show, such as partly cloudy by day, rain or a clear night.", "type": "select", "options": )" FW_WEATHER_IDS R"(, "default": "partly-day"},
    {"name": "size", "about": "How big the icon is drawn, from the small sizes in text up to the hero size of a screen.", "type": "select", "options": )" FW_WEATHER_SIZE_IDS R"(, "default": "card"},
    {"name": "animated", "about": "Makes the icon's parts move gently: rays turn, clouds drift, rain falls. With reduced motion it stays still.", "type": "bool", "default": true},
    {"name": "on", "about": "What the icon alone sits on: canvas for the screen background, card for a Card-coloured surface round it.", "type": "select", "options": ["canvas", "card"], "default": "canvas"},
    {"name": "layout", "about": "How the words sit with the icon: icon alone; column for one of the next hours (the hour over it, the temperature under it); row for one of the next days (day, icon, weather, low, high); hero for the weather now (the temperature with the icon at the end, the condition and a line under it).", "type": "select", "options": ["icon", "column", "row", "hero"], "default": "icon"},
    {"name": "label", "about": "The main words: the hour over the icon (column), the day (row) or the condition (hero), such as now, Saturday or Partly cloudy.", "type": "text", "default": ""},
    {"name": "value", "about": "The temperature: under the icon (column), the high at the end (row), or the big one (hero), such as 13°.", "type": "text", "default": ""},
    {"name": "line", "about": "A quieter line: the day's weather and rain (row), or feels like, high and low (hero).", "type": "text", "default": ""},
    {"name": "low", "about": "The day's low, before its high (row).", "type": "text", "default": ""},
    {"name": "accent", "about": "Draws the hour in the accent colour: the hour now (column).", "type": "bool", "default": false},
    {"name": "divider", "about": "Draws the List row's thin line above the row, for every day after the first (row).", "type": "bool", "default": false}
  ],
  "variants": [
    {"label": "card · animated", "props": {}},
    {"label": "on a card", "props": {"icon": "rain-light", "on": "card"}},
    {"label": "hour · now", "props": {"layout": "column", "size": "glyph", "animated": false, "label": "now", "value": "13°", "accent": true}},
    {"label": "day · rain", "props": {"layout": "row", "icon": "rain-light", "animated": false, "label": "Saturday", "line": "Rain · 4.2 mm", "low": "9°", "value": "13°", "divider": true}},
    {"label": "hero · now", "props": {"layout": "hero", "label": "Partly cloudy", "value": "13°", "line": "Feels 11° · high 16°, low 8°"}}
  ]
})") {
  lv_obj_t *root = lv_obj_create(parent);
  lv_obj_remove_style_all(root);
  lv_obj_remove_flag(root, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_add_flag(root, LV_OBJ_FLAG_EVENT_BUBBLE);
  fx::name(root, "weather_icon");
  fill(root, p);
}

FW_UPDATE(weather_icon) {
  if (!fw::shape(root, sig(p))) {
    fw::st::refill_fade(root, [&] { fw::refill(root); fill(root, p); }, true, true);
    return true;
  }
  lv_obj_t *icon = nullptr;
  std::vector<lv_obj_t *> t;
  parts(root, &icon, t);
  if (icon) fx::weather_set(icon, p["icon"] | "partly-day", p["animated"] | true);
  const char *L = layout_of(p);
  if (!strcmp(L, "column")) {
    size_t i = 0;
    if (*(p["label"] | "") && i < t.size()) fx::set_text(t[i++], p["label"] | "", "type:label");
    if (*(p["value"] | "") && i < t.size()) fx::set_text(t[i++], p["value"] | "", "type:body");
  } else if (!strcmp(L, "row") && t.size() == 4) {
    fx::set_text(t[0], p["label"] | "", "type:title");
    fx::set_text(t[1], p["line"] | "", "type:body");
    fx::set_text(t[2], p["low"] | "", "type:body");
    fx::set_text(t[3], p["value"] | "", "type:title");
  } else if (!strcmp(L, "hero") && t.size() == 3) {  // breadth first: label, line (root), then value (the top row)
    fx::set_text(t[0], p["label"] | "", "type:headline");
    fx::set_text(t[1], p["line"] | "", "type:body");
    fx::set_text(t[2], p["value"] | "", "type:display");
  }
  return true;
}
