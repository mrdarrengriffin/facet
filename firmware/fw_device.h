// Facet device glue (README "Device firmware"): the workbench's core + foundations + components on a real panel,
// driven over HTTP by the workbench's "Show on device" button. ESPHome only (lvgl + web_server); no house specifics.
//   GET  /fw/info   {v, density, core, files, ids, built, board, w, h, shown, heap}   what this firmware was built from
//   GET  /fw/meta   [{id, name, section, meta}]                                     the registry (same as fw_meta())
//   POST /fw/show   {id, props?, theme?: dark|light, accent?: <set id>}  -> {ok, error?, ms, heap}
//   GET  /fw/perf   frame stats while animations run (since boot or the last ?reset=1): fps, frame / render ms
//   POST /fw/bench  a batch of bench cases the panel runs on its own screen (fw_bench.h); GET /fw/bench = the report
// Probation: while the running OTA image is still ESP_OTA_IMG_PENDING_VERIFY (safe_mode has not confirmed it yet,
// boot_is_good_after = FW_PROBATION_S), a warn Status chip on the LVGL top layer (over every screen) says
// "Update settling: don't unplug m:ss"; a reboot then would roll back to the previous image. Gone when confirmed
// ("Update confirmed" ok chip for 4 s). /fw/info "probation_s" = seconds left (-1 = confirmed); YAML binary_sensor.
// Rendering = fw::render() on one screen of our own (the same call the WASM preview makes): the component centred at
// true size on the Facet bg; content taller than the screen scrolls (doc pages). The last request is kept in flash and
// shown again after a reboot. Requests run in the httpd task; anything touching LVGL is handed to the main loop (an
// lv_timer drains the job queue) and the request waits for it (6 s cap).
#pragma once
#include <algorithm>
#include <cstring>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "esp_attr.h"
#include "esphome/core/defines.h"
#include "esphome/core/log.h"
#include "esphome/core/preferences.h"
#include "esphome/components/web_server_base/web_server_base.h"
#include "fw.h"
#include "device.h"
#include "fw_bundle_gen.h"

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
// Counted only while an LVGL animation runs: each rendered frame's start (interval = frame time) and its render +
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
    P.counting = lv_anim_count_running() > 0;
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
  const int64_t t0 = esp_timer_get_time();
  const char *e = fw::render(S.scr, id, props.c_str(), light, accent_index(req["accent"] | ""));
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
  fw::T = fw::theme(S.light, S.accent);
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

#define FW_BENCH_FROM_DEVICE
#include "fw_bench.h"

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
    std::string s;
    int code = 200;
    if (path == "/fw/meta") {
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
      in_loop([&] { sh = S.shown; }, 2000);
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
    } else if (path == "/fw/bench") {
      ArduinoJson::JsonDocument d;
      in_loop([&] { bench::result_json(d.to<ArduinoJson::JsonObject>()); }, 2000);
      serializeJson(d, s);
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
      s = R"({"ok":false,"error":"GET /fw/info, GET /fw/meta, GET /fw/perf[?reset=1], POST /fw/show, POST|GET /fw/bench"})";
    }
    body_.clear();  // a body is only delivered for some content types: never reuse the previous request's
    too_big_ = false;
    auto *r = req->beginResponse(code, "application/json", s);
    r->addHeader("Cache-Control", "no-store");
    r->addHeader("Access-Control-Allow-Origin", "*");
    req->send(r);
  }

 private:
  std::string body_;
  bool too_big_ = false;
};

// on_boot (after lvgl + web_server): our own screen, the last request again (or the first component), the handler
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
  probation_start();
  if (auto *ws = esphome::web_server_base::global_web_server_base) ws->add_handler(new Handler());
}

}  // namespace fw::device
