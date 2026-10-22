// Bench probes (facet-153): what the headless bench runner (tools/bench.mjs) reads from a WASM build, per refresh of
// every display. Same window as the panels' GET /fw/perf (firmware/fw_device.h): LVGL RENDER_START -> RENDER_READY =
// render + flush. WASM only (emscripten.h): not in the panel bundle (firmware/bundle.mjs lists its core files).
//   fw_bench_attach()       hook every display (idempotent) and zero the counters
//   fw_bench_take() -> f64[8] since the last call: [renders, render_ms, render_ms_max, flush_ms, px, areas, refr_ms, full]
//                           render_ms = sum of RENDER_START..READY (incl. the host flush), flush_ms = the host flush part
//                           (RGB565 -> RGBA copy, a workbench cost), px = pixels flushed, areas = flushed areas,
//                           refr_ms = REFR_START..READY (layout + render), full = refreshes that flushed the whole display
//   fw_bench_mem() -> f64[4] [heap bytes in use (malloc), heap bytes reserved (sbrk), LVGL objects on the default
//                           display (screens + layers), running animations]
//   fw_bench_nav(id, back)  the device runtime on the default display: back = 0 go(id), 1 back, 2 home ("" = ok / error)
#include <emscripten.h>
#include <malloc.h>
#include <unistd.h>
#include <vector>
#include "fw.h"
#include "device.h"

namespace {
struct Acc {
  double renders = 0, render_ms = 0, render_max = 0, flush_ms = 0, px = 0, areas = 0, refr_ms = 0, full = 0;
};
Acc acc;
double t_refr = 0, t_render = 0, t_flush = 0, frame_px = 0;
std::vector<lv_display_t *> hooked;

void on_disp(lv_event_t *e) {
  const double t = emscripten_get_now();
  auto *d = (lv_display_t *) lv_event_get_current_target(e);
  switch (lv_event_get_code(e)) {
    case LV_EVENT_REFR_START: t_refr = t; break;
    case LV_EVENT_REFR_READY: acc.refr_ms += t - t_refr; break;
    case LV_EVENT_RENDER_START: t_render = t; frame_px = 0; break;
    case LV_EVENT_RENDER_READY: {
      const double r = t - t_render;
      acc.renders++;
      acc.render_ms += r;
      if (r > acc.render_max) acc.render_max = r;
      acc.px += frame_px;
      if (frame_px >= (double) lv_display_get_horizontal_resolution(d) * lv_display_get_vertical_resolution(d)) acc.full++;
      break;
    }
    case LV_EVENT_FLUSH_START: {
      t_flush = t;
      auto *a = (const lv_area_t *) lv_event_get_param(e);
      if (a) { frame_px += (double) lv_area_get_size(a); acc.areas++; }
      break;
    }
    case LV_EVENT_FLUSH_FINISH: acc.flush_ms += t - t_flush; break;
    default: break;
  }
}

int count(lv_obj_t *o) {
  if (!o) return 0;
  int n = 1;
  for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) n += count(lv_obj_get_child(o, i));
  return n;
}
}  // namespace

extern "C" {
EMSCRIPTEN_KEEPALIVE void fw_bench_attach() {
  for (lv_display_t *d = lv_display_get_next(nullptr); d; d = lv_display_get_next(d)) {
    bool have = false;
    for (auto *h : hooked) have |= h == d;
    if (have) continue;
    for (auto c : {LV_EVENT_REFR_START, LV_EVENT_REFR_READY, LV_EVENT_RENDER_START, LV_EVENT_RENDER_READY,
                   LV_EVENT_FLUSH_START, LV_EVENT_FLUSH_FINISH})
      lv_display_add_event_cb(d, on_disp, c, nullptr);
    hooked.push_back(d);
  }
  acc = Acc{};
}

EMSCRIPTEN_KEEPALIVE const double *fw_bench_take() {
  static double out[8];
  out[0] = acc.renders; out[1] = acc.render_ms; out[2] = acc.render_max; out[3] = acc.flush_ms;
  out[4] = acc.px; out[5] = acc.areas; out[6] = acc.refr_ms; out[7] = acc.full;
  acc = Acc{};
  return out;
}

EMSCRIPTEN_KEEPALIVE const double *fw_bench_mem() {
  static double out[4];
  struct mallinfo mi = mallinfo();
  out[0] = (double) mi.uordblks;
  out[1] = (double) (uintptr_t) sbrk(0);
  lv_display_t *d = lv_display_get_default();
  out[2] = d ? count(lv_display_get_screen_active(d)) + count(lv_display_get_layer_top(d)) +
                   count(lv_display_get_layer_sys(d)) + count(lv_display_get_layer_bottom(d))
             : 0;
  out[3] = lv_anim_count_running();
  return out;
}

EMSCRIPTEN_KEEPALIVE const char *fw_bench_nav(const char *id, int back) {
  lv_display_t *d = lv_display_get_default();
  if (!d) return "no display";
  lv_obj_t *scr = lv_display_get_screen_active(d);
  for (uint32_t i = 0; i < lv_obj_get_child_count(scr); i++) {
    lv_obj_t *c = lv_obj_get_child(scr, i);
    if (!*fw::dev::current(c)) continue;
    if (back == 1) fw::dev::back(c);
    else if (back == 2) fw::dev::home(c);
    else fw::dev::go(c, id);
    return "";
  }
  return "no device runtime on this stage (render the device screen)";
}
}
