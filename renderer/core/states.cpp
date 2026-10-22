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
  // the contents again; what kept its place fades to its new look (a tone, a mark: an update animates like a tap)
  refill_fade(root, [&] { fw::refill(root); fill(root, p); });
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

namespace fw {
namespace st {
namespace {
struct Fade {
  lv_obj_t *obj;
  lv_part_t part;
  lv_style_prop_t prop;
  lv_style_value_t from, to;
  bool colour;
};
std::vector<Fade *> g_fades;
void fade_free(lv_anim_t *a) {
  auto *f = (Fade *) lv_anim_get_user_data(a);
  for (size_t i = 0; i < g_fades.size(); i++) if (g_fades[i] == f) { g_fades.erase(g_fades.begin() + i); break; }
  delete f;
}
void fade_exec(lv_anim_t *a, int32_t v) {
  auto *f = (Fade *) lv_anim_get_user_data(a);
  lv_style_value_t x;
  if (f->colour) x.color = lv_color_mix(f->to.color, f->from.color, (uint8_t) v);
  else x.num = f->from.num + (f->to.num - f->from.num) * v / 255;
  lv_obj_set_local_style_prop(f->obj, f->prop, x, f->part);
}
void fades_of_obj_gone(lv_event_t *e) {  // an object going away takes its fades with it
  auto *o = (lv_obj_t *) lv_event_get_current_target(e);
  std::vector<Fade *> mine;
  for (Fade *f : g_fades) if (f->obj == o) mine.push_back(f);
  for (Fade *f : mine) lv_anim_delete(f, nullptr);
}
bool is_colour(lv_style_prop_t p) {
  return p == LV_STYLE_TEXT_COLOR || p == LV_STYLE_BG_COLOR || p == LV_STYLE_BORDER_COLOR || p == LV_STYLE_IMAGE_RECOLOR ||
         p == LV_STYLE_ARC_COLOR || p == LV_STYLE_LINE_COLOR || p == LV_STYLE_OUTLINE_COLOR;
}
}  // namespace

namespace {
// one fade from w (the old value) to what o has now (its local value), as restyle() makes them
void fade_value(lv_obj_t *root, lv_obj_t *obj, lv_part_t part, lv_style_prop_t prop, lv_style_value_t from, uint32_t ms, int32_t big);
void walk(lv_obj_t *o, const std::string &path, std::vector<std::pair<std::string, lv_obj_t *>> &out) {
  out.push_back({path, o});
  for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) walk(lv_obj_get_child(o, (int32_t) i), path + "/" + std::to_string(i), out);
}
}  // namespace
void refill_fade(lv_obj_t *root, const std::function<void()> &rebuild, bool animate) {
  const uint32_t ms = animate && root ? mo::duration(root, mo::fade, mo::OPA) : 0;
  struct Was { std::string path; const lv_obj_class_t *cls; lv_part_t part; lv_style_prop_t prop; lv_style_value_t v; };
  std::vector<Was> was;
  if (ms) {
    std::vector<std::pair<std::string, lv_obj_t *>> objs;
    walk(root, "", objs);
    for (auto &[path, x] : objs)
      for (lv_part_t part : PARTS)
        for (lv_style_prop_t prop : PROPS) {
          lv_style_value_t v;
          if (lv_obj_get_local_style_prop(x, prop, &v, part) == LV_STYLE_RES_FOUND) was.push_back({path, lv_obj_get_class(x), part, prop, v});
        }
  }
  rebuild();
  if (!ms) return;
  std::vector<std::pair<std::string, lv_obj_t *>> now;
  walk(root, "", now);
  lv_display_t *disp = lv_obj_get_display(root);
  const int32_t big = disp ? lv_display_get_horizontal_resolution(disp) * lv_display_get_vertical_resolution(disp) / 16 : 0;
  for (const Was &w : was)
    for (auto &[path, x] : now)
      if (path == w.path && lv_obj_get_class(x) == w.cls) { fade_value(root, x, w.part, w.prop, w.v, ms, big); break; }
}

void restyle(lv_obj_t *o, const std::function<void()> &change, bool animate) {
  if (!o) { change(); return; }
  struct Was { lv_obj_t *obj; lv_part_t part; lv_style_prop_t prop; lv_style_value_t v; };
  std::vector<lv_obj_t *> objs;
  subtree(o, objs);
  const uint32_t ms = animate ? mo::duration(o, mo::fade, mo::OPA) : 0;
  std::vector<Was> was;
  if (ms)
    for (lv_obj_t *x : objs)
      for (lv_part_t part : PARTS)
        for (lv_style_prop_t prop : PROPS) {
          lv_style_value_t v;
          if (lv_obj_get_local_style_prop(x, prop, &v, part) == LV_STYLE_RES_FOUND) was.push_back({x, part, prop, v});
        }
  change();
  if (!ms) return;
  lv_display_t *disp = lv_obj_get_display(o);
  const int32_t big = disp ? lv_display_get_horizontal_resolution(disp) * lv_display_get_vertical_resolution(disp) / 16 : 0;
  std::vector<lv_obj_t *> alive;
  subtree(o, alive);  // objects made again inside change() are new: they just appear
  for (const Was &w : was)
    if (std::find(alive.begin(), alive.end(), w.obj) != alive.end()) fade_value(o, w.obj, w.part, w.prop, w.v, ms, big);
}
namespace {
void fade_value(lv_obj_t *root, lv_obj_t *obj, lv_part_t part, lv_style_prop_t prop, lv_style_value_t from, uint32_t ms, int32_t big) {
  lv_style_value_t now;
  if (lv_obj_get_local_style_prop(obj, prop, &now, part) != LV_STYLE_RES_FOUND) return;
  const bool col = is_colour(prop);
  if (col ? lv_color_eq(now.color, from.color) : now.num == from.num) return;
  // no large fades on a display without fades (the Guition): a large FILL changes at once (thin marks still fade)
  const bool fill = prop == LV_STYLE_BG_COLOR || prop == LV_STYLE_BG_OPA || prop == LV_STYLE_OPA;
  if (fill && !mo::fades(root) && lv_obj_get_width(obj) * lv_obj_get_height(obj) > big) return;
  for (Fade *f : std::vector<Fade *>(g_fades))  // a fade of the same value still running: replaced from where it is
    if (f->obj == obj && f->part == part && f->prop == prop) lv_anim_delete(f, nullptr);
  auto *f = new Fade{obj, part, prop, from, now, col};
  lv_obj_remove_event_cb(obj, fades_of_obj_gone);  // one handler per object
  lv_obj_add_event_cb(obj, fades_of_obj_gone, LV_EVENT_DELETE, nullptr);
  g_fades.push_back(f);
  lv_obj_set_local_style_prop(obj, prop, from, part);  // back to the old look; the fade brings the new one
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, f);
  lv_anim_set_user_data(&a, f);
  lv_anim_set_custom_exec_cb(&a, fade_exec);
  lv_anim_set_values(&a, 0, 255);
  lv_anim_set_duration(&a, ms);
  lv_anim_set_path_cb(&a, mo::path(mo::fade));
  lv_anim_set_deleted_cb(&a, fade_free);
  lv_anim_start(&a);
}
}  // namespace
}  // namespace st
}  // namespace fw
