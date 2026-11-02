// The Feed base (core/feed.h): rounds, state, fields and placeholders, the sample words, the live reads.
#include "feed.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include "fw.h"
#include "ha.h"

namespace fw::feed {
namespace {
std::vector<Feed *> &list() { static std::vector<Feed *> v; return v; }
// ha::on_local takes a plain function: one per feed slot
template <int N> void tramp() { if (N < (int) list().size()) list()[N]->run(); }
using Fn = void (*)();
constexpr Fn TRAMP[] = {tramp<0>, tramp<1>, tramp<2>, tramp<3>, tramp<4>, tramp<5>, tramp<6>, tramp<7>};
bool bad(const std::string *v) { return !v || v->empty() || *v == "unavailable" || *v == "unknown"; }
}  // namespace

const char *state_id(State s) { return s == LOADING ? "loading" : s == ERROR ? "error" : s == EMPTY ? "empty" : "ok"; }
const char *words(State s) { return s == LOADING ? "Loading" : s == ERROR ? "Offline" : s == EMPTY ? "Nothing yet" : ""; }
const std::vector<Feed *> &all() { return list(); }

void on_put(const std::string &e, const std::string &a, const std::string &v) {  // an entity a feed reads changed
  for (Feed *f : list()) {
    if (f->in_round_ || std::none_of(f->reads_.begin(), f->reads_.end(), [&](const auto &r) { return r.first == e && r.second == a; })) continue;
    bool came = false;
    if (a.empty()) {
      const bool was = f->down_.count(e), down = v.empty() || v == "unavailable" || v == "unknown";
      if (down) f->down_.insert(e); else f->down_.erase(e);
      came = was && !down;
    }
    f->back_ = came;
    f->run();
    f->back_ = false;
  }
}

Feed::Feed(const char *entity, uint32_t every_ms, const char *source, std::vector<Field> fields)
    : entity_(entity), default_source_(source), every_ms_(every_ms), fields_(std::move(fields)) {
  const size_t n = list().size();
  if (n >= sizeof TRAMP / sizeof TRAMP[0]) abort();  // more feeds than slots: add a slot
  list().push_back(this);
  ha::on_local(entity, TRAMP[n]);
  static const bool hooked = (ha::on_put(on_put), true);
  (void) hooked;
}

void Feed::run() {
  if (in_round_) return;
  if (!tick_ && every_ms_)
    tick_ = lv_timer_create([](lv_timer_t *t) {
      auto *f = (Feed *) lv_timer_get_user_data(t);
      if (ha::watched(f->entity_)) f->run();
    }, every_ms_, this);
  const std::string &c = ha::feed_config(entity_);
  changed_ = c != cfg_seen_;
  cfg_seen_ = c;
  cfg_.clear();
  deserializeJson(cfg_, c.empty() ? "{}" : c.c_str());
  in_round_ = true;
  put_now_.clear();
  if (source() == "sample") {
    in_sample_ = true;
    sample();
    in_sample_ = false;
  } else fill();
  in_round_ = false;
  const std::string *st = ha::get(entity_);
  const bool ok = st && *st == "ok";
  for (const Field &f : fields_) {
    if (put_now_.count(f.name)) continue;
    if (!f.none) continue;
    if (f.required && ok && !put_now_.empty()) {  // a source that cannot fill what the screen needs: an error, not a gap
      printf("feed %s: required field %s not filled\n", entity_, f.name);
      ha::put(entity_, "", state_id(ERROR));
    }
    ha::put(entity_, f.name, f.none);
  }
}

void Feed::put(const char *field, const std::string &v) {
  if (std::none_of(fields_.begin(), fields_.end(), [&](const Field &f) { return !strcmp(f.name, field); })) {
    printf("feed %s: %s is not one of its fields\n", entity_, field);
    abort();
  }
  put_now_.insert(field);
  if (in_sample_ && v.size() >= 4) samples_.insert(v);
  ha::put(entity_, field, v);
}

void Feed::state(State s) { ha::put(entity_, "", state_id(s)); }

void Feed::reads(const std::vector<std::pair<std::string, std::string>> &pairs) {
  if (pairs == reads_) return;
  reads_ = pairs;
  down_.clear();
  for (const auto &r : reads_) {
    if (r.second.empty() && bad(ha::get(r.first))) down_.insert(r.first);
    ha::want(r.first, r.second);
  }
}
}  // namespace fw::feed
