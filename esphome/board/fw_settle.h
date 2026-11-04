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

}  // namespace fw::settle
