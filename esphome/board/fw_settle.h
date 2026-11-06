#pragma once
// What the Firmware update entity (fw_update.h) shows after a restart, one step a second. Plain C++, no ESPHome:
// tested on the host (fw_settle.test.cpp, renderer/check.sh step fw-settle).
//   left = seconds until the running image is confirmed (fw::device::probation_left(): >= 0 on probation, 0 = due;
//   -1 = confirmed, or not an image under verify). step() says what to do now (bits of Act):
//   SETTLING    on probation and the countdown moved: Installing, the countdown as progress
//   UP_TO_DATE  the image is the one running for good (just confirmed, or a boot with no probation) and no manifest
//               check has landed yet: up to date with this build's version, at once, so Home Assistant never waits on
//               a manifest fetch (or shows a blank entity) after the restart
//   CHECK       ask the manifest: at once when confirmed, 30 s after a boot (the entity's own check at setup runs
//               before Wi-Fi), then again every RETRY until one lands (landed()): a check that could not start (one
//               already running, no network) or failed never left the entity waiting for the hourly poll
//   CONFIRM     still unconfirmed GRACE after it was due: safe_mode did not confirm it (it confirms at
//               boot_is_good_after, = the probation), so the caller confirms it itself; the countdown never sits at
//               0:00 with the entity on Installing for good
#include <cstdint>
#include <cstring>
#include <string>

namespace fw::settle {

enum Act : unsigned { NONE = 0, SETTLING = 1, UP_TO_DATE = 2, CHECK = 4, CONFIRM = 8 };
constexpr int64_t FIRST_CHECK = 30000000;  // us after boot
constexpr int64_t RETRY = 60000000;        // us between checks until one lands
constexpr int64_t GRACE = 60000000;        // us past due before the image is confirmed here

struct Clock {
  int was = -2;           // left at the last step; -2 = no step yet
  bool known = false;     // a manifest check has landed (the entity holds the manifest's state)
  int64_t next = FIRST_CHECK;  // when the next check is due (us since boot)
  int64_t due_at = -1;    // when the countdown reached 0 (us), -1 = not yet
  void landed() { known = true; }
  unsigned step(int left, int64_t now) {
    unsigned a = NONE;
    if (left >= 0) {
      known = false;  // the entity shows the countdown now, not a check's result: check again once confirmed
      if (left != was) a |= SETTLING;
      was = left;
      if (left == 0) {
        if (due_at < 0) due_at = now;
        else if (now - due_at >= GRACE) a |= CONFIRM;
      }
      return a;
    }
    if (was != -1) {  // just confirmed, or the first step of a boot with no probation
      if (!known) a |= UP_TO_DATE;
      if (was >= 0) next = now;  // confirmed: ask the manifest now
      was = -1;
    }
    if (!known && now >= next) {
      a |= CHECK;
      next = now + RETRY;
    }
    return a;
  }
};

// The Update channel (workbench.yaml channel_hook): the manifest beside the YAML's ota.json for a channel (beta / dev:
// ota-beta.json / ota-dev.json; anything else = the URL as given, stable).
inline std::string channel_url(std::string base, const char *ch) {
  const size_t k = base.rfind("ota.json");
  if (ch && k != std::string::npos && (!strcmp(ch, "beta") || !strcmp(ch, "dev")))
    base.replace(k, 8, std::string("ota-") + ch + ".json");
  return base;
}
// the manifest URL with a throwaway query (?t=<stamp>): GitHub Pages serves ota*.json with max-age 600, so without it
// a check up to 10 min after a release could get the old manifest from the CDN. A ?t= / &t= already there is replaced
// (it is the last part); relative firmware paths still resolve (ESPHome joins them at the last '/').
inline std::string fresh_url(std::string url, uint32_t stamp) {
  size_t q = url.find("?t=");
  if (q == std::string::npos) q = url.find("&t=");
  if (q != std::string::npos) url.erase(q);
  url += url.find('?') == std::string::npos ? "?t=" : "&t=";
  return url + std::to_string(stamp);
}
// A channel change only records the manifest (want()): the hook runs inside the select's state publish, before the
// select has stored its value, and ESPHome writes stored values to flash once a minute, so a restart soon after a
// check started inside the change (a crash in the fetch, a power dip) came back on the old channel. The next tick
// (step()) writes the stored settings to flash (save()), then points the entity at the manifest and checks it
// (check(url)). Several changes before a tick = one check, of the last one. While a check runs (busy: the entity's
// task reads the URL it was started with, by reference, for the whole fetch) the change waits: setting the URL then
// would swap the string under that task.
struct Channel {
  std::string url;
  bool pending = false;
  void want(const std::string &u) { url = u, pending = true; }
  template <class Save, class Check> bool step(bool busy, Save save, Check check) {
    if (!pending || busy) return false;
    pending = false;
    save();
    check(url);
    return true;
  }
};

}  // namespace fw::settle
