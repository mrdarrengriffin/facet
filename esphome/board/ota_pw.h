// Facet PUBLIC build: the per-panel update password (#149), the decisions free of ESP-IDF so the host test
// (ota_pw.test.cpp) can run them; update_password.h is the ESPHome glue. ESPHome's espota (port 3232) takes any image
// from anyone on the network without a password, so each panel makes its own on the first boot: 16 characters from
// an alphabet without look-alikes (no 0/o, 1/l/i), ~79 bits, from the hardware RNG, kept in flash (own preference
// slot, NOT a restored entity: prefs_schema does not touch it; a USB flash with erase makes a new one). It is never
// logged: it shows only in Settings (hold to show) and through Home Assistant's facet_update_password action.
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

namespace facet::ota_pw {

static constexpr const char *ALPHABET = "abcdefghjkmnpqrstuvwxyz23456789";  // 31 symbols, ~4.95 bits each
static constexpr size_t LEN = 16;

struct Saved {
  char pw[LEN + 1];
};

inline bool valid(const Saved &s) {
  if (s.pw[LEN] != 0 || std::strlen(s.pw) != LEN) return false;
  for (size_t i = 0; i < LEN; i++)
    if (!std::strchr(ALPHABET, s.pw[i])) return false;
  return true;
}

// a new password; rand() = a random 32-bit word. Rejection sampling: no modulo bias over 31 symbols.
template<class Rand> Saved make(Rand rand) {
  Saved s{};
  for (size_t i = 0; i < LEN; i++) {
    uint8_t b;
    do { b = rand() & 0xFF; } while (b >= 248);
    s.pw[i] = ALPHABET[b % 31];
  }
  return s;
}

// the panel's password: the saved one when it is sound, else a new one, saved once. load(Saved &) -> bool,
// save(const Saved &), rand() -> uint32_t.
template<class Load, class Save, class Rand> std::string ensure(Load load, Save save, Rand rand) {
  Saved s{};
  if (!load(s) || !valid(s)) {
    s = make(rand);
    save(s);
  }
  return s.pw;
}

}  // namespace facet::ota_pw
