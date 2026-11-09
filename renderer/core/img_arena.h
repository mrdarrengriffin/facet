// The pictures' own memory (the panel): one PSRAM block reserved at boot, while the PSRAM is whole, and run as its own
// heap (ESP-IDF multi_heap). A long run breaks the shared PSRAM into thousands of small blocks (Home Assistant's values,
// LVGL's objects): a cover then found no free block big enough and was refused ("too big (needs 21 KB, 0 KB free)"
// with hundreds of KB free in all), so album art dropped out. Every image buffer (img_scale.h FW_IMG_*, the decoder's
// FW_IMGDEC_*) comes from here first, from the shared PSRAM when the arena is full; free() knows which by the address.
// The WASM build: plain malloc, no arena.
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#ifdef ESP_PLATFORM
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "multi_heap.h"

namespace fw::img::arena {
struct State {
  multi_heap_handle_t h = nullptr;
  uint8_t *start = nullptr;
  size_t size = 0;
  portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;  // the decode task allocates, the main loop frees
};
inline State &S() { static State s; return s; }
// reserve `bytes` now (once; false = no block that big)
inline bool init(size_t bytes) {
  State &s = S();
  if (s.h) return true;
  void *m = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!m) return false;
  s.h = multi_heap_register(m, bytes);
  if (!s.h) { heap_caps_free(m); return false; }
  multi_heap_set_lock(s.h, &s.mux);
  s.start = (uint8_t *) m, s.size = bytes;
  return true;
}
inline bool mine(const void *p) { const State &s = S(); return s.h && p >= s.start && (const uint8_t *) p < s.start + s.size; }
inline void *alloc(size_t n) {
  void *p = S().h ? multi_heap_malloc(S().h, n) : nullptr;
  return p ? p : heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}
// a buffer that only needs one plain block (a download): the shared PSRAM first, the arena when it has no such block,
// so the arena keeps its room for the decode's own buffers
inline void *alloc_shared_first(size_t n) {
  void *p = heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  return p ? p : (S().h ? multi_heap_malloc(S().h, n) : nullptr);
}
inline void free(void *p) {
  if (!p) return;
  if (mine(p)) multi_heap_free(S().h, p);
  else heap_caps_free(p);
}
inline void *realloc(void *p, size_t n) {
  if (!p) return alloc(n);
  if (!mine(p)) return heap_caps_realloc(p, n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (void *q = multi_heap_realloc(S().h, p, n)) return q;
  void *q = heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);  // the arena is full: move it out
  if (!q) return nullptr;
  const size_t old = multi_heap_get_allocated_size(S().h, p);
  memcpy(q, p, old < n ? old : n);
  multi_heap_free(S().h, p);
  return q;
}
inline size_t largest() {  // the biggest piece free now
  if (!S().h) return 0;
  multi_heap_info_t i;
  multi_heap_get_info(S().h, &i);
  return i.largest_free_block;
}
inline size_t size() { return S().size; }
}  // namespace fw::img::arena
#define FW_IMG_ARENA 1
#endif
