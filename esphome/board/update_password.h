#pragma once
// Facet PUBLIC build: the per-panel update password (#149; the decisions in ota_pw.h). apply() runs before the OTA
// component takes a connection (on_boot priority 600, packages/net_public.yaml): it reads the panel's password from
// flash (a new one on the first boot) and gives it to ESPHome's espota, so a manual flash needs it. Home Assistant's
// own installs (the Firmware entity, http_request from the manifest) and USB are untouched. No log call here, ever:
// the password must not reach the log, the USB log or the problem report (/fw/report keeps logs).
#include <string>
#include <bootloader_random.h>
#include <esp_random.h>
#include "esphome/core/helpers.h"
#include "esphome/core/preferences.h"
#include "esphome/components/esphome/ota/ota_esphome.h"
#include "ota_pw.h"

namespace facet::update_password {

inline std::string &pw_() { static std::string s; return s; }
inline const std::string &get() { return pw_(); }

inline void apply(esphome::ESPHomeOTAComponent *ota) {
  auto pref = esphome::global_preferences->make_preference<ota_pw::Saved>(esphome::fnv1_hash("facet_ota_pw_v1"));
  pw_() = ota_pw::ensure(
      [&](ota_pw::Saved &s) { return pref.load(&s); },
      [&](const ota_pw::Saved &s) {
        pref.save(&s);
        esphome::global_preferences->sync();
      },
      [] {
        // Wi-Fi is not up yet, so esp_random() alone is only pseudo-random: the RNG entropy source on around it
        bootloader_random_enable();
        const uint32_t r = esp_random();
        bootloader_random_disable();
        return r;
      });
  if (ota) ota->set_auth_password(pw_());
}

}  // namespace facet::update_password
