// Native driver for the theme parser (core/theme_file.cpp), built with ASan + UBSan by tools/theme_fuzz.sh.
//   theme_fuzz --load <file.json> ...           load each file: "ok <id> <bytes> <digest>" or its errors (exit 1 if any refused)
//   theme_fuzz --fuzz <n> --seed <s> <seed.json> ...
//       n seeded mutations of the seed files (byte flips, cuts, copies, swaps, inserted JSON pieces and schema
//       names, deep nesting, truncation). For each: a Skin XOR errors (never both, never neither), at most
//       MAX_ERRORS errors each with a message, a refused theme_apply leaves the live Skin's digest and the
//       generation exactly as they were, an accepted file loads again to the same digest. Any failure prints the
//       seed and iteration (reproducible) and exits 1. ASan / UBSan reports abort the run.
#include "theme_file.h"
#include "theme_gen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

namespace {
struct Rng {  // mulberry32 (the functional tests' seeded())
  uint32_t a;
  uint32_t next() { a += 0x6d2b79f5u; uint32_t t = a; t = (t ^ (t >> 15)) * (t | 1); t ^= t + (t ^ (t >> 7)) * (t | 61); return t ^ (t >> 14); }
  int in(int lo, int hi) { return hi <= lo ? lo : lo + (int) (next() % (uint32_t) (hi - lo + 1)); }
};
std::string slurp(const char *f) {
  FILE *fp = fopen(f, "rb");
  if (!fp) { fprintf(stderr, "theme_fuzz: cannot read %s\n", f); exit(2); }
  std::string s;
  char b[4096];
  size_t n;
  while ((n = fread(b, 1, sizeof b, fp)) > 0) s.append(b, n);
  fclose(fp);
  return s;
}
const char *const PIECES[] = {"{", "}", "[", "]", ",", ":", "\"", "\"#FFFFFF\"", "\"#12345\"", "1e309", "-1", "0.5", "999999",
  "null", "true", "false", "\"\\u0000\"", "\"\\ud800\"", "\"\\uDC00x\"", "\"\\x\"", "\xC0\xAF", "\xF5\x80\x80\x80", "\xE2\x82",
  "[[[[[[[[[[1]]]]]]]]]]", "{\"a\":{\"b\":{\"c\":{\"d\":{\"e\":{\"f\":{\"g\":{\"h\":1}}}}}}}}", "\"schema\": 2", "\"modes\": []",
  "\"edges\": [{}, {}, {}, {}, {}, {}, {}, {}, {}]", "\"pressed\": {\"pressed\": {}}", "\"size\": {\"layout:inset\": [8, 8]}",
  "\"accents\": []", "\"parts\": {\"card\": {\"face\": {\"checker\": [\"text\", \"bg\"]}}}", "\"type\": {\"title\": [99, 99]}",
  "\t", "\n", " ", "\r\n", "/* c */", "\"\xff\""};
std::string name() {  // a random schema name: what a mutation that keeps the syntax would say
  static Rng r{7};
  switch (r.in(0, 6)) {
    case 0: return fx::tgen::COLOURS[r.in(0, fx::tgen::N_COLOURS - 1)];
    case 1: return fx::tgen::PARTS[r.in(0, fx::tgen::N_PARTS - 1)].name;
    case 2: return fx::tgen::SIZES[r.in(0, fx::tgen::N_SIZES - 1)].name;
    case 3: return fx::tgen::STATES[r.in(0, fx::tgen::N_STATES - 1)];
    case 4: return fx::tgen::KEYS[r.in(0, fx::tgen::N_KEYS - 1)];
    case 5: return fx::tgen::ROLES[r.in(0, fx::tgen::N_ROLES - 1)];
    default: return fx::tgen::SIDES[r.in(0, fx::tgen::N_SIDES - 1)];
  }
}
std::string mutate(std::string s, Rng &r) {
  const int ops = r.in(1, 4);
  for (int k = 0; k < ops; k++) {
    const int n = (int) s.size(), at = r.in(0, n > 0 ? n - 1 : 0), len = r.in(1, 24);
    switch (r.in(0, 9)) {
      case 0: if (n) s[(size_t) at] = (char) (s[(size_t) at] ^ (1 << r.in(0, 7))); break;              // a bit flip
      case 1: if (n) s.erase((size_t) at, (size_t) len); break;                                     // a cut
      case 2: if (n) s.insert((size_t) at, s.substr((size_t) at, (size_t) len)); break;              // a copy (dup keys)
      case 3: s.insert((size_t) at, PIECES[r.in(0, (int) (sizeof PIECES / sizeof *PIECES) - 1)]); break;
      case 4: if (n) s.resize((size_t) at); break;                                                  // truncation
      case 5: { const int b = r.in(0, n > 0 ? n - 1 : 0); if (n && at + len < n && b + len < n) { std::string x = s.substr((size_t) at, (size_t) len); s.replace((size_t) at, (size_t) len, s.substr((size_t) b, (size_t) len)); s.replace((size_t) b, (size_t) len, x); } break; }
      case 6: { size_t q = s.find('"', (size_t) at); if (q != std::string::npos) { size_t e = s.find('"', q + 1); if (e != std::string::npos) s.replace(q + 1, e - q - 1, name()); } break; }  // a different name
      case 7: { size_t q = s.find_first_of("0123456789", (size_t) at); if (q != std::string::npos) s.replace(q, 1, std::to_string(r.in(-40, 400))); break; }  // a different number
      case 8: { const int dpt = r.in(1, 12); s = std::string((size_t) dpt, '[') + s + std::string((size_t) dpt, ']'); break; }  // deep nesting
      default: { std::string b((size_t) r.in(1, 3000), ' '); s.insert((size_t) at, b); break; }    // padding (size cap)
    }
  }
  return s;
}
}  // namespace

int main(int argc, char **argv) {
  std::vector<std::string> files;
  long n = 0;
  uint32_t seed = 1;
  bool load = false;
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--load")) load = true;
    else if (!strcmp(argv[i], "--fuzz") && i + 1 < argc) n = atol(argv[++i]);
    else if (!strcmp(argv[i], "--seed") && i + 1 < argc) seed = (uint32_t) strtoul(argv[++i], nullptr, 10);
    else files.push_back(argv[i]);
  }
  std::vector<fx::ThemeError> err;
  if (load) {
    int bad = 0;
    for (const auto &f : files) {
      const std::string s = slurp(f.c_str());
      auto sk = fx::theme_load(s.data(), s.size(), err);
      if (sk) printf("ok %s %zu bytes, %u checks, digest %016llx\n", sk->id, sizeof(fx::Skin), (unsigned) sk->checks, (unsigned long long) sk->digest());
      else { bad++; printf("REFUSED %s\n", f.c_str()); for (auto &e : err) printf("  %s%s%s (line %d)\n", e.path.c_str(), e.path.empty() ? "" : ": ", e.msg.c_str(), e.line); }
    }
    return bad ? 1 : 0;
  }
  std::vector<std::string> seeds;
  for (const auto &f : files) seeds.push_back(slurp(f.c_str()));
  if (seeds.empty()) { fprintf(stderr, "usage: theme_fuzz --fuzz <n> --seed <s> <seed.json> ...\n"); return 2; }
  Rng r{seed};
  long ok = 0, refused = 0;
  // the live theme = the first seed (a real theme), so a partial apply would show as a changed digest
  if (!fx::theme_apply(seeds[0].data(), seeds[0].size(), err)) { fprintf(stderr, "theme_fuzz: the first seed is refused\n"); return 1; }
  const uint64_t live = fx::theme_live()->digest();
  const uint32_t gen = fx::theme_generation();
  for (long i = 0; i < n; i++) {
    const std::string s = mutate(seeds[(size_t) r.in(0, (int) seeds.size() - 1)], r);
    auto sk = fx::theme_load(s.data(), s.size(), err);
    auto bad = [&](const char *why) { fprintf(stderr, "theme_fuzz FAIL seed %u iteration %ld: %s (%zu bytes)\n", seed, i, why, s.size()); exit(1); };
    if (!sk == err.empty()) bad(sk ? "a Skin AND errors" : "no Skin and no errors");
    if ((int) err.size() > fx::tgen::MAX_ERRORS) bad("more errors than the cap");
    for (auto &e : err) if (e.msg.empty()) bad("an error without a message");
    if (sk) {
      ok++;
      std::vector<fx::ThemeError> e2;
      auto again = fx::theme_load(s.data(), s.size(), e2);
      if (!again || again->digest() != sk->digest()) bad("an accepted file loads differently the second time");
    } else {
      refused++;
      if (i % 16 == 0) {  // a refused file through the live path: nothing changes
        std::vector<fx::ThemeError> e2;
        if (fx::theme_apply(s.data(), s.size(), e2)) bad("theme_apply accepted a file theme_load refused");
        if (fx::theme_live()->digest() != live || fx::theme_generation() != gen) bad("a refused file changed the live theme (partial apply)");
      }
    }
  }
  printf("theme_fuzz: %ld cases (seed %u): %ld accepted, %ld refused, 0 failures\n", n, seed, ok, refused);
  return 0;
}
