// fw::st::set_state / swap: in-place state changes for the update path (core/states.h).
#include "fw.h"
#include "core/lv_obj_style_private.h"  // lv_obj_style_create_transition (LVGL internal, stable in 9.x)

namespace fw {
namespace st {
namespace {
const lv_style_prop_t PROPS[] = {LV_STYLE_TEXT_COLOR, LV_STYLE_BG_COLOR,   LV_STYLE_BORDER_COLOR, LV_STYLE_IMAGE_RECOLOR,
                                 LV_STYLE_ARC_COLOR,  LV_STYLE_LINE_COLOR, LV_STYLE_OUTLINE_COLOR, LV_STYLE_BG_OPA,
                                 LV_STYLE_BORDER_OPA, LV_STYLE_TEXT_OPA,   LV_STYLE_IMAGE_RECOLOR_OPA, LV_STYLE_OUTLINE_OPA,
                                 LV_STYLE_ARC_OPA,    LV_STYLE_LINE_OPA,   LV_STYLE_OPA};
const lv_part_t PARTS[] = {LV_PART_MAIN, LV_PART_INDICATOR, LV_PART_KNOB};
void subtree(lv_obj_t *o, std::vector<lv_obj_t *> &out) {
  out.push_back(o);
  for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) subtree(lv_obj_get_child(o, (int32_t) i), out);
}
}  // namespace

// The fade is an LVGL style transition (the same machinery as a press fade): LVGL reads each prop in the old and the
// new state (a fade still running included, so a quick back-and-forth reverses smoothly), keeps it in the object's
// transition style while it runs and drops it at the end. It never touches the component's own local styles.
void set_state(lv_obj_t *o, lv_state_t state, bool on, bool animate) {
  if (!o) return;
  std::vector<lv_obj_t *> objs;
  subtree(o, objs);
  const uint32_t ms = animate ? mo::duration(o, mo::fade, mo::OPA) : 0;
  for (lv_obj_t *x : objs) {
    const lv_state_t was = lv_obj_get_state(x), now = on ? (lv_state_t) (was | state) : (lv_state_t) (was & ~state);
    if (was == now) continue;
    lv_obj_set_state(x, state, on);  // LVGL's own state transitions (press fades) as before
    if (!ms) continue;
    for (lv_part_t part : PARTS)
      for (lv_style_prop_t prop : PROPS) {
        lv_obj_style_transition_dsc_t tr{};
        tr.time = (uint16_t) ms;
        tr.selector = part;
        tr.prop = prop;
        tr.path_cb = mo::path(mo::fade);
        lv_obj_style_create_transition(x, part, was, now, &tr);  // returns at once when the prop doesn't change
      }
  }
}

namespace {
void set_opa(void *o, int32_t v) { lv_obj_set_style_opa((lv_obj_t *) o, (lv_opa_t) v, 0); }
}  // namespace
void swap(lv_obj_t *out, lv_obj_t *in, bool animate) {
  if (out && out != in) lv_obj_add_flag(out, LV_OBJ_FLAG_HIDDEN);
  if (!in || !lv_obj_has_flag(in, LV_OBJ_FLAG_HIDDEN)) return;
  lv_obj_remove_flag(in, LV_OBJ_FLAG_HIDDEN);
  if (!animate) { lv_obj_set_style_opa(in, LV_OPA_COVER, 0); return; }
  fw::animate(in, mo::fade, {set_opa, LV_OPA_TRANSP, LV_OPA_COVER, mo::OPA});
}
namespace {
std::string press_sig(ArduinoJson::JsonObjectConst p) {  // the props with a pressed state read as default
  ArduinoJson::JsonDocument rest;
  rest.set(p);
  if (!strcmp(p["state"] | "default", "pressed")) rest["state"] = "default";
  std::string s;
  serializeJson(rest, s);
  return s;
}
}  // namespace
void built(lv_obj_t *root, ArduinoJson::JsonObjectConst p) { fw::shape(root, press_sig(p)); }
bool update(lv_obj_t *root, ArduinoJson::JsonObjectConst p, Fill fill) {
  const bool pr = !strcmp(p["state"] | "default", "pressed");
  if (fw::shape(root, press_sig(p))) {
    if (pr != lv_obj_has_state(root, LV_STATE_PRESSED)) lv_obj_set_state(root, LV_STATE_PRESSED, pr);
    return true;
  }
  const bool was_off = lv_obj_has_state(root, OFF);
  fw::refill(root);
  fill(root, p);
  if (pr != lv_obj_has_state(root, LV_STATE_PRESSED)) lv_obj_set_state(root, LV_STATE_PRESSED, pr);
  if (was_off != lv_obj_has_state(root, OFF)) fade_in_children(root);
  return true;
}

void fade_in_children(lv_obj_t *root, bool animate) {
  if (!root || !animate || !mo::duration(root, mo::fade, mo::OPA)) return;
  for (uint32_t i = 0; i < lv_obj_get_child_count(root); i++) {
    lv_obj_t *c = lv_obj_get_child(root, (int32_t) i);
    if (lv_obj_has_flag(c, LV_OBJ_FLAG_HIDDEN)) continue;
    fw::animate(c, mo::fade, {set_opa, LV_OPA_TRANSP, LV_OPA_COVER, mo::OPA});
  }
}
}  // namespace st
}  // namespace fw
