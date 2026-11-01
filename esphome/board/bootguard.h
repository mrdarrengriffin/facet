// OTA safety (v3). The v2 -> v3 OTA "succeeded", then the panel hung on its first boot until a power
// cycle, and the bootloader rolled back to v2. A hang (not a crash) never trips the task watchdog: ESPHome's setup
// keeps feeding it while it waits for Wi-Fi (WiFiComponent::can_proceed), and the C6 (Wi-Fi over esp32_hosted SDIO)
// is what the first boot waits on. So:
//   1. boot guard: an esp_timer (own task, runs even while setup() is stuck) checks every 5 s. While the running image
//      is PENDING_VERIFY (first boot after an OTA) and Wi-Fi is not up after GUARD_PENDING_S, it resets the C6 and
//      restarts: the bootloader then rolls back to the previous image on its own (no power cycle needed). A
//      confirmed image that boots with Wi-Fi configured but stuck gets up to GUARD_RETRIES warm restarts (the
//      counter lives in RTC no-init RAM, cleared once Wi-Fi is up); a credential-less public build never restarts.
//   2. ESPHome marks the image valid after safe_mode boot_is_good_after (set to 3 min in packages/safety.yaml), i.e.
//      only after the guard had its chance.
//   3. the C6 is held in reset just before every restart (on_shutdown + the guard), so the next boot always meets a
//      freshly booting co-processor, like the cold boot after a USB flash (the case that always worked).
//   4. preferences: PREFS_SCHEMA (packages/safety.yaml) is stored; a stored schema from another firmware -> every
//      config entity is reset to its default once, and numbers are clamped (safe restore).
#pragma once
#include "driver/gpio.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_attr.h"

namespace panel {
namespace boot {

static const int GUARD_PENDING_S = 150;   // first boot after OTA: Wi-Fi must be up by then, else roll back
static const int GUARD_STUCK_S = 240;     // confirmed image with Wi-Fi credentials: warm-restart retry after this
static const int GUARD_RETRIES = 2;
#if CONFIG_IDF_TARGET_ESP32P4
static const gpio_num_t C6_RESET = GPIO_NUM_54;  // esp32_hosted reset_pin (C6 EN: high = run, low = reset)
#endif

RTC_NOINIT_ATTR inline uint32_t rtc_magic;
RTC_NOINIT_ATTR inline uint32_t rtc_retries;
static const uint32_t MAGIC = 0x50414e4c;  // "PANL"

struct Guard {
  esp_timer_handle_t t = nullptr;
  bool pending = false;       // running image still PENDING_VERIFY (first boot after an OTA)
  bool done = false;
  std::string why;            // last action, for the diagnostic text sensor
};
inline Guard G;

inline void hold_c6_in_reset() {
#if CONFIG_IDF_TARGET_ESP32P4  // boards without the C6 co-processor (Guition S3): nothing to hold
  // pull the C6 enable line LOW (= reset; esp-hosted's "active_high" means the C6 RUNS while the pin is high: its init
  // pulses high -> low 10 ms -> high), so the C6 restarts cleanly with the next host boot (esp-hosted pulses it again
  // on init: CONFIG_ESP_HOSTED_SLAVE_RESET_ON_EVERY_HOST_BOOTUP)
  gpio_reset_pin(C6_RESET);
  gpio_set_direction(C6_RESET, GPIO_MODE_OUTPUT);
  gpio_set_level(C6_RESET, 0);
#endif
}

inline void restart(const char *why) {
  ESP_LOGE("bootguard", "%s -> restart", why);
  hold_c6_in_reset();
  esp_rom_delay_us(20000);
  esp_restart();
}

inline void check(void *) {
  if (G.done) return;
  auto *w = esphome::wifi::global_wifi_component;
  bool up = w != nullptr && w->is_connected();
  bool has_sta = w != nullptr && w->has_sta();  // Wi-Fi credentials compiled in or saved (public build: none yet)
  uint32_t s = (uint32_t) (esp_timer_get_time() / 1000000);
  if (up) {
    G.done = true;
    rtc_retries = 0;
    G.why = G.pending ? "first boot after OTA: Wi-Fi up after " + std::to_string(s) + " s" : "Wi-Fi up after " + std::to_string(s) + " s";
    esp_timer_stop(G.t);
    return;
  }
  if (G.pending && s >= (uint32_t) GUARD_PENDING_S) restart("first boot after OTA without Wi-Fi: rolling back");
  if (!G.pending && has_sta && s >= (uint32_t) GUARD_STUCK_S && rtc_retries < (uint32_t) GUARD_RETRIES) {
    rtc_retries++;
    restart("boot without Wi-Fi: warm restart retry");
  }
}

// call as early as possible (on_boot priority 1000 = before Wi-Fi setup)
inline void start() {
  if (rtc_magic != MAGIC) { rtc_magic = MAGIC; rtc_retries = 0; }
  esp_ota_img_states_t st;
  const esp_partition_t *run = esp_ota_get_running_partition();
  G.pending = run && esp_ota_get_state_partition(run, &st) == ESP_OK && st == ESP_OTA_IMG_PENDING_VERIFY;
  G.why = G.pending ? "first boot after OTA: guarding" : "guarding";
  esp_timer_create_args_t a = {};
  a.callback = &check;
  a.name = "bootguard";
  esp_timer_create(&a, &G.t);
  esp_timer_start_periodic(G.t, 5 * 1000000ULL);
}

}  // namespace boot
}  // namespace panel
