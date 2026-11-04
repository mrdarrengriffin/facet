// Host test for fw_notes.h (renderer/check.sh step fw-notes): c++ -std=c++17 fw_notes.test.cpp && ./a.out
#include "fw_notes.h"

#include <cstdio>

using namespace fw::notes;
static int fails = 0;
#define EXPECT(c)                                                \
  do {                                                           \
    if (!(c)) fails++, printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); \
  } while (0)

static int c(const char *a, const char *b) { return cmp(parse(a), parse(b)); }
static size_t count(const std::string &s, const char *w) {
  size_t n = 0;
  for (size_t at = s.find(w); at != std::string::npos; at = s.find(w, at + 1)) n++;
  return n;
}

int main() {
  // versions
  EXPECT(c("0.5.0-beta.4", "0.5.0-beta.3") > 0);
  EXPECT(c("0.5.0-beta.10", "0.5.0-beta.9") > 0);  // numeric, not by text
  EXPECT(c("0.5.0", "0.5.0-beta.9") > 0);          // the release after its betas
  EXPECT(c("0.5.0-rc.1", "0.5.0-beta.9") > 0);
  EXPECT(c("0.5.0-beta.1", "0.4.0") > 0);
  EXPECT(c("0.4.0-dev", "0.4.0") < 0);
  EXPECT(c("0.5.0-dev", "0.5.0-beta.1") < 0);
  EXPECT(c("0.10.0", "0.9.9") > 0);
  EXPECT(c("1.0.0", "0.99.0") > 0);
  EXPECT(c("v0.5.0-beta.3", "0.5.0-beta.3") == 0);
  EXPECT(c("0.5.0-beta.3.2611041319", "0.5.0-beta.3") == 0);  // a build stamp is not a newer version
  EXPECT(c("0.5.0-beta.3+wip", "0.5.0-beta.3") == 0);
  EXPECT(c("0.5.0.2611041319", "0.5.0") == 0);
  EXPECT(!parse("").ok && !parse("dev").ok && !parse("0.5").ok && !parse(nullptr).ok && !parse("0.5.0-").ok);

  const std::vector<Entry> e = {
      {"0.5.0-beta.6", "six"}, {"0.5.0-beta.5", "five"}, {"0.5.0-beta.4", "four"}, {"0.5.0-beta.3", "three"}};
  // only the versions newer than the running one, newest first, each under its heading
  EXPECT(join(e, "0.5.0-beta.3") == "### 0.5.0-beta.6\n\nsix\n\n### 0.5.0-beta.5\n\nfive\n\n### 0.5.0-beta.4\n\nfour");
  EXPECT(join(e, "0.5.0-beta.3.2611041319") == join(e, "0.5.0-beta.3"));
  EXPECT(join(e, "0.5.0-beta.5") == "### 0.5.0-beta.6\n\nsix");
  EXPECT(join(e, "0.4.0") == "### 0.5.0-beta.6\n\nsix\n\n### 0.5.0-beta.5\n\nfive\n\n### 0.5.0-beta.4\n\nfour\n\n### 0.5.0-beta.3\n\nthree");
  // nothing newer, or a running version it can't read: empty (the caller keeps ota.summary)
  EXPECT(join(e, "0.5.0-beta.6").empty());
  EXPECT(join(e, "0.5.0").empty());
  EXPECT(join(e, "").empty() && join(e, "unknown").empty());
  EXPECT(join({}, "0.4.0").empty());
  // the cap: whole entries only, the newest always, then the pointer to the release
  const std::string big(1000, 'x');
  std::vector<Entry> many;
  for (int i = 9; i >= 1; i--) many.push_back({"0.6.0-beta." + std::to_string(i), big});
  const std::string j = join(many, "0.5.0");
  EXPECT(j.size() <= CAP);
  EXPECT(count(j, "### ") == 2);
  EXPECT(j.find("### 0.6.0-beta.9") == 0);
  EXPECT(j.size() >= strlen(MORE) && j.compare(j.size() - strlen(MORE), strlen(MORE), MORE) == 0);
  EXPECT(join(many, "0.6.0-beta.8").find("earlier changes") == std::string::npos);
  const std::string huge(5000, 'y');
  EXPECT(join({{"0.6.0", huge}, {"0.5.1", "z"}}, "0.5.0").find(huge) != std::string::npos);  // the newest, even alone over

  printf(fails ? "fw_notes: %d failed\n" : "fw_notes: all passed\n", fails);
  return fails ? 1 : 0;
}
