// A web read for the feeds' direct sources (core/net.h).
#include "net.h"
#include <map>
#include "fw.h"

namespace fw::net {
namespace {
struct Read { std::string url, body; size_t max = MAX_BODY; Done done; uint32_t at = 0; bool over = false; };
std::map<uint32_t, Read> &reads() { static std::map<uint32_t, Read> m; return m; }
Sink sink = nullptr;
uint32_t next_id = 1;
lv_timer_t *watch = nullptr;
void end(uint32_t id, bool ok, const std::string &why) {
  auto it = reads().find(id);
  if (it == reads().end()) return;
  Read r = std::move(it->second);
  reads().erase(it);
  if (r.over) ok = false;
  if (r.done) r.done(ok, ok ? r.body : std::string(), ok ? std::string() : r.over ? std::string("too much data") : why);
}
void check_timeouts() {  // a read the platform never ends fails "timeout" (the feed tries again on its next round)
  std::vector<uint32_t> late;
  for (auto &kv : reads())
    if (lv_tick_elaps(kv.second.at) > TIMEOUT_MS) late.push_back(kv.first);
  for (uint32_t id : late) end(id, false, "timeout");
}
}  // namespace

void on_get(Sink f) { sink = f; }
bool ready() { return sink != nullptr; }
int open_reads() { return (int) reads().size(); }

uint32_t get(const std::string &url, Done done, size_t max) {
  for (auto &kv : reads())
    if (kv.second.url == url) { kv.second.done = std::move(done); return kv.first; }
  if (!sink) {
    if (done) done(false, "", "no web access on this build");
    return 0;
  }
  if (!watch) watch = lv_timer_create([](lv_timer_t *) { check_timeouts(); }, 1000, nullptr);
  const uint32_t id = next_id++;
  Read &r = reads()[id];
  r.url = url, r.max = max, r.done = std::move(done), r.at = lv_tick_get();
  sink(id, url);
  return id;
}
void cancel(uint32_t id) { reads().erase(id); }
void chunk(uint32_t id, const char *p, size_t n) {
  auto it = reads().find(id);
  if (it == reads().end() || it->second.over) return;
  if (it->second.body.size() + n > it->second.max) { it->second.over = true; it->second.body.clear(); it->second.body.shrink_to_fit(); return; }
  it->second.body.append(p, n);
}
void finish(uint32_t id, bool ok, const std::string &why) { end(id, ok, why); }
}  // namespace fw::net
