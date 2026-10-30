// sheet.cpp: the Bottom sheet's overlay layer (core/sheet.h says what it is). Moved out of components/bottom_sheet.cpp
// when the component went on the contract; the motion and the bench notes are as they were there.
// Performance (Guition: the first sheet ran at 12 fps, README "Motion tokens"): every frame of the open / close
// redraws the whole screen, because the scrim fades over all of it and the panel (most of the screen) fades and
// moves. Kept small where it can be: the scrim is ONE full-screen rect whose bg_opa animates (no layer, no per-child
// opacity), the panel moves as ONE object (translate_y + opa on the panel only, its rows never re-layout), nothing
// else is invalidated. Browser bench 2026-10-22 (8x CPU throttle, open + close looping): the cost is the blending
// area, not the objects: flattening the screen under the sheet into one opaque lv_snapshot image measured no gain, a
// panel snapshot image moving instead of the panel was worse (ARGB blend), an opaque panel that only rises was
// ~15-18 % cheaper. Kept as the bench options flatten and solid.
#include "fw.h"
#include "fx.h"
#include "device.h"
#include "sheet.h"

namespace fw {
namespace sheet {
namespace {
struct Layer {
  lv_obj_t *host = nullptr, *ov = nullptr, *scrim = nullptr, *panel = nullptr;
  lv_obj_t *snap = nullptr;  // flatten: the screen under the sheet as one image while it animates
  lv_draw_buf_t *buf = nullptr;
  bool open = false, flatten = false, solid = false, busy = false;
  int rows = 0, row_h = 0;  // the last fit (a retune fits again)
};
// one hair past the left / right edges and the ring radius past the bottom: the full hair edge with its rounded top
// corners shows, its sides and bottom fall off the screen (Darren 2026-10-22: a top-only border drew square corners)
void pads(Layer *L) {
  const int in = fx::size("layout:inset"), hair = fx::size("stroke:hair");
  lv_obj_set_style_pad_top(L->panel, in - hair, 0);
  lv_obj_set_style_pad_hor(L->panel, in, 0);
  lv_obj_set_style_pad_bottom(L->panel, in + fx::size("radius:ring") - hair, 0);  // + the (off-screen) bottom edge
  lv_obj_set_style_pad_row(L->panel, fx::size("layout:gutter"), 0);
}
void layer_free(lv_event_t *e) {
  auto *L = (Layer *) lv_event_get_user_data(e);
  lv_anim_delete(L->scrim, nullptr);
  lv_anim_delete(L->panel, nullptr);
  if (L->snap) lv_obj_delete(L->snap);  // the image first, then its buffer (the overlay's children go after this event)
  if (L->buf) lv_draw_buf_destroy(L->buf);
  delete L;
}
Layer *of(lv_obj_t *ov) {  // the Layer rides on the overlay's DELETE event (its user data is the caller's)
  if (!ov) return nullptr;
  for (uint32_t i = 0, n = lv_obj_get_event_count(ov); i < n; i++) {
    lv_event_dsc_t *d = lv_obj_get_event_dsc(ov, i);
    if (lv_event_dsc_get_cb(d) == layer_free) return (Layer *) lv_event_dsc_get_user_data(d);
  }
  return nullptr;
}
lv_obj_t *bare(lv_obj_t *parent) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  return o;
}

void set_bg_opa(void *o, int32_t v) { lv_obj_set_style_bg_opa((lv_obj_t *) o, (lv_opa_t) v, 0); }
void set_ty(void *o, int32_t v) { lv_obj_set_style_translate_y((lv_obj_t *) o, v, 0); }
// scrim: colour.json scrim (near-black in both themes) at motion.json opacity scrim (dark) / scrim-light (Darren
// 2026-10-22: the backdrop must read clearly darker, not just faded towards the bg)
lv_opa_t scrim_opa() { return (lv_opa_t) lroundf((fw::T->light ? fw::mo::OPA_SCRIM_LIGHT : fw::mo::OPA_SCRIM) * 255); }

void unflatten(Layer *L) {
  if (L->snap) { lv_obj_delete(L->snap); L->snap = nullptr; }
  if (L->buf) { lv_draw_buf_destroy(L->buf); L->buf = nullptr; }
}
void flatten(Layer *L) {
  if (!L->flatten || L->snap) return;
  const bool was_hidden = lv_obj_has_flag(L->ov, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(L->ov, LV_OBJ_FLAG_HIDDEN);
  lv_obj_update_layout(L->host);
  L->buf = lv_snapshot_take(L->host, LV_COLOR_FORMAT_RGB565);  // the panels' format; no alpha = it covers
  if (!was_hidden) lv_obj_remove_flag(L->ov, LV_OBJ_FLAG_HIDDEN);
  if (!L->buf) return;
  L->snap = lv_image_create(L->ov);
  lv_obj_remove_flag(L->snap, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_add_flag(L->snap, LV_OBJ_FLAG_IGNORE_LAYOUT);
  lv_obj_move_to_index(L->snap, 0);  // under the scrim and the panel
  lv_image_set_src(L->snap, L->buf);
  const int32_t ext = ((int32_t) L->buf->header.w - lv_obj_get_width(L->host)) / 2;  // snapshot = box + ext draw
  lv_obj_set_pos(L->snap, -ext, -ext);
}
void opened(lv_anim_t *a) { unflatten((Layer *) lv_anim_get_user_data(a)); }
void closed(lv_anim_t *a) {
  auto *L = (Layer *) lv_anim_get_user_data(a);
  unflatten(L);
  if (!L->open) {
    set_bg_opa(L->scrim, LV_OPA_TRANSP);  // no fades: the scrim stayed while the panel dropped, then snaps off
    lv_obj_add_flag(L->ov, LV_OBJ_FLAG_HIDDEN);
  }
}
void dismiss(Layer *L, const char *how) {  // the scrim or the key: closes like Close does
  fw::emit(L->scrim, "close", how);
  fw::sound(L->scrim, "back");
  close(L->ov);
}
}  // namespace

lv_obj_t *make(lv_obj_t *host, bool flat, bool solid) {
  auto *L = new Layer;
  L->host = host;
  L->flatten = flat;
  L->solid = solid;
  L->ov = bare(host);
  lv_obj_add_flag(L->ov, (lv_obj_flag_t) (LV_OBJ_FLAG_FLOATING | LV_OBJ_FLAG_IGNORE_LAYOUT | LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_USER_4));
  lv_obj_set_size(L->ov, LV_PCT(100), LV_PCT(100));
  lv_obj_set_pos(L->ov, 0, 0);
  fw::qa::free(L->ov, "overlay: bottom sheet");  // sits over the screen on purpose (tools/audit_screens.mjs skips it)
  lv_obj_add_event_cb(L->ov, layer_free, LV_EVENT_DELETE, L);

  L->scrim = bare(L->ov);
  lv_obj_add_flag(L->scrim, LV_OBJ_FLAG_CLICKABLE);
  fx::use(L->scrim, "fill");
  fx::use(L->scrim, "bg:scrim");
  set_bg_opa(L->scrim, LV_OPA_TRANSP);  // local: the open / close animate it
  lv_obj_add_event_cb(L->scrim, [](lv_event_t *e) {
    auto *L = (Layer *) lv_event_get_user_data(e);
    if (L->open) dismiss(L, "scrim");
  }, LV_EVENT_CLICKED, L);
  fw::dev::claim_button(L->ov, fw::dev::BUTTON_SHEET, [](void *u, const char *press) {
    auto *L = (Layer *) u;
    if (!L->open || strcmp(press, "single")) return false;
    dismiss(L, "button");
    return true;
  }, L);

  L->panel = bare(L->ov);
  lv_obj_add_flag(L->panel, LV_OBJ_FLAG_CLICKABLE);  // taps on the panel never reach the scrim
  fx::use(L->panel, "bg:bg");
  fx::use(L->panel, "border:line-hi");
  fx::use(L->panel, "radius:ring");
  pads(L);
  lv_obj_set_flex_flow(L->panel, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_opa(L->panel, LV_OPA_TRANSP, 0);
  fit(L->ov, 0);
  // a token tuned / a theme switched: the panel's padding and height again from the live tokens
  lv_obj_add_event_cb(L->ov, [](lv_event_t *e) {
    auto *L = (Layer *) lv_event_get_user_data(e);
    if (lv_event_get_target(e) != L->ov || L->busy) return;
    L->busy = true;
    pads(L);
    fit(L->ov, L->rows, L->row_h);
    L->busy = false;
  }, LV_EVENT_STYLE_CHANGED, L);
  return L->ov;
}

lv_obj_t *panel(lv_obj_t *ov) { Layer *L = of(ov); return L ? L->panel : nullptr; }
bool is_open(lv_obj_t *ov) { Layer *L = of(ov); return L && L->open; }
lv_obj_t *open_on(lv_obj_t *o) {
  if (lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN)) return nullptr;
  if (Layer *L = of(o)) return L->open ? o : nullptr;
  for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++)
    if (lv_obj_t *f = open_on(lv_obj_get_child(o, (int32_t) i))) return f;
  return nullptr;
}
int open_close_ms() { return fw::mo::ms::sheet_in + fw::mo::ms::sheet_out; }

// the panel's height = its content (padding + head + gutter + the rows + the part past the bottom), capped at the
// tallest sheet (from sheet-top to the bottom; the list scrolls only then), its top on the grid: every term is a
// layout token on the 4 px grid. The overlay's own box, not the host's: a caller may stretch the overlay over the
// whole screen from inside a smaller box (the Thermostat and Calendar views); the panel follows it (width too).
void fit(lv_obj_t *ov, int rows, int row_h) {
  Layer *L = of(ov);
  if (!L) return;
  L->rows = rows, L->row_h = row_h;
  if (!L->busy) lv_obj_update_layout(L->host);
  // the overlay covers the host's whole box, its padding too (a spec Page pads its top: C4, over the whole screen)
  const int pl = lv_obj_get_style_pad_left(L->host, LV_PART_MAIN), pt = lv_obj_get_style_pad_top(L->host, LV_PART_MAIN);
  if (pl || pt || lv_obj_get_style_pad_right(L->host, LV_PART_MAIN) || lv_obj_get_style_pad_bottom(L->host, LV_PART_MAIN)) {
    lv_obj_set_pos(L->ov, -pl, -pt);
    lv_obj_set_size(L->ov, lv_obj_get_width(L->host), lv_obj_get_height(L->host));
  }
  const int ring = fx::size("radius:ring"), hair = fx::size("stroke:hair"), in = fx::size("layout:inset");
  const int W = lv_obj_get_style_width(L->ov, LV_PART_MAIN) > 0 && !LV_COORD_IS_SPEC(lv_obj_get_style_width(L->ov, LV_PART_MAIN)) ? lv_obj_get_style_width(L->ov, LV_PART_MAIN) : lv_obj_get_width(L->ov);
  const int H = lv_obj_get_style_height(L->ov, LV_PART_MAIN) > 0 && !LV_COORD_IS_SPEC(lv_obj_get_style_height(L->ov, LV_PART_MAIN)) ? lv_obj_get_style_height(L->ov, LV_PART_MAIN) : lv_obj_get_height(L->ov);
  const int max_h = H - fx::size("layout:sheet-top") + ring;
  lv_obj_set_width(L->panel, W + 2 * hair);
  lv_obj_set_x(L->panel, -hair);
  const int need = 2 * in + fx::size("layout:row") + fx::size("layout:gutter") + rows * (row_h > 0 ? row_h : fx::size("layout:sheet-row-h")) + ring;
  const int h = std::min(max_h, need);
  lv_obj_set_height(L->panel, h);
  lv_obj_set_y(L->panel, H + ring - h);
}

void open(lv_obj_t *ov, bool still) {
  Layer *L = of(ov);
  if (!L || L->open) return;
  L->open = true;
  lv_obj_remove_flag(L->ov, LV_OBJ_FLAG_HIDDEN);
  if (still) {
    set_bg_opa(L->scrim, scrim_opa());
    lv_obj_set_style_opa(L->panel, LV_OPA_COVER, 0);
    lv_obj_set_style_translate_y(L->panel, 0, 0);
    return;
  }
  if (!fw::mo::fades(L->panel)) {  // no fades (core/fw.h, Guition): the scrim snaps on, the opaque panel slides up
    lv_anim_delete(L->scrim, set_bg_opa);
    lv_anim_delete(L->panel, nullptr);
    set_bg_opa(L->scrim, scrim_opa());
    lv_obj_set_style_opa(L->panel, LV_OPA_COVER, 0);
    lv_obj_update_layout(L->panel);
    const int32_t ty = lv_obj_get_style_translate_y(L->panel, LV_PART_MAIN);
    fw::Anim m{set_ty, ty ? ty : lv_obj_get_height(L->panel), 0, fw::mo::MOVE};
    m.done = opened; m.user = L;
    fw::animate(L->panel, fw::mo::sheet_open, m);
    return;
  }
  if (lv_obj_get_style_opa(L->panel, LV_PART_MAIN) == LV_OPA_TRANSP) set_bg_opa(L->scrim, LV_OPA_TRANSP);
  flatten(L);
  fw::animate(L->scrim, fw::mo::sheet_open, {set_bg_opa, lv_obj_get_style_bg_opa(L->scrim, LV_PART_MAIN), scrim_opa(), fw::mo::OPA});
  if (L->solid) {  // bench option: the panel opaque, rising only (it covers, so LVGL skips what is under it)
    lv_obj_set_style_opa(L->panel, LV_OPA_COVER, 0);
    fw::Anim m{set_ty, fw::mo::pat(fw::mo::sheet_open).offset, 0, fw::mo::MOVE};
    m.done = opened; m.user = L;
    fw::animate(L->panel, fw::mo::sheet_open, m);
  } else fw::appear(L->panel, fw::mo::sheet_open, true, 1, 0, opened, L);
}

void close(lv_obj_t *ov) {
  Layer *L = of(ov);
  if (!L || !L->open) return;
  L->open = false;
  if (!fw::mo::fades(L->panel)) {  // no fades: the panel slides down (translate only), the scrim snaps off at the end
    lv_anim_delete(L->panel, nullptr);
    lv_obj_set_style_opa(L->panel, LV_OPA_COVER, 0);
    fw::Anim m{set_ty, lv_obj_get_style_translate_y(L->panel, LV_PART_MAIN), lv_obj_get_height(L->panel), fw::mo::MOVE};
    m.done = closed; m.user = L;
    fw::animate(L->panel, fw::mo::sheet_close, m);
    return;
  }
  flatten(L);
  fw::animate(L->scrim, fw::mo::sheet_close, {set_bg_opa, lv_obj_get_style_bg_opa(L->scrim, LV_PART_MAIN), LV_OPA_TRANSP, fw::mo::OPA});
  if (L->solid) {
    fw::Anim m{set_ty, lv_obj_get_style_translate_y(L->panel, LV_PART_MAIN), lv_obj_get_height(L->panel), fw::mo::MOVE};
    m.done = closed; m.user = L;
    fw::animate(L->panel, fw::mo::sheet_close, m);
  } else fw::appear(L->panel, fw::mo::sheet_close, false, 1, 0, closed, L);
}
}  // namespace sheet
}  // namespace fw
