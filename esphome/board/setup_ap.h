#pragma once
// Facet PUBLIC build: the setup hotspot (packages/net_public.yaml). SSID "Facet-1A2B" (last 4 hex of the MAC = the end of the device name
// facet-xx1a2b), WPA2 with a per-panel random password: 10 characters from an alphabet without look-alikes
// (no 0/o, 1/l/i), made once from the hardware RNG on the first boot and kept in flash (own preference slot, NOT a
// restored entity: prefs_schema does not touch it; a USB flash with erase makes a new one). The setup screen shows it
// and puts it in the Wi-Fi QR (wifi_qr()). ESPHome's AP password is compile-time, so apply() hands the wifi component
// a runtime AP config; it must run before wifi setup (on_boot priority > 250, preferences are ready by then).
#include <cstring>
#include <string>
#include <bootloader_random.h>
#include <esp_random.h>
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/preferences.h"
#include "esphome/components/wifi/wifi_component.h"

namespace facet::setup_ap {

static constexpr const char *ALPHABET = "abcdefghjkmnpqrstuvwxyz23456789";  // 31 symbols, ~4.95 bits each
static constexpr size_t PW_LEN = 10;                                        // ~49 bits

struct Saved {
  char pw[PW_LEN + 1];
};

inline std::string &ssid_() { static std::string s; return s; }
inline std::string &password_() { static std::string s; return s; }

inline const std::string &ssid() { return ssid_(); }
inline const std::string &password() { return password_(); }

// "WIFI:T:WPA;S:<ssid>;P:<password>;;" (no characters in either that need escaping)
inline std::string wifi_qr() { return "WIFI:T:WPA;S:" + ssid_() + ";P:" + password_() + ";;"; }

inline bool valid_(const Saved &s) {
  if (s.pw[PW_LEN] != 0 || std::strlen(s.pw) != PW_LEN) return false;
  for (size_t i = 0; i < PW_LEN; i++)
    if (!std::strchr(ALPHABET, s.pw[i])) return false;
  return true;
}

inline void apply() {
  uint8_t mac[6];
  esphome::get_mac_address_raw(mac);
  char id[5];
  snprintf(id, sizeof(id), "%02X%02X", mac[4], mac[5]);
  ssid_() = std::string("Facet-") + id;

  auto pref = esphome::global_preferences->make_preference<Saved>(esphome::fnv1_hash("facet_setup_ap_pw_v1"));
  Saved s{};
  if (!pref.load(&s) || !valid_(s)) {
    // Wi-Fi is not up yet, so esp_random() alone is only pseudo-random: switch the RNG entropy source on around it
    bootloader_random_enable();
    std::memset(&s, 0, sizeof(s));
    for (size_t i = 0; i < PW_LEN; i++) {
      // rejection sampling: no modulo bias over 31 symbols
      uint8_t b;
      do { b = esp_random() & 0xFF; } while (b >= 248);
      s.pw[i] = ALPHABET[b % 31];
    }
    bootloader_random_disable();
    pref.save(&s);
    esphome::global_preferences->sync();
  }
  password_() = s.pw;

  esphome::wifi::WiFiAP ap;
  ap.set_ssid(ssid_());
  ap.set_password(password_());
  esphome::wifi::global_wifi_component->set_ap(ap);
  // INFO: the Set up screen shows it (setup_screen.h); the public build logs at WARN, so not in its USB log
  ESP_LOGI("setup_ap", "hotspot %s, password %s", ssid_().c_str(), password_().c_str());
}

}  // namespace facet::setup_ap
