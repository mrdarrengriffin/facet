// Host test for fw_trial.h (renderer/check.sh step fw-trial): c++ -std=c++17 fw_trial.test.cpp && ./a.out
// Each case is a run of boots as the Guition lives them: the OTA restart, the deep-sleep hop, crashes, power cycles.
#include "fw_trial.h"

#include <cstdio>

using namespace fw::trial;
static int fails = 0;
#define EXPECT(c)                                                \
  do {                                                           \
    if (!(c)) fails++, printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); \
  } while (0)

static constexpr uint32_t A = 0x10000, B = 0x810000;  // the two OTA slots

int main() {
  bool mark = false;
  {  // install B, boot, hop, healthy: the trial ends after the health check, never before
    Rec r;
    EXPECT(boot(r, B, true, false, mark) == Boot::TRIAL && mark && r.slot == B && r.boots == 1);  // the old image's restart
    EXPECT(boot(r, B, false, true, mark) == Boot::TRIAL && !mark && r.boots == 1);  // our hop: not counted
    EXPECT(!healthy(30, true, 30, true));    // too early
    EXPECT(!healthy(120, true, 100, false)); // no Home Assistant yet, Wi-Fi not long enough
    EXPECT(healthy(90, true, 60, true));
  }
  {  // the reason of the first boot doesn't matter (SW, RTC watchdog, panic of the old image): same start
    Rec r;
    EXPECT(boot(r, B, true, false, mark) == Boot::TRIAL && mark);
  }
  {  // a crash loop (each crash + its hop): back to A on the 4th counted boot, hops never count
    Rec r;
    EXPECT(boot(r, B, true, false, mark) == Boot::TRIAL);
    EXPECT(boot(r, B, false, true, mark) == Boot::TRIAL);   // hop
    EXPECT(boot(r, B, false, false, mark) == Boot::TRIAL);  // crash 1
    EXPECT(boot(r, B, false, true, mark) == Boot::TRIAL);
    EXPECT(boot(r, B, false, false, mark) == Boot::TRIAL && r.boots == 3);  // crash 2
    EXPECT(boot(r, B, false, true, mark) == Boot::TRIAL);
    EXPECT(boot(r, B, false, false, mark) == Boot::ROLLBACK && r.slot == 0);  // crash 3: rollback, record cleared
    EXPECT(boot(r, A, false, false, mark) == Boot::NONE && !mark);  // A runs with no trial
  }
  {  // hops alone can't run the count up, nor end the trial
    Rec r;
    boot(r, B, true, false, mark);
    for (int i = 0; i < 20; i++) EXPECT(boot(r, B, false, true, mark) == Boot::TRIAL);
    EXPECT(r.boots == 1 && r.slot == B);
  }
  {  // power cycles during a trial count like crashes
    Rec r;
    boot(r, B, true, false, mark);
    boot(r, B, false, false, mark);
    boot(r, B, false, false, mark);
    EXPECT(boot(r, B, false, false, mark) == Boot::ROLLBACK);
  }
  {  // a stale record (the other image's) is dropped; no record = no trial
    Rec r{A, 2};
    EXPECT(boot(r, B, false, false, mark) == Boot::NONE && r.slot == 0);
    Rec n;
    EXPECT(boot(n, A, false, false, mark) == Boot::NONE && !mark);
  }
  {  // a panel without Home Assistant: Wi-Fi up 10 min is healthy; no Wi-Fi: retry only with Wi-Fi configured
    EXPECT(healthy(700, true, 600, false));
    EXPECT(!healthy(700, false, 0, false));
    EXPECT(retry(150, false, true));
    EXPECT(!retry(150, false, false));  // a public build before onboarding never restarts
    EXPECT(!retry(149, false, true));
    EXPECT(!retry(400, true, true));
  }
  printf("fw_trial: %s\n", fails ? "FAILED" : "ok");
  return fails ? 1 : 0;
}
