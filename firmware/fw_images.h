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
#include "fw_busy.h"
#include "fw_stacks.h"
#include "fw_imgdec.h"
#if __has_include("soc/soc_caps.h")
#include "soc/soc_caps.h"
#endif
// the ESP32-P4: JPEG through the chip's JPEG engine + PPA first (fw_imgdec_hw.h), the software decode when it says no
#if defined(SOC_JPEG_DECODE_SUPPORTED) && defined(SOC_PPA_SUPPORTED) && __has_include("driver/jpeg_decode.h") && __has_include("driver/ppa.h")
#include "fw_imgdec_hw.h"
#define FW_IMG_HW 1
#endif
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
// the last decode's biggest view (GET /fw/info "art", fw_device.h): what it was made for and what came out
struct Art {
  bool any = false;
  std::string err;           // "" = decoded
  int bw = 0, bh = 0;        // the box
  int ow = 0, oh = 0;        // the pixels kept (smaller than the box = the memory fallback; LVGL scales them up)
  int sw = 0, sh = 0, sc = 0;  // the source as decoded (a JPEG at 1/2^sc)
  bool nearest = false, slot = false;
  int side = 0;              // the fallback's cap on the long side (0 = none)
  size_t need = 0, need_full = 0, room = 0;
  int64_t at_us = 0;
  bool hw = false;           // the chip's JPEG engine decoded it (else software)
  int got_ms = 0, dec_ms = 0;  // the download, the decode (into every box)
};
struct State {
  std::mutex m;
  Art art;
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
// PSRAM questions by trial allocation, never heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM): that one walks every
// block of the PSRAM heap (thousands, their headers in PSRAM: milliseconds) inside the heap's spinlock with this core's
// interrupts off, and the other core's next malloc spins on the lock with ITS interrupts off. On the Guition the RGB
// panel's bounce-buffer refill is an interrupt on the LVGL core: it missed its slot = a band of stale rows (#118).
// A TLSF malloc + free is O(1); the internal heap's walk is short (few blocks, fast RAM) and keeps its call.
inline bool psram_has(size_t n) {
  void *p = heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!p) return false;
  heap_caps_free(p);
  return true;
}
// the largest free PSRAM block up to cap, to 16 KB: bisected trial allocations (~7 for 1 MB)
inline size_t psram_largest(size_t cap) {
  size_t lo = 0, hi = std::min(cap, heap_caps_get_free_size(MALLOC_CAP_SPIRAM)) + 1;
  while (hi - lo > 16 * 1024) {
    const size_t mid = lo + (hi - lo) / 2;
    if (psram_has(mid)) lo = mid;
    else hi = mid;
  }
  return lo;
}
// psram = the shared PSRAM block the fetch wants free first (psram_need); own = what it needs in the pictures' own
// memory instead (arena_need: no margin, nothing else lives there)
inline bool low_memory(size_t psram, size_t own = SIZE_MAX) {  // own: SIZE_MAX = not a picture (the feeds)
  return heap_caps_get_free_size(MALLOC_CAP_INTERNAL) < INTERNAL_KEEP ||
         heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL) < INTERNAL_BLOCK ||
         (!psram_has(psram) && fw::img::arena::largest() < own);
}
// PSRAM_KEEP stands in for the decode's pixels (a 480 x 480 picture is 450 KB). A picture whose views all go into the
// idle art's reserved slots (img_scale.h Slots) or are small needs only the body, the decoder's rows and those small
// views: then the block asked for is that (with the 96 KB decode margin), not PSRAM_KEEP. On the Guition the slots
// take 900 KB for good, so the old 320 KB block was often not there (#127)
inline size_t psram_need(const std::vector<fw::img::Box> &boxes) {
  const size_t old = PSRAM_KEEP + fw::img::budget().body_max / 4;
  size_t small = 0;
  int slotted = 0;
  for (const auto &b : boxes) {
    const size_t px = (size_t) b.w * b.h;
    if (!b.r && b.s == fw::img::cover && fw::img::slot_fits(px * 2) && slotted < fw::img::slots_free()) slotted++;
    else small += px * 3;
  }
  if (!slotted) return old;
  return std::min(old, fw::img::budget().body_max / 4 + 96 * 1024 + small);
}
// the same in the pictures' own memory (img_arena.h): the body's usual size and the views outside the slots, as they
// are (the decode's own check, fw_imgdec.h, refuses what does not fit after all)
inline size_t arena_need(const std::vector<fw::img::Box> &boxes) {
  size_t small = 0;
  int slotted = 0;
  for (const auto &b : boxes) {
    const size_t px = (size_t) b.w * b.h;
    if (!b.r && b.s == fw::img::cover && fw::img::slot_fits(px * 2) && slotted < fw::img::slots_free()) slotted++;
    else small += px * 3;
  }
  return fw::img::budget().body_max / 4 + small;
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
  const size_t big = psram_largest(8u << 20);  // a decode never plans more than this (both boards' budgets)
  // the shared PSRAM less the margin, or the pictures' own arena (its own: no margin), whichever is bigger
  return std::max(big > DECODE_MARGIN ? big - DECODE_MARGIN : 0, fw::img::arena::largest());
}
// the pictures' own memory (img_arena.h), reserved once at boot while the PSRAM is whole: room for one picture's
// download and decode (a 480 panel's idle art needs ~110 KB beside its reserved slots), more on a big panel
inline void arena_setup() {
  const size_t want = fw::img::budget().cache_bytes >= (2u << 20) ? 512 * 1024 : 192 * 1024;
  if (!fw::img::arena::init(want)) ESP_LOGW("facet_img", "no %u KB block for the pictures' own memory: pictures use the shared PSRAM", (unsigned) (want / 1024));
}

// the download as a stream: the first bytes (read to see what the picture is) from buf, then the connection. 0 = the
// end, or a failure (its words in *err: a timeout, left the screen, broke off)
struct In {
  esp_http_client_handle_t h = nullptr;
  const uint8_t *buf = nullptr;
  size_t hn = 0, hat = 0, total = 0;
  int64_t len = -1, quiet = 0, t0 = 0;
  int timeout_ms = 0;
  bool chunked = false, end = false;
  std::string *err = nullptr;
  const char *tmo = "";
};
inline size_t more(void *c, uint8_t *dst, size_t want) {
  In &s = *(In *) c;
  if (s.hat < s.hn) {
    const size_t k = std::min(want, s.hn - s.hat);
    if (dst != s.buf + s.hat) memmove(dst, s.buf + s.hat, k);
    s.hat += k;
    return k;
  }
  for (;;) {
    if (s.end || !s.err->empty()) return 0;
    if (S().cancel) { *s.err = "left the screen"; return 0; }
    const bool late = (esp_timer_get_time() - s.t0) / 1000 >= s.timeout_ms;
    if (late) { *s.err = s.tmo; return 0; }
    const int k = esp_http_client_read(s.h, (char *) dst, (int) want);
    if (k < 0) { *s.err = k == -ESP_ERR_HTTP_EAGAIN ? s.tmo : "the download broke off"; return 0; }
    if (k > 0) { s.quiet = 0; s.total += (size_t) k; return (size_t) k; }
    if (esp_http_client_is_complete_data_received(s.h)) { s.end = true; return 0; }  // the length, or the last chunk
    if (s.len < 0 && !s.chunked) { s.end = true; return 0; }  // no length, not chunked: the server closes at the end
    const int64_t now = esp_timer_get_time();  // nothing this read: chunked framing, or quiet / closed before the end
    if (!s.quiet) s.quiet = now;
    else if (now - s.quiet > QUIET_US) { s.end = true; return 0; }
    vTaskDelay(1);
  }
}

inline void fetch(const std::string &src, const std::vector<fw::img::Box> &boxes, const std::string &ha, Done &d) {
  const auto &B = fw::img::budget();
  if (src[0] == '/' && ha.empty()) { d.err = "no Home Assistant link (Settings > Home Assistant)"; return; }
  const std::string url = src[0] == '/' ? ha + src : src;
  const int64_t t0 = esp_timer_get_time();
  const auto late = [&] { return (esp_timer_get_time() - t0) / 1000 >= B.timeout_ms; };
  char tmo[24];
  snprintf(tmo, sizeof tmo, "timeout (%d s)", B.timeout_ms / 1000);
  const size_t keep = psram_need(boxes), own = arena_need(boxes);
  if (low_memory(keep, own)) { d.err = "the panel is low on memory"; return; }
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
    if (hop && low_memory(keep, own)) { d.err = "the panel is low on memory"; goto out; }
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
    uint8_t *buf = (uint8_t *) heap_caps_malloc(4096, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buf) { d.err = "out of memory"; goto out; }
    In in;
    in.h = h, in.buf = buf, in.len = len, in.chunked = esp_http_client_is_chunked_response(h), in.err = &d.err, in.tmo = tmo;
    in.t0 = t0, in.timeout_ms = B.timeout_ms;
    fw::busy::on(fw::busy::FETCH);
    in.hn = more(&in, buf, 4096);  // the first bytes: what the picture is (sniff), then served again from buf
    bool ok = false;
    // a JPEG for the software decoder: decoded as it downloads, the file never held (a 300 KB cover on the 480 panel)
    const bool stream = d.err.empty() && in.hn && !strcmp(fw::imgdec::sniff(buf, in.hn), "jpeg") && !fw::imgdec::hw_jpeg();
    int64_t t1 = 0;
    if (stream) {
      fw::busy::off(fw::busy::FETCH);
      t1 = esp_timer_get_time();
      d.got_us = t1 - t0;
      fw::busy::on(fw::busy::DECODE);
      ok = fw::imgdec::decode_jpeg_stream(more, &in, B.src_max_px, decode_room(), boxes, d.px);
      fw::busy::off(fw::busy::DECODE);
      d.bytes = in.total;
      if (!d.err.empty()) ok = false;  // the download failed under the decode: its words
    } else {
      fw::imgdec::Body body(B.body_max);
      if (d.err.empty() && !body.expect(len)) d.err = body.err;
      if (d.err.empty() && in.hn && !body.add(buf, in.hn)) d.err = body.err;
      in.hat = in.hn;  // the first bytes are in the body
      while (d.err.empty()) {
        const size_t k = more(&in, buf, 4096);
        if (!k) break;
        if (!body.add(buf, k)) { d.err = body.err; break; }
      }
      fw::busy::off(fw::busy::FETCH);
      if (d.err.empty() && len >= 0 && body.n != (size_t) len) {
        char t[64];
        snprintf(t, sizeof t, "the download broke off (%u of %u KB)", (unsigned) (body.n / 1024), (unsigned) ((len + 1023) / 1024));
        d.err = t;
      } else if (d.err.empty() && in.chunked && !esp_http_client_is_complete_data_received(h))
        d.err = "the download broke off (no end to the chunked body)";
      if (d.err.empty()) {
        esp_http_client_close(h);
        esp_http_client_cleanup(h);
        h = nullptr;
        t1 = esp_timer_get_time();
        d.got_us = t1 - t0, d.bytes = body.n;
        fw::busy::on(fw::busy::DECODE);
        ok = fw::imgdec::decode(body.b, body.n, B.src_max_px, decode_room(), boxes, d.px);
        fw::busy::off(fw::busy::DECODE);
      }
    }
    heap_caps_free(buf);
    if (!d.err.empty()) goto out;
    d.dec_us = esp_timer_get_time() - t1;
    // a finished picture leaves the pictures' own memory for the shared PSRAM when a block there takes it: the arena
    // is the decode's room, not a store (a kept cover there left the next decode 29 KB, img_arena.h)
    if (ok) for (auto &p : d.px.px)
      if (p.data && !p.slot && fw::img::arena::mine(p.data))
        if (void *q = heap_caps_malloc(p.size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)) {
          memcpy(q, p.data, p.size);
          fw::img::arena::free(p.data);
          p.data = (uint8_t *) q;
        }
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
#ifdef FW_IMG_HW
  fw::imgdec::hw_jpeg() = fw::imgdec::hw::jpeg;
#endif
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
        // the lock is let go BEFORE the task ends: a task deleted while holding it leaves it locked for ever, and the next
        // pump() on the LVGL task waits on it = the UI and the loop stop, the task watchdog restarts the panel
        // (the TBOI grid: exactly 20 s after the last download)
        bool go = false;
        {
          std::lock_guard<std::mutex> l(s.m);
          if (s.queue.empty()) { s.task = nullptr; go = true; }
        }
        if (go) fw::stacks::end(fw::stacks::IMG);
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
    fw::stacks::note(fw::stacks::IMG);
    if (uxTaskGetStackHighWaterMark(nullptr) < 1024) ESP_LOGW("facet_img", "fetch task stack low (%u B left)", (unsigned) uxTaskGetStackHighWaterMark(nullptr));
    if (d->err.empty() && d->px.px.empty()) d->err = "failed";
    Art a;
    if (d->err != "left the screen" && !boxes.empty()) {
      a.any = true, a.err = d->err, a.at_us = esp_timer_get_time();
      size_t big = 0;
      for (const auto &bx : boxes)
        if ((size_t) bx.w * bx.h > big) big = (size_t) bx.w * bx.h, a.bw = bx.w, a.bh = bx.h, a.nearest = bx.nearest || bx.s == fw::img::pixel;
      for (const auto &p : d->px.px)
        if (p.box.w == a.bw && p.box.h == a.bh) a.ow = p.w, a.oh = p.h, a.slot = p.slot;
      a.sw = d->px.sw, a.sh = d->px.sh, a.sc = d->px.sc, a.side = d->px.side;
      a.need = d->px.need, a.need_full = d->px.need_full, a.room = d->px.room;
      a.hw = d->px.hw, a.got_ms = (int) (d->got_us / 1000), a.dec_ms = (int) (d->dec_us / 1000);
      if (d->err.empty() && a.side)  // the logger runs at WARN: this shows in the web log
        ESP_LOGW("facet_img", "%s: kept %dx%d for its %dx%d box (memory fallback: %u KB free in one block, %u KB needed at box size)", url.c_str(),
                 a.ow, a.oh, a.bw, a.bh, (unsigned) (a.room / 1024), (unsigned) (a.need_full / 1024));
    }
    if (!d->err.empty()) {
      if (d->err != "left the screen") ESP_LOGW("facet_img", "%s: %s", url.c_str(), d->err.c_str());
    } else
      ESP_LOGI("facet_img", "%s: %u B in %d ms, decoded %dx%d (1/%d, %s) into %u box(es) in %d ms, held %u KB", url.c_str(), (unsigned) d->bytes,
               (int) (d->got_us / 1000), d->px.sw, d->px.sh, 1 << d->px.sc, d->px.hw ? "hardware" : "software", (unsigned) d->px.px.size(),
               (int) (d->dec_us / 1000), (unsigned) (d->px.need / 1024));
    std::lock_guard<std::mutex> l(s.m);
    if (a.any) s.art = a;
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
               (unsigned) (heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024), (unsigned) (psram_largest(8u << 20) / 1024),
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
        // 12 KB stack in PSRAM (fw_stacks.h: the worker never touches flash): the TLS handshake runs on it. Core 0 at the
        // loop's priority: decoding never holds up LVGL
        if (!fw::stacks::spawn(worker, "facet_img", 12288, nullptr, 1, &s.task, portNUM_PROCESSORS > 1 ? 0 : tskNO_AFFINITY, fw::stacks::IMG)) {
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
