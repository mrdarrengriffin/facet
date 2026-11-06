// Facet device glue (README "Device firmware"): the workbench's core + foundations + components on a real panel,
// driven over HTTP by the workbench's "Show on device" button. ESPHome only (lvgl + web_server); no house specifics.
//   GET  /fw/info   (heap: + httpd_stack_min / loop_stack_min, the web server's and main loop's least free stack bytes)
//   GET  /fw/info   {v, density, core, files, ids, built, board, w, h, shown, devices: [{stack, builds, objects}], heap, art}   what this firmware was built from
//                   art = the last picture decode's biggest view (null = none yet): {box, kept, source: [w, h], jpeg: 1/N,
//                   filter: area | bilinear | nearest, drawn: 1:1 | scaled up by LVGL, reserved (in a screen slot), slots,
//                   slots_free, fallback (why it was kept smaller; null = it wasn't), error, need_kb, room_kb, age_s}
//                = What this firmware is: its build, board, screen size, free memory and the last album art decode.
//   GET  /fw/meta   [{id, name, section, meta}]                                     the registry (same as fw_meta())
//                = Every component and screen this firmware has, with its props.
//   POST /fw/show   {id, props?, theme?: dark|light, accent?: <set id>}  -> {ok, error?, ms, heap}
//                = Shows one component or screen with the props you give (the workbench's Show on device).
//   GET  /fw/perf   frame stats while animations run (since boot or the last ?reset=1): fps, frame / render ms;
//                   ?nav=1|0 = navigation timing on / off (each screen change: tap, build, feeds, first frame);
//                   ?keep=N = keep N pages built (until the next boot; "kept": each page's PSRAM, objects, build ms)
//                = How smoothly the panel draws: frames per second and render times.
//   GET  /fw/settings  the settings backup {"facet":"settings","version":1,"values":{...}} (core/device.h; no secrets)
//                = A backup of the panel's settings (no secrets in it).
//   POST /fw/settings  {values:{id: value}} (a backup, or a few) -> each through fw::dev::set (HOST): applied, the
//                      entities follow and keep it (settings.yaml from firmware/settings_yaml.mjs) -> {ok, error?, settings}
//                = Restores a settings backup, or changes a few settings.
//   GET  /screenshot  the glass as a 16-bit BMP, streamed from the framebuffer (no frame copy; paced chunks)
//                = What the screen shows now (a BMP image), so an assistant can check its work.
//   GET  /fw/report  the problem report, plain text: version, boots + reset reasons, the crash record, Wi-Fi / HA drops,
//                    /fw/info, settings, the log kept across restarts; tokens, keys and passwords removed ("problem report")
//                = Everything needed to report a problem, also from before the last restart (paste it into a bug report).
//   GET  /fw/spec   the spec hello (core/spec.h: protocol, catalogue hash, components, screens + revs, budgets, heap)
//                = Components, screens, budgets and free memory.
//   POST /fw/spec   {kind: screen|deck|revert, spec?: {...}, id?} -> the apply answer (the HA actions facet_screen /
//                   facet_deck / facet_revert do the same; workbench.yaml)
//                = Installs a screen, the page list, or goes back to the factory screen.
//   GET  /fw/deck   the deck this panel runs (core/spec.h deck_json: the override, else factory) {deck, rev, override}
//                = The page list this panel runs now.
//   GET  /fw/ha     the Home Assistant store (core/ha.h): {wants: [[e, a]...], values: {"e|a": value}, bound: n};
//                   ?want=e1,e2[&attrs=a1] subscribes to those first (the panel-served editor's live preview)
//                = The Home Assistant values the panel holds, and what its screens ask for.
//   POST /fw/bench  a batch of bench cases the panel runs on its own screen (fw_bench.h); GET /fw/bench = the report
//                = Runs speed tests on the panel's own screen; GET reads the report.
//   POST /fw/show   {screen: <deck page id | screen id | name>} opens that screen on the device (agents)
//                = Opens a screen on the panel.
//   POST /fw/spec   kind check | check_deck = validate (+ build off screen) only, nothing stored
//                = Checks a spec and lists its errors; stores nothing.
//   GET  /fw/act    every action of the closed list (core/spec.h ACTION_DOCS: fields, limits, one example each)
//                = Every action a screen or an assistant can run, with an example each.
//   POST /fw/act    {do, ...fields} -> {ok, why?, state?}: one action, checked + run like a spec tap (fw::spec::act,
//                   CONTRACT.md C50; HA's facet_act does the same: act_from_ha)
//                = Runs one action (a light, a timer, a screen) as a tap would.
//   GET  /fw/schema   the one schema surface (catalogue/schema.json: tokens, components, screens, feeds, settings,
//                     themes, these routes; gzip; open), bundle fw_docs_gen.h; GET /schema.json is the same file
//                = Everything this firmware accepts in one file: tokens, components, screens, feeds, settings, themes and these requests.
//   GET  /llms.txt, /agents.md   the agent docs (bundle fw_docs_gen.h from tools/gen_agents.mjs; open)
//                = The agent guide for this firmware. Open, no key needed.
//   GET  /ai   the prompt to paste to an agent (plain text, firmware/ai-prompt.txt; {panel} = this panel's address)
//                = The one prompt that sets an agent up for this panel, its address already in.
//   GET  /fw/keys   {on, max, waiting, keys: [{id, name, made, used, ends, used_words, ends_words}]} (core/keys.h)
//                = The API keys this panel has: names and times only, never a key.
//   POST /fw/keys/request   {name, expires?: 1d | 30d | never} -> {ok, id, state: waiting, wait_s} (no key needed)
//                = Asks the panel for a key: the person taps Allow on the glass.
//   POST /fw/keys/poll   {id} -> {state: waiting, left_s} | {state: allowed, key} (once) | {state: denied, error}
//                = Whether the person allowed the request; the key comes back once.
//   POST /fw/keys/revoke   {id | "all"} -> {ok, revoked}
//                = Revokes one key, or every key.
// API keys (core/keys.h; the logic is the core's, this file is its glue): every POST under /fw/ goes through
// keys::allow(): the panel's own editor (a browser request from this origin: Sec-Fetch-Site same-origin, or Origin =
// this host) always passes; anything else needs the setting Allow API access on (only the glass, HA or the
// editor can turn it on, core/device.cpp set()) AND `Authorization: Bearer <key>` a live key. Else 403 (off) / 401
// (key) with one plain line. Reads (GET) are open and never answer a key. POST /fw/keys/request and /poll need no key
// (that is how one is got). A change made with a key (a screen or the deck, a setting, the HA link) runs through
// keys::change in the loop job: the glass toasts it with Undo. The keys (hashes, names, times) live in NVS
// "facet_keys" / "keys" (a JSON blob, keys::on_store). Plain HTTP on the owner's own Wi-Fi: the editor is open on the
// LAN, as panels at this tier are; the same-origin check is browser-enforced only. CORS (browser-based agents):
// OPTIONS /fw/* answers 204 with Allow-Origin = the request's Origin, Allow-Methods GET, POST, OPTIONS, Allow-Headers
// Authorization, Content-Type (+ Private-Network for Chrome), Max-Age 600; every /fw/ response echoes the Origin
// (Vary: Origin; "*" without one). No credentials (cookies) are involved: the key is the only one, so echoing is safe.
// Images (fw_images.h): every picture URL the screen wants (the Media feed's album art) is downloaded + decoded on the
// panel (one worker task, only while a view waits for it) and handed to fw::img::put() / fail() on the LVGL task.
// Web UI (optional): a bundle may add fw_web_gen.h (fw::web::FILES, gzipped files linked into the image's rodata by
// the host's build, e.g. the Facet editor + the 1x renderer): GET / and each FILES path are served straight from
// flash in 4 KB chunks (no RAM copy; Content-Encoding gzip, ETag + If-None-Match -> 304, each file's Cache-Control),
// ahead of ESPHome's own web_server page. No fw_web_gen.h = no web UI (/ stays ESPHome's).
// Probation: while the running OTA image is still ESP_OTA_IMG_PENDING_VERIFY (safe_mode has not confirmed it yet,
// boot_is_good_after = FW_PROBATION_S), a warn Status chip on the LVGL top layer (over every screen) says
// "Update settling: don't unplug m:ss"; a reboot then would roll back to the previous image. Gone when confirmed
// ("Update confirmed" ok chip for 4 s). /fw/info "probation_s" = seconds left (-1 = confirmed); probation_left() for
// the host's entities (e.g. its Firmware update entity shows the countdown as install progress).
// Screen: screen_names() / screen_now() / screen_go() = the deck as readable names for a host select (see below).
// Rendering = fw::render() on one screen of our own (the same call the WASM preview makes): the component centred at
// true size on the Facet bg; content taller than the screen scrolls (doc pages). The last request is kept in flash and
// shown again after a reboot. Requests run in the httpd task; anything touching LVGL is handed to the main loop (an
// lv_timer drains the job queue) and the request waits for it (6 s cap).
#pragma once
#include <algorithm>
#include <atomic>
#include <vector>
#include <cstring>
#include <ctime>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "esp_random.h"
#include "esp_attr.h"
#include "fw_busy.h"
#include "esphome/core/defines.h"
#include "esphome/core/log.h"
#include "esphome/core/preferences.h"
#include "esphome/components/web_server_base/web_server_base.h"
#if __has_include("fw_web_gen.h")
#include "fw_web_gen.h"  // namespace fw::web: File, FILES, blob() (the host build's web UI; see "Web UI" above)
#endif
#if __has_include("fw_docs_gen.h")
#include "fw_docs_gen.h"  // namespace fw::docs: the agent docs /llms.txt + /agents.md (bundle.mjs, tools/gen_agents.mjs)
#endif
#include "fw.h"
#include "device.h"
#include "settings_gen.h"
#include "ha.h"
#include "spec.h"
#include "keys.h"
#include "nvs.h"
#include "esp_http_client.h"
#include "lwip/netdb.h"
#include "esphome/components/network/util.h"
#include "esp_crt_bundle.h"
#include "esp_transport.h"
#include "esp_transport_tcp.h"
#include "esp_transport_ssl.h"
#include "esp_transport_ws.h"
#include "theme_file.h"
#include "theme_slot.h"
#ifdef USE_API
#include "esphome/components/api/api_server.h"
#endif
#include "fw_bundle_gen.h"
#include "src/misc/lv_timer_private.h"  // lv_timer_t period (screenshot: hold LVGL's refresh)
namespace fw::report_screen { void set_board(const std::string &b); }  // screens/report.cpp
extern "C" char facet_overflow_task[20];  // esphome/board/stack_guard.h: the task that overflowed its stack
extern "C" uint32_t facet_overflow_magic;
extern "C" bool facet_hop(int *reason, bool *restart) __attribute__((weak));  // esphome/board/s3_reset.h (Guition)
#include "fw_report.h"  // the problem report: kept log, boots, connection drops, redaction
#include "esp_wifi.h"
#include "esp_event.h"
#include "esphome/components/logger/logger.h"
#include "esphome/components/wifi/wifi_component.h"
#ifdef USE_ESP32_CRASH_HANDLER
#include "esphome/components/esp32/crash_handler.h"
#endif
#include "fw_shot.h"  // screenshot: the framebuffer read back through the rotation
#include "fw_images.h"  // pictures (album art): fetched + decoded on the panel
#include "fw_net.h"  // the feeds' web reads (core/net.h: a public API a screen reads itself, adsb.lol)

namespace fw::device {
using namespace esphome::web_server_idf;

struct Saved {
  char req[1536];  // the last /fw/show body (JSON), "" = none
};
struct State {
  lv_obj_t *scr = nullptr;
  std::string shown = "null";  // last successful request (JSON)
  bool light = false;          // theme + accent of the last request (the probation chip follows it)
  int accent = -1;
  esphome::ESPPreferenceObject pref;
};
inline State S;

// ---------------------------------------------------------------- main-loop jobs
struct Job {
  std::function<void()> fn;
  SemaphoreHandle_t done;
};
inline std::mutex &qlock() {
  static std::mutex m;
  return m;
}
inline std::deque<Job *> &queue() {
  static std::deque<Job *> q;
  return q;
}
inline void drain(lv_timer_t *) {
  for (;;) {
    Job *j = nullptr;
    {
      std::lock_guard<std::mutex> g(qlock());
      if (queue().empty()) return;
      j = queue().front();
      queue().pop_front();
    }
    j->fn();
    xSemaphoreGive(j->done);
  }
}
// the web server task's least free stack, in bytes, after each Facet request (-1 = none yet): its stack is ESP-IDF's
// 4 KB + 256 (ESPHome), and /fw/info built there overflowed it on the Guition (crash record: stack overflow in task httpd)
inline std::atomic<int> httpd_stack_min{-1};
inline void httpd_stack_note() {
  const int f = (int) uxTaskGetStackHighWaterMark(nullptr);
  int m = httpd_stack_min.load();
  while ((m < 0 || f < m) && !httpd_stack_min.compare_exchange_weak(m, f)) {}
}
inline bool in_loop(std::function<void()> fn, uint32_t ms = 6000) {
  Job j{std::move(fn), xSemaphoreCreateBinary()};
  if (!j.done) return false;
  {
    std::lock_guard<std::mutex> g(qlock());
    queue().push_back(&j);
  }
  bool ok = xSemaphoreTake(j.done, pdMS_TO_TICKS(ms)) == pdTRUE;
  if (!ok) {
    bool removed = false;
    {
      std::lock_guard<std::mutex> g(qlock());
      for (auto it = queue().begin(); it != queue().end(); ++it)
        if (*it == &j) {
          queue().erase(it);
          removed = true;
          break;
        }
    }
    if (!removed) ok = xSemaphoreTake(j.done, portMAX_DELAY) == pdTRUE;  // running now: the Job lives on this stack
  }
  vSemaphoreDelete(j.done);
  return ok;
}

// ---------------------------------------------------------------- frame stats (Motion: frame-rate check)
// Counted while an LVGL animation runs or a finger is down (a drag): each rendered frame's start (interval = frame time) and its render +
// flush time. Gaps over 100 ms start a new segment (a pause between animations, not a dropped frame). slow = frames
// that came later than 1.5 refresh periods after the previous one (a missed refresh slot = a dropped frame).
struct Perf {
  int64_t start = 0, last = 0, active_us = 0, render_us = 0, render_max = 0, gap_max = 0;
  uint32_t frames = 0, intervals = 0, slow = 0;
  bool counting = false;
};
inline Perf P;
inline void perf_reset() { P = Perf{}; }
inline void perf_event(lv_event_t *e) {
  const int64_t t = esp_timer_get_time();
  static int64_t flush_t0 = 0, flush_us = 0;  // the flush (copy to the panel + its rotation) inside a render
  if (lv_event_get_code(e) == LV_EVENT_FLUSH_START) { flush_t0 = t; return; }
  if (lv_event_get_code(e) == LV_EVENT_FLUSH_FINISH) { flush_us += t - flush_t0; return; }
  if (lv_event_get_code(e) == LV_EVENT_RENDER_START) flush_us = 0;
  if (lv_event_get_code(e) == LV_EVENT_RENDER_START && fw::dev::nav_timing()) fw::dev::nav_render();
  if (lv_event_get_code(e) == LV_EVENT_RENDER_READY && fw::dev::nav_timing()) {  // navigation timing (facet-243)
    fw::dev::nav_copy((uint32_t) (flush_us / 1000));
    const size_t had = fw::dev::nav_times().size();
    fw::dev::nav_frame();
    const auto &n = fw::dev::nav_times();
    if (!n.empty() && (n.size() != had || had == 32)) {
      const fw::dev::NavTime &x = n.back();
      ESP_LOGI("facet_nav", "%s: lift->tap %u ms, tap->run %u ms, build %u ms (feeds %u ms in %u rounds), build->frame %u ms, lift->frame %u ms",
               x.screen.c_str(), (unsigned) (x.tap - x.touch), (unsigned) (x.run - x.tap), (unsigned) (x.build1 - x.build0), (unsigned) x.feed_ms, (unsigned) x.feeds,
               (unsigned) (x.frame - x.build1), (unsigned) (x.frame - x.touch));
    }
  }
  if (lv_event_get_code(e) == LV_EVENT_RENDER_START) {
    // a drag repaints with no animation running (a dial, a slider): a pressed pointer counts too
    bool down = false;
    for (lv_indev_t *i = lv_indev_get_next(nullptr); i && !down; i = lv_indev_get_next(i))
      down = lv_indev_get_type(i) == LV_INDEV_TYPE_POINTER && lv_indev_get_state(i) == LV_INDEV_STATE_PRESSED;
    P.counting = lv_anim_count_running() > 0 || down;
    if (!P.counting) return;
    P.start = t;
    const int64_t gap = P.last ? t - P.last : 0;
    if (P.last && gap < 100000) {
      P.intervals++;
      P.active_us += gap;
      if (gap > P.gap_max) P.gap_max = gap;
      const int64_t period = LV_DEF_REFR_PERIOD * 1000;
      if (gap * 2 > period * 3) P.slow++;
    }
    P.last = t;
    P.frames++;
  } else if (P.counting) {  // RENDER_READY
    const int64_t r = t - P.start;
    P.render_us += r;
    if (r > P.render_max) P.render_max = r;
  }
}
inline void perf_json(ArduinoJson::JsonObject o) {
  o["refr_period_ms"] = LV_DEF_REFR_PERIOD;
  o["frames"] = P.frames;
  o["fps"] = P.active_us ? lroundf(P.intervals * 1e7f / P.active_us) / 10.f : 0.f;
  o["frame_ms_avg"] = P.intervals ? lroundf(P.active_us / 100.f / P.intervals) / 10.f : 0.f;
  o["frame_ms_max"] = lroundf(P.gap_max / 100.f) / 10.f;
  o["render_ms_avg"] = P.frames ? lroundf(P.render_us / 100.f / P.frames) / 10.f : 0.f;
  o["render_ms_max"] = lroundf(P.render_max / 100.f) / 10.f;
  o["slow"] = P.slow;
  o["active_s"] = lroundf(P.active_us / 1e5f) / 10.f;
  if (fw::dev::keep_pages() > 0) {  // kept pages (facet-243): what each costs in PSRAM
    o["keep"] = fw::dev::keep_pages();
    if (fw::dev::kept_all()) o["kept_all_s"] = lroundf(fw::dev::kept_all() / 100.f) / 10.f;  // boot to all pages kept (lv_tick runs from boot)
    fw::dev::kept_json(o["kept"].to<ArduinoJson::JsonArray>());
  }
  if (!fw::dev::nav_timing()) return;  // navigation timing (GET /fw/perf?nav=1): each change, ms from its tap
  ArduinoJson::JsonArray nv = o["nav"].to<ArduinoJson::JsonArray>();
  for (const fw::dev::NavTime &x : fw::dev::nav_times()) {
    ArduinoJson::JsonObject r = nv.add<ArduinoJson::JsonObject>();
    r["screen"] = x.screen, r["input"] = x.tap - x.touch, r["run"] = x.run - x.tap, r["build"] = x.build1 - x.build0, r["feeds"] = x.feed_ms, r["rounds"] = x.feeds;
    if (x.del) r["delete"] = x.del - x.build0;
    if (x.made) r["make"] = x.made - x.del, r["layout"] = x.laid - x.made, r["rest"] = x.build1 - x.laid;
    if (x.screen_layout0) r["screen_layout"] = x.build1 - x.screen_layout0;  // the whole display screen's layout
    r["frame"] = x.frame - x.build1, r["total"] = x.frame - x.touch;
    if (x.render0) r["wait"] = x.render0 - x.build1, r["render"] = x.frame - x.render0;  // frame = wait (the loop) + render
    if (x.render0) r["draw"] = x.frame - x.render0 - x.copy, r["copy"] = x.copy;  // render = draw (LVGL) + copy (flush: to the panel, rotated)
    if (!x.prof.empty()) {  // the build's nodes (spec.h), ms each (a node's time includes what it builds inside it)
      ArduinoJson::JsonArray pf = r["profile"].to<ArduinoJson::JsonArray>();
      for (const auto &p : x.prof) { ArduinoJson::JsonArray e = pf.add<ArduinoJson::JsonArray>(); e.add(p.first); e.add(p.second); }
    }
  }
}

// ---------------------------------------------------------------- render
inline int accent_index(const char *id) {
  for (int i = 0; id && *id && i < tokens::N_ACCENTS; i++)
    if (!strcmp(tokens::ACCENTS[i].id, id)) return i;
  return -1;  // default set
}
inline void heap(ArduinoJson::JsonObject o) {
  o["internal_free"] = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
  o["internal_min"] = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
  o["psram_free"] = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
  o["internal_largest"] = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
}
}  // namespace fw::device
// the display's frame counters, when the board's display driver has them (Facet's mipi_rgb, Guition RGB panel; a weak
// symbol, so null elsewhere): /fw/info "lcd" {vsyncs, frames, missed}. missed = VSYNCs without a whole frame streamed
// = frames the RGB bounce buffers underran (PSRAM too busy), each a one-frame flicker or shift (#118)
// with[i] = missed frames while fw::busy job i ran (fw_busy.h), idle = missed with none running, busy[i] = all frames
// job i ran in (missed or not: with / busy = how often that job costs a frame).
struct FacetLcdStats {
  uint32_t vsyncs, frames, missed, idle, with[8], busy[8];
};
extern "C" bool facet_lcd_stats(FacetLcdStats *out) __attribute__((weak));
namespace fw::device {
// content taller than the screen (doc pages): top-aligned and scrollable instead of centred and clipped
inline void fit_scroll() {
  int32_t y = 0;
  for (uint32_t i = 0; i < lv_obj_get_child_count(S.scr); i++) {
    lv_area_t a;
    lv_obj_get_coords(lv_obj_get_child(S.scr, i), &a);
    if (a.y2 + 1 > y) y = a.y2 + 1;
  }
  int32_t top = 0;
  if (lv_obj_get_child_count(S.scr)) {
    lv_area_t a;
    lv_obj_get_coords(lv_obj_get_child(S.scr, 0), &a);
    top = a.y1;
  }
  if (y - top <= FW_DEVICE_H) return;
  lv_obj_set_flex_align(S.scr, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_add_flag(S.scr, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(S.scr, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(S.scr, LV_SCROLLBAR_MODE_OFF);
  lv_obj_update_layout(S.scr);
  lv_obj_scroll_to_y(S.scr, 0, LV_ANIM_OFF);
}
// {id, props, theme, accent} -> render; out = {ok, error?, ms, heap}
inline bool show(ArduinoJson::JsonObjectConst req, ArduinoJson::JsonObject out) {
  const char *id = req["id"] | "";
  std::string props = "{}";
  if (req["props"].is<ArduinoJson::JsonObjectConst>()) {
    props.clear();
    serializeJson(req["props"], props);
  }
  const bool light = !strcmp(req["theme"] | "dark", "light");
  // the panel's own settings win (an HA change must show): the request's theme / accent are only
  // the stage under them (fw::dev::look: Theme auto + no accent chosen yet = the request's), and the Settings screen
  // shows the panel's values (the request's setting props are dropped)
  const fw::Theme *look = fw::dev::look(fw::theme(light, accent_index(req["accent"] | "")));
  if (!strcmp(id, "settings")) {
    ArduinoJson::JsonDocument pd;
    if (!deserializeJson(pd, props)) {
      for (int i = 0; i < fw::set::N_SETTINGS; i++) pd.remove(fw::set::SETTINGS[i].id);
      props.clear();
      serializeJson(pd, props);
    }
  }
  const int64_t t0 = esp_timer_get_time();
  const char *e = fw::render(S.scr, id, props.c_str(), look->light, look->accent_set);
  if (!*e) fit_scroll();
  out["ok"] = !*e;
  if (*e) out["error"] = e;
  out["id"] = id;
  out["ms"] = (int) ((esp_timer_get_time() - t0) / 1000);
  heap(out["heap"].to<ArduinoJson::JsonObject>());
  if (*e) return false;
  S.shown.clear();
  serializeJson(req, S.shown);
  S.light = light;
  S.accent = accent_index(req["accent"] | "");
  return true;
}
inline void save() {
  Saved s{};
  if (S.shown.size() < sizeof s.req) memcpy(s.req, S.shown.data(), S.shown.size());
  S.pref.save(&s);
}

// ---------------------------------------------------------------- probation (OTA image not confirmed yet)
#ifndef FW_PROBATION_S
#define FW_PROBATION_S 180
#endif
// seconds until safe_mode confirms the running image (0 = due), -1 = confirmed / not an OTA image under verify. The
// running image only counts while it is also the one set to boot: otherwise a restart boots another image anyway
// (and safe_mode, which confirms only the running = boot image, never would), so there is nothing to wait for.
struct ProbationState {
  bool done = false;     // once valid it stays valid: no more partition reads
  int64_t checked = -1;  // the partition state is read at most once a second (callers poll every loop)
};
inline ProbationState PROB_;
inline int probation_left() {
  if (PROB_.done) return -1;
  const int64_t now = esp_timer_get_time();
  if (PROB_.checked < 0 || now - PROB_.checked >= 1000000) {
    PROB_.checked = now;
    esp_ota_img_states_t st;
    const esp_partition_t *run = esp_ota_get_running_partition();
    if (esp_ota_get_state_partition(run, &st) != ESP_OK || st != ESP_OTA_IMG_PENDING_VERIFY || run != esp_ota_get_boot_partition()) {
      PROB_.done = true;
      return -1;
    }
  }
  const int left = FW_PROBATION_S - (int) (now / 1000000);
  return left > 0 ? left : 0;
}
// the countdown is over and safe_mode has not confirmed the image (it confirms at boot_is_good_after, = FW_PROBATION_S):
// the boot has run that long, so it is confirmed here (as safe_mode would: the running image, only when it is the boot
// one); probation_left() is -1 from now on (the host's Firmware entity: up to date, fw_settle.h CONFIRM)
inline void probation_confirm() {
  if (PROB_.done) return;
  const esp_partition_t *run = esp_ota_get_running_partition();
  esp_err_t e = ESP_OK;
  if (run == esp_ota_get_boot_partition()) e = esp_ota_mark_app_valid_cancel_rollback();
  ESP_LOGW("facet_wb", "OTA image not confirmed %d s after boot: confirmed here (%s)", FW_PROBATION_S, esp_err_to_name(e));
  PROB_.done = true;
}
struct Probation {
  lv_obj_t *box = nullptr;  // top-layer holder (full width, top inset), the chip is rebuilt into it
  int shown = -2;           // last seconds value drawn
  int64_t ok_until = 0;     // "Update confirmed" until (us)
  lv_timer_t *timer = nullptr;
};
inline Probation PR;
inline void probation_chip(const char *tone, const char *text) {
  const fw::Component *cc = fw::find("status_chip");
  if (!cc) return;
  if (!PR.box) {
    PR.box = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(PR.box);
    lv_obj_remove_flag(PR.box, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
    lv_obj_set_size(PR.box, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_align(PR.box, LV_ALIGN_TOP_MID, 0, fw::sp::s4);
    lv_obj_set_flex_flow(PR.box, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(PR.box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  }
  lv_obj_clean(PR.box);
  const fw::Theme *old = fw::T;
  fw::T = fw::dev::look(fw::theme(S.light, S.accent));  // as the device shows it (Theme / Accent settings)
  // a solid backing under the 14 % tint so the chip reads over any screen (the tint alone is see-through)
  lv_obj_t *back = lv_obj_create(PR.box);
  lv_obj_remove_style_all(back);
  lv_obj_remove_flag(back, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_size(back, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_style_radius(back, fw::rad::chip, 0);
  lv_obj_set_style_bg_color(back, fw::T->surface, 0);
  lv_obj_set_style_bg_opa(back, LV_OPA_COVER, 0);
  ArduinoJson::JsonDocument p;
  p["tone"] = tone;
  p["text"] = text;
  cc->build(back, p.as<fw::Props>());
  fw::T = old;
}
inline void probation_tick(lv_timer_t *) {
  const int left = probation_left();
  if (left >= 0) {
    if (left == PR.shown) return;
    PR.shown = left;
    char t[64];
    snprintf(t, sizeof t, "Update settling: don't unplug %d:%02d", left / 60, left % 60);
    probation_chip("warn", t);
    return;
  }
  if (PR.shown >= -1 && PR.box) {  // was on probation: confirmed now
    if (PR.shown >= 0) {
      PR.shown = -1;
      PR.ok_until = esp_timer_get_time() + 4000000;
      probation_chip("ok", "Update confirmed");
      ESP_LOGI("facet_wb", "OTA image confirmed (probation over)");
      return;
    }
    if (esp_timer_get_time() < PR.ok_until) return;
  }
  if (PR.box) lv_obj_delete(PR.box);
  PR.box = nullptr;
  lv_timer_delete(PR.timer);  // nothing left to watch until the next OTA (a new boot)
  PR.timer = nullptr;
}
inline void probation_start() {
  if (probation_left() < 0) return;
  ESP_LOGW("facet_wb", "OTA image on probation: %d s until confirmed; a reboot now rolls back", probation_left());
  PR.timer = lv_timer_create(probation_tick, 500, nullptr);
  probation_tick(PR.timer);
}

// ---------------------------------------------------------------- firmware update notice
// Before new firmware installs (ESPHome http_request OTA: the update entity's Install), the panel shows the real
// Firmware update view (components/fw_update_view.cpp) full screen on the top layer, in the device's look; the board
// YAML (workbench.yaml ota on_begin) then turns the backlight off: neither board can draw while flash is written (the
// PSRAM framebuffer stalls). The OTA runs inside the main loop, so LVGL's timer never runs meanwhile: drawn at once
// (lv_refr_now). A failed OTA shows the failed state for 6 s (the loop runs again), then the screen underneath.
struct OtaScreen {
  lv_obj_t *cover = nullptr;
};
inline OtaScreen OTA;
inline void ota_show(const char *state, const char *version, const char *note) {
  const fw::Component *cc = fw::find("fw_update_view");
  if (!cc) return;
  const fw::Theme *old = fw::T;
  fw::T = fw::dev::look(fw::theme(S.light, S.accent));
  ArduinoJson::JsonDocument p;
  p["state"] = state, p["version"] = version ? version : "", p["note"] = note ? note : "";
  if (!OTA.cover) {
    OTA.cover = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(OTA.cover);
    lv_obj_add_flag(OTA.cover, LV_OBJ_FLAG_CLICKABLE);  // swallows taps: nothing underneath reacts
    lv_obj_remove_flag(OTA.cover, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(OTA.cover, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(OTA.cover, fw::T->bg, 0);
    lv_obj_set_style_bg_opa(OTA.cover, LV_OPA_COVER, 0);
  }
  lv_obj_clean(OTA.cover);  // built new (no fade: no loop runs to play it)
  cc->build(OTA.cover, p.as<fw::Props>());
  if (lv_obj_t *v = lv_obj_get_child_count(OTA.cover) ? lv_obj_get_child(OTA.cover, 0) : nullptr)
    lv_obj_align(v, LV_ALIGN_TOP_LEFT, fw::lay::inset, fw::lay::top);
  fw::T = old;
  lv_refr_now(nullptr);
}
inline void ota_begin(const char *version) { ota_show("updating", version, ""); }
inline void ota_error() {
  ota_show("failed", "", "");
  lv_timer_create([](lv_timer_t *tm) {  // back to the screen underneath after 6 s
    if (OTA.cover) lv_obj_delete(OTA.cover);
    OTA.cover = nullptr;
    lv_timer_delete(tm);
  }, 6000, nullptr);
}

// ---------------------------------------------------------------- Debug overlay (HA select "Debug overlay")
// Tap areas and / or component outlines drawn over the current screen, the panel side of the workbench Tap areas
// (workbench/src/wasm/taps.ts) and Components views. Firmware glue, not a contract component: one non-clickable full-screen
// object on the top layer that draws with lv_draw_*; boxes collected every 500 ms and after each show (redrawn only
// when they changed). Tap areas: every visible clickable object, hit area = coords grown by ext_click_pad (scroll
// containers with children skipped), green fill + outline = at least touch:min both ways, red = smaller, the drawn box
// dashed inside, "WxH" (device px). Components: thin cyan outlines + the component id where it fits; tagging
// (fw::comp_tags) runs only while this mode is on, and the shown page is built again when it turns on. Off = the
// object and timer deleted, tagging off: zero cost.
namespace dbg {
enum { OFF = 0, TAPS = 1, COMPS = 2 };  // bit mask: "Both" = 3
struct Box {
  lv_area_t a, b;  // tap: hit area + drawn box; component: a = its box
  bool tap, ok;
  std::string txt;
  bool operator==(const Box &o) const {
    return tap == o.tap && ok == o.ok && txt == o.txt && !memcmp(&a, &o.a, sizeof a) && !memcmp(&b, &o.b, sizeof b);
  }
};
struct State {
  int mode = OFF;
  bool ready = false;  // fw::device setup ran (the select restores before LVGL is up)
  lv_obj_t *o = nullptr;
  lv_timer_t *t = nullptr;
  std::vector<Box> boxes;
};
inline State D;
inline bool cut(lv_area_t *r, const lv_area_t *a, const lv_area_t *b) {  // lv_area_intersect is private in this LVGL
  *r = {LV_MAX(a->x1, b->x1), LV_MAX(a->y1, b->y1), LV_MIN(a->x2, b->x2), LV_MIN(a->y2, b->y2)};
  return r->x1 <= r->x2 && r->y1 <= r->y2;
}
// what is on screen of `c`: its coords clipped by every ancestor (scrolled-away content draws nothing)
inline void taps(lv_obj_t *o, const lv_area_t &clip, std::vector<Box> &out) {
  const int32_t min = fx::size("touch:min");
  for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) {
    lv_obj_t *c = lv_obj_get_child(o, (int32_t) i);
    if (lv_obj_has_flag(c, LV_OBJ_FLAG_HIDDEN)) continue;
    lv_area_t a, vis;
    lv_obj_get_coords(c, &a);
    if (!cut(&vis, &a, &clip)) continue;
    if (lv_obj_has_flag(c, LV_OBJ_FLAG_CLICKABLE) && !(lv_obj_has_flag(c, LV_OBJ_FLAG_SCROLLABLE) && lv_obj_get_child_count(c))) {
      Box b{};
      b.tap = true;
      b.b = a;
      lv_obj_get_click_area(c, &b.a);  // coords grown by ext_click_pad
      const int32_t w = lv_area_get_width(&b.a), h = lv_area_get_height(&b.a);
      b.ok = w >= min && h >= min;
      b.txt = std::to_string(w) + "x" + std::to_string(h);
      out.push_back(std::move(b));
    }
    taps(c, vis, out);
  }
}
inline const lv_font_t *font() { return fw::font(11.0f, true); }
inline void collect(std::vector<Box> &out) {
  lv_obj_t *scr = lv_screen_active();
  if (!scr) return;
  lv_area_t all;
  lv_obj_get_coords(scr, &all);
  if (D.mode & COMPS)
    fw::comps_each(scr, [&](lv_obj_t *c, const char *id, int) {
      Box b{};
      lv_obj_get_coords(c, &b.a);
      lv_area_t vis;
      if (!cut(&vis, &b.a, &all)) return;
      b.txt = id;
      out.push_back(std::move(b));
    });
  if (D.mode & TAPS) taps(scr, all, out);
}
inline void label(lv_layer_t *layer, const lv_area_t &box, const std::string &txt, lv_color_t col) {
  const lv_font_t *f = font();
  lv_point_t sz;
  lv_text_get_size(&sz, txt.c_str(), f, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
  const int32_t pad = fw::sp::s1;
  if (lv_area_get_width(&box) < sz.x + 2 * pad || lv_area_get_height(&box) < sz.y + 2 * pad) return;  // does not fit
  lv_area_t r = {box.x1 + pad, box.y1 + pad, box.x1 + pad + sz.x - 1, box.y1 + pad + sz.y - 1};
  lv_draw_rect_dsc_t bg;
  lv_draw_rect_dsc_init(&bg);
  bg.bg_color = lv_color_black();
  bg.bg_opa = LV_OPA_60;
  lv_draw_rect(layer, &bg, &r);
  lv_draw_label_dsc_t t;
  lv_draw_label_dsc_init(&t);
  t.font = f;
  t.color = col;
  t.text = txt.c_str();  // D.boxes outlives the draw (rebuilt only before an invalidate)
  lv_draw_label(layer, &t, &r);
}
inline void dashed(lv_layer_t *layer, const lv_area_t &a, lv_color_t col) {
  lv_draw_line_dsc_t l;
  lv_draw_line_dsc_init(&l);
  l.color = col;
  l.width = fw::stroke::hair;
  l.dash_width = fw::sp::s1;
  l.dash_gap = fw::sp::s1;
  const lv_value_precise_t x1 = a.x1, y1 = a.y1, x2 = a.x2, y2 = a.y2;
  const lv_value_precise_t pts[4][4] = {{x1, y1, x2, y1}, {x2, y1, x2, y2}, {x2, y2, x1, y2}, {x1, y2, x1, y1}};
  for (auto &p : pts) {
    l.p1 = {p[0], p[1]};
    l.p2 = {p[2], p[3]};
    lv_draw_line(layer, &l);
  }
}
inline void draw(lv_event_t *e) {
  lv_layer_t *layer = lv_event_get_layer(e);
  const lv_color_t cyan = lv_color_hex(0x22d3ee), green = lv_color_hex(0x22c55e), red = lv_color_hex(0xef4444);
  for (const Box &b : D.boxes) {
    lv_draw_rect_dsc_t r;
    lv_draw_rect_dsc_init(&r);
    r.border_width = fw::stroke::hair;
    r.border_opa = LV_OPA_COVER;
    if (b.tap) {
      const lv_color_t c = b.ok ? green : red;
      r.bg_color = c;
      r.bg_opa = LV_OPA_20;
      r.border_color = c;
      lv_draw_rect(layer, &r, &b.a);
      if (memcmp(&b.a, &b.b, sizeof b.a)) dashed(layer, b.b, c);
      label(layer, b.a, b.txt, c);
    } else {
      r.bg_opa = LV_OPA_TRANSP;
      r.border_color = cyan;
      lv_draw_rect(layer, &r, &b.a);
      label(layer, b.a, b.txt, cyan);
    }
  }
}
// the shown page built again (the components on it tagged now); the device home too (reshow leaves it alone)
inline void rebuild() {
  ArduinoJson::JsonDocument req, out;
  if (deserializeJson(req, S.shown) || !req["id"].is<const char *>()) return;
  show(req.as<ArduinoJson::JsonObjectConst>(), out.to<ArduinoJson::JsonObject>());
}
inline void refresh() {
  if (!D.o) return;
  std::vector<Box> now;
  collect(now);
  if (now == D.boxes) return;
  D.boxes = std::move(now);
  lv_obj_invalidate(D.o);
}
inline void apply() {
  fw::comp_tags(D.mode & COMPS);
  if (D.mode == OFF) {
    if (D.t) lv_timer_delete(D.t);
    if (D.o) lv_obj_delete(D.o);
    D.t = nullptr;
    D.o = nullptr;
    D.boxes.clear();
    return;
  }
  if (!D.o) {
    D.o = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(D.o);
    lv_obj_remove_flag(D.o, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICK_FOCUSABLE));
    lv_obj_add_flag(D.o, (lv_obj_flag_t) (LV_OBJ_FLAG_IGNORE_LAYOUT | LV_OBJ_FLAG_FLOATING));
    lv_obj_set_size(D.o, LV_PCT(100), LV_PCT(100));
    lv_obj_add_event_cb(D.o, draw, LV_EVENT_DRAW_MAIN, nullptr);
    D.t = lv_timer_create([](lv_timer_t *) { refresh(); }, 500, nullptr);
  }
  D.boxes.clear();
  refresh();
}
}  // namespace dbg
// the HA select: 0 Off, 1 Tap areas, 2 Components, 3 Both
inline void debug_overlay(int mode) {
  const int was = dbg::D.mode;
  dbg::D.mode = mode & 3;
  if (!dbg::D.ready) return;
  dbg::apply();
  if ((dbg::D.mode & dbg::COMPS) && !(was & dbg::COMPS)) { dbg::rebuild(); dbg::refresh(); }  // built again, now tagged
}

// ---------------------------------------------------------------- settings changed: the shown page follows
// A setting changed (any source: the screen, HA, POST /fw/settings, the sun for Theme auto). The device runtime restyles
// itself (core/device.cpp); a page shown on its own (POST /fw/show {"id": "settings"} and the like) is drawn again
// here: the look changed = render again in the new look, else (a value) the update path (the Settings controls move).
// Waits for the running animations (the tapped control's slide), at most 600 ms, never inside the tap's own event.
struct Reshow {
  lv_timer_t *timer = nullptr;
  uint32_t at = 0;
  bool look = false;
};
inline Reshow RS;
inline void reshow_tick(lv_timer_t *t) {
  if (lv_anim_count_running() > 0 && lv_tick_elaps(RS.at) < 600) return;
  lv_timer_delete(t);
  RS.timer = nullptr;
  ArduinoJson::JsonDocument req, out;
  if (deserializeJson(req, S.shown) || !req["id"].is<const char *>()) return;
  const char *id = req["id"];
  if (!strcmp(id, "device")) return;  // the device runtime did it
  if (!RS.look) {
    std::string props = "{}";
    if (req["props"].is<ArduinoJson::JsonObjectConst>()) { props.clear(); serializeJson(req["props"], props); }
    const fw::Theme *look = fw::dev::look(fw::theme(S.light, S.accent));
    if (!strcmp(id, "settings")) {
      ArduinoJson::JsonDocument pd;
      deserializeJson(pd, props);
      for (int i = 0; i < fw::set::N_SETTINGS; i++) pd.remove(fw::set::SETTINGS[i].id);
      props.clear();
      serializeJson(pd, props);
    }
    if (!*fw::update(S.scr, id, props.c_str(), look->light, look->accent_set)) return;
  }
  RS.look = false;
  show(req.as<ArduinoJson::JsonObjectConst>(), out.to<ArduinoJson::JsonObject>());
  dbg::refresh();  // the Debug overlay follows the new page
}
inline void reshow_later(bool look) {
  RS.look = RS.look || look;
  RS.at = lv_tick_get();
  if (!RS.timer) RS.timer = lv_timer_create(reshow_tick, 30, nullptr);
}

// ---------------------------------------------------------------- Home Assistant (core/ha.h)
// Every wanted (entity, attribute) pair is subscribed once (HA can't unsubscribe); HA asks for the list when it
// connects, a pair added while connected is announced to the connected client at once, one pair per message (re-sending
// the whole list would double every tracker: old panel spec.h subscribe()). Values go straight into the store (cheap:
// bound components repaint from its lv_timer). Actions (the closed whitelist in ha::act) go out as HA actions; HA
// runs them only with "Allow the device to perform Home Assistant actions" on in the ESPHome integration.
inline void ha_subscribe(const std::string &e, const std::string &a) {
#ifdef USE_API_HOMEASSISTANT_STATES
  auto *api = esphome::api::global_api_server;
  if (!api || e.empty()) return;
  std::function<void(esphome::StringRef)> f = [e, a](esphome::StringRef v) { fw::ha::put(e, a, std::string(v.c_str(), v.size())); };
  api->subscribe_home_assistant_state(e, a.empty() ? esphome::optional<std::string>() : esphome::optional<std::string>(a), std::move(f));
  for (auto &c : api->active_clients()) {
    if (!c->is_authenticated()) continue;
    esphome::api::SubscribeHomeAssistantStateResponse r;
    r.entity_id = esphome::StringRef(e);
    r.attribute = esphome::StringRef(a);
    r.once = false;
    (void) c->send_message(r);
  }
  ESP_LOGD("facet_wb", "HA subscribe %s%s%s", e.c_str(), a.empty() ? "" : " | ", a.c_str());
#endif
}
inline void ha_action(const char *service, const char *entity, const char *data_json) {
#ifdef USE_API_HOMEASSISTANT_SERVICES
  auto *api = esphome::api::global_api_server;
  if (!api) return;
  std::vector<std::pair<std::string, std::string>> kv{{"entity_id", entity}};
  ArduinoJson::JsonDocument d;
  if (data_json && *data_json && !deserializeJson(d, data_json))
    for (ArduinoJson::JsonPairConst p : d.as<ArduinoJson::JsonObjectConst>()) {
      std::string v;
      if (p.value().is<const char *>()) v = p.value().as<const char *>();
      else serializeJson(p.value(), v);
      kv.push_back({p.key().c_str(), v});
    }
  const std::string svc = service;
  esphome::api::HomeassistantActionRequest r;
  r.service = esphome::StringRef(svc);
  r.data.init(kv.size());
  for (auto &x : kv) {
    auto &m = r.data.emplace_back();
    m.key = esphome::StringRef(x.first);
    m.value = esphome::StringRef(x.second);
  }
  api->send_homeassistant_action(r);
  ESP_LOGI("facet_wb", "HA action %s %s %s", service, entity, data_json ? data_json : "");
#else
  ESP_LOGW("facet_wb", "HA action %s %s: this build has no homeassistant_services", service, entity);
#endif
}

// asks (core/ha.h, CONTRACT.md C51 adapters): an HA action whose reply a feed's adapter reads (calendar.get_events for
// the Bins feed), sent with wants_response; the reply's JSON goes back to fw::ha::answer. Needs the ESPHome build to
// have action responses (workbench.yaml script facet_ask_enable turns them on); without them the ask fails at once.
constexpr uint32_t ASK_BASE = 0x40000000u;  // above ESPHome's own call ids (its automations count from 1)
inline void ha_ask(uint32_t id, const char *service, const char *data_json) {
#if defined(USE_API_HOMEASSISTANT_SERVICES) && defined(USE_API_HOMEASSISTANT_ACTION_RESPONSES_JSON)
  auto *api = esphome::api::global_api_server;
  if (!api) { fw::ha::answer(id, false, ""); return; }
  std::vector<std::pair<std::string, std::string>> kv;
  ArduinoJson::JsonDocument d;
  if (data_json && *data_json && !deserializeJson(d, data_json))
    for (ArduinoJson::JsonPairConst p : d.as<ArduinoJson::JsonObjectConst>()) kv.push_back({p.key().c_str(), p.value() | ""});
  const std::string svc = service;
  esphome::api::HomeassistantActionRequest r;
  r.service = esphome::StringRef(svc);
  r.data.init(kv.size());
  for (auto &x : kv) {
    auto &m = r.data.emplace_back();
    m.key = esphome::StringRef(x.first);
    m.value = esphome::StringRef(x.second);
  }
  r.call_id = ASK_BASE + id;
  r.wants_response = true;
  api->register_action_response_callback(ASK_BASE + id, [id](const esphome::api::ActionResponse &resp) {
    std::string js;
    if (resp.is_success()) serializeJson(resp.get_json(), js);
    fw::ha::answer(id, resp.is_success(), js);
  });
  api->send_homeassistant_action(r);
  ESP_LOGI("facet_wb", "HA ask %u %s %s", (unsigned) id, service, data_json ? data_json : "");
#else
  ESP_LOGW("facet_wb", "HA ask %s: this build has no action responses", service);
  fw::ha::answer(id, false, "");
#endif
}

// ---------------------------------------------------------------- toasts from Home Assistant
// The ESPHome action facet_toast {title, body, icon, kind, duration (ms)} -> the device runtime's Toast layer: the toast
// JSON goes on the pseudo pair facet.toast (core/device.h TOAST_PAIR, ha::on_put), the same path the workbench uses.
// The renderer maps it (core/device.cpp toast_put, components/toast_layer.cpp push; functional test toast_ha): kind =
// the tone (ok warn error working info idle; else info), icon = one of the Toast's icons (else the tone's own),
// duration >= 10 s = urgent (stays much longer). Each call carries its own n, so the same message twice shows twice
// (ha::put drops an unchanged value). Main loop (ESPHome actions run there).
inline void toast_from_ha(const std::string &title, const std::string &body, const std::string &icon, const std::string &kind, float duration_ms) {
  static uint32_t n = 0;
  ArduinoJson::JsonDocument t;
  t["n"] = ++n;
  t["title"] = title.substr(0, 80);
  t["body"] = body.substr(0, 160);
  t["icon"] = icon.substr(0, 16);
  t["kind"] = kind.substr(0, 16);
  t["duration"] = duration_ms;
  std::string j;
  serializeJson(t, j);
  fw::ha::put(fw::dev::TOAST_PAIR, "", j);
  ESP_LOGI("facet_wb", "toast from HA: %s", j.c_str());
}

// ---------------------------------------------------------------- timers (core/device.h "the panel's timers")
// Home Assistant sees the panel's timers through the host's entities (workbench.yaml): Next timer ends (a timestamp
// text sensor: ISO UTC, "" = unknown when none runs; a numeric ESPHome sensor is a 32-bit float = 128 s steps for an
// epoch, too coarse for a timer), Timers (count running), Timers list (JSON text: ESPHome sensors have no attributes;
// [[name, ends epoch, total s, "running" | "done"], ...] soonest first, fits HA's 255), Timer finished (event,
// finished) + Last timer finished (the name: ESPHome events carry no data; published BEFORE the event fires).
// timers_hook / timer_done_hook are set by the host before setup(); called on start / extend / cancel / stop / done
// only, never per second. The HA actions (facet_timer_start / _extend / _cancel) go through timer_from_ha(): a name ""
// = the shown timer (extend / cancel); a refused start answers why and shows it as a warn toast. Main loop.
inline void (*timers_hook)() = nullptr;
inline void (*timer_done_hook)(const std::string &name) = nullptr;
inline std::string timers_next_iso() {
  long long best = 0;
  for (const fw::dev::TimerItem &t : fw::dev::timers::all()) {
    const long long e = fw::dev::timers::ends_at(t);
    if (e && (!best || e < best)) best = e;
  }
  if (!best) return "";
  const time_t e = (time_t) best;
  struct tm tm;
  gmtime_r(&e, &tm);
  char b[32];
  strftime(b, sizeof b, "%Y-%m-%dT%H:%M:%S+00:00", &tm);
  return b;
}
inline std::string timers_list() {
  std::string j;
  fw::dev::timers::list_json(j);
  return j;
}
inline void timers_setup() {
  fw::dev::timers::observe([](fw::dev::timers::Change w, const fw::dev::TimerItem &t) {
    ESP_LOGI("facet_wb", "timer %d %s (%d s)", (int) w, t.name.c_str(), t.total);
    if (w == fw::dev::timers::SELECT) return;  // which one the ring shows: nothing HA sees
    if (w == fw::dev::timers::DONE && timer_done_hook) timer_done_hook(t.name);
    if (timers_hook) timers_hook();
  });
  if (timers_hook) timers_hook();  // the first state (none): HA reads unknown / 0 / []
}
// Actions from Home Assistant (CONTRACT.md C50): facet_act {action: JSON} runs ONE action of the closed list through
// fw::spec::act (the same checks and code path as a spec tap, POST /fw/act); the friendly facet_timer_start / _extend
// / _cancel build that action. ESPHome evaluates api.respond's data only when the caller asked for a response, so the
// YAML does the work in a lambda step (act_from_ha / timer_from_ha) and answers with act_answer() after it
// (a call without return_response did nothing while the work sat in the respond lambda). Main loop.
inline ArduinoJson::JsonDocument &act_last() { static ArduinoJson::JsonDocument d; return d; }
inline void act_answer(ArduinoJson::JsonObject out) {
  for (ArduinoJson::JsonPair kv : act_last().as<ArduinoJson::JsonObject>()) out[kv.key()] = kv.value();
}
inline void timer_answer(ArduinoJson::JsonObject out) { act_answer(out); }
inline bool act_run(ArduinoJson::JsonVariantConst a, const char *from) {
  act_last().clear();
  ArduinoJson::JsonObject out = act_last().to<ArduinoJson::JsonObject>();
  std::string why;
  ArduinoJson::JsonDocument st;
  const bool ok = fw::spec::act(a, why, st.to<ArduinoJson::JsonObject>());
  out["ok"] = ok;
  if (!why.empty()) { out["why"] = why; ESP_LOGW("facet_wb", "%s %s: %s", from, a["do"] | "?", why.c_str()); }
  if (st.size()) out["state"] = st;
  return ok;
}
inline void act_from_ha(const std::string &json) {
  ArduinoJson::JsonDocument a;
  if (json.size() > 1024 || deserializeJson(a, json)) {
    act_last().clear();
    act_last()["ok"] = false;
    act_last()["why"] = "action: not JSON (an object {\"do\": ..., ...}, at most 1 KB)";
    return;
  }
  act_run(a.as<ArduinoJson::JsonVariantConst>(), "facet_act");
}
inline void timer_from_ha(const char *op, const std::string &name, int seconds) {
  ArduinoJson::JsonDocument a;
  a["do"] = std::string("timer.") + op;
  if (!strcmp(op, "start")) a["seconds"] = seconds;
  if (!strcmp(op, "extend")) a["s"] = seconds;
  if (!name.empty()) a["name"] = name;
  const bool ok = act_run(a.as<ArduinoJson::JsonVariantConst>(), "facet_timer");
  ArduinoJson::JsonObject out = act_last().as<ArduinoJson::JsonObject>();
  if (!ok) out["error"] = out["why"];  // the facet_timer_* answer kept its "error"
  if (!ok && !strcmp(op, "start")) {  // a refused start from HA: say so where the person is looking
    ArduinoJson::JsonDocument t;
    t["tone"] = "warn"; t["icon"] = "timer"; t["title"] = "No timer started"; t["body"] = out["why"] | "";
    fw::dev::toast(t.as<fw::Props>());
  }
  namespace tm = fw::dev::timers;
  int id = 0;
  if (ok && !strcmp(op, "start")) for (const fw::dev::TimerItem &t : tm::all()) id = std::max(id, t.id);  // the newest
  else if (ok) id = name.empty() ? (tm::shown() ? tm::shown()->id : 0) : tm::find(name.c_str());
  if (const fw::dev::TimerItem *t = tm::get(id)) out["name"] = t->name;
}

// ---------------------------------------------------------------- screen specs (core/spec.h)
// Overrides live in NVS namespace "facet_spec" (key s.<id> = the record, g.<id> = its last good copy, idx = the ids;
// a record = CRC32 + JSON). NVS writes one key atomically; the CRC catches the rest. Apply runs in the main loop
// (the HA action / the HTTP job): validate, build once off screen (a hidden box on our screen), store, swap.
inline nvs_handle_t spec_nvs() {
  static nvs_handle_t h = 0;
  static bool tried = false;
  if (!tried) { tried = true; if (nvs_open("facet_spec", NVS_READWRITE, &h) != ESP_OK) { h = 0; ESP_LOGE("facet_wb", "spec NVS: cannot open"); } }
  return h;
}
inline void spec_storage() {
  auto &io = fw::spec::S().io;
  io.load = [](const std::string &k, std::string &out) {
    nvs_handle_t h = spec_nvs();
    size_t n = 0;
    if (!h || nvs_get_blob(h, k.c_str(), nullptr, &n) != ESP_OK) return false;
    out.resize(n);
    return nvs_get_blob(h, k.c_str(), out.data(), &n) == ESP_OK;
  };
  io.save = [](const std::string &k, const std::string &v) {
    nvs_handle_t h = spec_nvs();
    return h && nvs_set_blob(h, k.c_str(), v.data(), v.size()) == ESP_OK && nvs_commit(h) == ESP_OK;
  };
  io.erase = [](const std::string &k) {
    if (nvs_handle_t h = spec_nvs()) { nvs_erase_key(h, k.c_str()); nvs_commit(h); }
  };
}
inline void spec_hello(ArduinoJson::JsonObject out) {
  fw::spec::hello(out);
  fw::dev::state(out["devices"].to<ArduinoJson::JsonArray>());
  out["board"] = FW_DEVICE_BOARD;
  out["w"] = FW_DEVICE_W;
  out["h"] = FW_DEVICE_H;
  ArduinoJson::JsonDocument info;
  if (!deserializeJson(info, fw::bundle::INFO)) out["core"] = info["core"];
  heap(out["heap"].to<ArduinoJson::JsonObject>());
  fx::theme_status(out["theme"].to<ArduinoJson::JsonObject>());  // look, live, the sent slot, the boot restore
}
// kind screen | deck: json = the spec; revert: json = the id (| deck | all)
inline void theme_from_ha(const std::string &json, ArduinoJson::JsonObject out);
// a theme file checked by the panel's own guards (structure, ranges, contrast in every mode and accent, press
// feedback): nothing stored, the look unchanged. {ok, errors: [{path, msg, line}] (first 4), n_errors}
inline void theme_check(const std::string &json, ArduinoJson::JsonObject out) {
  std::vector<fx::ThemeError> err;
  const bool ok = fx::theme_load(json.data(), json.size(), err) != nullptr && err.empty();
  out["ok"] = ok;
  if (ok) out["status"] = "ok: this theme passes every guard (nothing stored; send it with kind theme)";
  ArduinoJson::JsonArray a = out["errors"].to<ArduinoJson::JsonArray>();
  for (size_t i = 0; i < err.size() && i < 4; i++) {
    ArduinoJson::JsonObject e = a.add<ArduinoJson::JsonObject>();
    e["path"] = err[i].path, e["msg"] = err[i].msg, e["line"] = err[i].line;
  }
  out["n_errors"] = (int) err.size();
}
// POST /fw/spec kinds: screen | deck | check | revert (core/spec.h); theme = a theme file an agent sends (the same path
// as Home Assistant's facet_theme: guarded on the panel, stored as the sent slot, the Theme setting moves to Sent; ""
// drops it); theme_check = its guards only
inline void spec_apply(const char *kind, const std::string &json, ArduinoJson::JsonObject out) {
  if (!strcmp(kind, "revert")) { fw::spec::revert(json.empty() ? "all" : json, out); return; }
  if (!strcmp(kind, "theme")) { theme_from_ha(json, out); return; }
  if (!strcmp(kind, "theme_check")) { theme_check(json, out); return; }
  lv_obj_t *dry = lv_obj_create(S.scr);
  lv_obj_add_flag(dry, (lv_obj_flag_t) (LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_FLOATING));
  const int64_t t0 = esp_timer_get_time();
  fw::spec::apply(kind, json, dry, out);
  lv_obj_delete(dry);
  out["ms"] = (int) ((esp_timer_get_time() - t0) / 1000);
  ESP_LOGW("facet_wb", "spec %s %s rev %d: %s", kind, (const char *) (out["id"] | ""), (int) (out["rev"] | -1), (const char *) (out["status"] | ""));
}

// ---------------------------------------------------------------- themes from Home Assistant (theme engine T4)
// The ESPHome action facet_theme {theme: the theme file as a JSON string} (settings.yaml from firmware/settings_yaml.mjs)
// -> fx::theme_send (core/theme_slot.h): the PANEL parses + guards the file itself (no workbench in the product path);
// refused = the answer {ok: false, errors: [{path, msg, line}] (first 4), n_errors} + a log line, the live look and
// the stored copy unchanged; accepted = stored as the one sent slot's last-good copy (NVS "facet_theme"/"sent": "FXT1"
// + CRC32 + JSON, one key = one atomic write) and the Theme (look) select moves to Sent. theme = "" drops it.
// Boot: theme_storage() restores it (a damaged record = erased + Facet; a refused one = kept + Facet; both logged).
inline void theme_storage() {
  fx::SlotIO io;
  auto nvs = [] {
    static nvs_handle_t h = 0;
    static bool tried = false;
    if (!tried) { tried = true; if (nvs_open("facet_theme", NVS_READWRITE, &h) != ESP_OK) { h = 0; ESP_LOGE("facet_wb", "theme NVS: cannot open"); } }
    return h;
  };
  io.load = [nvs](std::string &out) {
    nvs_handle_t h = nvs();
    size_t n = 0;
    if (!h || nvs_get_blob(h, "sent", nullptr, &n) != ESP_OK) return false;
    if (n > fx::SLOT_MAX) { out.assign(n > fx::SLOT_MAX + 1 ? fx::SLOT_MAX + 1 : n, '\0'); return true; }  // too long: unpack says corrupt
    out.resize(n);
    return nvs_get_blob(h, "sent", out.data(), &n) == ESP_OK;
  };
  io.save = [nvs](const std::string &r) { nvs_handle_t h = nvs(); return h && nvs_set_blob(h, "sent", r.data(), r.size()) == ESP_OK && nvs_commit(h) == ESP_OK; };
  io.erase = [nvs] { if (nvs_handle_t h = nvs()) { nvs_erase_key(h, "sent"); nvs_commit(h); } };
  fx::theme_storage(std::move(io));
  ArduinoJson::JsonDocument st;
  fx::theme_status(st.to<ArduinoJson::JsonObject>());
  ESP_LOGW("facet_wb", "theme boot: %s (look %s, live %s)", (const char *) (st["boot"] | ""), (const char *) (st["look"] | ""), (const char *) (st["live"] | ""));
}
inline void theme_from_ha(const std::string &json, ArduinoJson::JsonObject out) {
  const int64_t t0 = esp_timer_get_time();
  fx::theme_send(json.data(), json.size(), out);
  out["ms"] = (int) ((esp_timer_get_time() - t0) / 1000);
  if (out["ok"] | false) ESP_LOGW("facet_wb", "theme sent: %s (%d bytes, stored %d, %d ms)", (const char *) (out["id"] | (out["dropped"] | false ? "(dropped)" : "")), (int) (out["bytes"] | 0), (int) (out["stored"] | false), (int) (out["ms"] | 0));
  else ESP_LOGW("facet_wb", "theme refused (%d errors): %s %s", (int) (out["n_errors"] | 0), (const char *) (out["errors"][0]["path"] | ""), (const char *) (out["errors"][0]["msg"] | ""));
}

// ---------------------------------------------------------------- Screen (the host's Home Assistant select)
// The deck as a list of readable names for a host entity (e.g. an ESPHome template select "Screen"): Home, every deck
// page that is not a Home (its name, else its spec's "name", else the component's name, else the kind), Settings. screen_names() is built once
// (the host gives the select these options at boot; a deck saved later shows new names after a restart, the select's
// option list is announced to HA when it connects). screen_now() = the name of the screen on the glass ("" = none of
// them, e.g. a workbench page or a screen opened from a card); screen_go(name or id) shows the device if a workbench
// page is up, then opens that screen (Home = back to the root). Main loop only.
struct Screens {
  std::vector<std::string> ids, names;
  bool built = false;
};
inline Screens SCR;
inline void screens_build() {
  if (SCR.built) return;
  SCR.built = true;
  auto add = [](const std::string &id, std::string name) {
    if (name.empty()) name = id;
    for (const auto &n : SCR.names)
      if (n == name) { name += " (" + id + ")"; break; }
    SCR.ids.push_back(id);
    SCR.names.push_back(name);
  };
  add("home", "Home");
  // deck v2 (core/spec.h): each page that is not a Home, opened by its kind (as Home's links do), named by the page's
  // name, else its spec's, else the component's
  ArduinoJson::JsonDocument d;
  if (!deserializeJson(d, fw::spec::deck_json()))
    for (ArduinoJson::JsonVariantConst x : d["pages"].as<ArduinoJson::JsonArrayConst>()) {
      const std::string kind = x["kind"] | "";
      if (kind.empty() || kind == "home" || kind == "settings") continue;
      std::string name = x["name"] | "";
      ArduinoJson::JsonDocument sd;
      if (name.empty())
        if (const char *j = fw::spec::screen_json(kind))
          if (!deserializeJson(sd, j)) name = sd["name"] | "";
      if (name.empty())
        if (const fw::Component *c = fw::find(kind.c_str())) name = c->name ? c->name : "";
      add(kind, name);
    }
  if (fw::find("settings")) add("settings", "Settings");
}
inline const std::vector<std::string> &screen_names() {
  screens_build();
  return SCR.names;
}
// the device root's stack top ("" = the device is not what is shown)
inline const char *device_top() {
  if (!S.scr) return "";
  lv_obj_t *x = lv_obj_get_child_count(S.scr) ? lv_obj_get_child(S.scr, 0) : nullptr;
  for (int depth = 0; x && depth < 4; depth++) {
    const char *t = fw::dev::current(x);
    if (*t) return t;
    x = lv_obj_get_child_count(x) ? lv_obj_get_child(x, 0) : nullptr;
  }
  return "";
}
inline lv_obj_t *device_in() {
  lv_obj_t *x = S.scr && lv_obj_get_child_count(S.scr) ? lv_obj_get_child(S.scr, 0) : nullptr;
  for (int depth = 0; x && depth < 4; depth++) {
    if (*fw::dev::current(x)) return x;
    x = lv_obj_get_child_count(x) ? lv_obj_get_child(x, 0) : nullptr;
  }
  return nullptr;
}
inline std::string screen_now() {
  screens_build();
  std::string top = device_top();
  const size_t q = top.find('?');
  if (q != std::string::npos) top.resize(q);
  for (size_t i = 0; i < SCR.ids.size(); i++)
    if (SCR.ids[i] == top) return SCR.names[i];
  return "";
}
// name (a screen_names() entry) or a deck screen id -> {ok, error?, screen}
inline bool screen_go(const std::string &what, ArduinoJson::JsonObject out) {
  screens_build();
  std::string id;
  for (size_t i = 0; i < SCR.ids.size(); i++)
    if (SCR.names[i] == what || SCR.ids[i] == what) { id = SCR.ids[i]; break; }
  if (id.empty() && fw::spec::screen_known(what)) id = what;
  out["screen"] = what;
  if (id.empty()) { out["ok"] = false; out["error"] = "no such screen"; return false; }
  if (!device_in()) {  // a workbench page is up: back to the device (same theme / accent as the page)
    ArduinoJson::JsonDocument req, o;
    deserializeJson(req, S.shown);
    if (!req.is<ArduinoJson::JsonObject>()) req.to<ArduinoJson::JsonObject>();
    req["id"] = "device";
    req.remove("props");
    if (!show(req.as<ArduinoJson::JsonObjectConst>(), o.to<ArduinoJson::JsonObject>()) || !device_in()) {
      out["ok"] = false; out["error"] = "the device runtime is not in this build"; return false;
    }
    save();
  }
  if (id == "home") fw::dev::home(device_in());
  else fw::dev::go(device_in(), id.c_str());
  out["ok"] = true;
  return true;
}

// ---------------------------------------------------------------- /fw/info "art"
// the last picture decode (fw_images.h Art): read under the image worker's lock (any task)
inline void art_info(ArduinoJson::JsonDocument &d) {
  images::Art a;
  {
    std::lock_guard<std::mutex> l(images::S().m);
    a = images::S().art;
  }
  if (!a.any) { d["art"] = nullptr; return; }
  ArduinoJson::JsonObject o = d["art"].to<ArduinoJson::JsonObject>();
  const auto pair = [&](const char *k, int w, int h) { auto v = o[k].to<ArduinoJson::JsonArray>(); v.add(w), v.add(h); };
  pair("box", a.bw, a.bh);
  if (a.err.empty()) {
    pair("kept", a.ow, a.oh);
    pair("source", a.sw, a.sh);
    o["jpeg"] = a.sc ? (a.sc == 1 ? "1/2" : "1/4") : "1/1";
    // the scaler's filter for this view (img_scale.h): nearest for pixel art, else an area average down, bilinear up
    const bool up = a.sw > 0 && a.sh > 0 && (double) std::max((double) a.ow / a.sw, (double) a.oh / a.sh) >= 1.0;
    o["filter"] = a.nearest ? "nearest" : up ? "bilinear" : "area";
    o["drawn"] = a.ow == a.bw && a.oh == a.bh ? "1:1" : "scaled up by LVGL";
    o["reserved"] = a.slot;
    o["decoder"] = a.hw ? "hardware" : "software";  // the P4's JPEG engine + PPA, or the software decode
    o["download_ms"] = a.got_ms, o["decode_ms"] = a.dec_ms;
  } else o["error"] = a.err;
  o["slots"] = fw::img::slots().n.load(), o["slots_free"] = fw::img::slots_free();
  if (a.side) {
    char t[120];
    snprintf(t, sizeof t, "largest free PSRAM block %u KB (less the margin), the box size needs %u KB: kept at %d px",
             (unsigned) (a.room / 1024), (unsigned) ((a.need_full + 1023) / 1024), a.side);
    o["fallback"] = t;
  } else o["fallback"] = nullptr;
  o["need_kb"] = (unsigned) ((a.need + 1023) / 1024), o["room_kb"] = (unsigned) (a.room / 1024);
  o["age_s"] = (int) ((esp_timer_get_time() - a.at_us) / 1000000);
}

// ---------------------------------------------------------------- screenshot (GET /screenshot)
// What the glass shows, as a 16-bit BMP, read straight from the panel's framebuffer (the board's frame_hook:
// esphome/board/rgb_flush.h, dsi_frame.h) and encoded as it streams: a band of rows at a time into a small buffer,
// never a whole-frame copy, so it works however little memory is free (a full lv_snapshot needed ~460 KB + 920 KB for
// the top layer on the Guition and failed once the idle art reserved its buffers). The framebuffer already holds the
// top layer (the probation chip). It is in panel orientation; the pixels are read back through LVGL's rotation.
// No tearing: the web server runs in its own task, so the request raises a flag and an lv_timer (main loop, between
// renders: every flush is synchronous) holds LVGL's refresh timer (period 5 min, as ESPHome does while a display is
// busy; lv_timer_pause would not stick, LVGL resumes it on every invalidation) until the last row is out. Changes made
// meanwhile are drawn when it is released. A stuck request releases it after HOLD_MAX_MS. The framebuffer is never
// freed or moved, so reading it can't fault. Rows go out 4 at a time with a pause (Guition: Wi-Fi TX buffers come out
// of internal RAM; an unpaced response crashed it).
namespace shot {
// board: framebuffer (RGB565, panel orientation), panel width / height, LVGL rotation in degrees; false = none
inline bool (*frame_hook)(const uint16_t **fb, int *w, int *h, int *rot) = nullptr;
constexpr uint32_t HOLD_MAX_MS = 20000;
struct State {
  std::atomic<int> state{0};  // 0 idle, 1 requested, 2 held (stream now), 3 done (release)
  lv_timer_t *refr = nullptr;
  uint32_t period = 0, held_at = 0;
  bool holding = false;
  bool swapped = false;  // the framebuffer's pixels have their bytes swapped (a big-endian panel: the Guition)
};
inline State SH;
inline void unhold() {
  if (!SH.holding) return;
  lv_timer_set_period(SH.refr, SH.period);
  lv_timer_ready(SH.refr);
  SH.holding = false;
}
inline void take(lv_timer_t *) {  // main loop (lv_timer): hold / release LVGL's refresh
  const int st = SH.state.load();
  if (st == 1) {
    lv_display_t *d = lv_display_get_default();
    SH.refr = d ? lv_display_get_refr_timer(d) : nullptr;
    if (!SH.refr) { SH.state = 0; return; }
    SH.period = SH.refr->period;
    // LVGL swaps the bytes just before the flush for a big-endian panel (ESPHome's lv_conf LV_COLOR_16_SWAP: the
    // Guition), or renders RGB565_SWAPPED: either way the framebuffer holds them swapped
#if defined(LV_COLOR_16_SWAP) && LV_COLOR_16_SWAP
    SH.swapped = true;
#else
    SH.swapped = lv_display_get_color_format(d) == LV_COLOR_FORMAT_RGB565_SWAPPED;
#endif
    lv_timer_set_period(SH.refr, 5 * 60 * 1000);
    SH.holding = true, SH.held_at = lv_tick_get();
    SH.state = 2;
  } else if (st == 3) {
    unhold();
    SH.state = 0;
  } else if (st == 2 && SH.holding && lv_tick_elaps(SH.held_at) > HOLD_MAX_MS) {
    unhold();  // the stream goes on (may tear), the screen doesn't stay frozen
  }
}
class Handler : public AsyncWebHandler {
 public:
  bool canHandle(AsyncWebServerRequest *request) const override {
    char buf[AsyncWebServerRequest::URL_BUF_SIZE];
    return request->url_to(buf) == "/screenshot";
  }
  void handleRequest(AsyncWebServerRequest *request) override {
    if (!frame_hook) { request->send(501, "text/plain", "no framebuffer on this board"); return; }
    int expected = 0;
    if (!SH.state.compare_exchange_strong(expected, 1)) { request->send(503, "text/plain", "busy"); return; }
    for (int i = 0; i < 60 && SH.state.load() == 1; i++) vTaskDelay(pdMS_TO_TICKS(50));
    expected = 1;
    if (SH.state.compare_exchange_strong(expected, 0)) {
      request->send(503, "text/plain", "LVGL is not running (paused?)");
      return;
    }
    const uint16_t *fb = nullptr;
    int pw = 0, ph = 0, rot = 0;
    if (!frame_hook(&fb, &pw, &ph, &rot) || !fb) {
      SH.state = 3;
      request->send(503, "text/plain", "framebuffer not ready");
      return;
    }
    int w, h;
    fw::shot::size(pw, ph, rot, w, h);
    const int row = w * 2;
    // a band of rows (16 = a 64 B cache line holds 32 px: a rotated read uses each line it loads); smaller if short
    int band = 16;
    uint8_t *out = nullptr;
    while (band >= 1 && !(out = (uint8_t *) malloc((size_t) row * band))) band /= 2;
    if (!out) {
      SH.state = 3;
      request->send(503, "text/plain", "out of memory");
      return;
    }
    uint8_t hd[66] = {'B', 'M'};
    auto le32 = [&](int o, uint32_t v) { for (int k = 0; k < 4; k++) hd[o + k] = (v >> (8 * k)) & 0xFF; };
    le32(2, 66 + row * h); le32(10, 66); le32(14, 40); le32(18, w); le32(22, h);
    hd[26] = 1; hd[28] = 16;
    le32(30, 3); le32(34, row * h); le32(54, 0xF800); le32(58, 0x07E0); le32(62, 0x001F);
    request->beginResponse(200, "image/bmp");
    httpd_req_t *req = *request;
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    bool ok = httpd_resp_send_chunk(req, (const char *) hd, sizeof hd) == ESP_OK;
    // BMP rows go bottom-up: the band covers LVGL rows y0 .. y0 - n + 1, out row k = LVGL row y0 - k
    for (int y0 = h - 1; ok && y0 >= 0; y0 -= band) {
      const int n = std::min(band, y0 + 1);
      fw::shot::band(fb, pw, ph, rot, y0, n, (uint16_t *) out);
      if (SH.swapped) fw::shot::swap((uint16_t *) out, (size_t) n * w);  // the BMP is little-endian RGB565
      for (int k = 0; ok && k < n; k += 4) {
        ok = httpd_resp_send_chunk(req, (const char *) out + k * row, (size_t) std::min(4, n - k) * row) == ESP_OK;
        if (heap_caps_get_free_size(MALLOC_CAP_INTERNAL) < 24 * 1024) vTaskDelay(pdMS_TO_TICKS(20));
        else vTaskDelay(1);
      }
    }
    free(out);
    SH.state = 3;  // the lv_timer releases LVGL's refresh
    if (ok) httpd_resp_send_chunk(req, nullptr, 0);
    httpd_stack_note();
  }
};
}  // namespace shot

#define FW_BENCH_FROM_DEVICE
#include "fw_bench.h"

// ---------------------------------------------------------------- web UI (fw_web_gen.h, optional)
#if __has_include("fw_web_gen.h")
namespace web {
using ::fw::web::File;
using ::fw::web::FILES;
class Handler : public AsyncWebHandler {
 public:
  static const File *find(const char *path) {
    for (const File &f : FILES) if (!strcmp(f.path, path)) return &f;
    return nullptr;
  }
  bool canHandle(AsyncWebServerRequest *req) const override {
    if (req->method() != HTTP_GET) return false;
    char buf[AsyncWebServerRequest::URL_BUF_SIZE];
    return find(req->url_to(buf).c_str()) != nullptr;  // url_to() has no query string (?v= cache busting)
  }
  void handleRequest(AsyncWebServerRequest *request) override {
    char buf[AsyncWebServerRequest::URL_BUF_SIZE];
    const File *f = find(request->url_to(buf).c_str());
    httpd_req_t *req = *request;
    httpd_resp_set_hdr(req, "ETag", f->etag);
    httpd_resp_set_hdr(req, "Cache-Control", f->cache);
    auto inm = request->get_header("If-None-Match");
    if (inm.has_value() && *inm == f->etag) {
      httpd_resp_set_status(req, "304 Not Modified");
      httpd_resp_send(req, nullptr, 0);
      return;
    }
    httpd_resp_set_status(req, HTTPD_200);
    httpd_resp_set_type(req, f->type);
    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    const uint8_t *p = ::fw::web::blob() + f->off;
    for (uint32_t done = 0; done < f->len;) {
      const uint32_t n = std::min<uint32_t>(4096, f->len - done);
      if (httpd_resp_send_chunk(req, (const char *) p + done, n) != ESP_OK) return;  // client gone
      done += n;
      // Wi-Fi TX buffers come out of internal RAM on the Guition (shot.h): give it room when it runs low
      if (heap_caps_get_free_size(MALLOC_CAP_INTERNAL) < 24 * 1024) vTaskDelay(pdMS_TO_TICKS(20));
    }
    httpd_resp_send_chunk(req, nullptr, 0);
  }
};
// ahead of every other handler (ESPHome's web_server claims "/"): AsyncWebServer's list is protected, reached through
// a pointer to member taken in a derived class (legal C++, no layout assumptions)
struct Front : esphome::web_server_idf::AsyncWebServer {
  static void add(esphome::web_server_idf::AsyncWebServer *s, AsyncWebHandler *h) {
    auto list = &Front::handlers_;
    (s->*list).insert((s->*list).begin(), h);
  }
};
}  // namespace web
#endif

// ---------------------------------------------------------------- API keys (see the header; core/keys.h)
namespace agent {
inline void setup() {  // on_boot (main loop): the keys from NVS, kept there on every change, esp_random for new keys
  nvs_handle_t h;
  if (nvs_open("facet_keys", NVS_READONLY, &h) == ESP_OK) {
    size_t n = 0;
    std::string js;
    if (nvs_get_blob(h, "keys", nullptr, &n) == ESP_OK && n && n < 8192) {
      js.resize(n);
      if (nvs_get_blob(h, "keys", js.data(), &n) != ESP_OK) js.clear();
    }
    nvs_close(h);
    if (!js.empty()) fw::keys::restore(js);
  }
  fw::keys::on_random([](uint8_t *out, size_t n) { esp_fill_random(out, n); });
  fw::keys::on_store([](const std::string &js) {
    nvs_handle_t w;
    if (nvs_open("facet_keys", NVS_READWRITE, &w) != ESP_OK) { ESP_LOGE("facet_wb", "keys NVS: cannot open"); return; }
    nvs_set_blob(w, "keys", js.data(), js.size());
    nvs_commit(w);
    nvs_close(w);
  });
}
// the panel's own editor: a browser request from this origin
inline bool editor(AsyncWebServerRequest *req) {
  auto site = req->get_header("Sec-Fetch-Site");
  if (site.has_value()) return *site == "same-origin";
  auto org = req->get_header("Origin"), host = req->get_header("Host");  // an older browser: Origin = this host
  return org.has_value() && host.has_value() && *org == "http://" + *host;
}
inline std::string auth(AsyncWebServerRequest *req) {
  auto a = req->get_header("Authorization");
  return a.has_value() ? std::string(*a) : std::string();
}
}  // namespace agent

// ---------------------------------------------------------------- agent docs (fw_docs_gen.h, optional)
#if __has_include("fw_docs_gen.h")
namespace docs {
class Handler : public AsyncWebHandler {
 public:
  static const ::fw::docs::File *find(const char *path) {
    if (!strcmp(path, "/schema.json")) path = "/fw/schema";  // the same file under the name the docs site uses
    for (int i = 0; i < ::fw::docs::N_FILES; i++) if (!strcmp(::fw::docs::FILES[i].path, path)) return &::fw::docs::FILES[i];
    return nullptr;
  }
  bool canHandle(AsyncWebServerRequest *req) const override {
    if (req->method() != HTTP_GET) return false;
    char buf[AsyncWebServerRequest::URL_BUF_SIZE];
    return find(req->url_to(buf).c_str()) != nullptr;
  }
  void handleRequest(AsyncWebServerRequest *request) override {
    char buf[AsyncWebServerRequest::URL_BUF_SIZE];
    const ::fw::docs::File *f = find(request->url_to(buf).c_str());
    httpd_req_t *req = *request;
    if (!strcmp(f->path, "/ai") && f->raw) {  // the prompt with this panel's address where it says {panel} (plain text)
      auto host = request->get_header("Host");
      const std::string at = "http://" + (host.has_value() && !host->empty() ? std::string(*host) : std::string("<panel address>"));
      std::string t((const char *) f->raw, f->raw_len);
      for (size_t i; (i = t.find("{panel}")) != std::string::npos;) t.replace(i, 7, at);
      httpd_resp_set_status(req, HTTPD_200);
      httpd_resp_set_type(req, f->type);
      httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
      httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
      httpd_resp_send(req, t.data(), t.size());
      return;
    }
    httpd_resp_set_hdr(req, "ETag", f->etag);
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Vary", "Accept-Encoding");
    auto inm = request->get_header("If-None-Match");
    if (inm.has_value() && *inm == f->etag) { httpd_resp_set_status(req, "304 Not Modified"); httpd_resp_send(req, nullptr, 0); return; }
    auto ae = request->get_header("Accept-Encoding");
    const bool gz = !f->raw || (ae.has_value() && ae->find("gzip") != std::string::npos);  // a gzip-only file: always gzip
    httpd_resp_set_status(req, HTTPD_200);
    httpd_resp_set_type(req, f->type);
    if (gz) httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    const uint8_t *p = gz ? f->gz : f->raw;
    const uint32_t len = gz ? f->gz_len : f->raw_len;
    for (uint32_t done = 0; done < len;) {
      const uint32_t n = std::min<uint32_t>(4096, len - done);
      if (httpd_resp_send_chunk(req, (const char *) p + done, n) != ESP_OK) return;
      done += n;
    }
    httpd_resp_send_chunk(req, nullptr, 0);
  }
};
}  // namespace docs
#endif

// ---------------------------------------------------------------- GET /fw/info (also in the problem report)
// built in the main loop (12 KB stack), not on the web server's
inline void info_build(std::string &s) {
  ArduinoJson::JsonDocument d;
  deserializeJson(d, fw::bundle::INFO);
  d["board"] = FW_DEVICE_BOARD;
  d["w"] = FW_DEVICE_W;
  d["h"] = FW_DEVICE_H;
  d["uptime_s"] = (int) (esp_timer_get_time() / 1000000);
  d["probation_s"] = probation_left();
  heap(d["heap"].to<ArduinoJson::JsonObject>());
  art_info(d);
  FacetLcdStats ls{};
  if (facet_lcd_stats && facet_lcd_stats(&ls)) {
    auto l = d["lcd"].to<ArduinoJson::JsonObject>();
    l["vsyncs"] = ls.vsyncs;
    l["frames"] = ls.frames;
    l["missed"] = ls.missed;
    auto w = l["with"].to<ArduinoJson::JsonObject>();
    w["none"] = ls.idle;
    auto b = l["frames_with"].to<ArduinoJson::JsonObject>();
    for (int i = 0; i < fw::busy::N; i++) w[fw::busy::NAMES[i]] = ls.with[i], b[fw::busy::NAMES[i]] = ls.busy[i];
  }
  std::string sh = "null";
  ArduinoJson::JsonDocument devs;
  size_t timers = 0, store = 0, store_bytes = 0, bounds = 0, wants = 0;
  {
    sh = S.shown;
    fw::dev::state(devs.to<ArduinoJson::JsonArray>());
    // the loop's own bookkeeping (memory hunts): LVGL timers (each object delete walks them all),
    // the HA store (every feed's attributes), bound components, subscriptions
    for (lv_timer_t *t = lv_timer_get_next(nullptr); t && timers < 100000; t = lv_timer_get_next(t)) timers++;
    for (const auto &kv : fw::ha::S().store) store++, store_bytes += kv.first.first.capacity() + kv.first.second.capacity() + kv.second.capacity() + 64;
    bounds = fw::ha::S().bounds.size(), wants = fw::ha::wants().size();
  }
  // stacks' least free bytes: the web server task's (after Facet requests) and the main loop's (now)
  d["heap"]["httpd_stack_min"] = httpd_stack_min.load(), d["heap"]["loop_stack_min"] = (int) uxTaskGetStackHighWaterMark(nullptr);
  d["devices"] = devs;
  ArduinoJson::JsonObject lp = d["loop"].to<ArduinoJson::JsonObject>();
  lp["timers"] = timers, lp["store"] = store, lp["store_bytes"] = store_bytes, lp["bounds"] = bounds, lp["wants"] = wants;  // what is on the glass: each mounted device's screen stack (top = shown), builds, objects
  ArduinoJson::JsonDocument shd;
  if (!deserializeJson(shd, sh)) d["shown"] = shd;
  serializeJson(d, s);
}
inline void info_json(std::string &s) {
  if (!in_loop([&] { info_build(s); }, 3000)) s = R"({"ok":false,"error":"the main loop did not answer in time"})";
}

// ---------------------------------------------------------------- problem report (GET /fw/report, fw_report.h)
// (Settings > Report a problem, screens/report.cpp, shows its address and a QR code to a filled-in bug report)
// Kept across restarts in RTC memory that boot doesn't clear (RTC_NOINIT: a software, watchdog or crash restart keeps it,
// and so does the Guition's deep-sleep hop; power loss clears it): the last log lines, boots and connection drops. The
// S3's RTC slow memory is 8 KB, so the Guition keeps 5 KB of log (~40 lines); the P4 keeps 16 KB.
// Crash record: ESPHome's crash handler (its wrap of esp_panic_handler) keeps a panic's reason and backtrace in
// .noinit RAM, which the Guition's hop clears and a power cycle loses. report::begin() (on_boot 850, before the hop at
// 800) has ESPHome log it again into a string and saves that in NVS "facet_report", so it survives power loss.
// The log ring gets every line the logger prints (its level: WARN on the Guition), without colour codes; the report
// passes everything through fw::report::redact (tokens, keys, passwords). The report streams section by section.
namespace report {
#if CONFIG_IDF_TARGET_ESP32S3
constexpr size_t LOG = 5 * 1024;
#else
constexpr size_t LOG = 16 * 1024;
#endif
using Kept = fw::report::Kept<LOG>;
RTC_NOINIT_ATTR inline Kept K;
inline portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
inline bool ready = false, kept = false;
inline std::string *capture = nullptr;  // while ESPHome logs the crash record again
inline std::string crash;               // the saved crash record (NVS)
inline std::atomic<int> wifi_reason{0}, wifi_rssi{0};
struct Watch { bool seen = false, up = false; uint32_t drops = 0; int rssi = 0; };
inline Watch W[fw::report::LINKS];
inline const char *built() {  // the bundle's build time (fw::bundle::INFO "built")
  static std::string b;
  if (b.empty()) {
    ArduinoJson::JsonDocument d;
    deserializeJson(d, fw::bundle::INFO);
    b = d["built"] | "?";
  }
  return b.c_str();
}
inline uint32_t up_s() { return (uint32_t) (esp_timer_get_time() / 1000000); }
inline uint32_t epoch_now() { const time_t t = ::time(nullptr); return t > 1600000000 ? (uint32_t) t : 0; }
inline std::string clock_text(uint32_t ep) {
  if (!ep) return "";
  const time_t t = ep;
  struct tm tm;
  localtime_r(&t, &tm);
  char b[32];
  strftime(b, sizeof b, "%Y-%m-%d %H:%M:%S", &tm);
  return b;
}
// one line into the ring: "[b<boot> <uptime>s <hh:mm:ss>] text"
inline void line(const std::string &text) {
  const uint32_t ms = (uint32_t) (esp_timer_get_time() / 1000), ep = epoch_now();
  char pre[48];
  if (ep) {
    const time_t t = ep;
    struct tm tm;
    localtime_r(&t, &tm);
    snprintf(pre, sizeof pre, "[b%u %u.%us %02d:%02d:%02d] ", (unsigned) K.boot, (unsigned) (ms / 1000),
             (unsigned) (ms % 1000 / 100), tm.tm_hour, tm.tm_min, tm.tm_sec);
  } else snprintf(pre, sizeof pre, "[b%u %u.%us] ", (unsigned) K.boot, (unsigned) (ms / 1000), (unsigned) (ms % 1000 / 100));
  std::string l = pre + text;
  portENTER_CRITICAL(&mux);
  K.put(l.data(), l.size());
  portEXIT_CRITICAL(&mux);
}
inline void on_log(void *, uint8_t, const char *, const char *msg, size_t n) {
  std::string p = fw::report::plain(msg, n);
  if (capture) *capture += p + "\n";
  if (ready) line(p);
}
inline const char *reason_text(int r) {
  switch (r) {
    case ESP_RST_POWERON: return "power on";
    case ESP_RST_SW: return "software restart";
    case ESP_RST_PANIC: return "crash (panic)";
    case ESP_RST_INT_WDT: return "interrupt watchdog";
    case ESP_RST_TASK_WDT: return "task watchdog";
    case ESP_RST_WDT: return "watchdog (RTC)";
    case ESP_RST_BROWNOUT: return "brownout";
    case ESP_RST_DEEPSLEEP: return "deep sleep (hardware reset hop)";
    case ESP_RST_USB: return "USB";
    case ESP_RST_EXT: return "reset pin";
    default: return "other";
  }
}
inline void on_wifi(void *, esp_event_base_t, int32_t id, void *data) {
  if (id != WIFI_EVENT_STA_DISCONNECTED || !data) return;
  const auto *e = (const wifi_event_sta_disconnected_t *) data;
  wifi_reason = e->reason, wifi_rssi = e->rssi;
}
inline void save_crash(const std::string &c) {
  nvs_handle_t h;
  if (nvs_open("facet_report", NVS_READWRITE, &h) != ESP_OK) return;
  nvs_set_str(h, "crash", c.substr(0, 3000).c_str());
  nvs_commit(h);
  nvs_close(h);
}
// on_boot priority 850 (workbench.yaml): before the Guition's hop (s3_reset.h, 800) and before anything else logs much
inline void begin() {
  int r = (int) esp_reset_reason();
  kept = K.begin();
  {
    // the Guition's deep-sleep hop (s3_reset.h): at boot after a reset it continues that boot; a software restart
    // goes straight to it (#121), and that one is a new boot with the reason it stands for
    int why = 0;
    bool restart = false;
    const bool ours = facet_hop && facet_hop(&why, &restart);
    const bool hop = ours && !restart && kept && K.this_boot();
    if (ours && restart) r = why;
    if (!hop) K.new_boot((uint8_t) r);
    ready = true;
    char t[96];
    if (hop) snprintf(t, sizeof t, "--- hardware reset hop (boot %u continues) ---", (unsigned) K.boot);
    else snprintf(t, sizeof t, "--- boot %u: %s%s ---", (unsigned) K.boot, reason_text(r), kept ? "" : " (log starts here)");
    line(t);
  }
  if (auto *lg = esphome::logger::global_logger) lg->add_log_callback(nullptr, on_log);
  nvs_handle_t h;
  if (nvs_open("facet_report", NVS_READONLY, &h) == ESP_OK) {
    size_t n = 0;
    if (nvs_get_str(h, "crash", nullptr, &n) == ESP_OK && n > 1) {
      crash.resize(n);
      nvs_get_str(h, "crash", crash.data(), &n);
      crash.resize(n - 1);
    }
    nvs_close(h);
  }
#ifdef USE_ESP32_CRASH_HANDLER
  if (esphome::esp32::crash_handler_has_data()) {
    std::string c;
    char t[160];
    snprintf(t, sizeof t, "Crash before boot %u (firmware %s, built %s):\n", (unsigned) K.boot,
#ifdef ESPHOME_PROJECT_VERSION
             ESPHOME_PROJECT_VERSION,
#else
             "?",
#endif
             built());
    c = t;
    capture = &c;
    esphome::esp32::crash_handler_log();  // its lines also go into the ring
    capture = nullptr;
    if (facet_overflow_magic == 0x534F5646) {  // a stack overflow: which task (stack_guard.h)
      facet_overflow_task[sizeof facet_overflow_task - 1] = 0;
      c += std::string("Stack overflow in task ") + facet_overflow_task + "\n";
      line(std::string("the crash was a stack overflow in task ") + facet_overflow_task);
    }
    crash = c;
    save_crash(c);
  }
#endif
  facet_overflow_magic = 0;
  esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, on_wifi, nullptr);
}
// every second (workbench.yaml interval, main loop): this boot's run time + wall time, and the links' drops
inline void tick() {
  if (!ready) return;
  const uint32_t now = up_s();
  if (fw::report::Boot *b = K.this_boot()) {
    b->ran_s = now;
    if (!b->epoch && epoch_now()) b->epoch = epoch_now() - now;
  }
  auto *wf = esphome::wifi::global_wifi_component;
  const bool wifi = wf && wf->is_connected();
  if (wifi) W[fw::report::WIFI].rssi = wf->wifi_rssi();
  const bool api = esphome::api::global_api_server && esphome::api::global_api_server->is_connected();
  // the panel's own HA link: watched once it has worked (checked) and while Wi-Fi is up (a Wi-Fi drop is its own line)
  const bool ha_ready = fw::dev::ha_link::ready() && fw::dev::ha_link::checked() && wifi;
  const bool ha = ha_ready && fw::dev::ha_link::status() != fw::dev::ha_link::FAIL;
  const bool up[fw::report::LINKS] = {wifi, api, ha};
  for (int k = 0; k < fw::report::LINKS; k++) {
    Watch &w = W[k];
    if (k == fw::report::HA && !ha_ready) continue;
    if (!w.seen) {  // a link counts drops only once it has been up
      if (up[k]) w.seen = true, w.up = true;
      continue;
    }
    if (w.up == up[k]) continue;
    w.up = up[k];
    if (!up[k]) {
      w.drops++;
      int reason = 0, rssi = 0;
      if (k == fw::report::WIFI) reason = wifi_reason.exchange(0), rssi = wifi_rssi.exchange(0);
      if (!rssi && k == fw::report::WIFI) rssi = w.rssi;
      fw::report::Conn *c;
      portENTER_CRITICAL(&mux);
      c = &K.drop((uint8_t) k, now, epoch_now(), (uint8_t) reason, (int8_t) rssi);
      portEXIT_CRITICAL(&mux);
      std::string t = fw::report::conn_line(*c, false);
      if (k == fw::report::HA) t += ": " + fw::dev::ha_link::why();
      line(t);
    } else {
      portENTER_CRITICAL(&mux);
      K.back((uint8_t) k, now);
      portEXIT_CRITICAL(&mux);
      fw::report::Conn c{};
      K.each_conn([&](const fw::report::Conn &x) { if (x.link == k && x.boot == K.boot && !x.open) c = x; });
      line(fw::report::conn_line(c, true));
    }
  }
}
// GET /fw/report: plain text, redacted, streamed a section at a time (the log ring is copied out first)
inline void send(AsyncWebServerRequest *request) {
  request->beginResponse(200, "text/plain; charset=utf-8");
  httpd_req_t *req = *request;
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  bool ok = true;
  // paced pieces, as the screenshot: Wi-Fi TX buffers come out of internal RAM on the Guition, and one 14 KB chunk
  // (/fw/info) stalled the send and crashed the panel
  auto out = [&](const std::string &t) {
    const std::string r = fw::report::redact(t);
    for (size_t at = 0; ok && at < r.size(); at += 1024) {
      ok = httpd_resp_send_chunk(req, r.data() + at, std::min<size_t>(1024, r.size() - at)) == ESP_OK;
      vTaskDelay(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) < 24 * 1024 ? pdMS_TO_TICKS(20) : 1);
    }
  };
  char t[256];
  const uint32_t now = up_s();
  snprintf(t, sizeof t, "Facet problem report\nfirmware %s, board %s, built %s\nuptime %s, now %s\n\n",
#ifdef ESPHOME_PROJECT_VERSION
           ESPHOME_PROJECT_VERSION,
#else
           "?",
#endif
           FW_DEVICE_BOARD, built(), fw::report::dur(now).c_str(),
           epoch_now() ? clock_text(epoch_now()).c_str() : "unknown (no time yet)");
  out(t);
  Kept *k = (Kept *) malloc(sizeof(Kept));  // a copy, so the ring can go on while this sends (PSRAM)
  if (!k) { out("(not enough memory to copy the kept log)\n"); httpd_resp_send_chunk(req, nullptr, 0); return; }
  portENTER_CRITICAL(&mux);
  memcpy((void *) k, (const void *) &K, sizeof(Kept));
  portEXIT_CRITICAL(&mux);
  std::string o = "== Boots (oldest first) ==\n";
  k->each_boot([&](const fw::report::Boot &b) {
    snprintf(t, sizeof t, "boot %u: %s, ran %s%s%s\n", (unsigned) b.n, reason_text(b.reason), fw::report::dur(b.ran_s).c_str(),
             b.epoch ? ", started " : "", b.epoch ? clock_text(b.epoch).c_str() : "");
    o += t;
  });
  if (!kept && k->boot <= 1) o += "(nothing kept from before this boot: power on)\n";
  out(o + "\n");
  out("== Crash record ==\n" + (crash.empty() ? std::string("none\n") : crash) + "\n");
  o = "== Connections ==\n";
  snprintf(t, sizeof t, "drops since this boot: Wi-Fi %u, HA API %u, HA link %u; now: Wi-Fi %s, HA API %s, HA link %s\n",
           (unsigned) W[0].drops, (unsigned) W[1].drops, (unsigned) W[2].drops, W[0].up ? "up" : "down",
           W[1].up ? "up" : "down", fw::dev::ha_link::ready() ? (W[2].up ? "up" : "down") : "not set up");
  o += t;
  int nc = 0;
  k->each_conn([&](const fw::report::Conn &c) {
    nc++;
    snprintf(t, sizeof t, "boot %u +%us%s%s: %s dropped", (unsigned) c.boot, (unsigned) c.at_s, c.epoch ? " " : "",
             c.epoch ? clock_text(c.epoch).c_str() : "", fw::report::link_name(c.link));
    o += t;
    if (c.link == fw::report::WIFI) {
      snprintf(t, sizeof t, " (reason %u, signal %d dBm)", c.reason, c.rssi);
      o += t;
    }
    o += c.open ? (c.boot == k->boot ? ", still down\n" : ", not back before the restart\n")
                : ", back after " + fw::report::dur(c.down_s) + "\n";
  });
  if (!nc) o += "no drops kept\n";
  out(o + "\n");
  std::string info;
  info_json(info);
  out("== Panel (/fw/info) ==\n" + info + "\n\n");
  std::string set;
  in_loop([&] { fw::dev::settings_json(set); }, 3000);
  out("== Settings (no secrets) ==\n" + (set.empty() ? std::string("(the main loop did not answer)") : set) + "\n\n");
  out("== Log (oldest first; kept across restarts) ==\n");
  o.clear();
  k->lines([&](const std::string &l) {
    o += l + "\n";
    if (o.size() > 1500) { out(o); o.clear(); vTaskDelay(1); }
  });
  out(o);
  free(k);
  if (ok) httpd_resp_send_chunk(req, nullptr, 0);
}
// its own handler: under the /fw/ handler's frame (every route's locals) the report's sends overflowed the web
// server task's stack (crash record: pthread_getspecific in lwip_send, from report::send)
class Handler : public AsyncWebHandler {
 public:
  bool canHandle(AsyncWebServerRequest *request) const override {
    char buf[AsyncWebServerRequest::URL_BUF_SIZE];
    return request->method() == HTTP_GET && request->url_to(buf) == "/fw/report";
  }
  void handleRequest(AsyncWebServerRequest *request) override { send(request); httpd_stack_note(); }
};
}  // namespace report

// ---------------------------------------------------------------- HTTP
class Handler : public AsyncWebHandler {
 public:
  bool canHandle(AsyncWebServerRequest *req) const override {
    char buf[AsyncWebServerRequest::URL_BUF_SIZE];
    auto u = req->url_to(buf);
    return u.size() > 4 && !strncmp(u.c_str(), "/fw/", 4) && strcmp(u.c_str(), "/fw/schema") &&  // /fw/schema: the docs handler
           !(req->method() == HTTP_GET && !strcmp(u.c_str(), "/fw/report"));                     // GET /fw/report: report::Handler
  }
  void handleBody(AsyncWebServerRequest *, uint8_t *data, size_t len, size_t index, size_t total) override {
    if (index == 0) {
      body_.clear();
      too_big_ = total > 32768;  // a bench batch (fw_bench.h) is ~10 KB
    }
    if (!too_big_) body_.append((const char *) data, len);
  }
  bool isRequestHandlerTrivial() const override { return false; }
  void handleRequest(AsyncWebServerRequest *req) override {
    char buf[AsyncWebServerRequest::URL_BUF_SIZE];
    std::string url = req->url_to(buf).c_str();
    std::string path = url.substr(0, url.find('?'));
    const bool post = req->method() == HTTP_POST;
    auto org = req->get_header("Origin");
    const std::string origin = org.has_value() && !org->empty() ? *org : "*";
    if (req->method() == HTTP_OPTIONS) {  // CORS preflight (a browser agent sending Authorization): no gate, no body
      auto *r = req->beginResponse(204, "text/plain", "");
      r->addHeader("Access-Control-Allow-Origin", origin.c_str());
      r->addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
      r->addHeader("Access-Control-Allow-Headers", "Authorization, Content-Type");
      r->addHeader("Access-Control-Allow-Private-Network", "true");
      r->addHeader("Access-Control-Max-Age", "600");
      r->addHeader("Vary", "Origin");
      req->send(r);
      return;
    }
    std::string s, who;  // who = the key's id ("" = the editor, or a read)
    int code = 200;
    const bool ed = agent::editor(req), keyed = path == "/fw/keys" || path.rfind("/fw/keys/", 0) == 0;
    const std::string auth = agent::auth(req), late = R"({"ok":false,"error":"the main loop did not answer in time"})";
    bool refused = false;
    if (keyed) {  // the key routes (core/keys.h http(): request + poll need no key, revoke does)
      const std::string body = too_big_ ? std::string() : body_;
      if (!in_loop([&] { fw::keys::http(post ? "POST" : "GET", path, body, auth, ed, &code, &s); }, 2000)) code = 503, s = late;
    } else if (post) {  // every write: the editor, or a live key with the switch on (keys::allow)
      bool ok = false;
      if (!in_loop([&] { ok = fw::keys::allow(auth, ed, &who, &code, &s); }, 2000)) code = 503, s = late;
      refused = !ok;
    }
    if (keyed || refused) {
      // answered above
    } else if (path == "/fw/meta") {
      fw::meta_json(s);  // the registry is fixed after boot: no loop job needed
    } else if (path == "/fw/info") {
      info_json(s);
    } else if (path == "/fw/perf") {
      ArduinoJson::JsonDocument d;
      const bool reset = req->hasArg("reset");  // url_to() has no query string
      const int nav = req->hasArg("nav") ? (req->arg("nav") == "1" ? 1 : 0) : -1;  // the navigation timing flag (facet-243)
      const int keep = req->hasArg("keep") ? atoi(req->arg("keep").c_str()) : -1;  // kept pages: the cap now (until the next boot)
      in_loop([&] { if (keep >= 0) fw::dev::keep_pages(keep); if (nav >= 0) fw::dev::nav_timing(nav == 1); perf_json(d.to<ArduinoJson::JsonObject>()); if (reset) perf_reset(); }, 2000);
      serializeJson(d, s);
    } else if (path == "/fw/bench" && post) {
      ArduinoJson::JsonDocument body, out;
      if (too_big_ || body_.empty() || deserializeJson(body, body_)) {
        code = 400;
        s = R"({"ok":false,"error":"body is not JSON"})";
      } else {
        std::string e = "the main loop did not answer in time";
        uint32_t total = 0, eta = 0;
        in_loop([&] { e = bench::start(body.as<ArduinoJson::JsonObjectConst>(), &total, &eta); }, 3000);
        if (e.empty()) { code = 200; out["ok"] = true; out["run"] = body["run"]; out["total"] = total; out["eta_s"] = eta; }  // (not 202: ESPHome's web server answers 500)
        else { code = e == "busy" ? 409 : 400; out["ok"] = false; out["error"] = e; }
        serializeJson(out, s);
      }
    } else if (path == "/fw/spec") {
      ArduinoJson::JsonDocument d, body;
      bool ok = true;
      if (post) {
        if (too_big_ || body_.empty() || deserializeJson(body, body_)) { code = 400; ok = false; s = R"({"ok":false,"error":"body is not JSON"})"; }
        else {
          const std::string kind = body["kind"] | "screen";
          std::string arg;
          if (kind == "revert") arg = body["id"] | "all";
          else if (body["spec"].is<const char *>()) arg = body["spec"].as<const char *>();  // a theme file sent as text
          else serializeJson(body["spec"], arg);
          fw::keys::Kind kk;
          const std::string what = fw::keys::describe("/fw/spec", body_, &kk);  // a key's change: toast + Undo (keys::change)
          if (!in_loop([&] { fw::keys::change(who, kk, what, [&] { spec_apply(kind.c_str(), arg, d.to<ArduinoJson::JsonObject>()); return (bool) (d["ok"] | false); }); }, 5000)) { code = 503; ok = false; s = late; }
          else if (!(d["ok"] | false)) code = 400;
        }
      } else in_loop([&] { spec_hello(d.to<ArduinoJson::JsonObject>()); }, 2000);
      if (ok) serializeJson(d, s);
    } else if (path == "/fw/heap") {  // PSRAM's used blocks by size (facet-256: what a slow drop is made of); ?size=N dumps
      // the first bytes of up to 8 blocks of that size. The walk holds the heap lock: no allocation in the callback.
      struct Sz { uint32_t size, n; };
      struct W { Sz t[512]; int k; uint32_t blocks, bytes, want; uint8_t dump[8][48]; int nd; };
      static W w;
      w.k = 0, w.blocks = 0, w.bytes = 0, w.nd = 0;
      w.want = req->hasArg("size") ? (uint32_t) atoi(req->arg("size").c_str()) : 0;
      heap_caps_walk(MALLOC_CAP_SPIRAM, [](walker_heap_into_t, walker_block_info_t b, void *u) -> bool {
        W &w = *(W *) u;
        if (!b.used) return true;
        w.blocks++, w.bytes += b.size;
        if (w.want && b.size == w.want && w.nd < 8) memcpy(w.dump[w.nd++], b.ptr, 48);
        for (int i = 0; i < w.k; i++) if (w.t[i].size == b.size) { w.t[i].n++; return true; }
        if (w.k < 512) w.t[w.k++] = {(uint32_t) b.size, 1};
        return true;
      }, &w);
      ArduinoJson::JsonDocument d;
      d["blocks"] = w.blocks, d["bytes"] = w.bytes, d["sizes"] = w.k;
      ArduinoJson::JsonArray a = d["by_size"].to<ArduinoJson::JsonArray>();
      for (int i = 0; i < w.k; i++) { ArduinoJson::JsonArray e = a.add<ArduinoJson::JsonArray>(); e.add(w.t[i].size); e.add(w.t[i].n); }
      ArduinoJson::JsonArray du = d["dump"].to<ArduinoJson::JsonArray>();
      for (int i = 0; i < w.nd; i++) {
        char hx[48 * 2 + 1], tx[49];
        for (int j = 0; j < 48; j++) { snprintf(hx + 2 * j, 3, "%02x", w.dump[i][j]); tx[j] = isprint(w.dump[i][j]) ? (char) w.dump[i][j] : '.'; }
        tx[48] = 0;
        ArduinoJson::JsonObject o = du.add<ArduinoJson::JsonObject>();
        o["hex"] = hx, o["text"] = tx;
      }
      serializeJson(d, s);
    } else if (path == "/fw/link") {  // the HA link's state for the link page (/link): never the token, its last 4 only
      namespace hl = fw::dev::ha_link;
      ArduinoJson::JsonDocument d;
      in_loop([&] {
        const hl::St st = hl::status();
        d["state"] = !hl::ready() ? "off" : st == hl::CHECKING ? "checking" : st == hl::FAIL ? "failed" : hl::checked() ? "linked" : "set";
        d["url"] = hl::url(), d["token"] = hl::token_hint(), d["why"] = hl::why();
        d["pairing"] = hl::pairing_word(hl::pairing());  // Home Assistant's encryption key: none / unpaired / paired / fixed
      }, 2000);
      // does homeassistant.local answer from here (mDNS through lwIP)? the page offers it as the address. Looked up on
      // this HTTP task, once, again a minute later while it does not
      static int seen = -1;
      static uint32_t seen_at = 0;
      if (seen != 1 && (seen < 0 || (uint32_t) (esp_timer_get_time() / 1000) - seen_at > 60000)) {
        addrinfo hints = {}, *r = nullptr;
        hints.ai_family = AF_INET;
        seen = getaddrinfo("homeassistant.local", nullptr, &hints, &r) == 0 && r ? 1 : 0;
        if (r) freeaddrinfo(r);
        seen_at = (uint32_t) (esp_timer_get_time() / 1000);
      }
      d["seen"] = seen == 1 ? "http://homeassistant.local:8123" : "";
      // Home Assistant's own address as the panel sees it: its ESPHome API connection's peer (works where mDNS does
      // not reach, a panel on its own IoT network); the page offers it when homeassistant.local is not seen
#ifdef USE_API
      std::string ha_at;
      in_loop([&] {
        if (auto *api = esphome::api::global_api_server)
          for (const auto &c : api->active_clients()) {
            if (!c || !strstr(c->get_name(), "Home Assistant")) continue;
            std::array<char, esphome::socket::SOCKADDR_STR_LEN> b{};
            std::string ip = c->get_peername_to(b);
            if (std::count(ip.begin(), ip.end(), ':') == 1) ip.resize(ip.find(':'));  // an IPv4 peer with its port
            if (!ip.empty() && ip.find(':') == std::string::npos) { ha_at = "http://" + ip + ":8123"; break; }
          }
      }, 1000);
      d["ha"] = ha_at;
#endif
      serializeJson(d, s);
    } else if (path == "/fw/pairing/reset" && post) {  // Reset pairing without the glass (the editor or a key, as every write)
      bool ok = false;
      std::string was;
      if (!in_loop([&] { was = fw::dev::ha_link::pairing_word(fw::dev::ha_link::pairing()); ok = fw::dev::ha_link::reset_pairing(); }, 2000)) code = 503, s = late;
      else {
        ArduinoJson::JsonDocument d;
        d["ok"] = ok, d["was"] = was;
        if (!ok) code = 409, d["why"] = was == "paired" ? "the key could not be cleared" : "no key from Home Assistant to clear";
        serializeJson(d, s);
      }
    } else if (path == "/fw/deck") {
      std::string dj;
      bool over = false;
      in_loop([&] { dj = fw::spec::deck_json(); over = !fw::spec::S().deck_over.empty(); }, 2000);
      ArduinoJson::JsonDocument d, dk;
      if (dj.empty() || deserializeJson(dk, dj)) { code = 503; s = R"({"ok":false,"error":"no deck"})"; }
      else { d["deck"] = dk; d["rev"] = dk["rev"]; d["override"] = over; serializeJson(d, s); }
    } else if (path == "/fw/ha") {
      ArduinoJson::JsonDocument d;
      // ?want=e1,e2[&attrs=a1,a2]: subscribe to these too (the panel's editor previews live values)
      std::vector<std::pair<std::string, std::string>> add;
      if (req->hasArg("want")) {
        auto split = [](const std::string &v) { std::vector<std::string> o; size_t i = 0; while (i <= v.size()) { size_t j = v.find(',', i); if (j == std::string::npos) j = v.size(); if (j > i) o.push_back(v.substr(i, j - i)); i = j + 1; } return o; };
        const auto es = split(req->arg("want").c_str()), as = req->hasArg("attrs") ? split(req->arg("attrs").c_str()) : std::vector<std::string>{};
        for (const auto &e : es) if (e.size() < 80 && add.size() < 32) { add.push_back({e, ""}); add.push_back({e, "friendly_name"}); for (const auto &a : as) if (a.size() < 40) add.push_back({e, a}); }
      }
      in_loop([&] {
        for (const auto &p : add) fw::ha::want(p.first, p.second);
        if (add.empty()) {  // ?want= answers only {values} of those entities (minimal)
          ArduinoJson::JsonArray w = d["wants"].to<ArduinoJson::JsonArray>();
          for (const auto &p : fw::ha::wants()) { auto x = w.add<ArduinoJson::JsonArray>(); x.add(p.first); x.add(p.second); }
        }
        ArduinoJson::JsonObject v = d["values"].to<ArduinoJson::JsonObject>();
        for (const auto &kv : fw::ha::S().store) {
          if (!add.empty() && std::none_of(add.begin(), add.end(), [&](const auto &p) { return p.first == kv.first.first; })) continue;  // ?want=: only those
          v[kv.first.first + (kv.first.second.empty() ? "" : "|" + kv.first.second)] = kv.second;
        }
        d["bound"] = (int) fw::ha::S().bounds.size();
      }, 2000);
      serializeJson(d, s);
    } else if (path == "/fw/act" && post) {  // one action of the closed list (CONTRACT.md C50; act_run)
      ArduinoJson::JsonDocument body;
      bool ok = false;
      if (too_big_ || body_.empty() || deserializeJson(body, body_)) { code = 400; s = R"J({"ok":false,"why":"body is not JSON: {\"do\": \"<action>\", ...} (GET /fw/act lists them)"})J"; }
      else {
        fw::keys::Kind kk;
        const std::string what = fw::keys::describe("/fw/act", body_, &kk);  // ha.connect with a key: toast + Undo
        if (!in_loop([&] { fw::keys::change(who, kk, what, [&] { ok = act_run(body.as<ArduinoJson::JsonVariantConst>(), "/fw/act"); return ok; }); serializeJson(act_last(), s); }, 3000)) { code = 503; s = R"({"ok":false,"why":"the main loop did not answer in time"})"; }
      }
      if (code == 200 && !ok) code = 400;
    } else if (path == "/fw/act") {  // the list: every action, its fields, limits, one example (core/spec.h ACTION_DOCS)
      ArduinoJson::JsonDocument d;
      d["usage"] = "POST /fw/act {\"do\": \"<action>\", ...fields} with Authorization: Bearer <key> -> {ok, why?, state?}";
      fw::spec::actions_json(d["actions"].to<ArduinoJson::JsonArray>());
      serializeJson(d, s);
    } else if (path == "/fw/settings") {
      ArduinoJson::JsonDocument d;
      std::string e, cur;
      fw::keys::Kind kk = fw::keys::N_KINDS;
      const std::string what = post ? fw::keys::describe("/fw/settings", body_, &kk) : std::string();
      if (post && (too_big_ || body_.empty())) e = "body is not JSON";
      else if (!in_loop([&] {
                 if (post) fw::keys::change(who, kk, what, [&] { e = fw::dev::restore(body_.c_str(), ed ? fw::dev::EDITOR : fw::dev::HOST); return e.empty(); });
                 fw::dev::settings_json(cur);
               }, 3000))
        e = "the main loop did not answer in time";
      if (post) {
        d["ok"] = e.empty();
        if (!e.empty()) d["error"] = e;
        if (!e.empty()) code = 400;
        ArduinoJson::JsonDocument c;
        if (!deserializeJson(c, cur)) d["settings"] = c;
        serializeJson(d, s);
      } else s = cur.empty() ? R"({"ok":false,"error":"the main loop did not answer in time"})" : cur;
    } else if (path == "/fw/bench") {
      ArduinoJson::JsonDocument d;
      in_loop([&] { bench::result_json(d.to<ArduinoJson::JsonObject>()); }, 2000);
      serializeJson(d, s);
    } else if (path == "/fw/show" && post && !too_big_ && !body_.empty() && body_.find("\"screen\"") != std::string::npos && body_.find("\"id\"") == std::string::npos) {
      ArduinoJson::JsonDocument body, out;  // {screen: <id | name>}: open a deck page / screen on the device
      bool ok = false;
      if (deserializeJson(body, body_) || !body["screen"].is<const char *>()) { code = 400; s = R"({"ok":false,"error":"{\"screen\": \"<id>\"}"})"; }
      else if (!in_loop([&] { ok = screen_go(body["screen"].as<const char *>(), out.to<ArduinoJson::JsonObject>()); })) { code = 503; s = R"({"ok":false,"error":"the main loop did not answer in time"})"; }
      else { if (ok) out["ok"] = true; else code = 400; serializeJson(out, s); }
    } else if (path == "/fw/show" && post) {
      ArduinoJson::JsonDocument body, out;
      if (too_big_ || body_.empty() || deserializeJson(body, body_)) {
        code = 400;
        s = too_big_ ? R"({"ok":false,"error":"body over 32 KB"})"
                     : "{\"ok\":false,\"error\":\"body is not JSON (send Content-Type: application/json)\"}";
      } else {
        bool ok = false;
        if (!in_loop([&] { ok = show(body.as<ArduinoJson::JsonObjectConst>(), out.to<ArduinoJson::JsonObject>()); })) {
          code = 503;
          s = R"({"ok":false,"error":"the main loop did not answer in time"})";
        } else {
          if (ok) save();
          else code = 400;
          serializeJson(out, s);
        }
      }
    } else {
      code = 404;
      s = R"({"ok":false,"error":"docs: /ai /llms.txt /agents.md /schema.json; keys: GET /fw/keys, POST /fw/keys/request, /fw/keys/poll, /fw/keys/revoke; GET /fw/info, GET /fw/meta, GET /fw/perf[?reset=1], POST /fw/show, GET|POST /fw/act, GET|POST /fw/settings, GET|POST /fw/spec, GET /fw/deck, GET /fw/ha, POST|GET /fw/bench"})";
    }
    body_.clear();  // a body is only delivered for some content types: never reuse the previous request's
    too_big_ = false;
    auto *r = req->beginResponse(code, "application/json", s);
    // web_server_idf knows only 200 204 400 401 404 409 422 and sends any other code as 500: set those lines here
    static const std::pair<int, const char *> LINES[] = {{403, "403 Forbidden"}, {410, "410 Gone"}, {429, "429 Too Many Requests"}, {503, "503 Service Unavailable"}};
    for (const auto &[c, line] : LINES) if (c == code) httpd_resp_set_status(*req, line);
    r->addHeader("Cache-Control", "no-store");
    r->addHeader("Access-Control-Allow-Origin", origin.c_str());
    r->addHeader("Vary", "Origin");
    req->send(r);
    httpd_stack_note();
  }

 private:
  std::string body_;
  bool too_big_ = false;
};

// on_boot (after lvgl + web_server): our own screen, the last request again (or the first component), the handler
// ---------------------------------------------------------------- Rotate screen
// The setting "rotation" (0 / 90 / 180 / 270, clockwise) -> the host's LVGL rotation (ESPHome lvgl `rotation:` set in
// the YAML, so it can change at run time; touch points turn with it in the ESPHome LVGL touch path). The host sets
// rotate_hook at boot (workbench.yaml: id(lvgl_root)->set_rotation(deg)); applied at boot and on every change, from the
// next LVGL pass (never inside the tap that changed it: the press and its release must land in one orientation).
inline void (*rotate_hook)(int deg) = nullptr;
inline int rotation_now() { const char *v = fw::dev::setting("rotation"); return v ? atoi(v) : 0; }
inline void apply_rotation() {
  if (!rotate_hook) return;
  lv_async_call([](void *) { if (rotate_hook) rotate_hook(rotation_now()); }, nullptr);
}

// ---------------------------------------------------------------- Update channel
// The setting "updateChannel" (stable | beta | dev) -> which manifest the Firmware update entity checks: the host's
// channel_hook (workbench.yaml: the YAML's update_manifest, ota.json, or ota-beta.json beside it) points the entity at
// it and checks it on the next tick, after the settings are saved (board/fw_update.h set_channel). Applied at boot and
// on every change.
inline void (*channel_hook)(bool beta) = nullptr;
inline void apply_channel() {
  if (!channel_hook) return;
  const char *v = fw::dev::setting("updateChannel");
  channel_hook(v && !strcmp(v, "beta"));
}

// The panel's own Home Assistant link (core/device.h ha_link, Settings > Home Assistant) on the firmware: its address
// and token in NVS "facet_halink" (restored at boot, kept on every ha.connect through on_store; never logged), and its
// reads (GET <url>/api/states and the REST reads) through esp_http_client, and the Energy feed's WebSocket reads through
// esp_transport_ws, each on a task of its own: http and https (https / wss
// checks the server against the ESP-IDF certificate bundle). The body comes back to the main loop in pieces through a
// bounded queue (never held whole: the HTTP task waits while HELD bytes are queued), then done(id, ok, why).
namespace halink {
struct Piece { uint32_t id = 0; std::string data; bool end = false, ok = false; std::string why; };
struct Job { uint32_t id; std::string url, token; };
constexpr size_t HELD = 32 * 1024;
inline std::mutex &mu() { static std::mutex m; return m; }
inline std::deque<Piece> &queue() { static std::deque<Piece> q; return q; }
inline std::atomic<size_t> &held() { static std::atomic<size_t> n{0}; return n; }
inline void push(Piece p) {
  held() += p.data.size();
  std::lock_guard<std::mutex> g(mu());
  queue().push_back(std::move(p));
}
inline void task(void *arg) {
  Job *j = (Job *) arg;
  esp_http_client_config_t c = {};
  c.url = j->url.c_str();
  c.timeout_ms = 15000;
  c.buffer_size = 2048;
  if (j->url.rfind("https://", 0) == 0) c.crt_bundle_attach = esp_crt_bundle_attach;
  bool ok = false;
  std::string why;
  esp_http_client_handle_t h = esp_http_client_init(&c);
  if (!h) why = "the panel could not start the request";
  else {
    const std::string auth = "Bearer " + j->token;
    esp_http_client_set_header(h, "Authorization", auth.c_str());
    const esp_err_t e = esp_http_client_open(h, 0);
    if (e != ESP_OK) {  // plain words on the screens and the link page; the error's name in the log
      why = "cannot reach Home Assistant at that address";
      ESP_LOGW("facet_wb", "HA link: %s", esp_err_to_name(e));
    }
    else {
      esp_http_client_fetch_headers(h);
      const int st = esp_http_client_get_status_code(h);
      if (st == 401) why = "Home Assistant refused the token";
      else if (st != 200) why = "Home Assistant answered " + std::to_string(st);
      else {
        char buf[2048];  // on this task's own stack (10 KB): reads can overlap (the entity lists + an Energy read)
        int n;
        while ((n = esp_http_client_read(h, buf, sizeof buf)) > 0) {
          while (held() > HELD) vTaskDelay(pdMS_TO_TICKS(10));
          Piece p;
          p.id = j->id, p.data.assign(buf, n);
          push(std::move(p));
        }
        ok = n == 0;
        if (!ok) why = "the connection to Home Assistant dropped";
      }
      esp_http_client_close(h);
    }
    esp_http_client_cleanup(h);
  }
  Piece end;
  end.id = j->id, end.end = true, end.ok = ok, end.why = why;
  push(std::move(end));
  delete j;
  vTaskDelete(nullptr);
}
inline void get(uint32_t id, const std::string &url, const std::string &token) {
  auto *j = new Job{id, url, token};
  if (xTaskCreate(task, "fw_ha_get", 10240, j, 4, nullptr) != pdPASS) {
    delete j;
    Piece end;
    end.id = id, end.end = true, end.why = "the panel has no memory for the request now";
    push(std::move(end));
  }
}
// WebSocket reads (ha_link::ws, the Energy feed): <url>/api/websocket (ws, or wss for https), auth with the token, each
// command with its own id, HA's result message streamed back as HA sent it (the workbench host does the same). Reads
// only: the closed list below, else refused before connecting. One task, one connection kept for a round (#133): a read
// queued within WS_IDLE_MS of the last goes on the open connection (no TCP / TLS / auth again), a new link or a
// dropped connection = a fresh one (a reused one that died before its reply: once more on a fresh one), idle = closed
// (before HA's 55 s heartbeat could want a pong). The log says once an hour how many connections, reads and bytes.
struct WsJob { uint32_t id; std::string url, token, cmd; };
constexpr uint32_t WS_IDLE_MS = 15000;
inline bool ws_read_ok(const std::string &cmd) {
  ArduinoJson::JsonDocument d;
  if (deserializeJson(d, cmd)) return false;
  const std::string t = d["type"] | "";
  return t == "energy/get_prefs" || t == "energy/info" || t == "recorder/statistics_during_period" || t == "recorder/list_statistic_ids" ||
         t == "config/entity_registry/list_for_display" || t == "config/device_registry/list" || t == "config/area_registry/list";
}
// one whole frame (a small one: the auth messages) into out; false = closed / timed out / too big
inline bool ws_frame(esp_transport_handle_t ws, std::string &out, size_t most) {
  out.clear();
  char buf[512];
  int total = -1;
  while (total < 0 || (int) out.size() < total) {
    const int n = esp_transport_read(ws, buf, sizeof buf, 15000);
    if (n < 0) return false;
    if (total < 0) total = esp_transport_ws_get_read_payload_len(ws);
    if (n == 0 && total > 0) continue;
    out.append(buf, n);
    if (out.size() > most) return false;
    if (total == 0) break;
  }
  return true;
}
struct WsConn {
  esp_transport_handle_t base = nullptr, ws = nullptr;
  std::string url, token;
  int next_id = 1;
  uint32_t opens = 0, reads = 0, log_at = 0;
  uint64_t bytes = 0;
};
inline void ws_close(WsConn &c) {
  if (c.ws) { esp_transport_close(c.ws); esp_transport_destroy(c.ws); }  // the websocket layer, then its base
  if (c.base) esp_transport_destroy(c.base);
  c.ws = c.base = nullptr;
}
inline bool ws_open(WsConn &c, const WsJob &j, std::string &why) {
  ws_close(c);
  c.url = j.url, c.token = j.token, c.next_id = 1;
  const bool tls = j.url.rfind("https://", 0) == 0;
  std::string rest = j.url.substr(j.url.find("://") + 3), host = rest.substr(0, rest.find('/'));
  int port = tls ? 443 : 80;
  if (const size_t k = host.rfind(':'); k != std::string::npos) port = atoi(host.c_str() + k + 1), host = host.substr(0, k);
  c.base = tls ? esp_transport_ssl_init() : esp_transport_tcp_init();
  if (tls && c.base) esp_transport_ssl_crt_bundle_attach(c.base, esp_crt_bundle_attach);
  c.ws = c.base ? esp_transport_ws_init(c.base) : nullptr;
  if (!c.ws) { why = "the panel could not start the connection"; ws_close(c); return false; }
  esp_transport_ws_set_path(c.ws, "/api/websocket");
  c.opens++;
  std::string m;
  if (esp_transport_connect(c.ws, host.c_str(), port, 15000) < 0) why = "cannot reach Home Assistant at that address";
  else if (!ws_frame(c.ws, m, 1024) || m.find("auth_required") == std::string::npos) why = "Home Assistant did not ask for the token";
  else {
    ArduinoJson::JsonDocument a;
    a["type"] = "auth", a["access_token"] = j.token;
    std::string as;
    serializeJson(a, as);
    const int fin = WS_TRANSPORT_OPCODES_TEXT | WS_TRANSPORT_OPCODES_FIN;
    if (esp_transport_ws_send_raw(c.ws, (ws_transport_opcodes_t) fin, as.c_str(), as.size(), 15000) < 0 || !ws_frame(c.ws, m, 1024)) why = "Home Assistant closed the connection";
    else if (m.find("auth_ok") == std::string::npos) why = "Home Assistant refused the token";
    else return true;
  }
  ws_close(c);
  return false;
}
// one command on the open connection, its result streamed through the queue (never held whole). started = a byte of
// the reply was passed on (no second try then); a frame that is not this command's result (a ping, an older id) is
// skipped
inline bool ws_read(WsConn &c, const WsJob &j, std::string &why, bool &started) {
  ArduinoJson::JsonDocument cd;
  deserializeJson(cd, j.cmd);
  const int id = c.next_id++;
  cd["id"] = id;
  std::string cs;
  serializeJson(cd, cs);
  const int fin = WS_TRANSPORT_OPCODES_TEXT | WS_TRANSPORT_OPCODES_FIN;
  if (esp_transport_ws_send_raw(c.ws, (ws_transport_opcodes_t) fin, cs.c_str(), cs.size(), 15000) < 0) { why = "Home Assistant closed the connection"; return false; }
  const std::string mine = "{\"id\":" + std::to_string(id) + ",";
  char buf[2048];
  for (int frames = 0; frames < 8; frames++) {
    int total = -1, got = 0;
    bool keep = true, first = true;
    while (total < 0 || got < total) {
      const int n = esp_transport_read(c.ws, buf, sizeof buf, 20000);
      if (n < 0) { why = "the connection to Home Assistant dropped"; return false; }
      if (total < 0) {
        total = esp_transport_ws_get_read_payload_len(c.ws);
        const int op = esp_transport_ws_get_read_opcode(c.ws) & 0x0F;
        keep = op == WS_TRANSPORT_OPCODES_TEXT || op == WS_TRANSPORT_OPCODES_CONT;
      }
      if (n == 0) { if (total == 0) break; continue; }
      if (first && keep) keep = std::string(buf, std::min<size_t>(n, mine.size())) == mine, first = false;
      got += n;
      if (!keep) continue;  // read past it
      started = true;
      c.bytes += n;
      while (held() > HELD) vTaskDelay(pdMS_TO_TICKS(10));
      Piece p;
      p.id = j.id, p.data.assign(buf, n);
      push(std::move(p));
    }
    if (keep && total >= 0 && got == total && total > 0) { c.reads++; return true; }
  }
  why = "Home Assistant did not answer the read";
  return false;
}
inline QueueHandle_t &ws_queue() { static QueueHandle_t q = nullptr; return q; }
inline void ws_task(void *) {
  WsConn c;
  for (;;) {
    WsJob *j = nullptr;
    if (xQueueReceive(ws_queue(), &j, c.ws ? pdMS_TO_TICKS(WS_IDLE_MS) : portMAX_DELAY) != pdTRUE) { ws_close(c); continue; }
    std::string why;
    bool ok = false;
    for (int attempt = 0; attempt < 2 && !ok; attempt++) {
      const bool reused = c.ws && c.url == j->url && c.token == j->token;
      if (!reused && !ws_open(c, *j, why)) break;
      bool started = false;
      ok = ws_read(c, *j, why, started);
      if (!ok) { ws_close(c); if (started || !reused) break; }
    }
    const uint32_t now = (uint32_t) (esp_timer_get_time() / 1000);
    if (now - c.log_at >= 3600000u) {
      if (c.log_at) ESP_LOGI("facet_wb", "HA link (WebSocket) in the last hour: %u connections, %u reads, %llu bytes", (unsigned) c.opens, (unsigned) c.reads, (unsigned long long) c.bytes);
      c.log_at = now, c.opens = c.reads = 0, c.bytes = 0;
    }
    Piece end;
    end.id = j->id, end.end = true, end.ok = ok, end.why = why;
    push(std::move(end));
    delete j;
  }
}
inline void ws(uint32_t id, const std::string &url, const std::string &token, const std::string &cmd) {
  Piece end;
  end.id = id, end.end = true;
  if (!ws_read_ok(cmd)) { end.why = "not a read the panel may make"; push(std::move(end)); return; }
  if (!ws_queue()) {  // the one reader task, made on the first read
    ws_queue() = xQueueCreate(8, sizeof(WsJob *));
    if (ws_queue() && xTaskCreate(ws_task, "fw_ha_ws", 12288, nullptr, 4, nullptr) != pdPASS) { vQueueDelete(ws_queue()); ws_queue() = nullptr; }
  }
  auto *j = new WsJob{id, url, token, cmd};
  if (!ws_queue() || xQueueSend(ws_queue(), &j, 0) != pdTRUE) {
    delete j;
    end.why = "the panel has no memory for the request now";
    push(std::move(end));
  }
}
inline void drain(lv_timer_t *) {  // main loop: what the HTTP task read, into ha_link
  for (int k = 0; k < 16; k++) {
    Piece p;
    {
      std::lock_guard<std::mutex> g(mu());
      if (queue().empty()) return;
      p = std::move(queue().front());
      queue().pop_front();
    }
    held() -= p.data.size();
    if (p.end) fw::dev::ha_link::done(p.id, p.ok, p.why);
    else fw::dev::ha_link::chunk(p.id, p.data.data(), p.data.size());
  }
}
inline std::string nvs_str(nvs_handle_t h, const char *key) {
  size_t n = 0;
  if (nvs_get_str(h, key, nullptr, &n) != ESP_OK || !n) return "";
  std::string v(n, '\0');
  if (nvs_get_str(h, key, v.data(), &n) != ESP_OK) return "";
  v.resize(n ? n - 1 : 0);
  return v;
}
// Home Assistant's pairing (core/device.h ha_link::pairing): the ESPHome API's encryption key. The public build has
// none in its YAML (api: encryption: {}), so Home Assistant sets one when it adopts the panel and the API keeps it in
// its own preference; Reset pairing clears that record (APIServer::clear_noise_psk: an all-zeros key = none, written
// at once, every client told to go), so the API answers without encryption and Home Assistant can adopt it again with
// a new key. A key in the YAML (USE_API_NOISE_PSK_FROM_YAML) is FIXED: nothing to clear.
inline void pairing_setup() {
#if defined(USE_API) && defined(USE_API_NOISE)
  namespace hl = fw::dev::ha_link;
  hl::on_pairing([]() -> hl::Pairing {
#ifdef USE_API_NOISE_PSK_FROM_YAML
    return hl::FIXED;
#else
    auto *api = esphome::api::global_api_server;
    if (!api) return hl::NO_PAIRING;
    return api->get_noise_ctx().has_psk() ? hl::PAIRED : hl::UNPAIRED;
#endif
  }, []() -> bool {
#ifdef USE_API_NOISE_PSK_FROM_YAML
    return false;
#else
    auto *api = esphome::api::global_api_server;
    if (!api || !api->clear_noise_psk(true)) return false;
    ESP_LOGW("facet_wb", "Home Assistant pairing reset: the API encryption key is cleared");
    return true;
#endif
  });
#endif
}
inline void setup() {
  nvs_handle_t h;
  if (nvs_open("facet_halink", NVS_READONLY, &h) == ESP_OK) {
    const std::string url = nvs_str(h, "url"), tok = nvs_str(h, "tok");
    nvs_close(h);
    if (!url.empty()) fw::dev::ha_link::restore(url, tok);
  }
  fw::dev::ha_link::on_store([](const std::string &url, const std::string &tok) {
    nvs_handle_t w;
    if (nvs_open("facet_halink", NVS_READWRITE, &w) != ESP_OK) { ESP_LOGE("facet_wb", "HA link NVS: cannot open"); return; }
    nvs_set_str(w, "url", url.c_str());
    nvs_set_str(w, "tok", tok.c_str());
    nvs_commit(w);
    nvs_close(w);
  });
  fw::dev::ha_link::on_get(get);
  fw::dev::ha_link::on_ws(ws);
  pairing_setup();
  lv_timer_create(drain, 20, nullptr);
  // the link page's address (Settings > Home Assistant and Set up show it as a QR code while not linked)
  lv_timer_create([](lv_timer_t *) {
    for (auto &a : esphome::network::get_ip_addresses()) {
      if (!a.is_set() || !a.is_ip4()) continue;
      char b[esphome::network::IP_ADDRESS_BUFFER_SIZE];
      fw::dev::ha_link::set_page(std::string("http://") + a.str_to(b) + "/link");
      return;
    }
    fw::dev::ha_link::set_page("");
  }, 2000, nullptr);
}
}  // namespace halink

inline void setup() {
  fw::bundle::link_all();
  fw::report_screen::set_board(FW_DEVICE_BOARD);  // Settings > Report a problem fills the board in
  // component events + sound requests: logged (TODO: sound playback on the panels; no audio path yet)
  fw::set_sink([](lv_obj_t *, const char *kind, const char *name, const char *detail) {
    ESP_LOGI("facet_wb", "%s %s %s", kind, name, detail);
  });
  lv_display_t *d = lv_display_get_default();
  lv_display_set_theme(d, nullptr);  // clean slate like the WASM host: components style everything themselves
  lv_display_add_event_cb(d, perf_event, LV_EVENT_RENDER_START, nullptr);  // frame stats (GET /fw/perf)
  lv_display_add_event_cb(d, perf_event, LV_EVENT_RENDER_READY, nullptr);
  lv_display_add_event_cb(d, perf_event, LV_EVENT_FLUSH_START, nullptr);   // navigation timing: draw vs copy
  lv_display_add_event_cb(d, perf_event, LV_EVENT_FLUSH_FINISH, nullptr);
  // the underrun counters' REFRESH bit (fw_busy.h, #118)
  lv_display_add_event_cb(d, [](lv_event_t *) { fw::busy::on(fw::busy::REFRESH); }, LV_EVENT_REFR_START, nullptr);
  lv_display_add_event_cb(d, [](lv_event_t *) { fw::busy::off(fw::busy::REFRESH); }, LV_EVENT_REFR_READY, nullptr);
  for (lv_indev_t *i = lv_indev_get_next(nullptr); i; i = lv_indev_get_next(i))  // navigation timing: a finger's lift
    if (lv_indev_get_type(i) == LV_INDEV_TYPE_POINTER) lv_indev_add_event_cb(i, [](lv_event_t *) { fw::dev::nav_touch(); }, LV_EVENT_RELEASED, nullptr);
  S.scr = lv_obj_create(nullptr);
  lv_screen_load(S.scr);
  spec_storage();     // screen specs: the overrides from NVS (each falls back to its last good copy, then factory)
  theme_storage();    // the sent theme: its last-good copy from NVS (theme engine T4)
  agent::setup();     // API keys (core/keys.h): restored from NVS, kept there
  fw::dev::on_mem([] { return heap_caps_get_free_size(MALLOC_CAP_SPIRAM); });  // kept pages: each one's PSRAM
#ifndef FW_KEEP_PAGES
#define FW_KEEP_PAGES 0
#endif
  fw::dev::keep_pages(FW_KEEP_PAGES);  // kept pages (facet-243): the board's cap (0 = every screen built on tap)
  halink::setup();    // the panel's own Home Assistant link: its address + token (NVS), its reads (esp_http_client)
  fw::spec::load();
  for (const auto &x : fw::spec::S().log) ESP_LOGW("facet_wb", "spec boot: %s", x.c_str());
  S.pref = esphome::global_preferences->make_preference<Saved>(0xFAC37B01u, true);
  Saved sv{};
  ArduinoJson::JsonDocument req, out;
  bool shown = false;
  // boot safety (a page that crashes the panel must not come back on every boot): after a crash reset
  // (panic / watchdog / brownout) the saved page is NOT shown again; the device home is, and it replaces the saved one
  const esp_reset_reason_t why = esp_reset_reason();
  const bool crashed = why == ESP_RST_PANIC || why == ESP_RST_INT_WDT || why == ESP_RST_TASK_WDT || why == ESP_RST_WDT || why == ESP_RST_BROWNOUT;
  if (crashed) ESP_LOGW("facet_wb", "reset reason %d (crash): not showing the saved page, showing the device home", (int) why);
  if (!crashed && S.pref.load(&sv) && sv.req[0] && !deserializeJson(req, sv.req, strnlen(sv.req, sizeof sv.req)))
    shown = show(req.as<ArduinoJson::JsonObjectConst>(), out.to<ArduinoJson::JsonObject>());
  if (!shown && !fw::reg().empty()) {
    req.clear();
    req["id"] = fw::find("device") ? "device" : fw::find("button") ? "button" : fw::reg()[0].id;
    if (show(req.as<ArduinoJson::JsonObjectConst>(), out.to<ArduinoJson::JsonObject>()) && crashed) save();
  }
  ESP_LOGI("facet_wb", "%u components, showing %s", (unsigned) fw::reg().size(), S.shown.c_str());
  lv_timer_create(drain, 20, nullptr);
  // settings (core/device.h): the shown page follows every change; the probation chip follows the look
  fw::dev::observe([](const char *id, const char *, fw::dev::From from) {
    if (!strcmp(id, "rotation")) { apply_rotation(); return; }
    if (!strcmp(id, "updateChannel")) { apply_channel(); return; }
    const bool look = !strcmp(id, "theme") || !strcmp(id, "accent");
    if (!look && from != fw::dev::SCREEN) reshow_later(false);  // a value from elsewhere: the controls move
  });
  fw::ha::on_want(ha_subscribe);  // (the pairs wanted so far too)
#ifdef USE_API
  // Home Assistant connected: a state it never answers is a missing entity (core/ha.h MISSING_MS)
  lv_timer_create([](lv_timer_t *) {
    fw::ha::set_connected(esphome::api::global_api_server && esphome::api::global_api_server->is_connected());
  }, 1000, nullptr);
#endif
  fw::ha::on_act(ha_action);
  fw::ha::on_ask(ha_ask);
  fw::spec::want_all();  // every entity the deck's specs bind (the overrides were loaded before the first show)
  images::setup();       // Image URLs: the panel's own download + decode (fw_images.h)
  netread::setup();      // web reads for the feeds' direct sources (fw_net.h)
  {  // Idle shows = Album art: its screen-size picture slots now, while the PSRAM is whole (img_scale.h Slots)
    const char *show = fw::dev::setting("idleShow");
    const int n = show && !strcmp(show, "art") ? fw::img::reserve_screen() : 2;
    if (n < 2) ESP_LOGW("facet_img", "idle art: only %d of 2 screen-size picture slots (no memory)", n);
  }
  fw::dev::on_look([] {  // theme / accent / the sun: draw the page again in the new look (+ the probation chip)
    reshow_later(true);
    PR.shown = -2;
  });
  timers_setup();    // the panel's timers -> the host's HA entities (timers_hook)
  apply_rotation();  // the restored Rotate screen (the select restored it before on_boot)
  apply_channel();   // the restored Update channel
  probation_start();
  dbg::D.ready = true;  // the Debug overlay select restored its value before LVGL was up
  if (dbg::D.mode) { dbg::apply(); if (dbg::D.mode & dbg::COMPS) { dbg::rebuild(); dbg::refresh(); } }
  if (auto *ws = esphome::web_server_base::global_web_server_base) {
    ws->add_handler(new Handler());
    ws->add_handler(new report::Handler());  // GET /fw/report (the /fw/ handler leaves it)
    ws->add_handler(new shot::Handler());  // GET /screenshot
#if __has_include("fw_docs_gen.h")
    ws->add_handler(new docs::Handler());  // GET /ai, /llms.txt, /agents.md, /fw/schema (= /schema.json), /link
#endif
#if __has_include("fw_web_gen.h")
    if (auto *srv = ws->get_server()) web::Front::add(srv, new web::Handler());  // GET / = the web UI
    else ws->add_handler(new web::Handler());  // no server yet: behind web_server ("/" stays ESPHome's, /ed/* works)
#endif
    lv_timer_create(shot::take, 50, nullptr);
  }
}

}  // namespace fw::device
