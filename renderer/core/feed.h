#pragma once
// The Feed base (CONTRACT.md C51, "Feeds"): one class every feed adapter is built on, so a feed's skeleton is written
// once. A feed is a reserved local entity in the HA store ("panel.weather", core/ha.h on_local) whose fields screens
// bind to; the adapter says only how to fill them. The base owns:
//   rounds     run(): the feed's settings (the spec's "feeds", ha::feed_config) read and compared with the last round,
//              then the one hook fill(). A round runs when a bind first reads the feed, every `every_ms` while something
//              reads it (ha::watched), when the settings change, and when an entity it reads changes (reads()), which
//              is also how it refreshes when Home Assistant comes back (back(): one of them was unavailable, now is not)
//   state      "" = ok | loading | error | empty | missing | unavailable, put with state(); the words a screen shows
//              for a state that is not ok are words() (one place: the same "Loading" / "Offline" / "Nothing yet" /
//              "Not found" / "Unavailable" on every screen). error = the source cannot be reached or is not set up
//              (Home Assistant itself); missing = the entity it reads is not in Home Assistant (the host's empty state);
//              unavailable = the entity is there but has no state now (unavailable / unknown). of(): an entity's state
//              as one of these (an adapter's one test of the entity it reads)
//   fields     declared once (name, required, placeholder): put() takes only a declared field; after a round, a field
//              the round did not put gets its placeholder (an optional field the source cannot fill: "" hides it, the
//              component shows what is left), and a required field missing from an ok round turns the feed to error
//   sample     sample(): the sample adapter's data, the one place it lives per feed; fill() calls it for source
//              "sample". The words it puts are the feed's sample words (samples(), the leak rule: a screen on real data
//              never shows one)
#include <functional>
#include <set>
#include <string>
#include <vector>
#include "ArduinoJson.h"

namespace fw::feed {
enum State { OK, LOADING, ERROR, EMPTY, MISSING, UNAVAILABLE };
const char *state_id(State s);  // "ok" | "loading" | "error" | "empty" | "missing" | "unavailable"
const char *words(State s);     // the screen's words: "" | "Loading" | "Offline" | "Nothing yet" | "Not found" | "Unavailable"
// an entity's state (nullptr = not answered yet): LOADING; the host says Home Assistant is not reached (ha::offline):
// ERROR; "" (no such entity): MISSING; unavailable / unknown: UNAVAILABLE; anything else OK
State of(const std::string *st);

struct Field {
  const char *name;
  bool required;
  const char *none;  // what it shows when a round did not fill it ("" = hidden; nullptr = it keeps its last value: a
                     // control's position the source did not report this time)
};

class Feed {
 public:
  Feed(const char *entity, uint32_t every_ms, const char *source, std::vector<Field> fields);
  virtual ~Feed() = default;
  void run();  // one round (the base calls it; an adapter calls it after an answer it waited for)
  const char *entity() const { return entity_; }
  const std::set<std::string> &samples() const { return samples_; }  // the words the last sample round put (the leak rule)
  const std::vector<Field> &fields() const { return fields_; }
  uint32_t every_ms() const { return every_ms_; }
  const char *default_source() const { return default_source_; }
  void sample_round();  // sample() once (the lab: the sample words without a screen); the next run() puts the real data
  std::string json();   // {entity, every_ms, source, fields: [{name, required, none}], samples: [...]} (lab "feeds", schema)

 protected:
  virtual void fill() = 0;    // put the fields for the settings now (called by run())
  virtual void sample() = 0;  // the sample data: put every field from the feed's one sample
  // for fill()
  void put(const char *field, const std::string &v);
  void state(State s);
  const ArduinoJson::JsonDocument &cfg() const { return cfg_; }  // this round's settings ({} = none)
  std::string source() const { return cfg_["source"] | default_source_; }
  bool changed() const { return changed_; }  // the settings differ from the last round's
  bool back() const { return back_; }        // an entity it reads came back from unavailable (HA reconnected)
  // the entity / attribute pairs it reads now (ha::want: live); a change of one runs a round; {} = none
  void reads(const std::vector<std::pair<std::string, std::string>> &pairs);

 private:
  friend void on_put(const std::string &, const std::string &, const std::string &);
  const char *entity_, *default_source_;
  uint32_t every_ms_;
  std::vector<Field> fields_;
  ArduinoJson::JsonDocument cfg_;
  std::string cfg_seen_ = "\x01";  // never a config: the first round is a change
  std::vector<std::pair<std::string, std::string>> reads_;
  std::set<std::string> put_now_, samples_, down_;  // down_: the entities it reads that are unavailable now
  bool changed_ = false, back_ = false, in_round_ = false, in_sample_ = false;
  void *tick_ = nullptr;
};

const std::vector<Feed *> &all();  // every feed (the leak rule, the schema)
}  // namespace fw::feed
