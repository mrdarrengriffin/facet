// Links inside a screen's body (the specs' "open" rule, core/spec.h: a component's own "open" event with the detail
// "<screen>[?{params}]" opens that screen): fw::link_box(root, parent, target) = a box the full width of parent that
// listens to what is built inside it (a More row, a PR row, a List row) and turns its tap into "open" <target> emitted
// from the view's root. The emit has to come from outside the listened box: the nearest listener takes an emit, so the
// box's own listener would swallow it. Header-only (the firmware bundle picks up core/*.h by itself).
#pragma once
#include <string>
#include "fw.h"

namespace fw {
inline lv_obj_t *link_box(lv_obj_t *root, lv_obj_t *parent, const std::string &target) {
  struct Link { lv_obj_t *root; std::string target; };
  lv_obj_t *box = lv_obj_create(parent);
  lv_obj_remove_style_all(box);
  lv_obj_remove_flag(box, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_size(box, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_add_flag(box, LV_OBJ_FLAG_OVERFLOW_VISIBLE);  // the row inside keeps its own marks outside its box (a press fill, an edge)
  auto *l = new Link{root, target};
  lv_obj_add_event_cb(box, [](lv_event_t *e) { delete (Link *) lv_event_get_user_data(e); }, LV_EVENT_DELETE, l);
  listen(box, [](lv_obj_t *, const char *name, const char *, void *user) {
    if (strcmp(name, st::TAP_EVENT)) return;
    auto *l = (Link *) user;
    emit(l->root, "open", l->target.c_str());
  }, l);
  return box;
}
}  // namespace fw
