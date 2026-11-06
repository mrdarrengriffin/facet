// The task behind a stack overflow, for the problem report (firmware/fw_device.h "problem report"): FreeRTOS calls
// vApplicationStackOverflowHook (task switch, canary check) before it aborts, and the abort text with the task's name
// is gone after the restart. The link wraps the hook (workbench.yaml: -Wl,--wrap=vApplicationStackOverflowHook): the
// name goes into RTC memory a restart keeps (and the Guition's deep-sleep hop), then FreeRTOS's own hook runs.
// IRAM, no locks, no calls: it runs inside the scheduler.
#pragma once
#include "esp_attr.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern "C" {
RTC_NOINIT_ATTR char facet_overflow_task[20];
RTC_NOINIT_ATTR uint32_t facet_overflow_magic;
void __real_vApplicationStackOverflowHook(TaskHandle_t task, char *name);
void IRAM_ATTR __wrap_vApplicationStackOverflowHook(TaskHandle_t task, char *name) {
  int i = 0;
  for (; name && name[i] && i < (int) sizeof facet_overflow_task - 1; i++) facet_overflow_task[i] = name[i];
  facet_overflow_task[i] = 0;
  facet_overflow_magic = 0x534F5646;  // "SOVF"
  __real_vApplicationStackOverflowHook(task, name);
}
}
