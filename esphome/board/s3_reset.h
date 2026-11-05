#pragma once
// Guition 4848S040 (ESP32-S3 + octal PSRAM + RGB panel): the panel does not come back after a software reset (black
// screen after OTA), so software/USB resets and OTA end in a 100 ms deep sleep = full hardware reset.
// A new OTA image is confirmed first, or the bootloader's rollback would undo it after the sleep. Same as ESPControl.
// Panics and watchdog resets get the same hardware reset (one crash left the panel dead, the next boots failed
// too and ESPHome went into safe mode); the original reason is kept in RTC memory for the "Last reset" sensor.
#include <esp_attr.h>
#include <esp_ota_ops.h>
#include <esp_sleep.h>
#include <esp_system.h>
#include <string>

namespace panel::s3 {

RTC_NOINIT_ATTR inline uint32_t rtc_reason;
RTC_NOINIT_ATTR inline uint32_t rtc_magic;
static const uint32_t MAGIC = 0x53335253;  // "S3RS"

inline void hw_reset() {
  esp_sleep_enable_timer_wakeup(100000);
  esp_deep_sleep_start();
}
// hardware reset that the "Last reset" sensor reports as `why` (OTA: software), not as a leftover older reason
inline void hw_reset_as(esp_reset_reason_t why) {
  rtc_reason = why;
  rtc_magic = MAGIC;
  hw_reset();
}

// the reset behind this boot, before the deep-sleep hop
inline std::string last_reason() {
  esp_reset_reason_t r = esp_reset_reason();
  if (r == ESP_RST_DEEPSLEEP && rtc_magic == MAGIC) r = (esp_reset_reason_t) rtc_reason;
  switch (r) {
    case ESP_RST_POWERON: return "power on";
    case ESP_RST_SW: return "software (OTA / restart)";
    case ESP_RST_USB: return "USB";
    case ESP_RST_PANIC: return "crash (panic)";
    case ESP_RST_INT_WDT: return "interrupt watchdog";
    case ESP_RST_TASK_WDT: return "task watchdog";
    case ESP_RST_WDT: return "watchdog";
    case ESP_RST_BROWNOUT: return "brownout";
    case ESP_RST_DEEPSLEEP: return "deep sleep";
    default: return "other (" + std::to_string((int) r) + ")";
  }
}

// on_boot priority 800
inline void reset_if_soft() {
  auto reason = esp_reset_reason();
  if (reason != ESP_RST_DEEPSLEEP) rtc_magic = 0;
  // a new OTA image is confirmed on its first boot after the old image's restart. That restart can end in the RTC
  // watchdog (ESP_RST_WDT: esp_restart arms it for 1 s; f9a5a1e's restart always did), so WDT counts too: on this first
  // boot the reason is always the OLD image's; a new image that fails later is still caught (boot guard, safe mode)
  if (reason == ESP_RST_SW || reason == ESP_RST_WDT) {
    esp_ota_img_states_t st;
    const esp_partition_t *run = esp_ota_get_running_partition();
    if (run && esp_ota_get_state_partition(run, &st) == ESP_OK && st == ESP_OTA_IMG_PENDING_VERIFY)
      esp_ota_mark_app_valid_cancel_rollback();
  }
  if (reason == ESP_RST_USB || reason == ESP_RST_SW || reason == ESP_RST_PANIC || reason == ESP_RST_INT_WDT ||
      reason == ESP_RST_TASK_WDT || reason == ESP_RST_WDT) {
    rtc_reason = reason;
    rtc_magic = MAGIC;
    hw_reset();
  }
}

}  // namespace panel::s3
