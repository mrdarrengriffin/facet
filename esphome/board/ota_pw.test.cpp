// Host test for ota_pw.h (renderer/check.sh step ota-pw): c++ -std=c++17 ota_pw.test.cpp && ./a.out
// The update password: made once, kept, made again only when the saved one is missing or broken; and never logged
// (the glue, update_password.h, holds no log call: checked here on its source).
#include "ota_pw.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

using namespace facet::ota_pw;
static int fails = 0;
#define EXPECT(c)                                                \
  do {                                                           \
    if (!(c)) fails++, printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); \
  } while (0)

struct Flash {  // one preference slot
  bool has = false;
  Saved s{};
  int saves = 0;
};
static std::string boot(Flash &f, uint32_t seed) {
  return ensure([&](Saved &o) { if (f.has) o = f.s; return f.has; },
                [&](const Saved &n) { f.s = n, f.has = true, f.saves++; },
                [&] { seed = seed * 1664525u + 1013904223u; return seed >> 8; });
}

int main() {
  {  // first boot: a new one, 16 characters of the alphabet, saved once; later boots: the same, no save
    Flash f;
    const std::string a = boot(f, 1);
    EXPECT(a.size() == LEN);
    for (char ch : a) EXPECT(std::strchr(ALPHABET, ch));
    EXPECT(f.saves == 1);
    for (int i = 0; i < 5; i++) EXPECT(boot(f, 100 + i) == a);
    EXPECT(f.saves == 1);
  }
  {  // two panels: their own passwords
    Flash f1, f2;
    EXPECT(boot(f1, 1) != boot(f2, 2));
  }
  {  // a broken slot (wrong length, a look-alike, no terminator) = a new one, saved
    for (int k = 0; k < 3; k++) {
      Flash f;
      f.has = true;
      std::memcpy(f.s.pw, "abcdefghjkmnpqrs", LEN + 1);
      if (k == 0) f.s.pw[5] = 0;
      if (k == 1) f.s.pw[3] = '0';
      if (k == 2) f.s.pw[LEN] = 'x';
      const std::string a = boot(f, 7);
      EXPECT(f.saves == 1);
      EXPECT(valid(f.s));
      EXPECT(a == f.s.pw);
    }
  }
  {  // a sound saved one is kept as it is
    Flash f;
    f.has = true;
    std::memcpy(f.s.pw, "abcdefghjkmnpqrs", LEN + 1);
    EXPECT(boot(f, 9) == "abcdefghjkmnpqrs");
    EXPECT(f.saves == 0);
  }
  {  // no modulo bias: bytes >= 248 are thrown away (a rand that gives 255 first, then 0)
    int n = 0;
    const Saved s = make([&] { return (n++ % 2) ? 0u : 255u; });
    for (size_t i = 0; i < LEN; i++) EXPECT(s.pw[i] == ALPHABET[0]);
  }
  {  // never logged: the glue has no log call at all
    const char *src = std::getenv("OTA_PW_GLUE");
    std::ifstream in(src ? src : "update_password.h");
    std::stringstream b;
    b << in.rdbuf();
    EXPECT(!b.str().empty());
    EXPECT(b.str().find("ESP_LOG") == std::string::npos);
    EXPECT(b.str().find("printf") == std::string::npos);
  }
  if (fails) return printf("ota_pw: %d failed\n", fails), 1;
  printf("ota_pw: ok\n");
  return 0;
}
