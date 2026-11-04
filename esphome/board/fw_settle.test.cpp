// Host test for fw_settle.h (renderer/check.sh step fw-settle): c++ -std=c++17 fw_settle.test.cpp && ./a.out
// Each case is one restart as the panel lives it, one step a second (fw_update.h tick).
#include "fw_settle.h"

#include <cstdio>
#include <string>

using namespace fw::settle;
static int fails = 0;
#define EXPECT(c)                                                \
  do {                                                           \
    if (!(c)) fails++, printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); \
  } while (0)

static constexpr int64_t S = 1000000;

int main() {
  {  // after an install: Installing with the countdown, then up to date the second it is confirmed, and a check
    Clock c;
    for (int t = 1; t <= 180; t++) EXPECT(c.step(180 - t, t * S) & SETTLING);
    const unsigned a = c.step(-1, 181 * S);
    EXPECT(a & UP_TO_DATE);
    EXPECT(a & CHECK);
    EXPECT(!(a & SETTLING));
    EXPECT(c.step(-1, 182 * S) == NONE);  // once
  }
  {  // a boot with no probation (a restart, or the image confirmed on the way down): up to date at once, not blank
    Clock c;
    EXPECT(c.step(-1, 1 * S) & UP_TO_DATE);
    for (int t = 2; t < 30; t++) EXPECT(c.step(-1, t * S) == NONE);
    EXPECT(c.step(-1, 30 * S) & CHECK);  // the first check, once Wi-Fi is up
  }
  {  // a check that did not land (one already running, no network, a failed fetch): asked again, not in an hour
    Clock c;
    c.step(-1, 1 * S);
    EXPECT(c.step(-1, 30 * S) & CHECK);
    int again = 0;
    for (int t = 31; t <= 300; t++) if (c.step(-1, t * S) & CHECK) again++;
    EXPECT(again == 4);  // 90, 150, 210, 270 s
    c.landed();
    for (int t = 301; t <= 600; t++) EXPECT(c.step(-1, t * S) == NONE);
  }
  {  // a check that landed is never overwritten with this build's own "up to date" (an update on offer stays offered)
    Clock c;
    c.landed();
    EXPECT(!(c.step(-1, 1 * S) & UP_TO_DATE));
    EXPECT(!(c.step(-1, 40 * S) & CHECK));
  }
  {  // the countdown at 0:00 and the image still not confirmed: confirmed here after GRACE, never Installing for good
    Clock c;
    for (int t = 1; t <= 180; t++) c.step(180 - t, t * S);
    bool confirm = false;
    int t = 181;
    for (; t <= 400 && !confirm; t++) confirm = c.step(0, t * S) & CONFIRM;
    EXPECT(confirm);
    EXPECT(t - 1 == 180 + 60);  // due at 180 s, confirmed 60 s later
    const unsigned a = c.step(-1, t * S);  // the next read: confirmed
    EXPECT((a & UP_TO_DATE) && (a & CHECK));
  }
  {  // a check that lands during the countdown (the entity's own at setup) is overwritten by it: checked again after
    Clock c;
    c.step(150, 30 * S);
    c.landed();
    c.step(149, 31 * S);
    for (int t = 32; t <= 180; t++) c.step(180 - t, t * S);
    const unsigned a = c.step(-1, 181 * S);
    EXPECT((a & UP_TO_DATE) && (a & CHECK));
  }
  {  // a stalled countdown (the same second again) is not a new Installing publish every step
    Clock c;
    EXPECT(c.step(100, 80 * S) & SETTLING);
    EXPECT(!(c.step(100, 81 * S) & SETTLING));
  }
  {  // the Update channel: each channel's manifest beside ota.json
    const std::string b = "https://example.org/firmware/guition-4848s040/ota.json";
    EXPECT(channel_url(b, "stable") == b);
    EXPECT(channel_url(b, "beta") == "https://example.org/firmware/guition-4848s040/ota-beta.json");
    EXPECT(channel_url(b, "dev") == "https://example.org/firmware/guition-4848s040/ota-dev.json");
    EXPECT(channel_url(b, nullptr) == b);
    EXPECT(channel_url("https://example.org/manifest.json", "dev") == "https://example.org/manifest.json");
  }
  {  // a channel change checks nothing itself; the next step saves the settings, then checks the last one, once
    Channel ch;
    std::string log;
    auto save = [&] { log += "save;"; };
    auto check = [&](const std::string &u) { log += "check " + u + ";"; };
    EXPECT(!ch.step(save, check));
    ch.want("ota-beta.json");
    ch.want("ota-dev.json");
    EXPECT(log.empty());
    EXPECT(ch.step(save, check));
    EXPECT(log == "save;check ota-dev.json;");
    EXPECT(!ch.step(save, check));
    EXPECT(log == "save;check ota-dev.json;");
  }
  if (fails) printf("fw_settle: %d failed\n", fails);
  else printf("fw_settle: ok\n");
  return fails ? 1 : 0;
}
