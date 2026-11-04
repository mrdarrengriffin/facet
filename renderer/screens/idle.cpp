// Idle clock: what the panel shows after Settings > Clock after with no touch (core/device.cpp "idle"). The Clock
// screen (specs/clock.json: the orbit, the time and the date) with its Header hidden, so the clock sits in the middle
// of the screen. Nothing on it takes a tap: the first touch anywhere is swallowed (the finger's
// release clicks nothing) and goes back to Home. The device fades it in, dims the screen (Idle brightness) and
// undims when it leaves.
// Idle shows = Album art (setting idleShow "art"): a Media art over the clock, bound to the Media feed (panel.media:
// art, playing): while music plays with art it covers the screen (the art alone), else it draws nothing and the clock
// shows; it moves with the feed in place (a new track, a pause). It takes no touch: the tap reaches the clock under it.
// The feed reads the Media screen's source (the deck's pick, else Home Assistant) when no screen has set one yet.
#include "fw.h"
#include "spec.h"
#include "device.h"
#include "ha.h"
#include "fx.h"

namespace {
const char *META = R"({
  "about": "The idle clock: the Clock screen without its header, shown after Clock after with no touch. With Idle shows set to Album art, the art of the music playing covers it while it plays. A touch goes back to Home.",
  "frame": [480, 480],
  "icons": [],
  "props": []
})";
void unclick(lv_obj_t *o) {
  lv_obj_remove_flag(o, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) unclick(lv_obj_get_child(o, (int32_t) i));
}
void build_idle(lv_obj_t *parent, fw::Props p) {
  lv_obj_t *s = fw::spec::build(parent, "clock", p);
  if (!s) return;
  if (lv_obj_t *h = lv_obj_get_child(s, 0)) lv_obj_add_flag(h, LV_OBJ_FLAG_HIDDEN);  // the Page's head: the body fills the screen
  unclick(s);
  lv_obj_add_flag(s, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(s, [](lv_event_t *e) {
    if (lv_indev_t *i = lv_indev_active()) lv_indev_wait_release(i);
    fw::dev::home((lv_obj_t *) lv_event_get_current_target(e));
  }, LV_EVENT_PRESSED, nullptr);
  const char *show = fw::dev::setting("idleShow");
  if (!show || strcmp(show, "art")) return;
  if (fw::ha::feed_config("panel.media").empty()) {  // no screen set the feed's source yet: the Media screen's
    ArduinoJson::JsonDocument de, fc;
    fc["source"] = fw::spec::deck_entry("media", de) && de["params"]["source"].is<const char *>() ? de["params"]["source"].as<const char *>() : "ha";
    std::string cfg;
    serializeJson(fc, cfg);
    fw::ha::set_feed_config("panel.media", cfg);
  }
  lv_obj_t *layer = fx::box(parent, "free");  // over the clock, the whole screen, outside its layout
  fx::use(layer, "fill");
  lv_obj_add_flag(layer, (lv_obj_flag_t) (LV_OBJ_FLAG_IGNORE_LAYOUT | LV_OBJ_FLAG_FLOATING));
  lv_obj_remove_flag(layer, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  ArduinoJson::JsonDocument b;
  for (const char *a : {"art", "playing"}) b[a]["e"] = "panel.media", b[a]["a"] = a;
  fw::ha::bound(layer, "media_art", b.as<fw::Props>());
}
const int reg = fw::add({"idle", "Idle clock", "screens", META, build_idle});
}  // namespace
