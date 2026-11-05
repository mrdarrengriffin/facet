// The panel's own image fetch (renderer core/images.h piece 2): every URL an Image wants is downloaded
// and decoded HERE, on the panel, and handed to the same fw::img::put() / fail() the WASM workbench feeds.
//   One worker task (made on the first wanted URL, gone after 20 s with nothing to do), one download at a time, off the
//   LVGL task: ESP-IDF esp_http_client, https checked against the IDF certificate bundle, redirects followed (5 hops),
//   a desktop browser's User-Agent (some hosts refuse unknown clients). Limits from fw::img::budget():
//   body_max bytes (Content-Length over it = refused before reading; more bytes = cut off), timeout_ms for the whole
//   download. A download must be whole: a Content-Length not reached, a chunked body without its last chunk, a
//   picture that stops early = failed ("the download broke off ..."), never half a picture.
//   Decode: fw_imgdec.h (PNG pngle, JPEG TJpgDec) row by row straight into the pixels of each view that wants the URL
//   (fw::img::boxes(), taken on the LVGL task when the URL is queued), in PSRAM: it reserves what this decode needs
//   (its outputs + a few rows + the decoder) and refuses unless the largest free PSRAM block has that plus
//   DECODE_MARGIN ("too big (needs N KB, M KB free)"). A big JPEG shrinks while decoding to the smallest step that
//   still covers its boxes. The old picture stays on screen meanwhile (images.h Swap). Each picture logs its download
//   and decode times and the free PSRAM / internal RAM after it (tag facet_img, INFO); every failure is a WARN with its reason.
//   Only while the screen with the image is visible: a URL is fetched only while fw::img::waiting() says a view still
//   wants it (leaving the screen clears that), queued URLs nothing waits for are dropped and the running download stops.
//   Pixels and failures go back on the LVGL task (an lv_timer, 100 ms): put() / fail() with the same words as the workbench's image fetch
//   ("the site blocked the download (403, bot check)", "not found (404)", "not a picture (text/html)", "timeout (8 s)").
#pragma once
#include <algorithm>
#include <atomic>
#include <deque>
#include <map>
#include <mutex>
#include <new>
#include <string>
#include <vector>
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
#include "device.h"

namespace fw::device::images {

inline const char *const UA = "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/130.0 Safari/537.36";

struct Done {
  std::string url, err;
  fw::imgdec::Result px;
  int64_t got_us = 0, dec_us = 0;  // the download and the decode
  size_t bytes = 0;
};
struct State {
  std::mutex m;
  std::deque<std::string> queue;  // waiting for the worker
  std::string ha;                 // the panel's Home Assistant address (a path's host), set by pump()
  std::map<std::string, std::vector<fw::img::Box>> boxes;  // a queued URL's views (their boxes), set by pump()
  std::deque<Done *> done;        // waiting for the LVGL task
  std::string current;            // the URL being downloaded ("" = none)
  std::atomic<bool> cancel{false};
  TaskHandle_t task = nullptr;
  uint32_t fetched = 0, failed = 0;
  // LVGL task only
  std::string seen;         // the download pump() saw running last tick
  uint32_t progress = 0;    // lv_tick of the worker's last sign of life (a new download or a result)
};
// at most this many URLs wait for the worker; more = failed "too many pictures" (each queued URL is ~150 B of PSRAM,
// the bound is time: the downloads run one after another)
constexpr size_t MAX_QUEUE = 128;
// internal RAM the fetch never takes the panel below (Wi-Fi, LVGL draw, the API need it): checked before every
// connect. TLS buffers come from PSRAM (CONFIG_MBEDTLS_EXTERNAL_MEM_ALLOC), software AES (no DMA bounce buffers)
constexpr size_t INTERNAL_KEEP = 32 * 1024, INTERNAL_BLOCK = 8 * 1024, PSRAM_KEEP = 256 * 1024;
inline bool low_memory() {
  return heap_caps_get_free_size(MALLOC_CAP_INTERNAL) < INTERNAL_KEEP ||
         heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL) < INTERNAL_BLOCK ||
         heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM) < PSRAM_KEEP + fw::img::budget().body_max / 4;
}
inline State &S() { static State s; return s; }
// reads that bring nothing for this long, before the body is whole = the download broke off
constexpr int64_t QUIET_US = 1500 * 1000;

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

// what the decode leaves free for everything else (LVGL's draw layers, Wi-Fi, the API) while it runs
constexpr size_t DECODE_MARGIN = 96 * 1024;
// the most the decode may hold: the largest free PSRAM block, less the margin. The decode itself works out what it
// needs (fw_imgdec.h: outputs + rows + decoder) and is refused if that is more
inline size_t decode_room() {
  const size_t big = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
  return big > DECODE_MARGIN ? big - DECODE_MARGIN : 0;
}

inline void fetch(const std::string &src, const std::vector<fw::img::Box> &boxes, const std::string &ha, Done &d) {
  const auto &B = fw::img::budget();
  if (src[0] == '/' && ha.empty()) { d.err = "no Home Assistant link (Settings > Home Assistant)"; return; }
  const std::string url = src[0] == '/' ? ha + src : src;
  const int64_t t0 = esp_timer_get_time();
  const auto late = [&] { return (esp_timer_get_time() - t0) / 1000 >= B.timeout_ms; };
  char tmo[24];
  snprintf(tmo, sizeof tmo, "timeout (%d s)", B.timeout_ms / 1000);
  if (low_memory()) { d.err = "the panel is low on memory"; return; }
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
    if (hop && low_memory()) { d.err = "the panel is low on memory"; goto out; }
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
    const bool chunked = esp_http_client_is_chunked_response(h);
    int64_t quiet = 0;  // since when reads have brought nothing (0 = they bring bytes)
    for (;;) {
      if (S().cancel) { d.err = "left the screen"; break; }
      if (late()) { d.err = tmo; break; }
      const int k = esp_http_client_read(h, (char *) buf, 4096);
      if (k < 0) { d.err = late() || k == -ESP_ERR_HTTP_EAGAIN ? tmo : "the download broke off"; break; }
      if (k > 0) {
        if (!body.add(buf, (size_t) k)) { d.err = body.err; break; }
        quiet = 0;
        continue;
      }
      if (esp_http_client_is_complete_data_received(h)) break;  // the length reached, or the chunked body's last chunk
      if (len < 0 && !chunked) break;  // no length, not chunked: the server closes at the end (the decode checks it is whole)
      // nothing this read: a chunked body's framing, or the connection went quiet / closed before the end
      const int64_t now = esp_timer_get_time();
      if (!quiet) quiet = now;
      else if (now - quiet > QUIET_US) break;
      vTaskDelay(1);
    }
    heap_caps_free(buf);
    if (d.err.empty() && len >= 0 && body.n != (size_t) len) {
      char t[64];
      snprintf(t, sizeof t, "the download broke off (%u of %u KB)", (unsigned) (body.n / 1024), (unsigned) ((len + 1023) / 1024));
      d.err = t;
    } else if (d.err.empty() && chunked && !esp_http_client_is_complete_data_received(h))
      d.err = "the download broke off (no end to the chunked body)";
    if (!d.err.empty()) goto out;
    esp_http_client_close(h);
    esp_http_client_cleanup(h);
    h = nullptr;
    const int64_t t1 = esp_timer_get_time();
    d.got_us = t1 - t0, d.bytes = body.n;
    const bool ok = fw::imgdec::decode(body.b, body.n, B.src_max_px, decode_room(), boxes, d.px);
    d.dec_us = esp_timer_get_time() - t1;
    if (!ok) {
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
    std::vector<fw::img::Box> boxes;
    std::string ha;
    if (!url.empty()) {
      std::lock_guard<std::mutex> l(s.m);
      auto f = s.boxes.find(url);
      if (f != s.boxes.end()) boxes.swap(f->second), s.boxes.erase(f);
      ha = s.ha;
    }
    if (url.empty()) {
      if (!ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(20000))) {  // 20 s with nothing to do: the task goes (its stack too)
        // the lock is let go BEFORE vTaskDelete: a task deleted while holding it leaves it locked for ever, and the next
        // pump() on the LVGL task waits on it = the UI and the loop stop, the task watchdog restarts the panel
        // (the TBOI grid: exactly 20 s after the last download)
        bool go = false;
        {
          std::lock_guard<std::mutex> l(s.m);
          if (s.queue.empty()) { s.task = nullptr; go = true; }
        }
        if (go) vTaskDelete(nullptr);
      }
      continue;
    }
    Done *d = new (std::nothrow) Done{url, "", {}};  // (a nothrow new; its strings and vector are small)
    if (!d) {  // no memory for the result: the view times out (fails) on its own
      std::lock_guard<std::mutex> l(s.m);
      s.current.clear();
      continue;
    }
    fetch(url, boxes, ha, *d);
    if (uxTaskGetStackHighWaterMark(nullptr) < 1024) ESP_LOGW("facet_img", "fetch task stack low (%u B left)", (unsigned) uxTaskGetStackHighWaterMark(nullptr));
    if (d->err.empty() && d->px.px.empty()) d->err = "failed";
    if (!d->err.empty()) {
      if (d->err != "left the screen") ESP_LOGW("facet_img", "%s: %s", url.c_str(), d->err.c_str());
    } else
      ESP_LOGI("facet_img", "%s: %u B in %d ms, decoded %dx%d (1/%d) into %u box(es) in %d ms, held %u KB", url.c_str(), (unsigned) d->bytes,
               (int) (d->got_us / 1000), d->px.sw, d->px.sh, 1 << d->px.sc, (unsigned) d->px.px.size(), (int) (d->dec_us / 1000),
               (unsigned) (d->px.need / 1024));
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
    if (d->err.empty()) {
      fw::img::put(d->url.c_str(), d->px.px);  // the views take their pixels (no copy)
      s.fetched++;
      ESP_LOGI("facet_img", "in; cache %s; free PSRAM %u KB (largest %u KB), internal %u KB", fw::img::stats().c_str(),
               (unsigned) (heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024), (unsigned) (heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM) / 1024),
               (unsigned) (heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024));
    }
    else if (d->err != "left the screen") { fw::img::fail(d->url.c_str(), d->err.c_str()); s.failed++; }
    fw::imgdec::release(d->px);  // what no view took
    delete d;
  }
  if (!done.empty()) s.progress = lv_tick_get();
  const auto want = fw::img::wants();
  std::vector<std::string> over, keep;  // fail() / alive() run after the lock is let go (they redraw components)
  {
    std::lock_guard<std::mutex> l(s.m);
    s.ha = fw::dev::ha_link::url();
    for (const auto &u : want) {
      if (std::find(s.queue.begin(), s.queue.end(), u) != s.queue.end() || u == s.current) continue;
      if (s.queue.size() >= MAX_QUEUE) over.push_back(u);
      else s.queue.push_back(u), s.boxes[u] = fw::img::boxes(u.c_str());
    }
    s.queue.erase(std::remove_if(s.queue.begin(), s.queue.end(), [](const std::string &u) { return !fw::img::waiting(u.c_str()); }), s.queue.end());
    for (auto f = s.boxes.begin(); f != s.boxes.end();) f = std::find(s.queue.begin(), s.queue.end(), f->first) == s.queue.end() ? s.boxes.erase(f) : std::next(f);
    if (!s.current.empty() && !fw::img::waiting(s.current.c_str())) s.cancel = true;
    if (s.current != s.seen) {  // a new download started: its timeout counts from now
      s.seen = s.current;
      s.progress = lv_tick_get();
      if (!s.current.empty()) keep.push_back(s.current);
    }
    // queued URLs don't time out while the worker is moving (one download at a time: the 35th picture of a grid waits
    // its turn); a worker stuck past two timeouts stops vouching for them, so they fail instead of loading for ever
    if (s.task && lv_tick_diff(lv_tick_get(), s.progress) < (uint32_t) (2 * fw::img::budget().timeout_ms + 2000))
      keep.insert(keep.end(), s.queue.begin(), s.queue.end());
    if (!s.queue.empty()) {
      if (!s.task) {
        // 12 KB internal stack: the TLS handshake runs on it. Core 0 at the loop's priority: decoding never holds up LVGL
        if (xTaskCreatePinnedToCore(worker, "facet_img", 12288, nullptr, 1, &s.task, portNUM_PROCESSORS > 1 ? 0 : tskNO_AFFINITY) != pdPASS) {
          s.task = nullptr;
          ESP_LOGW("facet_img", "no memory for the fetch task");
        } else s.progress = lv_tick_get();
      }
      if (s.task) xTaskNotifyGive(s.task);
    }
  }
  for (const auto &u : over) fw::img::fail(u.c_str(), "too many pictures");
  for (const auto &u : keep) fw::img::alive(u.c_str());
}

inline void setup() { lv_timer_create(pump, 100, nullptr); }

}  // namespace fw::device::images
