// API keys (core/keys.cpp): who may change this panel from outside its own editor, the same C++ on the panels and
// in the WASM tests (tests/functional/agent_keys.test.mjs). The firmware is thin glue (firmware/fw_device.h "API keys":
// the HTTP routes call http() / allow() / change() in the main loop, the keys live in NVS through on_store / restore).
//   switch   the setting aiAgents ("Allow API access", tokens/settings.json, off on a new panel): off = every
//            key refused with one plain line, and no new key can be asked for. Only the glass, Home Assistant and the
//            panel's own editor can turn it on (core/device.cpp set()).
//   asking   POST /fw/keys/request {name, expires?: 1d | 30d | never (default 30d)} -> {ok, id, wait_s}: the glass shows
//            "Allow <name>?" (a Bottom sheet on the display's top layer, over whatever shows; one at a time, the rest
//            wait their turn) with Deny and Allow; no tap in ASK_MS (two minutes), Close, the scrim or the panel key =
//            Deny. The caller polls POST /fw/keys/poll {id}: waiting {left_s}; allowed = the key, ONCE (taken after);
//            denied = one plain line. The panel's editor makes the same request (its Create key button): one path.
//   keys     "fk-" + 32 hex (128 bits from on_random). Only its SHA-256 is kept, with a public id (8 hex, for Revoke),
//            the name, made / used (wall-clock s) and ends (0 = until revoked). Three kinds (expires): 30d (the default,
//            a tap-issued key) = ends after IDLE_S (30 days) of inactivity: every use moves ends to that use + IDLE_S;
//            1d = a fixed day from when it was made, used or not; never = until revoked. Stored ends follow the
//            last-used write throttle (below), so after a reboot a 30d key may end up to an hour early: fine.
//            At most MAX_KEYS: a request past that
//            is refused (revoke one first). A key unused for DROP_S (90 days) is dropped; an ended one is refused and
//            dropped. Last used is kept in memory at once and stored at most once an hour per key (flash wear).
//            The clock: wall-clock time (SNTP / HA on the panel); before it is set no key can be made, and nothing is
//            ended or dropped (it cannot tell).
//   gate     allow(): the panel's own editor (a same-origin browser request) passes; else the switch must be on and
//            Authorization: Bearer <key> a live key. Refusals: 403 agents "off", 401 agents "key", each one plain line.
//            Reads (GET) never need a key and never answer a key or a token (GET /fw/keys lists names and times only).
//   change   change(who, kind, what, apply): a write made with a key (who = its id; "" = the editor: no toast). The state
//            it may replace is kept first (one previous state per kind: SCREENS = every sent screen + the deck, SETTING
//            = every setting, HA_LINK = the address and token), then apply() runs; kept = a toast on the glass "<what>"
//            "By <key name>" with Undo. Undo puts that kind back as it was before that change (a newer change of the
//            same kind since = "Can't undo"); it is not itself a key's change. describe() = the words for a route's body.
//   rev()    moves on every change to the keys, the requests or the switch (Settings > API keys polls it).
#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace fw {
namespace keys {
constexpr int MAX_KEYS = 16, MAX_WAITING = 4, NAME_LEN = 60;
constexpr uint32_t ASK_MS = 120000;        // no tap in two minutes = Deny
constexpr uint32_t KEEP_MS = 600000;       // an answered request is kept this long for its poll
constexpr long long DROP_S = 90LL * 86400;  // unused this long = dropped
constexpr long long IDLE_S = 30LL * 86400;  // a 30d key: ends after this long unused
constexpr long long USED_STORE_S = 3600;    // last used is stored at most this often per key
struct Key { std::string id, name, hash; long long made = 0, used = 0, ends = 0, idle = 0; };  // ends 0 = until revoked; idle > 0 = ends moves to used + idle
enum Kind : uint8_t { SCREENS, SETTING, HA_LINK, N_KINDS };

bool on();  // the switch
// one /fw/keys route (false = not one): method GET | POST, path without the query, editor = a same-origin request
bool http(const char *method, const std::string &path, const std::string &body, const std::string &auth, bool editor, int *code, std::string *out);
// the write gate: true = go on (*who = the key's id, "" for the editor); false = *code + *out the refusal
bool allow(const std::string &auth, bool editor, std::string *who, int *code, std::string *out);
bool change(const std::string &who, Kind kind, const std::string &what, const std::function<bool()> &apply);
// the words for a change: route "/fw/spec" (body {kind, spec | id}), "/fw/settings" ({values}), "/fw/act" (ha.connect)
std::string describe(const std::string &route, const std::string &body, Kind *kind);
const std::vector<Key> &list();
std::string name_of(const std::string &id);
int revoke(const std::string &id);  // "all" = every key; returns how many went
int waiting();                      // requests nobody has answered yet
std::string ends_words(const Key &k);  // "Ends after 30 days unused", "Ends within a day", "Until revoked"
std::string used_words(const Key &k);  // "Used 2 h ago", "Not used yet"
// storage: the host keeps the JSON (no key in it, hashes only); restore at boot (main loop, no store call)
void on_store(void (*f)(const std::string &json));
void restore(const std::string &json);
std::string json();
void on_random(void (*f)(uint8_t *out, size_t n));
long long now();       // wall-clock s (+ the test skew)
bool clock_set();
uint32_t rev();
// tests (fw_test "keys"): move the wall clock, everything gone (no store call)
void skew(long long s);
void reset();
std::string sha256(const std::string &s);  // hex
}  // namespace keys
}  // namespace fw
