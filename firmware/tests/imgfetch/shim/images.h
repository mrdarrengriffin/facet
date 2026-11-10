#pragma once
// host shim for renderer core/images.h: the host-side API fw_images.h uses, backed by a fake cache with the real
// pending + timeout semantics (fetch_test.cpp). Runs only on the "LVGL" (main) thread, like the real one
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>
#include "lvgl.h"
#include "img_scale.h"  // renderer/core (the real scaler: Box, Pixels, Sink)
namespace fw::img {
struct Budget {
  size_t cache_bytes;
  int src_max_px;
  int timeout_ms;
  const char *board;
  size_t body_max;
};
const Budget &budget();
std::vector<std::string> wants();
void put(const char *src, int w, int h, const uint8_t *rgba);
void fail(const char *src, const char *why);
bool waiting(const char *src);
void alive(const char *src);
std::vector<Box> boxes(const char *src);
void put(const char *src, std::vector<Pixels> &px);
inline std::string stats() { return "{}"; }
}  // namespace fw::img
// the pictures' own memory (renderer/core/img_arena.h) is panel-only: on the host there is none, so every buffer comes
// from the shared heap, as on a panel whose boot found no block for it
namespace fw::img::arena {
inline bool init(size_t) { return false; }
inline bool mine(const void *) { return false; }
inline size_t largest() { return 0; }
inline void free(void *p) { ::free(p); }
}  // namespace fw::img::arena
