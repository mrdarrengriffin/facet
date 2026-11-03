// Idle clock: what the panel shows after Settings > Clock after with no touch (core/device.cpp "idle"). The Clock
// screen (specs/clock.json: the orbit, the time and the date) with its Header hidden, so the clock sits in the middle
// of the screen. Nothing on it takes a tap: the first touch anywhere is swallowed (the finger's
// release clicks nothing) and goes back to Home. The device fades it in, dims the screen (Idle brightness) and
// undims when it leaves.
#include "fw.h"
#include "spec.h"
#include "device.h"

namespace {
const char *META = R"({
  "about": "The idle clock: the Clock screen without its header, shown after Clock after with no touch. A touch goes back to Home.",
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
}
const int reg = fw::add({"idle", "Idle clock", "screens", META, build_idle});
}  // namespace
