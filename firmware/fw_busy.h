// What the panel is busy with, for the display's underrun counters (#118): each heavy job sets its bit while it
// runs; the Guition's RGB driver (esphome/components/mipi_rgb) reads the bits at every VSYNC whose frame underran and
// counts the miss against each job that was running (/fw/info "lcd" "with"). facet_busy is defined by that driver
// only: a weak symbol, so on boards without it (and in host tests) these are no-ops.
#pragma once
#include <cstdint>

// facet_busy = jobs running now; facet_busy_seen = jobs that ran at any time since the last VSYNC (the driver resets it
// each frame), so a short job (a 2 ms flush) still counts for the frame it disturbed.
extern "C" volatile uint32_t facet_busy __attribute__((weak));
extern "C" volatile uint32_t facet_busy_seen __attribute__((weak));

namespace fw::busy {

enum : uint32_t {
  REFRESH = 1u << 0,  // an LVGL refresh: layout, render, flush (loop task, core 1); screen builds run outside it
  FLUSH = 1u << 1,    // the flush itself: draw buffer -> PSRAM framebuffer
  FETCH = 1u << 2,    // an image download (TLS + body into PSRAM, worker task, core 0)
  DECODE = 1u << 3,   // an image decode + scale (PSRAM to PSRAM, worker task)
  FLASH = 1u << 4,    // a flash write or erase (NVS settings, OTA, specs: the board hooks the flash driver)
};
constexpr int N = 5;
constexpr const char *NAMES[N] = {"refresh", "flush", "fetch", "decode", "flash"};

inline void on(uint32_t b) {
  if (&facet_busy) __atomic_fetch_or(const_cast<uint32_t *>(&facet_busy), b, __ATOMIC_RELAXED);
  if (&facet_busy_seen) __atomic_fetch_or(const_cast<uint32_t *>(&facet_busy_seen), b, __ATOMIC_RELAXED);
}
inline void off(uint32_t b) {
  if (&facet_busy) __atomic_fetch_and(const_cast<uint32_t *>(&facet_busy), ~b, __ATOMIC_RELAXED);
}
struct Scope {
  uint32_t b;
  explicit Scope(uint32_t b) : b(b) { on(b); }
  ~Scope() { off(b); }
};

}  // namespace fw::busy
