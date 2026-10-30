// The github feed (CONTRACT.md C51): the reserved entity panel.github (core/spec.h "local sources") in the HA store,
// the one shape the GitHub screen and its Home card bind to, whatever fetches the pull requests. Same file on the
// panels and in WASM. Wording = docs @facet/ui screens/github.ts (hero, rowStatus, statusesFor, the Home card).
//   state       ok | loading | error
//   hero_n      how many pull requests need you ("3"; "--" loading / error); hero_label need you | needs you | nothing
//               needs you; hero_line "2 reviews · 1 assigned · updated 2m ago" / "3 of your PRs open"; hero_accent on
//               while any need you
//   top         the summary's rows (the ones that need you, else your own; at most 3), a JSON list of
//               {id, label, title, status, tone} (the Status row's props); more "all pull requests · 3 more", more_on
//   needs_title "need you · 3" / "nothing needs you"; needs: every one that needs you (rows as above)
//   mine_title  "your prs · 3"; mine: your own (rows as above)
//   head.<view>.<pr>  the header's words: "GitHub" (sum.none), "Pull requests" (list.none), "#412" (a pull request)
//   <pr>.meta / .title / .by / .statuses / .body / .url   one pull request's view (the GitHub PR component's props;
//               statuses a JSON list of {tone, text}); <pr>.checks_k "checks · 4 · 1 failing", <pr>.checks_more "+3
//               more", <pr>.checks a JSON list of {id, text, tone, ink} (Status lines: failing first, at most 10)
//   card_value  the Home card: "3" / "--"; card_sub "to review · 3 open" / "nothing to review · 3 open" / "offline";
//               card_accent on while any need you
// <pr> = the pull request's id in the feed (n1.. need you, m1.. your own): the spec's "pr" parameter picks one.
// Adapters (the feed's settings, the spec's "feeds"; core/ha.h feed_config): source
//   ha      a Home Assistant sensor in the panel github shape (setting sensor): attributes needs and mine = JSON lists
//           of pull requests in the docs GithubData shape ({repo, number, title, reason review | assigned | both,
//           review approved | changes | pending, ci pass | fail | pending | none, draft, author, head, base, created
//           (unix s), url, body, files, additions, deletions, checks [{name, state}]}), at = unix s of the fetch; a
//           live subscription like any bind (ages move on every minute)
//   sample  the docs story (core/github.h sample: three need you, three of your own), always there
// The panel fetching GitHub itself (a read-only token kept on the panel) = one more adapter here, no screen change.
#include <cmath>
#include <cstdio>
#include <ctime>
#include <vector>
#include "fw.h"
#include "ha.h"
#include "github.h"

namespace fw::dev::github_source {
namespace {
constexpr const char *E = "panel.github";
using Doc = ArduinoJson::JsonDocument;
struct Check { std::string name, state; };
struct PR {
  std::string repo, title, reason, review, ci, author, head, base, url, body;
  int number = 0, files = 0, add = 0, del = 0;
  bool draft = false;
  double age_h = 0;
  std::vector<Check> checks;
};
struct Data { std::vector<PR> needs, mine; double at_min = 0; };
std::string cfg_seen, ent_now;
lv_timer_t *tick = nullptr;

void put(const std::string &a, const std::string &v) { ha::put(E, a, v); }
std::string ser(const Doc &d) { std::string s; serializeJson(d, s); return s; }
std::string num(long n) { return std::to_string(n); }
std::string id_of(bool mine, size_t i) { return std::string(mine ? "m" : "n") + num((long) i + 1); }

// docs rowStatus: ci fail, approved / changes (your own), draft, review / assigned (needs you)
void row_status(const PR &p, bool mine, const char **tone, const char **text) {
  *tone = "", *text = "";
  if (p.ci == "fail") *tone = "error", *text = "ci fail";
  else if (mine && p.review == "approved") *tone = "ok", *text = "approved";
  else if (mine && p.review == "changes") *tone = "error", *text = "changes";
  else if (p.draft) *tone = "idle", *text = "draft";
  else if (!mine) *tone = "warn", *text = p.reason == "assigned" ? "assigned" : "review";
}
void rows(Doc &d, const std::vector<PR> &prs, bool mine, size_t max) {
  d.to<ArduinoJson::JsonArray>();
  for (size_t i = 0; i < prs.size() && i < max; i++) {
    const PR &p = prs[i];
    ArduinoJson::JsonObject o = d.add<ArduinoJson::JsonObject>();
    const char *tone, *text;
    row_status(p, mine, &tone, &text);
    o["id"] = id_of(mine, i);
    o["label"] = p.repo + " \xC2\xB7 #" + num(p.number) + " \xC2\xB7 " + fw::gh::age(p.age_h);
    o["title"] = p.title, o["status"] = text, o["tone"] = *tone ? tone : "idle";
  }
}
// docs statusesFor: why it needs you (or its review state, for your own), draft, CI
void statuses(Doc &d, const PR &p, bool mine) {
  d.to<ArduinoJson::JsonArray>();
  auto add = [&](const char *tone, const char *text) { ArduinoJson::JsonObject o = d.add<ArduinoJson::JsonObject>(); o["tone"] = tone, o["text"] = text; };
  if (!mine) {
    add("warn", p.reason == "assigned" ? "assigned" : "review");
    if (p.reason == "both") add("info", "assigned");
  } else if (!p.draft) {
    if (p.review == "approved") add("ok", "approved");
    else if (p.review == "changes") add("error", "changes");
    else add("idle", "review pending");
  }
  if (p.draft) add("idle", "draft");
  if (p.ci == "fail") add("error", "ci fail");
  else if (p.ci == "pass") add("ok", "ci pass");
  else if (p.ci == "pending") add("working", "ci running");
}
const char *check_tone(const std::string &s) { return s == "pass" ? "ok" : s == "fail" ? "error" : s == "pending" ? "working" : "idle"; }
void detail(const PR &p, bool mine, const std::string &id) {
  const std::string k = id + ".";
  put("head.sum." + id, "#" + num(p.number)), put("head.list." + id, "#" + num(p.number));
  put(k + "meta", p.repo + " \xC2\xB7 " + p.head + " \xE2\x86\x92 " + p.base);
  put(k + "title", p.title);
  put(k + "by", p.author + " \xC2\xB7 " + fw::gh::age(p.age_h) + " \xC2\xB7 " + num(p.files) + " files \xC2\xB7 +" + num(p.add) + " \xE2\x88\x92" + num(p.del));
  Doc st;
  statuses(st, p, mine);
  put(k + "statuses", ser(st));
  std::string body, line;  // the first three lines with text
  int n = 0;
  for (size_t i = 0; i <= p.body.size() && n < 3; i++) {
    if (i == p.body.size() || p.body[i] == '\n') {
      if (line.find_first_not_of(" \t\r") != std::string::npos) body += (n++ ? "\n" : "") + line;
      line.clear();
    } else line += p.body[i];
  }
  put(k + "body", body.empty() ? "No description." : body);
  put(k + "url", p.url);
  // fails first, then the first 10 (or 9 and "+N more")
  std::vector<Check> sorted;
  for (const Check &c : p.checks) if (c.state == "fail") sorted.push_back(c);
  for (const Check &c : p.checks) if (c.state != "fail") sorted.push_back(c);
  const size_t MAX = 10, shown = sorted.size() > MAX ? MAX - 1 : sorted.size();
  int f = 0;
  for (const Check &c : p.checks) f += c.state == "fail";
  std::string ck = "checks";
  if (!p.checks.empty()) ck += " \xC2\xB7 " + num((long) p.checks.size()) + (f ? " \xC2\xB7 " + num(f) + " failing" : "");
  put(k + "checks_k", ck);
  put(k + "checks_more", sorted.size() > MAX ? "+" + num((long) (sorted.size() - shown)) + " more" : "");
  Doc cd;
  cd.to<ArduinoJson::JsonArray>();
  for (size_t i = 0; i < shown; i++) {
    ArduinoJson::JsonObject o = cd.add<ArduinoJson::JsonObject>();
    o["id"] = "c" + num((long) i), o["text"] = sorted[i].name, o["tone"] = check_tone(sorted[i].state);
    o["ink"] = sorted[i].state == "fail" ? "text" : "dim";  // docs .fs-check: text-dim, a failing one text
  }
  put(k + "checks", ser(cd));
}
void put_data(const Data &d) {
  const size_t n = d.needs.size();
  int reviews = 0, assigned = 0;
  for (const PR &p : d.needs) reviews += p.reason == "review" || p.reason == "both", assigned += p.reason == "assigned" || p.reason == "both";
  char b[96];
  put("hero_n", num((long) n));
  put("hero_label", n ? (n == 1 ? "needs you" : "need you") : "nothing needs you");
  const long ago = std::lround(d.at_min);
  const std::string upd = ago < 1 ? "just now" : ago < 60 ? num(ago) + "m ago" : num(ago / 60) + "h ago";
  if (n) snprintf(b, sizeof b, "%d review%s \xC2\xB7 %d assigned \xC2\xB7 updated %s", reviews, reviews == 1 ? "" : "s", assigned, upd.c_str());
  else snprintf(b, sizeof b, "%zu of your PRs open", d.mine.size());
  put("hero_line", b);
  put("hero_accent", n ? "on" : "off");
  Doc top;
  rows(top, n ? d.needs : d.mine, !n, 3);
  put("top", ser(top));
  const size_t total = n + d.mine.size(), rest = total - std::min<size_t>(3, n ? n : d.mine.size());
  put("more", rest ? "all pull requests \xC2\xB7 " + num((long) rest) + " more" : "all pull requests");
  put("more_on", total ? "1" : "");
  put("needs_title", n ? "need you \xC2\xB7 " + num((long) n) : "nothing needs you");
  Doc nd, md;
  rows(nd, d.needs, false, 24);
  rows(md, d.mine, true, 24);
  put("needs", ser(nd)), put("mine", ser(md));
  put("mine_title", "your prs \xC2\xB7 " + num((long) d.mine.size()));
  put("head.sum.none", "GitHub"), put("head.list.none", "Pull requests");
  for (size_t i = 0; i < n; i++) detail(d.needs[i], false, id_of(false, i));
  for (size_t i = 0; i < d.mine.size(); i++) detail(d.mine[i], true, id_of(true, i));
  put("card_value", num((long) n));
  put("card_sub", (n ? "to review \xC2\xB7 " : "nothing to review \xC2\xB7 ") + num((long) d.mine.size()) + " open");
  put("card_accent", n ? "on" : "off");
  put("", "ok");
}
void put_empty(bool err) {
  put("hero_n", "--"), put("hero_label", err ? "github offline" : "loading"), put("hero_line", ""), put("hero_accent", "off");
  put("top", "[]"), put("more", "all pull requests"), put("more_on", "");
  put("needs_title", err ? "github offline" : "loading"), put("needs", "[]"), put("mine_title", ""), put("mine", "[]");
  put("head.sum.none", "GitHub"), put("head.list.none", "Pull requests");
  put("card_value", "--"), put("card_sub", err ? "offline" : "loading"), put("card_accent", "off");
  put("", err ? "error" : "loading");
}

// ---- adapter: ha (a sensor in the panel github shape)
void parse(const std::string *s, std::vector<PR> &out, double now) {
  Doc j;
  if (!s || deserializeJson(j, s->c_str() + (!s->empty() && (*s)[0] == 'J' ? 1 : 0))) return;
  for (ArduinoJson::JsonVariantConst x : j.as<ArduinoJson::JsonArrayConst>()) {
    PR p;
    p.repo = x["repo"] | "", p.title = x["title"] | "", p.reason = x["reason"] | "review", p.review = x["review"] | "pending";
    p.ci = x["ci"] | "none", p.author = x["author"] | "", p.head = x["head"] | "", p.base = x["base"] | "main";
    p.url = x["url"] | "", p.body = x["body"] | "";
    p.number = x["number"] | 0, p.files = x["files"] | 0, p.add = x["additions"] | 0, p.del = x["deletions"] | 0;
    p.draft = x["draft"] | false;
    double c = x["created"] | now;
    if (c > 1e12) c /= 1000;  // ms
    p.age_h = std::max(0.0, (now - c) / 3600);
    for (ArduinoJson::JsonVariantConst k : x["checks"].as<ArduinoJson::JsonArrayConst>()) p.checks.push_back({k["name"] | "", k["state"] | "skipped"});
    out.push_back(p);
  }
}
void rebuild() {
  if (ent_now.empty()) return;
  const std::string *st = ha::get(ent_now);
  if (!st) { put_empty(false); return; }
  if (*st == "unavailable" || *st == "unknown") { put_empty(true); return; }
  const double now = (double) time(nullptr);
  Data d;
  parse(ha::get(ent_now, "needs"), d.needs, now);
  parse(ha::get(ent_now, "mine"), d.mine, now);
  const std::string *at = ha::get(ent_now, "at");
  d.at_min = at ? std::max(0.0, (now - atof(at->c_str())) / 60) : 0;
  put_data(d);
}
void entity_put(const std::string &e, const std::string &a, const std::string &) {
  if (!ent_now.empty() && e == ent_now && ha::watched(E) && (a.empty() || a == "needs" || a == "mine" || a == "at")) rebuild();
}

// ---- adapter: sample (the docs story)
void sample_now() {
  const fw::gh::Data g = fw::gh::sample();
  Data d;
  auto conv = [](const fw::gh::PR &s) {
    PR p;
    p.repo = s.repo, p.title = s.title, p.reason = s.reason, p.review = s.review, p.ci = s.ci, p.author = s.author;
    p.head = s.head, p.base = "main", p.url = s.url(), p.body = s.body;
    p.number = s.number, p.files = s.files, p.add = s.additions, p.del = s.deletions, p.draft = s.draft, p.age_h = s.age_h;
    for (const fw::gh::Check &c : s.checks) p.checks.push_back({c.name, c.state});
    return p;
  };
  for (const fw::gh::PR &p : g.needs) d.needs.push_back(conv(p));
  for (const fw::gh::PR &p : g.mine) d.mine.push_back(conv(p));
  d.at_min = 2;
  put_data(d);
}
void fresh() {
  if (!tick) tick = lv_timer_create([](lv_timer_t *) { if (ha::watched(E)) fresh(); }, 60000, nullptr);  // ages move on
  const std::string &cfg = ha::feed_config(E);
  Doc c;
  deserializeJson(c, cfg.empty() ? "{}" : cfg.c_str());
  const std::string src = c["source"] | "ha", ent = c["sensor"] | "";
  const bool changed = cfg != cfg_seen;
  cfg_seen = cfg;
  static const bool hooked = (ha::on_put(entity_put), true);
  (void) hooked;
  if (src == "ha" && ent.rfind("sensor.", 0) == 0) {
    if (changed || ent != ent_now) {
      ent_now = ent;
      for (const char *a : {"", "needs", "mine", "at"}) ha::want(ent, a);
    }
    rebuild();
  } else if (src == "ha") { ent_now.clear(); put_empty(true); }  // no sensor picked
  else { ent_now.clear(); sample_now(); }
}
[[maybe_unused]] const bool registered = (ha::on_local(E, fresh), true);  // binds on panel.github never reach a host
}  // namespace
}  // namespace fw::dev::github_source
