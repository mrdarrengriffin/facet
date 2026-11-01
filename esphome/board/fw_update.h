#pragma once
// HA "Firmware" update entity (ESPHome http_request update, packages/workbench.yaml) with the OTA probation folded in
// (no separate "Firmware on probation" entity). While the running image is not confirmed yet
// (fw::device::probation_left() >= 0, firmware/fw_device.h) the entity reads "installing" with the
// settling countdown as its progress and the summary "Update settling: don't unplug (m:ss)"; a power cut now rolls
// back. When confirmed it checks the manifest again (normal "up to date" / "update available"). Installs only when
// someone presses Install in HA: nothing here or in the YAML installs on its own.
// tick() runs every second (YAML interval). Also does the first manifest check 30 s after boot, because the entity's
// own check at setup runs before Wi-Fi is up and its next poll is hours away.
#include <cstdio>
#include <string>
#include "esp_timer.h"
#include "esphome/components/http_request/update/http_request_update.h"

namespace panel::fwu {
using esphome::http_request::HttpRequestUpdate;

// reach the entity's protected state (no other way to show progress without installing). HttpRequestUpdate is final,
// so: a class derived from its base may name the base's protected members as pointers-to-member (&Peek::state_ has
// type UpdateState UpdateEntity::*), which then apply to any UpdateEntity. Peek is never instantiated.
using esphome::update::UpdateEntity;
struct Peek : UpdateEntity {
  static esphome::update::UpdateState UpdateEntity::*state() { return &Peek::state_; }
  static esphome::update::UpdateInfo UpdateEntity::*info() { return &Peek::update_info_; }
};
inline void settling(UpdateEntity *u, int left, int total) {
  u->*Peek::state() = esphome::update::UPDATE_STATE_INSTALLING;
  auto &i = u->*Peek::info();
  i.has_progress = true;
  i.progress = total > 0 ? 100.0f * (float) (total - left) / (float) total : 0.0f;
#ifdef ESPHOME_PROJECT_VERSION
  i.current_version = ESPHOME_PROJECT_VERSION;
  if (i.latest_version.empty()) i.latest_version = ESPHOME_PROJECT_VERSION;
#endif
  char b[96];
  snprintf(b, sizeof b, "Update settling: don't unplug (%d:%02d). A restart from Home Assistant is fine.", left / 60, left % 60);
  i.summary = b;
  u->publish_state();
}
inline void settled(UpdateEntity *u) {
  u->*Peek::state() = esphome::update::UPDATE_STATE_UNKNOWN;
  auto &i = u->*Peek::info();
  i.has_progress = false;
  i.summary.clear();
  u->publish_state();
}

inline void tick(HttpRequestUpdate *u, int left, int total) {
  static int was = -2;
  static bool checked = false;
  if (left >= 0) {
    if (left != was) settling(u, left, total);
    was = left;
    return;
  }
  if (was >= 0) {  // just confirmed
    was = -1;
    settled(u);
    u->check();
    checked = true;
    return;
  }
  was = -1;
  if (!checked && esp_timer_get_time() > 30000000LL) {
    checked = true;
    u->check();
  }
}

}  // namespace panel::fwu
