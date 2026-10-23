// GitHub feed (docs @facet/ui GithubData + @facet/demo-data github()): the pull requests that need you and your own,
// with what the screens say about them (the docs wording: row status, detail statuses, "3 to review · 2 open"). A
// panel fills the same Data from a host (the hub's GitHub poller); sample() is the docs story until then. Header-only.
#pragma once
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace fw::gh {
struct Check { const char *name, *state; };  // pass | fail | pending | skipped
struct PR {
  const char *id, *repo; int number; const char *title, *reason, *review, *ci, *author, *head, *body;
  bool draft; int additions, deletions, files; double age_h; std::vector<Check> checks;
  std::string url() const { return std::string("https://example.com/orchard-labs/") + repo + "/pull/" + std::to_string(number); }
};
struct Data { std::vector<PR> needs, mine; };
inline Data sample() {
  const std::vector<Check> ok3 = {{"build", "pass"}, {"lint", "pass"}, {"test", "pass"}};
  Data d;
  d.needs = {
      {"n1", "lantern", 412, "Add a quiet hours schedule to the notification router", "review", "pending", "pending", "sam-okafor", "quiet-hours",
       "Holds non-urgent notifications between 22:00 and 07:00.\nUrgent ones still come through.\nAdds a settings screen and tests.", false, 120, 14, 6, 26,
       {{"build", "pass"}, {"lint", "pass"}, {"test", "pass"}, {"preview", "pending"}}},
      {"n2", "hearth-ui", 88, "Card grid: snap scrolling one row at a time", "assigned", "pending", "pass", "ada-lindqvist", "snap-rows",
       "Snaps the Home grid per row instead of free scrolling.", false, 64, 9, 3, 5, ok3},
      {"n3", "seedling", 1290, "Fix the sunrise time drifting by a minute each week", "both", "pending", "fail", "jo-marsh", "sunrise-drift",
       "Uses the solar noon from the table instead of adding 24 h.", false, 120, 14, 6, 72, {{"build", "pass"}, {"test", "fail"}, {"lint", "pass"}}},
  };
  d.mine = {
      {"m1", "lantern", 409, "Docs: getting started on a Raspberry Pi", "", "approved", "pass", "you", "pi-docs", "", false, 120, 14, 6, 50, ok3},
      {"m2", "hearth-ui", 86, "Theme tokens for the light mode charts", "", "changes", "pass", "you", "light-charts", "", false, 120, 14, 6, 120, ok3},
      {"m3", "tidepool", 37, "Experiment: ambient screen with drifting dots", "", "pending", "none", "you", "drifting-dots", "", true, 120, 14, 6, 300, {}},
  };
  return d;
}
inline const PR *find(const Data &d, const std::string &id, bool *mine) {
  for (const PR &p : d.needs) if (id == p.id) { *mine = false; return &p; }
  for (const PR &p : d.mine) if (id == p.id) { *mine = true; return &p; }
  return nullptr;
}
// "5h", "3d", "2w" (docs age)
inline std::string age(double h) {
  char b[16];
  if (h < 1) snprintf(b, sizeof b, "%ldm", std::lround(h * 60));
  else if (h < 48) snprintf(b, sizeof b, "%ldh", std::lround(h));
  else if (h < 24 * 14) snprintf(b, sizeof b, "%ldd", std::lround(h / 24));
  else snprintf(b, sizeof b, "%ldw", std::lround(h / 24 / 7));
  return b;
}
struct Status { const char *tone, *text; };
// the detail view's status line: why it needs you (or its review state, for your own), draft, CI
inline std::vector<Status> statuses(const PR &p, bool mine) {
  std::vector<Status> out;
  const std::string r = p.reason, rv = p.review, ci = p.ci;
  if (!mine) {
    out.push_back({"warn", r == "assigned" ? "assigned" : "review"});
    if (r == "both") out.push_back({"info", "assigned"});
  } else if (!p.draft) out.push_back(rv == "approved" ? Status{"ok", "approved"} : rv == "changes" ? Status{"error", "changes"} : Status{"idle", "review pending"});
  if (p.draft) out.push_back({"idle", "draft"});
  if (ci == "fail") out.push_back({"error", "ci fail"});
  else if (ci == "pass") out.push_back({"ok", "ci pass"});
  else if (ci == "pending") out.push_back({"working", "ci running"});
  return out;
}
}  // namespace fw::gh
