#pragma once
// The update dialog's release notes for every version between the installed one and the one offered. The OTA
// manifest (firmware/manifests.mjs) lists one short summary per version since the last stable release, newest first
// (release-notes.mjs entries); join() keeps those newer than the running version, each under a "### <version>"
// heading, newest first, held under `cap` bytes ("...and earlier changes: see the release" when cut). An empty result
// = nothing newer (or a running version it can't read): the caller keeps the manifest's ota.summary.
// Plain C++, no ESPHome: tested on the host (fw_notes.test.cpp, renderer/check.sh step fw-notes).
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace fw::notes {

// X.Y.Z, X.Y.Z-tag.N, a leading "v"; anything after that (a build stamp ".2611041319", "+wip") is ignored, so a
// workbench build of 0.5.0-beta.3 counts as 0.5.0-beta.3. A pre-release sorts before its release (dev < alpha <
// beta < rc < release; another tag sits with alpha, then by name).
struct Ver {
  bool ok = false;
  long n[3] = {0, 0, 0};
  int rank = 4;  // 4 = a release
  std::string tag;
  long pre = 0;
};

inline Ver parse(const char *s) {
  Ver v;
  if (!s) return v;
  if (*s == 'v' || *s == 'V') s++;
  for (int i = 0; i < 3; i++) {
    if (*s < '0' || *s > '9') return v;
    char *e;
    v.n[i] = strtol(s, &e, 10);
    s = e;
    if (i < 2) {
      if (*s != '.') return v;
      s++;
    }
  }
  v.ok = true;
  if (*s != '-') return v;
  s++;
  while ((*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z')) v.tag += (char) (*s >= 'A' && *s <= 'Z' ? *s + 32 : *s), s++;
  if (v.tag.empty()) return v.ok = false, v;
  static const char *const tags[] = {"dev", "alpha", "beta", "rc"};
  v.rank = 1;
  for (int i = 0; i < 4; i++)
    if (v.tag == tags[i]) v.rank = i;
  if (*s == '.' && s[1] >= '0' && s[1] <= '9') v.pre = strtol(s + 1, nullptr, 10);
  return v;
}

// <0, 0, >0 as a is older, the same, newer than b
inline int cmp(const Ver &a, const Ver &b) {
  for (int i = 0; i < 3; i++)
    if (a.n[i] != b.n[i]) return a.n[i] < b.n[i] ? -1 : 1;
  if (a.rank != b.rank) return a.rank < b.rank ? -1 : 1;
  if (int c = a.tag.compare(b.tag)) return c < 0 ? -1 : 1;
  if (a.pre != b.pre) return a.pre < b.pre ? -1 : 1;
  return 0;
}

using Entry = std::pair<std::string, std::string>;  // version, Markdown summary
constexpr size_t CAP = 3072;
constexpr const char *MORE = "\n\n*…and earlier changes: see the release.*";

// entries newest first (the manifest's order; an older one out of place is still filtered by version)
inline std::string join(const std::vector<Entry> &entries, const char *running, size_t cap = CAP) {
  const Ver run = parse(running);
  if (!run.ok) return "";
  std::string out;
  bool cut = false;
  for (const auto &e : entries) {
    const Ver v = parse(e.first.c_str());
    if (!v.ok || cmp(v, run) <= 0) continue;
    if (cut) continue;
    std::string part = (out.empty() ? "### " : "\n\n### ") + e.first + "\n\n" + e.second;
    if (!out.empty() && out.size() + part.size() + strlen(MORE) > cap) {
      cut = true;
      continue;
    }
    out += part;
  }
  if (cut) out += MORE;
  return out;
}

}  // namespace fw::notes
