// Update trial (Guition, board/s3_reset.h; #118): the decisions, free of ESP-IDF so the host test (fw_trial.test.cpp)
// can run them. The deep-sleep hop of s3_reset.h is a reboot, and the bootloader rolls back an image that boots twice
// while PENDING_VERIFY, so a new image can't stay pending across it. It is marked valid for the bootloader on its first
// boot and put on trial here instead, with the app doing the rollback:
//   - a record (NVS) holds the trial image's partition and a boot count. Every boot of it counts except the
//     continuation of our own hop, so a crash or watchdog loop can't avoid counting;
//   - a counted boot past MAX_BOOTS without a healthy run = boot the previous image;
//   - healthy = up HEALTHY_S with Wi-Fi and Home Assistant's API connected, or Wi-Fi up WIFI_ONLY_S (a panel
//     without Home Assistant); only that ends the trial. No Wi-Fi by RETRY_S with Wi-Fi configured = restart (counted).
// A hang before the boot hook runs (still PENDING_VERIFY) is the bootloader's rollback, as before.
#pragma once
#include <cstdint>

namespace fw::trial {

constexpr int MAX_BOOTS = 3, HEALTHY_S = 90, WIFI_ONLY_S = 600, RETRY_S = 150;

struct Rec {
  uint32_t slot = 0;   // partition address of the image on trial (0 = no trial)
  uint32_t boots = 0;  // counted boots of it
};
enum class Boot { NONE, TRIAL, ROLLBACK };

// one boot. running = this image's partition address; pending = the bootloader still has it PENDING_VERIFY;
// hop = this boot continues our own deep-sleep hop. mark_valid = tell the bootloader it is valid now.
inline Boot boot(Rec &r, uint32_t running, bool pending, bool hop, bool &mark_valid) {
  mark_valid = false;
  if (pending) {  // first boot of a new image: the bootloader's part is done, ours starts
    mark_valid = true;
    r.slot = running, r.boots = 0;
  }
  if (r.slot != running) {  // no trial, or a stale one (the other image's)
    r = Rec{};
    return Boot::NONE;
  }
  if (!hop) r.boots++;
  if (r.boots > (uint32_t) MAX_BOOTS) {
    r = Rec{};
    return Boot::ROLLBACK;
  }
  return Boot::TRIAL;
}

// every few seconds while on trial: true = healthy, the trial ends (the record is cleared)
inline bool healthy(int64_t up_s, bool wifi, int64_t wifi_for_s, bool api) {
  return wifi && ((up_s >= HEALTHY_S && api) || wifi_for_s >= WIFI_ONLY_S);
}
// true = restart now (no Wi-Fi this long into a trial, with Wi-Fi configured)
inline bool retry(int64_t up_s, bool wifi, bool has_sta) { return !wifi && has_sta && up_s >= RETRY_S; }

}  // namespace fw::trial
