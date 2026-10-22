// Facet bench on the panel (facet-153, renderer/bench/README.md "Phase 2"): the panel tests itself. The host sends a
// whole batch at once; the panel runs the cases on its own screen one after another (visibly: a static "Bench n/N" chip
// on the top layer, changed between cases only), measures itself, keeps the results in RAM and hands back the report
// at the end. No per-case requests and no polling while it runs. Included by fw_device.h (inside namespace fw::device;
// uses S, show(), in_loop()).
//   POST /fw/bench {run: "<id>", theme?, timing?: {settle_ms, window_ms, drag_ms, after_ms, nav_ms},
//                   cases: [[case, component, props (object), scenario, path?: [[x0,y0],[x1,y1]], to?], ...]}
//        -> 200 {ok, run, total, eta_s} at once (409 while a batch runs; 400 over FW_BENCH_MAX cases or a bad body)
//   GET  /fw/bench -> {run, state: idle|running|done|interrupted, done, total, results: [row...]}
//        row = {case, scenario, error?, panel: {frames, active_ms, redraw_fps, fps, slow, build_ms, render_ms_avg|p95|max,
//        px_avg, px_pct, areas_avg, full_redraws}, mem: {heap_kb, internal_free_after, psram_free_after, internal_largest,
//        psram_largest, objs}}. interrupted = the panel restarted during a batch (RTC memory survives a crash / reset).
// Frame window = LVGL RENDER_START..RENDER_READY (render + flush), as GET /fw/perf; p95 from a ring of the last 512
// frames. SPIRAM largest_free_block is read only between cases, NEVER while frames run (Darren 2026-10-22). Drags go
// through a second LVGL pointer indev, so they take the same input path as a finger. At the end the panel shows again
// what it showed before the batch.
// Only expands inside fw_device.h (ESPHome's esphome.h includes every component header on its own).
#if defined(FW_BENCH_FROM_DEVICE) && !defined(FW_BENCH_H)
#define FW_BENCH_H
// (included inside namespace fw::device: no includes or namespace of its own here)
namespace bench {

#ifndef FW_BENCH_MAX
#define FW_BENCH_MAX 96
#endif
enum Phase : uint8_t { IDLE, BUILD, SETTLE, WINDOW, DRAG, AFTER, NAV1, NAV2, GAP, DONE };
enum Scen : uint8_t { S_IDLE, S_ANIMATE, S_DRAG, S_NAV };
struct Case {
  std::string name, comp, req;  // req = the /fw/show body for this case
  Scen sc = S_ANIMATE;
  float path[4] = {0.2f, 0.5f, 0.8f, 0.5f};
  std::string to = "settings";
};
struct Row {
  bool ok = false;
  std::string error;
  uint32_t frames = 0, slow = 0, full = 0, active_ms = 0, objs = 0, px_avg = 0;
  float redraw_fps = 0, fps = 0, build_ms = 0, avg = 0, p95 = 0, max = 0, px_pct = 0, areas_avg = 0;
  int heap_kb = 0;
  size_t int_free = 0, ps_free = 0, int_largest = 0, ps_largest = 0;
};
struct Batch {
  std::string run;
  Phase ph = IDLE;
  std::vector<Case> cases;
  std::vector<Row> rows;
  size_t i = 0;
  uint32_t settle_ms = 1000, window_ms = 2000, drag_ms = 600, after_ms = 400, nav_ms = 1200;
  std::string before;  // what the panel showed before the batch (shown again at the end)
  int64_t t_phase = 0, t_build = 0;
  bool gap_drawn = false;
  // the case being measured
  bool counting = false, first_pending = false;
  int64_t t_render = 0, first_frame = 0, last_frame = 0, build_us = 0;
  uint32_t frames = 0, intervals = 0, slow = 0, areas = 0, full = 0, render_max = 0;
  uint64_t render_us = 0, active_us = 0, px = 0, frame_px = 0;
  uint16_t ring[512];
  size_t int0 = 0, ps0 = 0;
  bool down = false;
  lv_point_t pt{};
};
inline Batch B;
inline lv_indev_t *indev = nullptr;
inline lv_timer_t *timer = nullptr;
inline lv_obj_t *chip = nullptr;
inline bool hooked = false;
// survives a crash / software reset (not a power cut): a batch that was running when the panel restarted
RTC_NOINIT_ATTR inline uint32_t rtc_magic;
RTC_NOINIT_ATTR inline char rtc_run[24];
constexpr uint32_t RUNNING = 0xBE7C4153u;

inline void on_disp(lv_event_t *e) {
  if (B.ph == IDLE || B.ph == DONE) return;
  const int64_t t = esp_timer_get_time();
  switch (lv_event_get_code(e)) {
    case LV_EVENT_RENDER_START: B.t_render = t; B.frame_px = 0; break;
    case LV_EVENT_FLUSH_START: {
      auto *a = (const lv_area_t *) lv_event_get_param(e);
      if (a) { B.frame_px += (uint64_t) lv_area_get_size(a); if (B.counting) B.areas++; }
      break;
    }
    case LV_EVENT_RENDER_READY: {
      if (B.first_pending) { B.build_us = t - B.t_build; B.first_pending = false; break; }  // the build's first frame
      if (!B.counting) break;
      const uint32_t r = (uint32_t) (t - B.t_render);
      if (B.last_frame) {
        const int64_t gap = B.t_render - B.last_frame;
        B.intervals++;
        B.active_us += gap;
        if (gap * 2 > (int64_t) LV_DEF_REFR_PERIOD * 1000 * 3) B.slow++;
      } else B.first_frame = B.t_render;
      B.last_frame = B.t_render;
      B.ring[B.frames % 512] = (uint16_t) std::min<uint32_t>(r / 10, 65535);
      B.frames++;
      B.render_us += r;
      B.render_max = std::max(B.render_max, r);
      B.px += B.frame_px;
      if (B.frame_px >= (uint64_t) FW_DEVICE_W * FW_DEVICE_H) B.full++;
      break;
    }
    default: break;
  }
}
inline void read_cb(lv_indev_t *, lv_indev_data_t *d) {
  d->point = B.pt;
  d->state = B.down ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}
inline uint32_t count(lv_obj_t *o) {
  uint32_t n = 1;
  for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) n += count(lv_obj_get_child(o, i));
  return n;
}
inline lv_obj_t *device_root() {
  for (uint32_t i = 0; i < lv_obj_get_child_count(S.scr); i++)
    if (*::fw::dev::current(lv_obj_get_child(S.scr, i))) return lv_obj_get_child(S.scr, i);
  return nullptr;
}
inline void go(Phase p) { B.ph = p; B.t_phase = esp_timer_get_time(); }
inline void touch_at(float fx, float fy, bool down) {
  B.pt = {(int32_t) lroundf(fx * FW_DEVICE_W), (int32_t) lroundf(fy * FW_DEVICE_H)};
  B.down = down;
}
// the static "Bench n/N" chip on the top layer (the Status chip component), changed between cases only
inline void caption() {
  const fw::Component *cc = fw::find("status_chip");
  if (!cc) return;
  if (!chip) {
    chip = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(chip);
    lv_obj_remove_flag(chip, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
    lv_obj_set_size(chip, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align(chip, LV_ALIGN_BOTTOM_MID, 0, -fw::sp::s4);
    lv_obj_set_style_radius(chip, fw::rad::chip, 0);
    lv_obj_set_style_bg_opa(chip, LV_OPA_COVER, 0);
  }
  lv_obj_clean(chip);
  if (B.ph == IDLE || B.ph == DONE) { lv_obj_add_flag(chip, LV_OBJ_FLAG_HIDDEN); return; }
  lv_obj_remove_flag(chip, LV_OBJ_FLAG_HIDDEN);
  const fw::Theme *old = fw::T;
  fw::T = fw::theme(S.light, S.accent);
  lv_obj_set_style_bg_color(chip, fw::T->surface, 0);
  char t[32];
  snprintf(t, sizeof t, "Bench %u/%u", (unsigned) (B.i + 1), (unsigned) B.cases.size());
  ArduinoJson::JsonDocument p;
  p["tone"] = "info";
  p["text"] = t;
  cc->build(chip, p.as<fw::Props>());
  fw::T = old;
}
inline void end_case(const char *err) {
  B.counting = false;
  B.down = false;
  Row r;
  if (err) r.error = err;
  else {
    r.ok = true;
    const uint32_t n = B.frames;
    const int64_t active = n ? (B.last_frame - B.first_frame) + (int64_t) LV_DEF_REFR_PERIOD * 1000 : 0;
    const uint32_t m = std::min<uint32_t>(n, 512);
    std::vector<uint16_t> tmp(B.ring, B.ring + m);
    std::sort(tmp.begin(), tmp.end());
    r.frames = n;
    r.active_ms = (uint32_t) (active / 1000);
    r.redraw_fps = active ? n * 1e6f / active : 0;
    r.fps = B.active_us ? B.intervals * 1e6f / B.active_us : 0;
    r.slow = B.slow;
    r.build_ms = B.build_us / 1000.f;
    r.avg = n ? B.render_us / 1000.f / n : 0;
    r.p95 = m ? tmp[std::min<uint32_t>(m - 1, m * 95 / 100)] / 100.f : 0;
    r.max = B.render_max / 1000.f;
    r.px_avg = n ? (uint32_t) (B.px / n) : 0;
    r.px_pct = n ? 100.f * B.px / n / (FW_DEVICE_W * FW_DEVICE_H) : 0;
    r.areas_avg = n ? (float) B.areas / n : 0;
    r.full = B.full;
    r.int_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    r.ps_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    r.int_largest = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
    r.ps_largest = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);  // between cases: no frames running now
    r.heap_kb = (int) (((int64_t) B.int0 + B.ps0 - (int64_t) r.int_free - r.ps_free) / 1024);
    r.objs = count(lv_display_get_screen_active(lv_display_get_default()));
  }
  B.rows.push_back(std::move(r));
  B.i++;
  B.gap_drawn = false;
  go(GAP);
}
inline void finish() {
  go(DONE);
  rtc_magic = 0;
  caption();
  ArduinoJson::JsonDocument req, out;
  if (!B.before.empty() && B.before != "null" && !deserializeJson(req, B.before))
    show(req.as<ArduinoJson::JsonObjectConst>(), out.to<ArduinoJson::JsonObject>());
}

inline void step(lv_timer_t *) {
  if (B.ph == IDLE || B.ph == DONE) return;
  const uint32_t el = (uint32_t) ((esp_timer_get_time() - B.t_phase) / 1000);
  const Case *c = B.i < B.cases.size() ? &B.cases[B.i] : nullptr;
  switch (B.ph) {
    case GAP:  // between cases: the caption changes here only, then the next build after a short pause
      if (!c) { finish(); return; }
      if (!B.gap_drawn) { caption(); B.gap_drawn = true; }
      if (el < 150) return;
      go(BUILD);
      return;
    case BUILD: {
      B.counting = false;
      B.frames = B.intervals = B.slow = B.areas = B.full = B.render_max = 0;
      B.render_us = B.active_us = B.px = 0;
      B.first_frame = B.last_frame = 0;
      ArduinoJson::JsonDocument req, out;
      deserializeJson(req, c->req);
      B.int0 = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
      B.ps0 = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
      B.t_build = esp_timer_get_time();
      B.first_pending = true;
      if (!show(req.as<ArduinoJson::JsonObjectConst>(), out.to<ArduinoJson::JsonObject>())) { end_case(out["error"] | "show failed"); return; }
      if (c->sc == S_NAV && !device_root()) { end_case("navigate needs the device screen"); return; }
      if (c->sc == S_ANIMATE) { B.counting = true; go(WINDOW); }
      else go(SETTLE);
      return;
    }
    case SETTLE:
      if (el < B.settle_ms) return;
      B.counting = true;
      if (c->sc == S_DRAG) { touch_at(c->path[0], c->path[1], true); go(DRAG); }
      else if (c->sc == S_NAV) { ::fw::dev::go(device_root(), c->to.c_str()); go(NAV1); }
      else go(WINDOW);
      return;
    case WINDOW:
      if (el >= B.window_ms) end_case(nullptr);
      return;
    case DRAG: {
      const float k = std::min(1.f, (float) el / (float) B.drag_ms);
      touch_at(c->path[0] + (c->path[2] - c->path[0]) * k, c->path[1] + (c->path[3] - c->path[1]) * k, k < 1.f);
      if (k >= 1.f) go(AFTER);
      return;
    }
    case AFTER:
      if (el >= B.after_ms) end_case(nullptr);
      return;
    case NAV1:
      if (el < B.nav_ms) return;
      if (lv_obj_t *d = device_root()) ::fw::dev::back(d);
      go(NAV2);
      return;
    case NAV2:
      if (el >= B.nav_ms) end_case(nullptr);
      return;
    default: return;
  }
}

// POST body -> start the batch (main loop); "" or an error ("busy" = 409)
inline std::string start(ArduinoJson::JsonObjectConst b, uint32_t *total, uint32_t *eta_s) {
  if (B.ph != IDLE && B.ph != DONE) return "busy";
  auto cs = b["cases"].as<ArduinoJson::JsonArrayConst>();
  if (cs.isNull() || !cs.size()) return "cases missing";
  if (cs.size() > FW_BENCH_MAX) return "too many cases (max " + std::to_string(FW_BENCH_MAX) + ")";
  Batch nb;
  nb.run = b["run"] | "";
  auto tm = b["timing"];
  auto ms = [&](const char *k, uint32_t d) { return std::min<uint32_t>(tm[k] | d, 10000); };
  nb.settle_ms = ms("settle_ms", 1000); nb.window_ms = ms("window_ms", 2000); nb.drag_ms = ms("drag_ms", 600);
  nb.after_ms = ms("after_ms", 400); nb.nav_ms = ms("nav_ms", 1200);
  const char *theme = b["theme"] | "dark";
  uint64_t eta = 0;
  for (auto v : cs) {
    auto a = v.as<ArduinoJson::JsonArrayConst>();
    if (a.size() < 4) return "a case is [case, component, props, scenario, path?, to?]";
    Case c;
    c.name = a[0] | "";
    c.comp = a[1] | "";
    const char *sc = a[3] | "animate";
    c.sc = !strcmp(sc, "idle") ? S_IDLE : !strcmp(sc, "drag") ? S_DRAG : !strcmp(sc, "navigate") ? S_NAV : S_ANIMATE;
    if (a[4].is<ArduinoJson::JsonArrayConst>() && a[4].size() == 2)
      for (int i = 0; i < 4; i++) c.path[i] = std::clamp(a[4][i / 2][i % 2] | c.path[i], 0.f, 1.f);
    if (a[5].is<const char *>()) c.to = a[5].as<const char *>();
    ArduinoJson::JsonDocument req;
    req["id"] = c.comp;
    if (a[2].is<ArduinoJson::JsonObjectConst>()) req["props"] = a[2];
    req["theme"] = theme;
    serializeJson(req, c.req);
    eta += 650 + (c.sc == S_ANIMATE ? nb.window_ms : c.sc == S_IDLE ? nb.settle_ms + nb.window_ms
                  : c.sc == S_DRAG ? nb.settle_ms + nb.drag_ms + nb.after_ms : nb.settle_ms + 2 * nb.nav_ms);
    nb.cases.push_back(std::move(c));
  }
  nb.rows.reserve(nb.cases.size());
  nb.before = S.shown;
  B = std::move(nb);
  *total = (uint32_t) B.cases.size();
  *eta_s = (uint32_t) (eta / 1000 + 1);
  lv_display_t *d = lv_display_get_default();
  if (!hooked) {
    for (auto c : {LV_EVENT_RENDER_START, LV_EVENT_RENDER_READY, LV_EVENT_FLUSH_START}) lv_display_add_event_cb(d, on_disp, c, nullptr);
    hooked = true;
  }
  if (!indev) {
    indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, read_cb);
    lv_indev_set_display(indev, d);
  }
  if (!timer) timer = lv_timer_create(step, 5, nullptr);
  rtc_magic = RUNNING;
  snprintf(rtc_run, sizeof rtc_run, "%s", B.run.c_str());
  B.gap_drawn = false;
  go(GAP);
  return "";
}

inline void result_json(ArduinoJson::JsonObject o) {
  const bool interrupted = B.ph == IDLE && rtc_magic == RUNNING;  // restarted during a batch
  o["run"] = interrupted ? std::string(rtc_run, strnlen(rtc_run, sizeof rtc_run)) : B.run;
  o["state"] = interrupted ? "interrupted" : B.ph == IDLE ? "idle" : B.ph == DONE ? "done" : "running";
  o["done"] = (uint32_t) B.rows.size();
  o["total"] = (uint32_t) B.cases.size();
  if (B.ph != DONE) return;
  auto rows = o["results"].to<ArduinoJson::JsonArray>();
  auto r2 = [](float v) { return std::round(v * 100) / 100; };
  for (size_t i = 0; i < B.rows.size(); i++) {
    const Row &r = B.rows[i];
    auto x = rows.add<ArduinoJson::JsonObject>();
    x["case"] = B.cases[i].name;
    x["scenario"] = B.cases[i].sc == S_IDLE ? "idle" : B.cases[i].sc == S_DRAG ? "drag" : B.cases[i].sc == S_NAV ? "navigate" : "animate";
    if (!r.ok) { x["error"] = r.error; continue; }
    auto p = x["panel"].to<ArduinoJson::JsonObject>();
    p["frames"] = r.frames; p["active_ms"] = r.active_ms; p["redraw_fps"] = r2(r.redraw_fps); p["fps"] = r2(r.fps);
    p["slow"] = r.slow; p["build_ms"] = r2(r.build_ms); p["render_ms_avg"] = r2(r.avg); p["render_ms_p95"] = r2(r.p95);
    p["render_ms_max"] = r2(r.max); p["px_avg"] = r.px_avg; p["px_pct"] = r2(r.px_pct); p["areas_avg"] = r2(r.areas_avg);
    p["full_redraws"] = r.full;
    auto m = x["mem"].to<ArduinoJson::JsonObject>();
    m["heap_kb"] = r.heap_kb; m["internal_free_after"] = r.int_free; m["psram_free_after"] = r.ps_free;
    m["internal_largest"] = r.int_largest; m["psram_largest"] = r.ps_largest; m["objs"] = r.objs;
  }
}

}  // namespace bench
#endif
