#pragma once
// HA "Firmware" update entity (ESPHome http_request update, packages/workbench.yaml) with the OTA probation folded in
// (no separate "Firmware on probation" entity). While the running image is not confirmed yet
// (fw::device::probation_left() >= 0, firmware/fw_device.h) the entity reads "installing" with the
// settling countdown as its progress and the summary "Update settling: don't unplug (m:ss)"; a power cut now rolls
// back. When confirmed it checks the manifest again (normal "up to date" / "update available"). Installs only when
// someone presses Install in HA: nothing here or in the YAML installs on its own.
// tick() runs every second (YAML interval). Also does the first manifest check 30 s after boot, because the entity's
// own check at setup runs before Wi-Fi is up and its next poll is hours away.
// The installed version is the build's (fw::bundle::VERSION, firmware/bundle.mjs --version, in its own small .cpp), not
// ESPHOME_PROJECT_VERSION: that one sits in defines.h, which ~135 ESPHome objects include, so a version stamped per
// build recompiled all of them. The project version stays the release's (facet_version in the YAML); the entity
// reports the build's, fixed in its state callback, which runs before Home Assistant is told. No --version = the
// project version.
#include <cstdio>
#include <string>
#include "esp_timer.h"
#include "esphome/components/http_request/update/http_request_update.h"

namespace fw::bundle {
extern const char *const VERSION;  // facet_wb fw_bundle_info.cpp
}

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
inline const char *version() {
  if (fw::bundle::VERSION[0]) return fw::bundle::VERSION;
#ifdef ESPHOME_PROJECT_VERSION
  return ESPHOME_PROJECT_VERSION;
#else
  return "";
#endif
}
// the entity's check compares the manifest with ESPHOME_PROJECT_VERSION: compare with the build's version instead
inline void fix(UpdateEntity *u) {
  auto &i = u->*Peek::info();
  if (!version()[0] || i.current_version == version()) return;
  i.current_version = version();
  auto &st = u->*Peek::state();
  if (st == esphome::update::UPDATE_STATE_NO_UPDATE || st == esphome::update::UPDATE_STATE_AVAILABLE)
    st = i.latest_version.empty() || i.latest_version == version() ? esphome::update::UPDATE_STATE_NO_UPDATE
                                                                    : esphome::update::UPDATE_STATE_AVAILABLE;
}
inline void watch(UpdateEntity *u) { u->add_on_state_callback([u]() { fix(u); }); }
inline void settling(UpdateEntity *u, int left, int total) {
  u->*Peek::state() = esphome::update::UPDATE_STATE_INSTALLING;
  auto &i = u->*Peek::info();
  i.has_progress = true;
  i.progress = total > 0 ? 100.0f * (float) (total - left) / (float) total : 0.0f;
  i.current_version = version();
  if (i.latest_version.empty()) i.latest_version = version();
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
