// Atoms: focus ring + corner ticks (atoms.h). Pure moves of the code Button / Icon button drew inline (frame-identical,
// hashed before and after at both densities, both themes, every accent set).
#include "fw.h"

namespace fw {

void focus_ring(lv_obj_t *o, lv_style_selector_t show, int offset) {
  lv_obj_set_style_outline_color(o, st::focus_ring(*T), 0);
  lv_obj_set_style_outline_width(o, 0, 0);
  lv_obj_set_style_outline_width(o, st::FOCUS_W, show);
  lv_obj_set_style_outline_pad(o, offset - st::FOCUS_W, show);
  lv_obj_set_style_outline_width(o, 0, st::OFF);  // disabled / unavailable hide it, even when active
  bleed(o, offset);                                // the ring sits outside the box: parents must not clip it
}

namespace {
lv_obj_t *tick(lv_obj_t *o, int len, int thick, lv_border_side_t sides, lv_style_selector_t show) {
  lv_obj_t *t = lv_obj_create(o);
  lv_obj_remove_style_all(t);
  lv_obj_add_flag(t, LV_OBJ_FLAG_IGNORE_LAYOUT);
  lv_obj_remove_flag(t, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_size(t, len, len);
  lv_obj_set_style_border_side(t, sides, 0);
  lv_obj_set_style_border_width(t, thick, 0);
  lv_obj_set_style_border_color(t, st::on_mark(*T), 0);
  lv_obj_set_style_border_opa(t, show ? LV_OPA_TRANSP : LV_OPA_COVER, 0);
  if (show) lv_obj_set_style_border_opa(t, LV_OPA_COVER, show);
  // pressed (the owner trickles it): accent-text, so the ticks keep 3:1 on the pressed grey (states.json on.mark-pressed)
  lv_obj_set_style_border_color(t, st::on_mark_pressed(*T), LV_STATE_PRESSED);
  return t;
}
}  // namespace

void corner_ticks(lv_obj_t *o, Corners c, lv_style_selector_t show, int len, int thick) {
  // children align inside padding + border: step back by both so the ticks sit on the border box
  const int sl = lv_obj_get_style_space_left(o, LV_PART_MAIN), st = lv_obj_get_style_space_top(o, LV_PART_MAIN);
  const int sr = lv_obj_get_style_space_right(o, LV_PART_MAIN), sb = lv_obj_get_style_space_bottom(o, LV_PART_MAIN);
  const auto side = [](int a, int b) { return (lv_border_side_t) (a | b); };
  const bool all = c == Corners::all;
  if (all) lv_obj_align(tick(o, len, thick, side(LV_BORDER_SIDE_TOP, LV_BORDER_SIDE_LEFT), show), LV_ALIGN_TOP_LEFT, -sl, -st);
  lv_obj_align(tick(o, len, thick, side(LV_BORDER_SIDE_TOP, LV_BORDER_SIDE_RIGHT), show), LV_ALIGN_TOP_RIGHT, sr, -st);
  lv_obj_align(tick(o, len, thick, side(LV_BORDER_SIDE_BOTTOM, LV_BORDER_SIDE_LEFT), show), LV_ALIGN_BOTTOM_LEFT, -sl, sb);
  if (all) lv_obj_align(tick(o, len, thick, side(LV_BORDER_SIDE_BOTTOM, LV_BORDER_SIDE_RIGHT), show), LV_ALIGN_BOTTOM_RIGHT, sr, sb);
}

}  // namespace fw
