#pragma once
// Guition 4848S040 (ESP32-S3 + octal PSRAM + RGB panel): the panel does not come back after a software reset (black
// screen after OTA), so software/USB resets and OTA end in a 100 ms deep sleep = full hardware reset.
// A new OTA image is marked valid for the bootloader first (its rollback would undo it after the sleep) and goes on
// an update trial instead (fw_trial.h: the app rolls back if it is not healthy within 3 boots).
// Panics and watchdog resets get the same hardware reset (one crash left the panel dead, the next boots failed
// too and ESPHome went into safe mode); the original reason is kept in RTC memory for the "Last reset" sensor.
#include <esp_attr.h>
#include <esp_ota_ops.h>
#include <esp_sleep.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <nvs.h>
#include "esphome/core/log.h"
#include "esphome/components/wifi/wifi_component.h"
#ifdef USE_API
#include "esphome/components/api/api_server.h"
#endif
#include <string>
#include "fw_trial.h"

namespace panel::s3 {

RTC_NOINIT_ATTR inline uint32_t rtc_reason;
RTC_NOINIT_ATTR inline uint32_t rtc_magic;
RTC_NOINIT_ATTR inline uint32_t rtc_counted;  // MAGIC: the hop stands for a restart, so the next boot counts (trial)
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

// ---------------------------------------------------------------- update trial (#118): fw_trial.h decides, this applies
namespace trial {
inline nvs_handle_t open_nvs() {
  nvs_handle_t h = 0;
  return nvs_open("fw_trial", NVS_READWRITE, &h) == ESP_OK ? h : 0;
}
inline void save(const fw::trial::Rec &r) {
  nvs_handle_t h = open_nvs();
  if (!h) return;
  if (r.slot) nvs_set_u32(h, "slot", r.slot), nvs_set_u32(h, "boots", r.boots);
  else nvs_erase_all(h);
  nvs_commit(h);
  nvs_close(h);
}
inline esp_timer_handle_t timer = nullptr;
inline int64_t wifi_since = -1;
inline void check(void *) {
  const int64_t s = esp_timer_get_time() / 1000000;
  auto *w = esphome::wifi::global_wifi_component;
  const bool up = w && w->is_connected();
  if (!up) wifi_since = -1;
  else if (wifi_since < 0) wifi_since = s;
  bool api = false;
#ifdef USE_API
  api = esphome::api::global_api_server && esphome::api::global_api_server->is_connected();
#endif
  if (fw::trial::healthy(s, up, wifi_since < 0 ? 0 : s - wifi_since, api)) {
    save(fw::trial::Rec{});
    ESP_LOGW("fw_trial", "update trial over: healthy after %d s (%s)", (int) s, api ? "Wi-Fi + Home Assistant" : "Wi-Fi");
    esp_timer_stop(timer);
  } else if (fw::trial::retry(s, up, w && w->has_sta())) {
    ESP_LOGE("fw_trial", "no Wi-Fi %d s into an update trial: restart", (int) s);
    esp_restart();  // counted on the next boot
  }
}
// on_boot priority 800, every boot; hop = this boot continues our own deep-sleep hop
inline void on_boot(bool hop) {
  const esp_partition_t *run = esp_ota_get_running_partition();
  nvs_handle_t h = open_nvs();
  if (!run || !h) return;
  fw::trial::Rec r;
  nvs_get_u32(h, "slot", &r.slot);
  nvs_get_u32(h, "boots", &r.boots);
  nvs_close(h);
  esp_ota_img_states_t st;
  const bool pending = esp_ota_get_state_partition(run, &st) == ESP_OK && st == ESP_OTA_IMG_PENDING_VERIFY;
  const uint32_t had = r.slot;
  bool mark = false;
  const fw::trial::Boot b = fw::trial::boot(r, run->address, pending, hop, mark);
  if (mark) esp_ota_mark_app_valid_cancel_rollback();
  if (b != fw::trial::Boot::NONE || had) save(r);
  if (b == fw::trial::Boot::ROLLBACK) {
    const esp_partition_t *prev = esp_ota_get_next_update_partition(nullptr);  // the other slot = the image before
    ESP_LOGE("fw_trial", "update not healthy after %d boots: back to the previous image", fw::trial::MAX_BOOTS);
    if (prev && esp_ota_set_boot_partition(prev) == ESP_OK) esp_restart();
    return;
  }
  if (b == fw::trial::Boot::TRIAL && !timer) {
    ESP_LOGW("fw_trial", "update trial: boot %u of %d", (unsigned) r.boots, fw::trial::MAX_BOOTS);
    esp_timer_create_args_t a{};
    a.callback = check;
    a.name = "fw_trial";
    if (esp_timer_create(&a, &timer) == ESP_OK) esp_timer_start_periodic(timer, 5 * 1000000);
  }
}
}  // namespace trial

// A software restart (the Restart button, an update trial's retry, any esp_restart) goes straight to the hop instead
// of through the chip's restart (#121): esp_restart arms the RTC watchdog for 1 s and resets the digital system, and
// on this board that reset hung about one restart in ten (the problem report: no panic, the last log line seconds
// before, next boot "watchdog (RTC)"). esp_restart's shutdown handlers run first (registered after this one: Wi-Fi's
// among them), then this one sleeps 100 ms = the same full hardware reset every software reset ends in anyway.
inline void restart_as_hop() {
  rtc_counted = MAGIC;
  hw_reset_as(ESP_RST_SW);
}

// on_boot priority 800
inline void reset_if_soft() {
  auto reason = esp_reset_reason();
  if (reason != ESP_RST_DEEPSLEEP) rtc_magic = 0;
  // a hop that stands for a restart counts as a boot of an update trial; our own boot-time hop doesn't
  const bool ours = reason == ESP_RST_DEEPSLEEP && rtc_magic == MAGIC, counted = ours && rtc_counted == MAGIC;
  rtc_counted = 0;
  trial::on_boot(ours && !counted);
  esp_register_shutdown_handler(restart_as_hop);
  if (reason == ESP_RST_USB || reason == ESP_RST_SW || reason == ESP_RST_PANIC || reason == ESP_RST_INT_WDT ||
      reason == ESP_RST_TASK_WDT || reason == ESP_RST_WDT) {
    rtc_reason = reason;
    rtc_magic = MAGIC;
    hw_reset();
  }
}

}  // namespace panel::s3

// for the problem report (firmware/fw_device.h report::begin, on_boot 850, before reset_if_soft): a deep-sleep boot that
// is our hop, with the reason it stands for, and whether it stands for a restart (else it continues the boot before)
extern "C" bool facet_hop(int *reason, bool *restart) {
  if (esp_reset_reason() != ESP_RST_DEEPSLEEP || panel::s3::rtc_magic != panel::s3::MAGIC) return false;
  *reason = (int) panel::s3::rtc_reason, *restart = panel::s3::rtc_counted == panel::s3::MAGIC;
  return true;
}
