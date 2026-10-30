// Hold button: a Button for an action that needs a confirm step (open the door's latch): the real Button component
// (secondary, fw::find) that fills with accent-dim from the left (drawn on the Button, its rounded shape clipped to
// the held share: no extra object) while held for Motion hold-confirm (1.2 s; the panel
// lab's "hold to open door"); let go early and the fill drains (Motion release), nothing happens. Full = sound
// confirm + event "confirm" (detail = prop value) for the spec's action. The fill is the feedback (the label stays).
//   width    fill = its parent's width (a Split column), hug = the label's
//   features / mask: shown only when the entity supports it (features & mask; mask 0 = always): an HA entity's
//            supported_features, so a lock without a latch has no Open button
// Disabled (prop state): the Button's disabled look, a press = blocked + error.
// Update path: the Button through its own update; shown / hidden by the features. Contrast: section "hold_button"
// (+ button).
// audit deps: button
#include "fw.h"

namespace {
struct HB {
  lv_obj_t *btn = nullptr;
  int prog = 0;  // the held share, 0..1000
  std::string label, value, width = "fill", state = "default";
  bool holding = false;
};
HB *of(lv_obj_t *o) { return (HB *) lv_obj_get_user_data(o); }
int width_dp(lv_obj_t *root, const HB *h) {
  if (h->width != "fill") return 0;
  lv_obj_t *p = lv_obj_get_parent(root);
  lv_obj_update_layout(p);
  return (int) std::lround(std::min(lv_obj_get_content_width(p), (int32_t) fw::lay::card_w) / fw::D);  // at most card-w
}
void button_props(lv_obj_t *root, HB *h, ArduinoJson::JsonDocument &d) {
  d["kind"] = "secondary", d["tap"] = false, d["label"] = h->label, d["state"] = h->state, d["width"] = width_dp(root, h);
}
void relabel(lv_obj_t *root, HB *h) {
  ArduinoJson::JsonDocument d;
  button_props(root, h, d);
  fw::update_child(fw::find("button"), h->btn, d.as<fw::Props>());

}
// the held share 0..1000 (on the root's HB; the anim runs on the Button, which it invalidates)
void fill_w(void *o, int32_t v) {
  auto *btn = (lv_obj_t *) o;
  if (HB *h = of(lv_obj_get_parent(btn))) h->prog = v;
  lv_obj_invalidate(btn);
}
void take(lv_obj_t *root, HB *h, fw::Props p) {
  h->label = p["label"] | "Hold to open";
  h->value = p["value"] | "open";
  h->width = p["width"] | "fill";
  h->state = p["state"] | "default";
  const int mask = p["mask"] | 0, feat = (int) (p["features"] | 0.0);
  if (mask && !(feat & mask)) lv_obj_add_flag(root, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_remove_flag(root, LV_OBJ_FLAG_HIDDEN);
}
}  // namespace

FW_COMPONENT(hold_button, "Hold button", "components", R"({
  "about": "A button for an action that needs a confirm: it fills while you hold it, and only acts when full.",
  "frame": [320, 0],
  "props": [
    {"name": "label", "about": "The words on the button, such as Hold to open.", "type": "text", "default": "Hold to open"},
    {"name": "value", "about": "The action the button sends when the hold completes, such as open.", "type": "text", "default": "open"},
    {"name": "width", "about": "fill stretches the button across the space it is in; hug fits it to its words.", "type": "select", "options": ["fill", "hug"], "default": "fill"},
    {"name": "features", "about": "What the device can do, as Home Assistant reports it in its supported features number.", "type": "range", "min": 0, "max": 65535, "step": 1, "default": 1},
    {"name": "mask", "about": "The feature this button needs. If the device's features do not include it, the button is hidden. 0 always shows it.", "type": "range", "min": 0, "max": 65535, "step": 1, "default": 0},
    {"name": "state", "debug": true, "about": "default can be held; disabled is greyed out and cannot be used.", "type": "select", "options": ["default", "disabled"], "default": "default"}
  ],
  "variants": [
    {"label": "default", "props": {}},
    {"label": "disabled", "props": {"state": "disabled"}}
  ]
})") {
  auto *h = new HB;
  lv_obj_t *root = lv_obj_create(parent);
  lv_obj_remove_style_all(root);
  lv_obj_remove_flag(root, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_size(root, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_user_data(root, h);
  lv_obj_add_event_cb(root, [](lv_event_t *e) { delete (HB *) lv_event_get_user_data(e); }, LV_EVENT_DELETE, h);
  take(root, h, p);
  ArduinoJson::JsonDocument d;
  button_props(root, h, d);
  if (const fw::Component *b = fw::find("button")) b->build(root, d.as<fw::Props>());
  h->btn = lv_obj_get_child_count(root) ? lv_obj_get_child(root, 0) : nullptr;
  if (!h->btn) return;
  if (const int w = width_dp(root, h)) lv_obj_set_width(h->btn, fw::px((float) w));  // fill: the Button fills what it is given
  // the fill: drawn on the Button itself (no child object, one invalidation a frame), between its fill and its label,
  // the button's rounded shape clipped to the held share
  lv_obj_add_event_cb(h->btn, [](lv_event_t *e) {
    auto *root = (lv_obj_t *) lv_event_get_user_data(e);
    HB *h = of(root);
    if (!h || h->prog <= 0) return;
    lv_layer_t *layer = lv_event_get_layer(e);
    lv_area_t a, clip;
    lv_obj_get_coords(h->btn, &a);
    lv_area_t part = a;
    part.x2 = a.x1 + (int32_t) ((int64_t) lv_area_get_width(&a) * h->prog / 1000) - 1;
    const lv_area_t &lc = layer->_clip_area;
    clip = {LV_MAX(part.x1, lc.x1), LV_MAX(part.y1, lc.y1), LV_MIN(part.x2, lc.x2), LV_MIN(part.y2, lc.y2)};
    if (clip.x1 > clip.x2 || clip.y1 > clip.y2) return;
    const lv_area_t keep = layer->_clip_area;
    layer->_clip_area = clip;
    lv_draw_rect_dsc_t d;
    lv_draw_rect_dsc_init(&d);
    d.bg_color = fw::T->accent_dim, d.bg_opa = LV_OPA_COVER, d.radius = fw::rad::button;
    lv_draw_rect(layer, &d, &a);
    layer->_clip_area = keep;
  }, LV_EVENT_DRAW_MAIN, root);
  const lv_event_cb_t cb = [](lv_event_t *e) {
    auto *root = (lv_obj_t *) lv_event_get_user_data(e);
    HB *h = of(root);
    const lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_PRESSED && h->state != "disabled") {
      h->holding = true;
      lv_anim_t a;
      lv_anim_init(&a);
      lv_anim_set_var(&a, h->btn);
      lv_anim_set_values(&a, h->prog, 1000);
      lv_anim_set_duration(&a, fw::mo::ms::hold_confirm * (1000 - h->prog) / 1000);
      lv_anim_set_path_cb(&a, fw::mo::ease::linear);
      lv_anim_set_exec_cb(&a, fill_w);
      lv_anim_set_user_data(&a, root);
      lv_anim_set_completed_cb(&a, [](lv_anim_t *a) {
        auto *root = (lv_obj_t *) lv_anim_get_user_data(a);
        HB *h = of(root);
        h->holding = false;
        h->prog = 0;
        lv_obj_invalidate(h->btn);
        fw::sound(h->btn, "confirm");
        fw::emit(root, "confirm", h->value.c_str());
        for (lv_indev_t *in = lv_indev_get_next(nullptr); in; in = lv_indev_get_next(in))  // letting go now is not
          if (lv_indev_get_type(in) == LV_INDEV_TYPE_POINTER) lv_indev_wait_release(in);     // also a tap
      });
      lv_anim_start(&a);
    } else if ((code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) && h->holding) {
      h->holding = false;
      lv_anim_delete(h->btn, fill_w);
      if (h->prog > 0) fw::animate(h->btn, fw::mo::release, {fill_w, h->prog, 0, fw::mo::VALUE});
    }
  };
  for (lv_event_code_t ev : {LV_EVENT_PRESSED, LV_EVENT_RELEASED, LV_EVENT_PRESS_LOST})  // not ALL: no delete-time events
    lv_obj_add_event_cb(h->btn, cb, ev, root);
}

FW_UPDATE(hold_button) {
  HB *h = of(root);
  if (!h || !h->btn) return false;
  take(root, h, p);
  relabel(root, h);
  return true;
}
