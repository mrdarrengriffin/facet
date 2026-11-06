// host test for fw_report.h: the kept memory across "restarts", the log ring's wrap, connection drops and redaction.
//   c++ -std=c++17 -Wall -Wextra -Werror firmware/fw_report.test.cpp && ./a.out
#include "fw_report.h"
#include <cstdio>
#include <vector>

static int fails = 0;
#define EXPECT(c, ...) do { if (!(c)) { fails++; printf("FAIL %s:%d ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

using K = fw::report::Kept<256>;
static std::vector<std::string> lines(const K &k) {
  std::vector<std::string> v;
  k.lines([&](const std::string &l) { v.push_back(l); });
  return v;
}
static void put(K &k, const std::string &s) { k.put(s.data(), s.size()); }

static void test_restart() {
  static K k;  // static = zeroed, like a power-on RTC that happens to be zero
  memset((void *) &k, 0xA5, sizeof k);  // power on: whatever was in the memory
  EXPECT(!k.begin(), "garbage taken for kept memory");
  EXPECT(lines(k).empty(), "garbage lines after power on");
  k.new_boot(1);
  put(k, "--- boot 1 ---");
  put(k, "before the restart");
  // a restart: the memory is kept, begin() keeps it
  EXPECT(k.begin(), "kept memory not kept after a restart");
  k.new_boot(3);
  put(k, "after");
  auto v = lines(k);
  EXPECT(v.size() == 3 && v[1] == "before the restart" && v[2] == "after", "lines across a restart: %zu", v.size());
  EXPECT(k.boot == 2 && k.this_boot()->reason == 3, "boot count %u", k.boot);
  int nb = 0;
  k.each_boot([&](const fw::report::Boot &b) { nb++, (void) b; });
  EXPECT(nb == 2, "boots %d", nb);
  // a corrupt head (a crash mid-write could leave anything): start empty, no out-of-range read
  k.log_head = 999;
  EXPECT(!k.begin() && lines(k).empty(), "corrupt ring not reset");
}

static void test_wrap() {
  static K k;
  k.begin();
  for (int i = 0; i < 100; i++) put(k, "line " + std::to_string(i));
  auto v = lines(k);
  EXPECT(!v.empty() && v.back() == "line 99", "last line %s", v.empty() ? "-" : v.back().c_str());
  for (auto &l : v) EXPECT(l.rfind("line ", 0) == 0, "cut line kept: '%s'", l.c_str());
  for (size_t i = 1; i < v.size(); i++) EXPECT(atoi(v[i].c_str() + 5) == atoi(v[i - 1].c_str() + 5) + 1, "order");
  // a line longer than a quarter of the ring keeps its end
  put(k, std::string(1000, 'x') + "END");
  v = lines(k);
  EXPECT(v.back().size() == 64 && v.back().substr(61) == "END", "long line %zu", v.back().size());
  // newlines inside a message don't split it
  put(k, "a\nb");
  v = lines(k);
  EXPECT(v.back() == "a b", "newline in a message");
}

static void test_conns() {
  static K k;
  k.begin();
  k.new_boot(1);
  k.drop(fw::report::WIFI, 100, 0, 8, -71);
  k.drop(fw::report::API, 101, 0, 0, 0);
  k.back(fw::report::WIFI, 130);
  int n = 0;
  k.each_conn([&](const fw::report::Conn &c) {
    n++;
    if (c.link == fw::report::WIFI) EXPECT(!c.open && c.down_s == 30 && c.reason == 8 && c.rssi == -71, "wifi drop");
    if (c.link == fw::report::API) EXPECT(c.open, "api drop closed");
  });
  EXPECT(n == 2, "conns %d", n);
  // kept across a restart; a reconnect on the next boot doesn't close the old boot's drop
  k.begin();
  k.new_boot(4);
  k.back(fw::report::API, 20);
  k.each_conn([&](const fw::report::Conn &c) { if (c.link == fw::report::API) EXPECT(c.open, "old boot's drop closed"); });
  for (int i = 0; i < 100; i++) k.drop(fw::report::HA, 200 + i, 0, 0, 0);
  n = 0;
  k.each_conn([&](const fw::report::Conn &) { n++; });
  EXPECT(n == fw::report::CONNS, "conn ring %d", n);
  EXPECT(fw::report::conn_line(fw::report::Conn{1, 5, 0, 0, fw::report::WIFI, 201, -80, 1}, false) ==
             "Wi-Fi dropped (reason 201, signal -80 dBm)", "conn line");
}

static void test_redact() {
  using fw::report::redact;
  const char *secret = "Zk3mQ9pL2xV8rT5wY1nB6cH4jF7gD0sA";  // a made-up 32-char key
  const std::vector<std::string> leaks = {
      std::string("api_key: ") + secret,
      std::string("{\"token\":\"") + secret + "\"}",
      std::string("Authorization: Bearer ") + secret,
      "wifi password=hunter2 ssid=Home",
      "psk: 'abc def'",
      std::string("got ") + secret + " from the panel",
      std::string("noise_psk=") + secret,
      "secret: short1",
      std::string("Bearer eyJhbGciOiJIUzI1NiJ9.eyJpc3MiOiIxMjM0In0.") + secret,  // a JWT (HA's long-lived tokens)
  };
  for (auto &l : leaks) {
    const std::string r = redact(l);
    EXPECT(r.find(secret) == std::string::npos && r.find("hunter2") == std::string::npos &&
               r.find("abc") == std::string::npos && r.find("short1") == std::string::npos,
           "leak: '%s' -> '%s'", l.c_str(), r.c_str());
    EXPECT(r.find("[redacted]") != std::string::npos, "nothing redacted: '%s'", r.c_str());
  }
  // ordinary lines stay as they are
  for (const char *ok : {"keys: 2", "[W][wifi:123]: Disconnected reason 8, signal -71", "build 0.5.0-beta.7+abc1234",
                         "free 112511 B, largest 47104", "tokens_ok yes", "/fw/report",
                         "firmware 0.5.0-beta6-dev202611061147+9d9ef80, board guition-4848s040"})
    EXPECT(redact(ok) == ok, "changed: '%s' -> '%s'", ok, redact(ok).c_str());
  EXPECT(redact(fw::report::HOWTO) == fw::report::HOWTO, "the how-to lines changed: '%s'", redact(fw::report::HOWTO).c_str());
  EXPECT(strstr(fw::report::HOWTO, "template=bug.yml") && strstr(fw::report::HOWTO, "Report box"), "how-to: the form + the box");
  const char *col = "\x1b[1;31mE x\x1b[0m\r";
  EXPECT(fw::report::plain(col, strlen(col)) == "E x", "plain");
}

int main() {
  test_restart();
  test_wrap();
  test_conns();
  test_redact();
  if (fails) { printf("%d failed\n", fails); return 1; }
  printf("fw_report: ok\n");
  return 0;
}
