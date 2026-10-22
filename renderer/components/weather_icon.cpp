// Weather icon: a Facet weather icon (docs @facet/icons weatherIcon(id, { size, animated }), weather.css), the Icons
// foundation's master via core fw::weather_icon: static = exactly the master (fw::icon), animated = the master's parts
// moving by the motion.json weather keyframes (sun rays turn, clouds drift, drops / snow / hail fall and fade, the
// bolt flashes, fog / wind lines slide, stars twinkle). Reduced motion = the static master. Animated parts ship at
// icons.json animated sizes (feature / card / hero); other sizes are always static.
//   icon      one of the 17 weather masters (HA condition -> icon is the host's job, docs wxIcon())
//   size      an icon size token the weather icons ship in
//   animated  moving parts on / off (docs `animated`)
//   on        canvas (bg) | card (surface): the colour a moving cloud's fill is drawn in, so it hides what is behind it
//             (docs --fwx-mask); card shows the icon on a Card-coloured surface
// Update path: icon / animated swap in place (same box, no rebuild); size / on refill. Static: no events, no sound.
#include "fw.h"

namespace {
using namespace fw;
ic::Size size_of(const char *s) {
  for (int i = 0; i < ic::N_SIZES; i++)
    if (!strcmp(ic::SIZE_NAMES[i], s)) return (ic::Size) i;
  return ic::card;
}
std::string sig(Props p) { return std::string(p["size"] | "card") + "|" + (p["on"] | "canvas"); }
void fill(lv_obj_t *r, Props p) {
  const bool card = !strcmp(p["on"] | "canvas", "card");
  fw::shape(r, sig(p));
  lv_obj_remove_style_all(r);
  lv_obj_remove_flag(r, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_size(r, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  if (card) {
    lv_obj_set_style_bg_color(r, T->surface, 0);
    lv_obj_set_style_bg_opa(r, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(r, rad::card, 0);
    lv_obj_set_style_pad_all(r, sp::s5, 0);
  }
  weather_icon(r, p["icon"] | "partly-day", size_of(p["size"] | "card"), p["animated"] | true, T->text, card ? T->surface : T->bg);
}
}  // namespace

FW_COMPONENT(weather_icon, "Weather icon", "components", R"({
  "about": "A weather icon from the Icons foundation; animated, its parts move gently (rays turn, clouds drift, rain falls). Reduced motion shows it still.",
  "frame": [240, 200],
  "props": [
    {"name": "icon", "type": "select", "options": )" FW_WEATHER_IDS R"(, "default": "partly-day"},
    {"name": "size", "type": "select", "options": )" FW_WEATHER_SIZE_IDS R"(, "default": "card"},
    {"name": "animated", "type": "bool", "default": true},
    {"name": "on", "type": "select", "options": ["canvas", "card"], "default": "canvas"}
  ],
  "variants": [
    {"label": "partly-day", "props": {"icon": "partly-day"}},
    {"label": "rain-heavy", "props": {"icon": "rain-heavy"}},
    {"label": "thunder-rain", "props": {"icon": "thunder-rain"}},
    {"label": "snow", "props": {"icon": "snow"}},
    {"label": "clear-night · card", "props": {"icon": "clear-night", "on": "card"}},
    {"label": "cloudy · card", "props": {"icon": "cloudy", "on": "card"}},
    {"label": "partly-day · static", "props": {"icon": "partly-day", "animated": false}}
  ]
})") {
  fill(lv_obj_create(parent), p);
}

// Update path: another icon or animated on / off = the same icon box swaps its parts (fw::weather_icon_set);
// size / on = refill (snaps). Silent.
FW_UPDATE(weather_icon) {
  lv_obj_t *wx = lv_obj_get_child(root, 0);
  if (fw::shape(root, sig(p)) && wx && fw::weather_icon_set(wx, p["icon"] | "partly-day", p["animated"] | true)) return true;
  fw::refill(root);
  fill(root, p);
  return true;
}
