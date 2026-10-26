#pragma once
// host shim: the bits of LVGL fw_images.h touches (timers are driven by the test's main loop)
#include <cstdint>
struct lv_timer_t;
typedef void (*lv_timer_cb_t)(lv_timer_t *);
lv_timer_t *lv_timer_create(lv_timer_cb_t cb, uint32_t period, void *user);
uint32_t lv_tick_get();
inline uint32_t lv_tick_diff(uint32_t now, uint32_t then) { return now - then; }
