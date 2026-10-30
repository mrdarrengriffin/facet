// Facet device glue (README "Device firmware"): the workbench's core + foundations + components on a real panel,
// driven over HTTP by the workbench's "Show on device" button. ESPHome only (lvgl + web_server); no house specifics.
//   GET  /fw/info   {v, density, core, files, ids, built, board, w, h, shown, devices: [{stack, builds, objects}], heap}   what this firmware was built from
//   GET  /fw/meta   [{id, name, section, meta}]                                     the registry (same as fw_meta())
//   POST /fw/show   {id, props?, theme?: dark|light, accent?: <set id>}  -> {ok, error?, ms, heap}
//   GET  /fw/perf   frame stats while animations run (since boot or the last ?reset=1): fps, frame / render ms
//   GET  /fw/settings  the settings backup {"facet":"settings","version":1,"values":{...}} (core/device.h; no secrets)
//   POST /fw/settings  {values:{id: value}} (a backup, or a few) -> each through fw::dev::set (HOST): applied, the
//                      entities follow and keep it (settings.yaml from firmware/settings_yaml.mjs) -> {ok, error?, settings}
//   GET  /screenshot  the glass as a 16-bit BMP (paced chunks; ported from the old panel's shot.h)
//   GET  /fw/spec   the spec hello (core/spec.h: protocol, catalogue hash, components, screens + revs, budgets, heap)
//   POST /fw/spec   {kind: screen|deck|revert, spec?: {...}, id?} -> the apply answer (the HA actions facet_screen /
//                   facet_deck / facet_revert do the same; workbench.yaml)
//   GET  /fw/deck   the deck this panel runs (core/spec.h deck_json: the override, else factory) {deck, rev, override}
//   GET  /fw/ha     the Home Assistant store (core/ha.h): {wants: [[e, a]...], values: {"e|a": value}, bound: n};
//                   ?want=e1,e2[&attrs=a1] subscribes to those first (the panel-served editor's live preview)
//   POST /fw/bench  a batch of bench cases the panel runs on its own screen (fw_bench.h); GET /fw/bench = the report
//   POST /fw/show   {screen: <deck page id | screen id | name>} opens that screen on the device (agents)
//   POST /fw/spec   kind check | check_deck = validate (+ build off screen) only, nothing stored
//   GET  /fw/act    every action of the closed list (core/spec.h ACTION_DOCS: fields, limits, one example each)
//   POST /fw/act    {do, ...fields} -> {ok, why?, state?}: one action, checked + run like a spec tap (fw::spec::act,
//                   CONTRACT.md C50; HA's facet_act does the same: act_from_ha)
//   GET  /llms.txt, /agents.md   the agent docs (bundle fw_docs_gen.h from tools/gen_agents.mjs; open)
// AI agents (facet-163, Darren 2026-10-25): every POST under /fw/ and every GET /fw/ha (it shows the subscribed HA
// values; the editor's ?want= subscribes too) go through agent::allow():
// the panel's own editor (a browser request from this origin: Sec-Fetch-Site same-origin, or Origin = this host)
// always passes; anything else needs the setting Allow AI agents on (only the panel or HA can turn it on, core/
// device.cpp set()) AND `Authorization: Bearer <token>` (or X-Facet-Token). Else 403 with what to do. The token =
// 32 hex in NVS "facet_agent" (made on first boot; agent_token() for the host's HA entity, agent_new_token() makes a
// new one). Not a wall against a hostile LAN client (the same-origin check is browser-enforced only); it stops other
// web pages (CSRF) and agents that were not let in. CORS (browser-based agents): OPTIONS /fw/* answers 204 with
// Allow-Origin = the request's Origin, Allow-Methods GET, POST, OPTIONS, Allow-Headers Authorization, Content-Type,
// X-Facet-Token (+ Private-Network for Chrome), Max-Age 600; every /fw/ response echoes the Origin (Vary: Origin;
// "*" without one). No credentials (cookies) are involved: the token is the only key, so echoing is safe.
// Images (fw_images.h): every URL an Image component wants is downloaded + decoded on the panel (one worker task,
// only while a view waits for it) and handed to fw::img::put() / fail() on the LVGL task.
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
#include "esp_attr.h"
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
#include "nvs.h"
#include "theme_slot.h"
#ifdef USE_API
#include "esphome/components/api/api_server.h"
#endif
#include "fw_bundle_gen.h"
#include "fw_images.h"  // the Image component's pictures: fetched + decoded on the panel (facet-171 piece 2)

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
  // the panel's own settings win (Darren 2026-10-22: an HA change must show): the request's theme / accent are only
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
// seconds until safe_mode confirms the running image (0 = due), -1 = confirmed / not an OTA image under verify
inline int probation_left() {
  static bool done = false;  // once valid it stays valid: no more partition reads
  static int64_t checked = -1;  // the partition state is read at most once a second (callers poll every loop)
  if (done) return -1;
  const int64_t now = esp_timer_get_time();
  if (checked < 0 || now - checked >= 1000000) {
    checked = now;
    esp_ota_img_states_t st;
    if (esp_ota_get_state_partition(esp_ota_get_running_partition(), &st) != ESP_OK || st != ESP_OTA_IMG_PENDING_VERIFY) {
      done = true;
      return -1;
    }
  }
  const int left = FW_PROBATION_S - (int) (now / 1000000);
  return left > 0 ? left : 0;
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

// ---------------------------------------------------------------- firmware update notice (facet-196)
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
// (hub src/wasm/taps.ts) and Components views. Firmware glue, not a contract component: one non-clickable full-screen
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

// ---------------------------------------------------------------- toasts from Home Assistant (facet-190)
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
// YAML does the work in a lambda step (act_from_ha / timer_from_ha) and answers with act_answer() after it (2026-10-28:
// a call without return_response did nothing while the work sat in the respond lambda). Main loop.
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
  if (!ok) out["error"] = out["why"];  // the facet_timer_* answer kept its "error" (2026-10-28)
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
inline void spec_apply(const char *kind, const std::string &json, ArduinoJson::JsonObject out) {
  if (!strcmp(kind, "revert")) { fw::spec::revert(json.empty() ? "all" : json, out); return; }
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
// -> fx::theme_send (core/theme_slot.h): the PANEL parses + guards the file itself (no hub in the product path);
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

// ---------------------------------------------------------------- screenshot (GET /screenshot)
// Ported from the old panel firmware (projects/esphome/panel/include/shot.h, facet-90): what the glass shows as a
// 16-bit BMP. The web server runs in its own task and LVGL is not thread-safe, so the request raises a flag and waits;
// an lv_timer (main loop) snapshots the active screen in the display's RGB565 (+ the top layer, ARGB8888, blended in:
// the probation chip) into PSRAM; the rows go out 4 at a time with a pause (Guition: Wi-Fi TX buffers come out of
// ~45 KB internal RAM; an unpaced response crashed it). Only the internal heap is read (no PSRAM walk while lit).
namespace shot {
struct State {
  std::atomic<int> state{0};  // 0 idle, 1 requested, 2 ready, 3 failed
  lv_draw_buf_t *scr = nullptr, *top = nullptr;
};
inline State SH;
inline void take(lv_timer_t *) {
  if (SH.state.load() != 1) return;
  SH.scr = lv_snapshot_take(lv_screen_active(), LV_COLOR_FORMAT_RGB565);
  SH.top = lv_obj_get_child_count(lv_layer_top()) ? lv_snapshot_take(lv_layer_top(), LV_COLOR_FORMAT_ARGB8888) : nullptr;
  SH.state = SH.scr ? 2 : 3;
}
inline void release() {
  if (SH.scr) lv_draw_buf_destroy(SH.scr);
  if (SH.top) lv_draw_buf_destroy(SH.top);
  SH.scr = SH.top = nullptr;
  SH.state = 0;
}
class Handler : public AsyncWebHandler {
 public:
  bool canHandle(AsyncWebServerRequest *request) const override {
    char buf[AsyncWebServerRequest::URL_BUF_SIZE];
    return request->url_to(buf) == "/screenshot";
  }
  void handleRequest(AsyncWebServerRequest *request) override {
    int expected = 0;
    if (!SH.state.compare_exchange_strong(expected, 1)) { request->send(503, "text/plain", "busy"); return; }
    for (int i = 0; i < 60 && SH.state.load() == 1; i++) vTaskDelay(pdMS_TO_TICKS(50));
    if (SH.state.load() != 2) {
      if (SH.state.load() == 1) SH.state = 0;
      else release();
      request->send(500, "text/plain", "snapshot failed (LVGL paused or out of memory)");
      return;
    }
    const int w = SH.scr->header.w, h = SH.scr->header.h, row = w * 2;
    uint8_t hd[66] = {'B', 'M'};
    auto le32 = [&](int o, uint32_t v) { for (int k = 0; k < 4; k++) hd[o + k] = (v >> (8 * k)) & 0xFF; };
    le32(2, 66 + row * h); le32(10, 66); le32(14, 40); le32(18, w); le32(22, h);
    hd[26] = 1; hd[28] = 16;
    le32(30, 3); le32(34, row * h); le32(54, 0xF800); le32(58, 0x07E0); le32(62, 0x001F);
    request->beginResponse(200, "image/bmp");
    httpd_req_t *req = *request;
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_send_chunk(req, (const char *) hd, sizeof hd);
    static const int ROWS = 4;
    std::vector<uint8_t> out(row * ROWS);
    for (int y0 = h - 1; y0 >= 0; y0 -= ROWS) {
      size_t n = 0;
      for (int y = y0; y > y0 - ROWS && y >= 0; y--) {
        const uint8_t *sp = SH.scr->data + y * SH.scr->header.stride;
        const uint8_t *t = SH.top && y < (int) SH.top->header.h ? SH.top->data + y * SH.top->header.stride : nullptr;
        for (int x = 0; x < w; x++) {
          uint16_t v = sp[2 * x] | (sp[2 * x + 1] << 8);
          if (t && x < (int) SH.top->header.w && t[4 * x + 3]) {
            const int a = t[4 * x + 3];
            int r = (v >> 11) & 31, g = (v >> 5) & 63, b = v & 31;
            r = r << 3 | r >> 2, g = g << 2 | g >> 4, b = b << 3 | b >> 2;
            b = (t[4 * x] * a + b * (255 - a)) / 255;
            g = (t[4 * x + 1] * a + g * (255 - a)) / 255;
            r = (t[4 * x + 2] * a + r * (255 - a)) / 255;
            v = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
          }
          out[n++] = v & 0xFF;
          out[n++] = v >> 8;
        }
      }
      if (httpd_resp_send_chunk(req, (const char *) out.data(), n) != ESP_OK) break;
      if (heap_caps_get_free_size(MALLOC_CAP_INTERNAL) < 24 * 1024) vTaskDelay(pdMS_TO_TICKS(20));
      else vTaskDelay(1);
    }
    httpd_resp_send_chunk(req, nullptr, 0);
    release();
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

// ---------------------------------------------------------------- AI agents (see the header)
namespace agent {
inline std::mutex &lock() { static std::mutex m; return m; }
inline std::string &tok() { static std::string t; return t; }
inline std::atomic<bool> &on() { static std::atomic<bool> o{false}; return o; }
inline void new_token() {
  static const char *H = "0123456789abcdef";
  std::string t;
  for (int i = 0; i < 32; i++) t += H[esp_random() & 15];
  nvs_handle_t h;
  if (nvs_open("facet_agent", NVS_READWRITE, &h) == ESP_OK) { nvs_set_str(h, "tok", t.c_str()); nvs_commit(h); nvs_close(h); }
  std::lock_guard<std::mutex> g(lock());
  tok() = t;
}
inline void setup() {  // on_boot (main loop): the token from NVS (else a new one), the setting's value + its observer
  char buf[40] = {0};
  size_t n = sizeof buf;
  nvs_handle_t h;
  bool have = false;
  if (nvs_open("facet_agent", NVS_READONLY, &h) == ESP_OK) { have = nvs_get_str(h, "tok", buf, &n) == ESP_OK && strlen(buf) == 32; nvs_close(h); }
  if (have) { std::lock_guard<std::mutex> g(lock()); tok() = buf; }
  else new_token();
  const char *v = fw::dev::setting("aiAgents");
  on() = v && !strcmp(v, "true");
  fw::dev::observe([](const char *id, const char *value, fw::dev::From) { if (!strcmp(id, "aiAgents")) on() = !strcmp(value, "true"); });
}
inline bool same(const std::string &a, const std::string &b) {  // constant time
  if (a.size() != b.size() || a.empty()) return false;
  uint8_t d = 0;
  for (size_t i = 0; i < a.size(); i++) d |= a[i] ^ b[i];
  return !d;
}
// nullptr = allowed; else the reason ("off" | "token") and *msg = the 403 body
inline const char *allow(AsyncWebServerRequest *req, std::string *msg) {
  auto site = req->get_header("Sec-Fetch-Site");
  if (site.has_value() && *site == "same-origin") return nullptr;  // the panel's own editor
  if (!site.has_value()) {  // an older browser: Origin = this host
    auto org = req->get_header("Origin"), host = req->get_header("Host");
    if (org.has_value() && host.has_value() && (*org == "http://" + *host)) return nullptr;
  }
  if (!on()) {
    *msg = R"J({"ok":false,"agents":"off","error":"AI agents are off on this panel. Ask its owner to turn on Settings > AI agents > Allow AI agents (on the panel or in Home Assistant), then send Authorization: Bearer <agent token> (Home Assistant: the panel's AI agent token). Docs: /agents.md"})J";
    return "off";
  }
  std::string given;
  auto a = req->get_header("Authorization");
  if (a.has_value() && a->rfind("Bearer ", 0) == 0) given = a->substr(7);
  else if (auto x = req->get_header("X-Facet-Token"); x.has_value()) given = *x;
  std::string want;
  { std::lock_guard<std::mutex> g(lock()); want = tok(); }
  if (same(given, want)) return nullptr;
  *msg = given.empty() ? R"J({"ok":false,"agents":"token","error":"send Authorization: Bearer <agent token> (Home Assistant: the panel's AI agent token entity). Docs: /agents.md"})J"
                       : R"J({"ok":false,"agents":"token","error":"wrong agent token (Home Assistant: the panel's AI agent token entity; it changes when someone makes a new one)"})J";
  return "token";
}
}  // namespace agent
inline std::string agent_token() { std::lock_guard<std::mutex> g(agent::lock()); return agent::tok(); }
inline void agent_new_token() { agent::new_token(); }
inline bool agents_on() { return agent::on(); }

// ---------------------------------------------------------------- agent docs (fw_docs_gen.h, optional)
#if __has_include("fw_docs_gen.h")
namespace docs {
class Handler : public AsyncWebHandler {
 public:
  static const ::fw::docs::File *find(const char *path) {
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
    httpd_resp_set_hdr(req, "ETag", f->etag);
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Vary", "Accept-Encoding");
    auto inm = request->get_header("If-None-Match");
    if (inm.has_value() && *inm == f->etag) { httpd_resp_set_status(req, "304 Not Modified"); httpd_resp_send(req, nullptr, 0); return; }
    auto ae = request->get_header("Accept-Encoding");
    const bool gz = ae.has_value() && ae->find("gzip") != std::string::npos;
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

// ---------------------------------------------------------------- HTTP
class Handler : public AsyncWebHandler {
 public:
  bool canHandle(AsyncWebServerRequest *req) const override {
    char buf[AsyncWebServerRequest::URL_BUF_SIZE];
    auto u = req->url_to(buf);
    return u.size() > 4 && !strncmp(u.c_str(), "/fw/", 4);
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
      r->addHeader("Access-Control-Allow-Headers", "Authorization, Content-Type, X-Facet-Token");
      r->addHeader("Access-Control-Allow-Private-Network", "true");
      r->addHeader("Access-Control-Max-Age", "600");
      r->addHeader("Vary", "Origin");
      req->send(r);
      return;
    }
    std::string s;
    int code = 200;
    if ((post || path == "/fw/ha") && agent::allow(req, &s)) {
      code = 403;  // s = what to do (agent::allow)
    } else if (path == "/fw/meta") {
      fw::meta_json(s);  // the registry is fixed after boot: no loop job needed
    } else if (path == "/fw/info") {
      ArduinoJson::JsonDocument d;
      deserializeJson(d, fw::bundle::INFO);
      d["board"] = FW_DEVICE_BOARD;
      d["w"] = FW_DEVICE_W;
      d["h"] = FW_DEVICE_H;
      d["uptime_s"] = (int) (esp_timer_get_time() / 1000000);
      d["probation_s"] = probation_left();
      heap(d["heap"].to<ArduinoJson::JsonObject>());
      std::string sh = "null";
      ArduinoJson::JsonDocument devs;
      size_t timers = 0, store = 0, store_bytes = 0, bounds = 0, wants = 0;
      in_loop([&] {
        sh = S.shown;
        fw::dev::state(devs.to<ArduinoJson::JsonArray>());
        // the loop's own bookkeeping (memory hunts, 2026-10-30): LVGL timers (each object delete walks them all),
        // the HA store (every feed's attributes), bound components, subscriptions
        for (lv_timer_t *t = lv_timer_get_next(nullptr); t && timers < 100000; t = lv_timer_get_next(t)) timers++;
        for (const auto &kv : fw::ha::S().store) store++, store_bytes += kv.first.first.capacity() + kv.first.second.capacity() + kv.second.capacity() + 64;
        bounds = fw::ha::S().bounds.size(), wants = fw::ha::wants().size();
      }, 2000);
      d["devices"] = devs;
      ArduinoJson::JsonObject lp = d["loop"].to<ArduinoJson::JsonObject>();
      lp["timers"] = timers, lp["store"] = store, lp["store_bytes"] = store_bytes, lp["bounds"] = bounds, lp["wants"] = wants;  // what is on the glass: each mounted device's screen stack (top = shown), builds, objects
      ArduinoJson::JsonDocument shd;
      if (!deserializeJson(shd, sh)) d["shown"] = shd;
      serializeJson(d, s);
    } else if (path == "/fw/perf") {
      ArduinoJson::JsonDocument d;
      const bool reset = req->hasArg("reset");  // url_to() has no query string
      in_loop([&] { perf_json(d.to<ArduinoJson::JsonObject>()); if (reset) perf_reset(); }, 2000);
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
          else serializeJson(body["spec"], arg);
          if (!in_loop([&] { spec_apply(kind.c_str(), arg, d.to<ArduinoJson::JsonObject>()); }, 5000)) { code = 503; ok = false; s = R"({"ok":false,"error":"the main loop did not answer in time"})"; }
          else if (!(d["ok"] | false)) code = 400;
        }
      } else in_loop([&] { spec_hello(d.to<ArduinoJson::JsonObject>()); }, 2000);
      if (ok) serializeJson(d, s);
    } else if (path == "/fw/deck") {
      std::string dj;
      bool over = false;
      in_loop([&] { dj = fw::spec::deck_json(); over = !fw::spec::S().deck_over.empty(); }, 2000);
      ArduinoJson::JsonDocument d, dk;
      if (dj.empty() || deserializeJson(dk, dj)) { code = 503; s = R"({"ok":false,"error":"no deck"})"; }
      else { d["deck"] = dk; d["rev"] = dk["rev"]; d["override"] = over; serializeJson(d, s); }
    } else if (path == "/fw/ha") {
      ArduinoJson::JsonDocument d;
      // ?want=e1,e2[&attrs=a1,a2]: subscribe to these too (the panel's editor previews live values: Darren 2026-10-25)
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
      else if (!in_loop([&] { ok = act_run(body.as<ArduinoJson::JsonVariantConst>(), "/fw/act"); serializeJson(act_last(), s); }, 3000)) { code = 503; s = R"({"ok":false,"why":"the main loop did not answer in time"})"; }
      else if (!ok) code = 400;
    } else if (path == "/fw/act") {  // the list: every action, its fields, limits, one example (core/spec.h ACTION_DOCS)
      ArduinoJson::JsonDocument d;
      d["usage"] = "POST /fw/act {\"do\": \"<action>\", ...fields} with Authorization: Bearer <agent token> -> {ok, why?, state?}";
      fw::spec::actions_json(d["actions"].to<ArduinoJson::JsonArray>());
      serializeJson(d, s);
    } else if (path == "/fw/settings") {
      ArduinoJson::JsonDocument d;
      std::string e, cur;
      if (post && (too_big_ || body_.empty())) e = "body is not JSON";
      else if (!in_loop([&] {
                 if (post) e = fw::dev::restore(body_.c_str(), fw::dev::HOST);
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
      s = R"({"ok":false,"error":"docs: /llms.txt /agents.md; GET /fw/info, GET /fw/meta, GET /fw/perf[?reset=1], POST /fw/show, GET|POST /fw/act, GET|POST /fw/settings, GET|POST /fw/spec, GET /fw/deck, GET /fw/ha, POST|GET /fw/bench"})";
    }
    body_.clear();  // a body is only delivered for some content types: never reuse the previous request's
    too_big_ = false;
    auto *r = req->beginResponse(code, "application/json", s);
    r->addHeader("Cache-Control", "no-store");
    r->addHeader("Access-Control-Allow-Origin", origin.c_str());
    r->addHeader("Vary", "Origin");
    req->send(r);
  }

 private:
  std::string body_;
  bool too_big_ = false;
};

// on_boot (after lvgl + web_server): our own screen, the last request again (or the first component), the handler
// ---------------------------------------------------------------- Rotate screen (facet-191)
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

inline void setup() {
  fw::bundle::link_all();
  // component events + sound requests: logged (TODO: sound playback on the panels; no audio path yet)
  fw::set_sink([](lv_obj_t *, const char *kind, const char *name, const char *detail) {
    ESP_LOGI("facet_wb", "%s %s %s", kind, name, detail);
  });
  lv_display_t *d = lv_display_get_default();
  lv_display_set_theme(d, nullptr);  // clean slate like the WASM host: components style everything themselves
  lv_display_add_event_cb(d, perf_event, LV_EVENT_RENDER_START, nullptr);  // frame stats (GET /fw/perf)
  lv_display_add_event_cb(d, perf_event, LV_EVENT_RENDER_READY, nullptr);
  S.scr = lv_obj_create(nullptr);
  lv_screen_load(S.scr);
  spec_storage();     // screen specs: the overrides from NVS (each falls back to its last good copy, then factory)
  theme_storage();    // the sent theme: its last-good copy from NVS (theme engine T4)
  agent::setup();     // AI agents: the token (NVS) + the setting (facet-163)
  fw::spec::load();
  for (const auto &x : fw::spec::S().log) ESP_LOGW("facet_wb", "spec boot: %s", x.c_str());
  S.pref = esphome::global_preferences->make_preference<Saved>(0xFAC37B01u, true);
  Saved sv{};
  ArduinoJson::JsonDocument req, out;
  bool shown = false;
  // boot safety (2026-10-22: a page that crashes the panel must not come back on every boot): after a crash reset
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
    const bool look = !strcmp(id, "theme") || !strcmp(id, "accent");
    if (!look && from != fw::dev::SCREEN) reshow_later(false);  // a value from elsewhere: the controls move
  });
  fw::ha::on_want(ha_subscribe);  // (the pairs wanted so far too)
  fw::ha::on_act(ha_action);
  fw::ha::on_ask(ha_ask);
  fw::spec::want_all();  // every entity the deck's specs bind (the overrides were loaded before the first show)
  images::setup();       // Image URLs: the panel's own download + decode (fw_images.h)
  fw::dev::on_look([] {  // theme / accent / the sun: draw the page again in the new look (+ the probation chip)
    reshow_later(true);
    PR.shown = -2;
  });
  timers_setup();    // the panel's timers -> the host's HA entities (timers_hook)
  apply_rotation();  // the restored Rotate screen (the select restored it before on_boot)
  probation_start();
  dbg::D.ready = true;  // the Debug overlay select restored its value before LVGL was up
  if (dbg::D.mode) { dbg::apply(); if (dbg::D.mode & dbg::COMPS) { dbg::rebuild(); dbg::refresh(); } }
  if (auto *ws = esphome::web_server_base::global_web_server_base) {
    ws->add_handler(new Handler());
    ws->add_handler(new shot::Handler());  // GET /screenshot
#if __has_include("fw_docs_gen.h")
    ws->add_handler(new docs::Handler());  // GET /llms.txt, /agents.md
#endif
#if __has_include("fw_web_gen.h")
    if (auto *srv = ws->get_server()) web::Front::add(srv, new web::Handler());  // GET / = the web UI
    else ws->add_handler(new web::Handler());  // no server yet: behind web_server ("/" stays ESPHome's, /ed/* works)
#endif
    lv_timer_create(shot::take, 100, nullptr);
  }
}

}  // namespace fw::device
