// Task stacks (#152): their sizes, where they live and each one's least free bytes, for /fw/info "stacks".
// - httpd (the web server) and loopTask (LVGL + every component) stay in internal RAM: both write to flash (settings,
//   decks, OTA), and a stack in PSRAM can't be touched while the flash cache is off. The web server's size is set
//   where ESPHome starts it (httpd_start is wrapped: esphome/board/stacks.h), the loop's in the board package
//   (loop_task_stack_size).
// - The workers that never touch flash (image fetch + decode, the feeds' web reads, the Home Assistant reads) put their
//   stacks in PSRAM (spawn() below): ~46 KB of internal RAM when they all run, which pays for the two bigger stacks.
//   A worker made by spawn() ends with end(), never vTaskDelete (its stack and TCB came from the caps allocator).
#pragma once
#include <atomic>
#include "sdkconfig.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#if defined(CONFIG_SPIRAM) && defined(CONFIG_FREERTOS_TASK_CREATE_ALLOW_EXT_MEM)
#include "freertos/idf_additions.h"
#endif

namespace fw::stacks {
enum Id { HTTPD, LOOP, IMG, NET, HA_WS, HA_GET, N };
inline constexpr const char *NAMES[N] = {"httpd", "loop", "img", "net", "ha_ws", "ha_get"};
#if defined(CONFIG_SPIRAM) && defined(CONFIG_FREERTOS_TASK_CREATE_ALLOW_EXT_MEM)
inline constexpr bool PSRAM = true;
#else
inline constexpr bool PSRAM = false;
#endif
struct Slot {
  std::atomic<int> size{0}, least{-1};  // bytes; least free = -1 until the task first notes it
  std::atomic<bool> psram{false};
};
inline Slot *slots() { static Slot s[N]; return s; }
// the calling task's least free bytes so far into its slot (a worker: after each job and before it ends)
inline void note(Id id) {
  const int f = (int) uxTaskGetStackHighWaterMark(nullptr);
  auto &m = slots()[id].least;
  int v = m.load();
  while ((v < 0 || f < v) && !m.compare_exchange_weak(v, f)) {}
}
inline void sized(Id id, int bytes, bool psram) { slots()[id].size = bytes, slots()[id].psram = psram; }
// a worker that never touches flash (no NVS, settings, OTA or partition reads): stack in PSRAM where the build allows it
inline bool spawn(TaskFunction_t fn, const char *name, uint32_t bytes, void *arg, UBaseType_t prio, TaskHandle_t *h, BaseType_t core, Id id) {
  sized(id, (int) bytes, PSRAM);
#if defined(CONFIG_SPIRAM) && defined(CONFIG_FREERTOS_TASK_CREATE_ALLOW_EXT_MEM)
  return xTaskCreatePinnedToCoreWithCaps(fn, name, bytes, arg, prio, h, core, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) == pdPASS;
#else
  return xTaskCreatePinnedToCore(fn, name, bytes, arg, prio, h, core) == pdPASS;
#endif
}
// a spawn()ed worker ends itself (notes its stack first)
inline void end(Id id) {
  note(id);
#if defined(CONFIG_SPIRAM) && defined(CONFIG_FREERTOS_TASK_CREATE_ALLOW_EXT_MEM)
  vTaskDeleteWithCaps(nullptr);
#else
  vTaskDelete(nullptr);
#endif
}
}  // namespace fw::stacks
