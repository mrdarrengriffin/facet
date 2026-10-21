// Facet device glue (README "Device firmware"): the workbench's core + foundations + components on a real panel,
// driven over HTTP by the workbench's "Show on device" button. ESPHome only (lvgl + web_server); no house specifics.
//   GET  /fw/info   {v, density, core, files, ids, built, board, w, h, shown, heap}   what this firmware was built from
//   GET  /fw/meta   [{id, name, section, meta}]                                     the registry (same as fw_meta())
//   POST /fw/show   {id, props?, theme?: dark|light, accent?: <set id>}  -> {ok, error?, ms, heap}
// Rendering = fw::render() on one screen of our own (the same call the WASM preview makes): the component centred at
// true size on the Facet bg; content taller than the screen scrolls (doc pages). The last request is kept in flash and
// shown again after a reboot. Requests run in the httpd task; anything touching LVGL is handed to the main loop (an
// lv_timer drains the job queue) and the request waits for it (6 s cap).
#pragma once
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "esphome/core/defines.h"
#include "esphome/core/log.h"
#include "esphome/core/preferences.h"
#include "esphome/components/web_server_base/web_server_base.h"
#include "fw.h"
#include "fw_bundle_gen.h"

namespace fw::device {
using namespace esphome::web_server_idf;

struct Saved {
  char req[1536];  // the last /fw/show body (JSON), "" = none
};
struct State {
  lv_obj_t *scr = nullptr;
  std::string shown = "null";  // last successful request (JSON)
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
  return true;
}
inline void save() {
  Saved s{};
  if (S.shown.size() < sizeof s.req) memcpy(s.req, S.shown.data(), S.shown.size());
  S.pref.save(&s);
}

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
      too_big_ = total > 8192;
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
      heap(d["heap"].to<ArduinoJson::JsonObject>());
      std::string sh = "null";
      in_loop([&] { sh = S.shown; }, 2000);
      ArduinoJson::JsonDocument shd;
      if (!deserializeJson(shd, sh)) d["shown"] = shd;
      serializeJson(d, s);
    } else if (path == "/fw/show" && post) {
      ArduinoJson::JsonDocument body, out;
      if (too_big_ || body_.empty() || deserializeJson(body, body_)) {
        code = 400;
        s = too_big_ ? R"({"ok":false,"error":"body over 8 KB"})"
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
      s = R"({"ok":false,"error":"GET /fw/info, GET /fw/meta, POST /fw/show"})";
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
  S.scr = lv_obj_create(nullptr);
  lv_screen_load(S.scr);
  S.pref = esphome::global_preferences->make_preference<Saved>(0xFAC37B01u, true);
  Saved sv{};
  ArduinoJson::JsonDocument req, out;
  bool shown = false;
  if (S.pref.load(&sv) && sv.req[0] && !deserializeJson(req, sv.req, strnlen(sv.req, sizeof sv.req)))
    shown = show(req.as<ArduinoJson::JsonObjectConst>(), out.to<ArduinoJson::JsonObject>());
  if (!shown && !fw::reg().empty()) {
    req.clear();
    req["id"] = fw::find("button") ? "button" : fw::reg()[0].id;
    show(req.as<ArduinoJson::JsonObjectConst>(), out.to<ArduinoJson::JsonObject>());
  }
  ESP_LOGI("facet_wb", "%u components, showing %s", (unsigned) fw::reg().size(), S.shown.c_str());
  lv_timer_create(drain, 20, nullptr);
  if (auto *ws = esphome::web_server_base::global_web_server_base) ws->add_handler(new Handler());
}

}  // namespace fw::device
