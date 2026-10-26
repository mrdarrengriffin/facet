// The panel's own image fetch (renderer core/images.h piece 2, board facet-171): every URL an Image wants is downloaded
// and decoded HERE, on the panel, and handed to the same fw::img::put() / fail() the WASM workbench feeds.
//   One worker task (made on the first wanted URL, gone after 20 s with nothing to do), one download at a time, off the
//   LVGL task: ESP-IDF esp_http_client, https checked against the IDF certificate bundle, redirects followed (5 hops),
//   a desktop browser's User-Agent (the hub proxy's: some hosts refuse unknown clients). Limits from fw::img::budget():
//   body_max bytes (Content-Length over it = refused before reading; more bytes = cut off), timeout_ms for the whole
//   download. Decode: fw_imgdec.h (PNG pngle, JPEG TJpgDec) into PSRAM, never more than the free PSRAM allows.
//   Only while the screen with the image is visible: a URL is fetched only while fw::img::waiting() says a view still
//   wants it (leaving the screen clears that), queued URLs nothing waits for are dropped and the running download stops.
//   Pixels and failures go back on the LVGL task (an lv_timer, 100 ms): put() / fail() with the hub proxy's words
//   ("the site blocked the download (403, bot check)", "not found (404)", "not a picture (text/html)", "timeout (8 s)").
#pragma once
#include <algorithm>
#include <atomic>
#include <deque>
#include <mutex>
#include <string>
#include "sdkconfig.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#if defined(CONFIG_MBEDTLS_CERTIFICATE_BUNDLE) && __has_include("esp_crt_bundle.h")
#include "esp_crt_bundle.h"
#define FW_IMG_CRT_BUNDLE 1
#endif
#include "esphome/core/log.h"
#include "fw_imgdec.h"
#include "images.h"

namespace fw::device::images {

inline const char *const UA = "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/130.0 Safari/537.36";

struct Done {
  std::string url, err;
  fw::imgdec::Out px;
};
struct State {
  std::mutex m;
  std::deque<std::string> queue;  // waiting for the worker
  std::deque<Done *> done;        // waiting for the LVGL task
  std::string current;            // the URL being downloaded ("" = none)
  std::atomic<bool> cancel{false};
  TaskHandle_t task = nullptr;
  uint32_t fetched = 0, failed = 0;
};
inline State &S() { static State s; return s; }

// response headers the reasons need (esp_http_client hands them only to the event handler)
struct Hdr {
  std::string type;
  bool bot = false;
};
inline esp_err_t on_event(esp_http_client_event_t *e) {
  if (e->event_id == HTTP_EVENT_ON_HEADER && e->user_data && e->header_key && e->header_value) {
    Hdr *h = (Hdr *) e->user_data;
    if (!strcasecmp(e->header_key, "content-type")) h->type = std::string(e->header_value).substr(0, 40);
    if (!strcasecmp(e->header_key, "cf-mitigated")) h->bot = true;
  }
  return ESP_OK;
}

// the most the decode may hold: the largest free PSRAM block, less a reserve for LVGL and the image cache
inline size_t decode_room() {
  const size_t big = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
  const size_t keep = fw::img::budget().cache_bytes + 512 * 1024;
  return big > keep ? big - keep : 0;
}

inline void fetch(const std::string &url, Done &d) {
  const auto &B = fw::img::budget();
  const int64_t t0 = esp_timer_get_time();
  const auto late = [&] { return (esp_timer_get_time() - t0) / 1000 >= B.timeout_ms; };
  char tmo[24];
  snprintf(tmo, sizeof tmo, "timeout (%d s)", B.timeout_ms / 1000);
  if (heap_caps_get_free_size(MALLOC_CAP_INTERNAL) < 20 * 1024) { d.err = "the panel is low on memory"; return; }
  Hdr hdr;
  esp_http_client_config_t c{};
  c.url = url.c_str();
  c.timeout_ms = B.timeout_ms;
  c.user_agent = UA;
  c.buffer_size = 2048;
  c.buffer_size_tx = 1024;
  c.event_handler = on_event;
  c.user_data = &hdr;
  c.disable_auto_redirect = true;  // followed below (open / fetch_headers don't follow on their own)
#ifdef FW_IMG_CRT_BUNDLE
  c.crt_bundle_attach = esp_crt_bundle_attach;
#endif
  esp_http_client_handle_t h = esp_http_client_init(&c);
  if (!h) { d.err = "out of memory"; return; }
  esp_http_client_set_header(h, "Accept", "image/png,image/jpeg,image/*;q=0.8");
  int status = 0;
  int64_t len = -1;
  for (int hop = 0;; hop++) {
    if (esp_http_client_open(h, 0) != ESP_OK) { d.err = late() ? tmo : "couldn't reach the site"; goto out; }
    len = esp_http_client_fetch_headers(h);
    status = esp_http_client_get_status_code(h);
    if ((status == 301 || status == 302 || status == 303 || status == 307 || status == 308) && hop < 5 && !S().cancel) {
      if (esp_http_client_set_redirection(h) != ESP_OK) { d.err = "a redirect without a Location"; goto out; }
      esp_http_client_close(h);
      hdr = Hdr{};
      continue;
    }
    break;
  }
  if (status < 200 || status > 299) {
    char t[64];
    if (hdr.bot) snprintf(t, sizeof t, "the site blocked the download (%d, bot check)", status);
    else if (status == 401 || status == 403) snprintf(t, sizeof t, "the site blocked the download (%d)", status);
    else if (status == 404 || status == 410) snprintf(t, sizeof t, "not found (%d)", status);
    else snprintf(t, sizeof t, "the site answered %d", status);
    d.err = t;
    goto out;
  }
  {
    fw::imgdec::Body body(B.body_max);
    if (!body.expect(len)) { d.err = body.err; goto out; }
    uint8_t *buf = (uint8_t *) heap_caps_malloc(4096, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buf) { d.err = "out of memory"; goto out; }
    for (;;) {
      if (S().cancel) { d.err = "left the screen"; break; }
      if (late()) { d.err = tmo; break; }
      const int k = esp_http_client_read(h, (char *) buf, 4096);
      if (k == 0 && esp_http_client_is_complete_data_received(h)) break;
      if (k == 0 && len < 0 && body.n) break;  // no length, connection closed: that's all
      if (k < 0) { d.err = late() || k == -ESP_ERR_HTTP_EAGAIN ? tmo : "the download broke off"; break; }
      if (k > 0 && !body.add(buf, (size_t) k)) { d.err = body.err; break; }
      if (k == 0) vTaskDelay(1);
    }
    heap_caps_free(buf);
    if (!d.err.empty()) goto out;
    esp_http_client_close(h);
    esp_http_client_cleanup(h);
    h = nullptr;
    const long max_px = B.src_max_px;
    if (!fw::imgdec::decode(body.b, body.n, max_px, decode_room(), d.px)) {
      d.err = d.px.err;
      if (d.err == "not a picture") d.err += " (" + (hdr.type.empty() ? std::string("no type") : hdr.type.substr(0, hdr.type.find(';'))) + ")";
    }
  }
out:
  if (h) { esp_http_client_close(h); esp_http_client_cleanup(h); }
}

inline void worker(void *) {
  State &s = S();
  for (;;) {
    std::string url;
    {
      std::lock_guard<std::mutex> l(s.m);
      if (!s.queue.empty()) { url = s.queue.front(); s.queue.pop_front(); s.current = url; s.cancel = false; }
    }
    if (url.empty()) {
      if (!ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(20000))) {  // 20 s with nothing to do: the task goes (its stack too)
        std::lock_guard<std::mutex> l(s.m);
        if (s.queue.empty()) { s.task = nullptr; vTaskDelete(nullptr); }
      }
      continue;
    }
    Done *d = new Done{url, "", {}};
    fetch(url, *d);
    if (d->err.empty() && !d->px.rgba) d->err = "failed";
    if (!d->err.empty()) ESP_LOGW("facet_img", "%s: %s", url.c_str(), d->err.c_str());
    else ESP_LOGI("facet_img", "%s: %dx%d", url.c_str(), d->px.w, d->px.h);
    std::lock_guard<std::mutex> l(s.m);
    s.current.clear();
    s.done.push_back(d);
  }
}

// LVGL task: results in, new wants out, stop what nothing waits for
inline void pump(lv_timer_t *) {
  State &s = S();
  std::deque<Done *> done;
  {
    std::lock_guard<std::mutex> l(s.m);
    done.swap(s.done);
  }
  for (Done *d : done) {
    if (d->px.rgba) { fw::img::put(d->url.c_str(), d->px.w, d->px.h, d->px.rgba); s.fetched++; }
    else if (d->err != "left the screen") { fw::img::fail(d->url.c_str(), d->err.c_str()); s.failed++; }
    fw::imgdec::release(d->px);
    delete d;
  }
  const auto want = fw::img::wants();
  std::lock_guard<std::mutex> l(s.m);
  for (const auto &u : want)
    if (std::find(s.queue.begin(), s.queue.end(), u) == s.queue.end() && u != s.current) s.queue.push_back(u);
  s.queue.erase(std::remove_if(s.queue.begin(), s.queue.end(), [](const std::string &u) { return !fw::img::waiting(u.c_str()); }), s.queue.end());
  if (!s.current.empty() && !fw::img::waiting(s.current.c_str())) s.cancel = true;
  if (s.queue.empty()) return;
  if (!s.task) {
    // 10 KB stack: the TLS handshake runs on it. Core 0 at the loop's priority: decoding never holds up LVGL
    if (xTaskCreatePinnedToCore(worker, "facet_img", 10240, nullptr, 1, &s.task, portNUM_PROCESSORS > 1 ? 0 : tskNO_AFFINITY) != pdPASS) {
      s.task = nullptr;
      ESP_LOGW("facet_img", "no memory for the fetch task");
      return;
    }
  }
  xTaskNotifyGive(s.task);
}

inline void setup() { lv_timer_create(pump, 100, nullptr); }

}  // namespace fw::device::images
