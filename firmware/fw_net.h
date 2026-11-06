// The panel's web read for the feeds' direct sources (renderer core/net.h; CONTRACT.md C51b): a public API a screen
// reads itself, such as adsb.lol for the Overhead screen. The renderer asks through fw::net::get(); here:
//   One worker task (made on the first read, gone after 20 s with nothing to do; 12 KB internal stack: the TLS
//   handshake runs on it), one read at a time, off the LVGL task: ESP-IDF esp_http_client, HTTPS checked against the IDF
//   certificate bundle (the image fetch's path, fw_images.h), redirects followed (3 hops), Accept: application/json.
//   Limits: the body at most fw::net::MAX_BODY (128 KB: a Content-Length over it is refused before reading, more bytes
//   cut it off), READ_MS for the whole read; refused when internal RAM is short (fw_images.h low_memory, the same
//   floor as a picture's download) so a TLS handshake never starves Wi-Fi or LVGL. The body grows in PSRAM (a
//   std::string past the IDF's internal-malloc threshold) and is handed over whole, moved, on the LVGL task
//   (fw::net::give / finish from an lv_timer, 100 ms): never two copies of it.
//   How often is the feed's call (core/flights_source.cpp: every 10 s while its screen or Home card shows, backing off
//   on errors). Each read logs its bytes, time and the internal RAM left at its lowest (tag facet_net, INFO); a failure
//   is a WARN with its reason ("couldn't reach the site", "the site answered 429", "too much data", "timeout (10 s)").
#pragma once
#include <deque>
#include <mutex>
#include <string>
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esphome/core/log.h"
#include "fw_busy.h"
#include "fw_images.h"
#include "net.h"

namespace fw::device::netread {
constexpr int READ_MS = 10000;
constexpr size_t PSRAM_FOR_BODY = 160 * 1024;  // the body's room (MAX_BODY + growth), asked of PSRAM before a read

struct Req { uint32_t id; std::string url; };
struct Res { uint32_t id; bool ok; std::string body, why; };
struct State {
  std::mutex m;
  std::deque<Req> queue;
  std::deque<Res> done;
  TaskHandle_t task = nullptr;
};
inline State &S() { static State s; return s; }

inline void read(const std::string &url, Res &r) {
  const int64_t t0 = esp_timer_get_time();
  if (fw::device::images::low_memory(PSRAM_FOR_BODY)) { r.why = "the panel is low on memory"; return; }
  esp_http_client_config_t c{};
  c.url = url.c_str();
  c.timeout_ms = READ_MS;
  c.user_agent = "Facet panel";
  c.buffer_size = 2048;
  c.buffer_size_tx = 1024;
  c.disable_auto_redirect = true;
#ifdef FW_IMG_CRT_BUNDLE
  c.crt_bundle_attach = esp_crt_bundle_attach;
#endif
  esp_http_client_handle_t h = esp_http_client_init(&c);
  if (!h) { r.why = "out of memory"; return; }
  esp_http_client_set_header(h, "Accept", "application/json");
  const auto late = [&] { return (esp_timer_get_time() - t0) / 1000 >= READ_MS; };
  int status = 0;
  int64_t len = -1;
  size_t low = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
  fw::busy::on(fw::busy::FETCH);
  for (int hop = 0;; hop++) {
    if (esp_http_client_open(h, 0) != ESP_OK) { r.why = late() ? "timeout (10 s)" : "couldn't reach the site"; goto out; }
    len = esp_http_client_fetch_headers(h);
    status = esp_http_client_get_status_code(h);
    low = std::min(low, heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    if ((status == 301 || status == 302 || status == 303 || status == 307 || status == 308) && hop < 3) {
      if (esp_http_client_set_redirection(h) != ESP_OK) { r.why = "a redirect without a Location"; goto out; }
      esp_http_client_close(h);
      continue;
    }
    break;
  }
  if (status < 200 || status > 299) {
    char t[40];
    snprintf(t, sizeof t, "the site answered %d", status);
    r.why = t;
    goto out;
  }
  if (len > (int64_t) fw::net::MAX_BODY) { r.why = "too much data"; goto out; }
  {
    r.body.reserve(len > 0 ? (size_t) len : 16 * 1024);
    char buf[1024];
    int64_t quiet = 0;
    for (;;) {
      if (late()) { r.why = "timeout (10 s)"; break; }
      const int k = esp_http_client_read(h, buf, sizeof buf);
      if (k < 0) { r.why = k == -ESP_ERR_HTTP_EAGAIN ? "timeout (10 s)" : "the read broke off"; break; }
      if (k > 0) {
        if (r.body.size() + (size_t) k > fw::net::MAX_BODY) { r.why = "too much data"; break; }
        r.body.append(buf, (size_t) k);
        quiet = 0;
        continue;
      }
      if (esp_http_client_is_complete_data_received(h)) break;
      if (len < 0 && !esp_http_client_is_chunked_response(h)) break;
      const int64_t now = esp_timer_get_time();
      if (!quiet) quiet = now;
      else if (now - quiet > 1500 * 1000) { r.why = "the read broke off"; break; }
      vTaskDelay(1);
    }
    if (r.why.empty() && len >= 0 && r.body.size() != (size_t) len) r.why = "the read broke off";
  }
out:
  fw::busy::off(fw::busy::FETCH);
  if (h) { esp_http_client_close(h); esp_http_client_cleanup(h); }
  r.ok = r.why.empty();
  if (!r.ok) {
    r.body.clear();
    r.body.shrink_to_fit();
    ESP_LOGW("facet_net", "%s: %s", url.c_str(), r.why.c_str());
  } else
    ESP_LOGI("facet_net", "%s: %u B in %d ms, internal RAM %u KB free at its lowest", url.c_str(), (unsigned) r.body.size(),
             (int) ((esp_timer_get_time() - t0) / 1000), (unsigned) (low / 1024));
}

inline void worker(void *) {
  State &s = S();
  for (;;) {
    Req q;
    bool have = false;
    {
      std::lock_guard<std::mutex> l(s.m);
      if (!s.queue.empty()) q = std::move(s.queue.front()), s.queue.pop_front(), have = true;
    }
    if (!have) {
      if (!ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(20000))) {  // 20 s idle: the task goes (its lock let go first)
        bool go = false;
        {
          std::lock_guard<std::mutex> l(s.m);
          if (s.queue.empty()) { s.task = nullptr; go = true; }
        }
        if (go) vTaskDelete(nullptr);
      }
      continue;
    }
    Res r{q.id, false, {}, {}};
    read(q.url, r);
    std::lock_guard<std::mutex> l(s.m);
    s.done.push_back(std::move(r));
  }
}
// the LVGL task: answers back to the renderer, the worker started for new reads
inline void pump(lv_timer_t *) {
  State &s = S();
  std::deque<Res> done;
  bool start = false;
  {
    std::lock_guard<std::mutex> l(s.m);
    done.swap(s.done);
    start = !s.queue.empty() && !s.task;
    if (start) {
      if (xTaskCreatePinnedToCore(worker, "facet_net", 12288, nullptr, 1, &s.task, portNUM_PROCESSORS > 1 ? 0 : tskNO_AFFINITY) != pdPASS) {
        s.task = nullptr;
        for (const Req &q : s.queue) done.push_back({q.id, false, {}, "no memory for the read task"});
        s.queue.clear();
      }
    } else if (s.task && !s.queue.empty()) xTaskNotifyGive(s.task);
  }
  for (Res &r : done) {
    if (r.ok) fw::net::give(r.id, std::move(r.body));
    else fw::net::finish(r.id, false, r.why);
  }
}
inline void setup() {
  fw::net::on_get([](uint32_t id, const std::string &url) {
    std::lock_guard<std::mutex> l(S().m);
    S().queue.push_back({id, url});
  });
  lv_timer_create(pump, 100, nullptr);
}
}  // namespace fw::device::netread
