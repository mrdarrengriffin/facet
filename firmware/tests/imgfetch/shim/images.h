#pragma once
// host shim for renderer core/images.h: the host-side API fw_images.h uses, backed by a fake cache with the real
// pending + timeout semantics (fetch_test.cpp). Runs only on the "LVGL" (main) thread, like the real one
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include "lvgl.h"
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
}  // namespace fw::img
