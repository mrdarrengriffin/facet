// API keys: see keys.h. The store, the requests and the glass's "Allow <name>?" sheet, the gate, the one previous
// state per change kind and its Undo toast. Main loop only (the firmware runs every call in its LVGL loop job).
#include <algorithm>
#include <cstring>
#include <ctime>
#include <map>
#include <random>
#include "keys.h"
#include "fw.h"
#include "fx.h"
#include "device.h"
#include "sheet.h"
#include "spec.h"
#include "settings_gen.h"

namespace fw {
namespace keys {
namespace {
using Doc = ArduinoJson::JsonDocument;

// ---- SHA-256 (FIPS 180-4), for the stored hashes: the key itself is never kept
std::string sha(const std::string &m) {
  static const uint32_t K[64] = {
      0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
      0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
      0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
      0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
      0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
      0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
  uint32_t h[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
  std::string b = m;
  const uint64_t bits = (uint64_t) m.size() * 8;
  b += (char) 0x80;
  while (b.size() % 64 != 56) b += (char) 0;
  for (int i = 7; i >= 0; i--) b += (char) ((bits >> (i * 8)) & 0xff);
  auto rr = [](uint32_t x, int n) { return (x >> n) | (x << (32 - n)); };
  for (size_t o = 0; o < b.size(); o += 64) {
    uint32_t w[64];
    for (int i = 0; i < 16; i++)
      w[i] = (uint32_t) (uint8_t) b[o + 4 * i] << 24 | (uint32_t) (uint8_t) b[o + 4 * i + 1] << 16 | (uint32_t) (uint8_t) b[o + 4 * i + 2] << 8 | (uint8_t) b[o + 4 * i + 3];
    for (int i = 16; i < 64; i++) {
      const uint32_t s0 = rr(w[i - 15], 7) ^ rr(w[i - 15], 18) ^ (w[i - 15] >> 3), s1 = rr(w[i - 2], 17) ^ rr(w[i - 2], 19) ^ (w[i - 2] >> 10);
      w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    uint32_t a = h[0], bb = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
    for (int i = 0; i < 64; i++) {
      const uint32_t t1 = hh + (rr(e, 6) ^ rr(e, 11) ^ rr(e, 25)) + ((e & f) ^ (~e & g)) + K[i] + w[i];
      const uint32_t t2 = (rr(a, 2) ^ rr(a, 13) ^ rr(a, 22)) + ((a & bb) ^ (a & c) ^ (bb & c));
      hh = g, g = f, f = e, e = d + t1, d = c, c = bb, bb = a, a = t1 + t2;
    }
    h[0] += a, h[1] += bb, h[2] += c, h[3] += d, h[4] += e, h[5] += f, h[6] += g, h[7] += hh;
  }
  char out[65];
  for (int i = 0; i < 8; i++) snprintf(out + 8 * i, 9, "%08x", (unsigned) h[i]);
  return out;
}

// ---- state
enum St : uint8_t { WAITING, ALLOWED, DENIED, TAKEN };
struct Req {
  std::string id, name, key, why;  // key: the new key until its poll takes it (then cleared)
  long long life = IDLE_S;          // 0 = until revoked; IDLE_S = the inactivity kind; 86400 = a fixed day
  uint32_t asked = 0, answered = 0;
  St st = WAITING;
};
struct Snap {
  uint32_t gen = 0;  // 0 = nothing to undo
  std::map<std::string, std::string> over;
  std::string deck, settings, what;
  dev::ha_link::Link link;
};
struct State {
  std::vector<Key> keys;
  std::vector<Req> reqs;
  std::map<std::string, long long> stored_used;  // the used time last stored per key (USED_STORE_S)
  void (*store)(const std::string &) = nullptr;
  void (*random)(uint8_t *, size_t) = nullptr;
  long long skew = 0;
  uint32_t rev = 1, gen = 0;
  Snap undo[N_KINDS];
  lv_obj_t *sheet = nullptr;  // the "Allow <name>?" sheet (the display's top layer), its request
  std::string sheet_req;
  bool sheet_done = false;
  lv_timer_t *tick = nullptr;
};
State &S() { static State s; return s; }
void bump() { S().rev++; }

std::string hex(size_t bytes) {
  std::vector<uint8_t> b(bytes);
  if (S().random) S().random(b.data(), bytes);
  else { static std::random_device rd; for (auto &x : b) x = (uint8_t) rd(); }
  static const char *H = "0123456789abcdef";
  std::string o;
  for (uint8_t x : b) o += H[x >> 4], o += H[x & 15];
  return o;
}
bool same(const std::string &a, const std::string &b) {  // constant time
  if (a.size() != b.size() || a.empty()) return false;
  uint8_t d = 0;
  for (size_t i = 0; i < a.size(); i++) d |= a[i] ^ b[i];
  return !d;
}
void save() {
  if (!S().store) return;
  for (const Key &k : S().keys) S().stored_used[k.id] = k.used;
  S().store(json());
}
std::string answer(Doc &d) { std::string s; serializeJson(d, s); return s; }
std::string line(const char *agents, const std::string &error) {
  Doc d;
  d["ok"] = false;
  if (agents) d["agents"] = agents;
  d["error"] = error;
  return answer(d);
}
const char *OFF_LINE = "API access is off on this panel. Ask its owner to turn on Allow API access (in the panel's Settings, its editor, or Home Assistant).";
const char *ASK_LINE = "Send Authorization: Bearer <key>. To get a key: POST /fw/keys/request {\"name\": \"<who you are>\"}, then the owner taps Allow on the panel. Guide: /ai";
const char *BAD_LINE = "That key does not work on this panel: it was revoked, it ended, or it was not used for 90 days. Ask for a new one: POST /fw/keys/request.";

// keys that ended or went unused for DROP_S go (only with the clock set); requests: no tap in ASK_MS = Deny, the
// answered ones kept KEEP_MS for their poll, the switch off = every waiting one denied
void close_sheet();
void sweep() {
  State &s = S();
  if (clock_set()) {
    const long long t = now();
    const size_t n = s.keys.size();
    s.keys.erase(std::remove_if(s.keys.begin(), s.keys.end(), [t](const Key &k) { return (k.ends && t >= k.ends) || t - std::max(k.used, k.made) >= DROP_S; }), s.keys.end());
    if (s.keys.size() != n) { save(); bump(); }
  }
  const uint32_t ms = lv_tick_get();
  for (Req &r : s.reqs) {
    if (r.st != WAITING) continue;
    if (!on()) r.st = DENIED, r.why = "API access was turned off on the panel.";
    else if (lv_tick_elaps(r.asked) >= ASK_MS) r.st = DENIED, r.why = "No one tapped Allow on the panel within two minutes.";
    else continue;
    r.answered = ms;
    bump();
    if (r.id == s.sheet_req) close_sheet();
  }
  s.reqs.erase(std::remove_if(s.reqs.begin(), s.reqs.end(), [](const Req &r) { return r.st != WAITING && lv_tick_elaps(r.answered) >= KEEP_MS; }), s.reqs.end());
}
Req *req(const std::string &id) {
  for (Req &r : S().reqs) if (r.id == id) return &r;
  return nullptr;
}
std::string life_words(long long life) { return life == 0 ? "until you revoke it" : life == IDLE_S ? "until it goes 30 days unused" : "for 1 day"; }

// ---- the glass: "Allow <name>?" over whatever shows (a Bottom sheet on the top layer; Close / scrim / key = Deny)
void ask_next();
void decide(const std::string &id, bool allow) {
  State &s = S();
  Req *r = req(id);
  if (!r || r->st != WAITING) return;
  r->answered = lv_tick_get();
  if (!allow) { r->st = DENIED, r->why = r->name + " was not allowed on the panel."; bump(); return; }
  if ((int) s.keys.size() >= MAX_KEYS) { r->st = DENIED, r->why = "This panel has " + std::to_string(MAX_KEYS) + " keys already. Revoke one in Settings > API keys, then ask again."; bump(); return; }
  Key k;
  r->key = "fk-" + hex(16);
  k.id = hex(4), k.name = r->name, k.hash = sha(r->key);
  k.made = now(), k.ends = r->life ? k.made + r->life : 0;
  k.idle = r->life == IDLE_S ? IDLE_S : 0;  // the inactivity kind: every use moves ends on (allow())
  s.keys.push_back(k);
  r->st = ALLOWED;
  save();
  bump();
}
void close_sheet() {
  State &s = S();
  if (!s.sheet || s.sheet_done) return;
  s.sheet_done = true;
  sheet::close(s.sheet);
  lv_obj_t *ov = s.sheet;
  // gone completely once the close has played (C4: back to the baseline), then the next request's turn
  lv_timer_t *t = lv_timer_create([](lv_timer_t *t) {
    auto *ov = (lv_obj_t *) lv_timer_get_user_data(t);
    lv_timer_delete(t);
    State &s = S();
    if (s.sheet == ov) { s.sheet = nullptr; s.sheet_req.clear(); s.sheet_done = false; }
    lv_obj_delete(ov);
    ask_next();
  }, (uint32_t) sheet::open_close_ms() + 40, ov);
  lv_timer_set_repeat_count(t, 1);
}
lv_obj_t *part(const char *id, lv_obj_t *parent, Doc &p) {
  const Component *c = find(id);
  if (!c) return nullptr;
  const uint32_t n = lv_obj_get_child_count(parent);
  c->build(parent, p.as<Props>());
  return lv_obj_get_child_count(parent) > n ? lv_obj_get_child(parent, -1) : nullptr;
}
void fill_sheet(lv_obj_t *ov, lv_obj_t *list) {
  Req *r = req(S().sheet_req);
  if (!r) return;
  Doc t;
  const std::string q = "Allow " + r->name + "?";
  t["text"] = q.c_str(), t["style"] = "headline", t["lines"] = 2;
  part("text", list, t);
  Doc b;
  const std::string about = "It can change this panel's screens and settings, as the editor can, " + life_words(r->life) + ".";
  b["text"] = about.c_str(), b["style"] = "body", b["tone"] = "dim", b["lines"] = 3;
  part("text", list, b);
  lv_obj_t *row = fx::box(list, "row");
  fx::use(row, "fill-w");
  for (int i = 0; i < 2; i++) {
    lv_obj_t *cell = fx::box(row, "col");
    fx::use(cell, "cell");
    Doc bp;
    bp["kind"] = i ? "primary" : "secondary", bp["label"] = i ? "Allow" : "Deny", bp["width"] = "fill";
    lv_obj_t *btn = part("button", cell, bp);
    if (!btn) continue;
    lv_obj_add_event_cb(btn, [](lv_event_t *e) {  // after the Button's own tap (event + sound)
      if (lv_obj_has_state((lv_obj_t *) lv_event_get_current_target(e), st::OFF) || S().sheet_done) return;
      decide(S().sheet_req, lv_event_get_user_data(e) != nullptr);
      close_sheet();
    }, LV_EVENT_CLICKED, i ? (void *) 1 : nullptr);
  }
  // the panel's height = what is in it (as the sheet's spec slot does: children + the gaps between)
  lv_obj_update_layout(list);
  int h = 0;
  const uint32_t n = lv_obj_get_child_count(list);
  for (uint32_t i = 0; i < n; i++) h += lv_obj_get_height(lv_obj_get_child(list, (int32_t) i));
  h += (int) (n > 1 ? n - 1 : 0) * lv_obj_get_style_pad_row(list, LV_PART_MAIN);
  sheet::fit(ov, 1, std::max(h, 1));
}
void ask_next() {
  State &s = S();
  if (s.sheet) return;
  Req *r = nullptr;
  for (Req &x : s.reqs) if (x.st == WAITING) { r = &x; break; }
  if (!r) return;
  lv_obj_t *root = dev::first_root();
  lv_display_t *d = root ? lv_obj_get_display(root) : lv_display_get_default();
  lv_obj_t *top = d ? lv_display_get_layer_top(d) : nullptr;
  if (!top) return;
  Doc p;
  p["overlay"] = true, p["open"] = "closed", p["title"] = "api key", p["rows"] = 1, p["row_h"] = 1;
  lv_obj_t *ov = part("bottom_sheet", top, p);
  if (!ov) return;
  s.sheet = ov, s.sheet_req = r->id, s.sheet_done = false;
  lv_obj_add_event_cb(ov, [](lv_event_t *e) { fill_sheet((lv_obj_t *) lv_event_get_current_target(e), (lv_obj_t *) lv_event_get_param(e)); }, LV_EVENT_REFRESH, nullptr);
  // Close, the scrim or the panel's key: Deny (the sheet closes itself)
  listen(ov, [](lv_obj_t *, const char *name, const char *, void *) {
    State &s = S();
    if (strcmp(name, "close") || s.sheet_done) return;
    decide(s.sheet_req, false);
    close_sheet();
  });
  Doc o;
  o.set(p);
  o["open"] = "open";
  update_child(find("bottom_sheet"), ov, o.as<Props>());  // fills (LV_EVENT_REFRESH) and opens
}
void ensure_tick() {
  State &s = S();
  if (s.tick) return;
  s.tick = lv_timer_create([](lv_timer_t *t) {
    sweep();
    if (S().reqs.empty() && !S().sheet) { lv_timer_delete(t); S().tick = nullptr; }
  }, 250, nullptr);
}

// ---- Undo: one previous state per kind
void keep(Kind k, Snap &s) {
  if (k == SCREENS) { s.over = spec::S().over; s.deck = spec::S().deck_over; }
  else if (k == SETTING) dev::settings_json(s.settings);
  else s.link = dev::ha_link::kept();
}
bool moved(Kind k, const Snap &s) {
  if (k == SCREENS) return s.over != spec::S().over || s.deck != spec::S().deck_over;
  if (k == SETTING) { std::string now_; dev::settings_json(now_); return now_ != s.settings; }
  const dev::ha_link::Link l = dev::ha_link::kept();
  return l.url != s.link.url || l.token != s.link.token;
}
void put_back(Kind k, const Snap &s) {
  if (k == SETTING) { dev::restore(s.settings.c_str(), dev::HOST); return; }
  if (k == HA_LINK) { dev::ha_link::put_back(s.link); return; }
  std::vector<std::string> gone;
  for (const auto &kv : spec::S().over) if (!s.over.count(kv.first)) gone.push_back(kv.first);
  for (const std::string &id : gone) { Doc o; spec::revert(id, o.to<ArduinoJson::JsonObject>()); }
  for (const auto &kv : s.over) {
    auto it = spec::S().over.find(kv.first);
    if (it != spec::S().over.end() && it->second == kv.second) continue;
    Doc o;
    spec::apply("screen_dev", kv.second, nullptr, o.to<ArduinoJson::JsonObject>());
  }
  if (s.deck != spec::S().deck_over) {
    Doc o;
    if (s.deck.empty()) spec::revert("deck", o.to<ArduinoJson::JsonObject>());
    else spec::apply("deck", s.deck, nullptr, o.to<ArduinoJson::JsonObject>());
  }
}
void undo(Kind k, uint32_t gen) {
  Snap &s = S().undo[k];
  Doc t;
  t["icon"] = "agent";
  if (s.gen != gen || !gen) {
    t["tone"] = "warn", t["title"] = "Can't undo", t["body"] = "It has changed again since.";
  } else {
    put_back(k, s);
    t["tone"] = "ok", t["title"] = "Undone", t["body"] = s.what;
    s = Snap{};
  }
  dev::toast(t.as<Props>());
}
}  // namespace

std::string sha256(const std::string &s) { return sha(s); }
bool on() {
  const char *v = dev::setting("aiAgents");
  return v && !strcmp(v, "true");
}
long long now() { return (long long) time(nullptr) + S().skew; }
bool clock_set() { return now() > 1700000000LL; }  // 2023-11: before that the clock was never set
uint32_t rev() { return S().rev + (on() ? 0x80000000u : 0); }
const std::vector<Key> &list() { return S().keys; }
std::string name_of(const std::string &id) {
  for (const Key &k : S().keys) if (k.id == id) return k.name;
  return "";
}
int waiting() {
  int n = 0;
  for (const Req &r : S().reqs) n += r.st == WAITING;
  return n;
}
int revoke(const std::string &id) {
  State &s = S();
  const size_t n = s.keys.size();
  s.keys.erase(std::remove_if(s.keys.begin(), s.keys.end(), [&](const Key &k) { return id == "all" || k.id == id; }), s.keys.end());
  const int gone = (int) (n - s.keys.size());
  if (gone) { save(); bump(); }
  return gone;
}
std::string ends_words(const Key &k) {
  if (!k.ends) return "Until revoked";
  if (k.idle) return "Ends after " + std::to_string(k.idle / 86400) + " days unused";
  const long long left = k.ends - now();
  if (left <= 86400) return "Ends within a day";
  return "Ends in " + std::to_string((left + 86399) / 86400) + " days";
}
std::string used_words(const Key &k) {
  if (!k.used) return "Not used yet";
  const long long a = std::max(0LL, now() - k.used);
  if (a < 60) return "Used just now";
  if (a < 3600) return "Used " + std::to_string(a / 60) + " min ago";
  if (a < 86400) return "Used " + std::to_string(a / 3600) + " h ago";
  const long long d = a / 86400;
  return d == 1 ? "Used yesterday" : "Used " + std::to_string(d) + " days ago";
}
void on_store(void (*f)(const std::string &)) { S().store = f; }
void on_random(void (*f)(uint8_t *, size_t)) { S().random = f; }
std::string json() {
  Doc d;
  d["v"] = 1;
  ArduinoJson::JsonArray a = d["keys"].to<ArduinoJson::JsonArray>();
  for (const Key &k : S().keys) {
    ArduinoJson::JsonObject o = a.add<ArduinoJson::JsonObject>();
    o["id"] = k.id, o["name"] = k.name, o["hash"] = k.hash, o["made"] = k.made, o["used"] = k.used, o["ends"] = k.ends, o["idle"] = k.idle;
  }
  return answer(d);
}
void restore(const std::string &js) {
  State &s = S();
  s.keys.clear();
  Doc d;
  if (!deserializeJson(d, js))
    for (ArduinoJson::JsonObjectConst o : d["keys"].as<ArduinoJson::JsonArrayConst>()) {
      Key k;
      k.id = o["id"] | "", k.name = o["name"] | "", k.hash = o["hash"] | "";
      k.made = o["made"] | 0LL, k.used = o["used"] | 0LL, k.ends = o["ends"] | 0LL, k.idle = o["idle"] | 0LL;
      if (k.id.size() == 8 && k.hash.size() == 64 && (int) s.keys.size() < MAX_KEYS) s.keys.push_back(k), s.stored_used[k.id] = k.used;
    }
  bump();
}
void skew(long long sec) { S().skew += sec; }
void reset() {
  State &s = S();
  if (s.sheet) lv_obj_delete(s.sheet);
  if (s.tick) lv_timer_delete(s.tick);
  void (*st)(const std::string &) = s.store;
  void (*rn)(uint8_t *, size_t) = s.random;
  s = State{};
  s.store = st, s.random = rn;
}

bool allow(const std::string &auth, bool editor, std::string *who, int *code, std::string *out) {
  who->clear();
  if (editor) return true;
  if (!on()) { *code = 403; *out = line("off", OFF_LINE); return false; }
  std::string k;
  if (auth.rfind("Bearer ", 0) == 0) k = auth.substr(7);
  while (!k.empty() && k.back() == ' ') k.pop_back();
  if (k.empty()) { *code = 401; *out = line("key", ASK_LINE); return false; }
  sweep();
  const std::string h = sha(k);
  for (Key &x : S().keys)
    if (same(x.hash, h)) {
      *who = x.id;
      if (clock_set()) {
        x.used = now();
        if (x.idle) x.ends = x.used + x.idle;  // 30 days of inactivity from this use
        if (x.used - S().stored_used[x.id] >= USED_STORE_S) save();
      }
      bump();
      return true;
    }
  *code = 401;
  *out = line("key", BAD_LINE);
  return false;
}

bool change(const std::string &who, Kind kind, const std::string &what, const std::function<bool()> &apply) {
  if (who.empty() || kind >= N_KINDS) return apply();  // the editor, or not a change a key makes: no toast
  Snap s;
  keep(kind, s);
  if (!apply()) return false;
  if (!moved(kind, s)) return true;  // the same as before: nothing to undo or say
  s.what = what;
  s.gen = ++S().gen;
  S().undo[kind] = s;
  Doc t;
  const std::string by = "By " + name_of(who);
  t["tone"] = "info", t["icon"] = "agent", t["title"] = what, t["body"] = by, t["action"] = "Undo", t["life"] = "urgent";
  const uint32_t gen = s.gen;
  dev::toast(t.as<Props>(), [kind, gen] { undo(kind, gen); });
  return true;
}

std::string describe(const std::string &route, const std::string &body, Kind *kind) {
  *kind = N_KINDS;
  Doc d;
  if (deserializeJson(d, body)) return "";
  if (route == "/fw/spec") {
    const std::string k = d["kind"] | "screen";
    if (k == "theme") {  // the Theme setting moves to Sent: Undo puts the look back
      *kind = SETTING;
      const std::string l = d["spec"]["label"] | (d["spec"]["id"] | "");
      return l.empty() ? "Theme sent" : "Theme " + l + " sent";
    }
    if (k != "screen" && k != "deck" && k != "revert") return "";
    *kind = SCREENS;
    if (k == "deck") return "Page list changed";
    if (k == "revert") {
      const std::string id = d["id"] | "all";
      return id == "all" ? "Every sent screen removed" : id == "deck" ? "Page list put back" : "Screen " + id + " put back";
    }
    const std::string n = d["spec"]["name"] | (d["spec"]["id"] | "");
    return "Screen " + n + " sent";
  }
  if (route == "/fw/settings") {
    ArduinoJson::JsonObjectConst v = d["values"].is<ArduinoJson::JsonObjectConst>() ? d["values"].as<ArduinoJson::JsonObjectConst>() : d.as<ArduinoJson::JsonObjectConst>();
    *kind = SETTING;
    if (v.size() != 1) return std::to_string(v.size()) + " settings changed";
    const ArduinoJson::JsonPairConst kv = *v.begin();
    std::string val;
    if (kv.value().is<bool>()) val = kv.value().as<bool>() ? "on" : "off";
    else if (kv.value().is<const char *>()) val = dev::option_label(kv.key().c_str(), kv.value().as<const char *>());
    else serializeJson(kv.value(), val);
    for (int i = 0; i < set::N_SETTINGS; i++)
      if (!strcmp(set::SETTINGS[i].id, kv.key().c_str())) return std::string(set::SETTINGS[i].label) + " set to " + val;
    return std::string(kv.key().c_str()) + " set to " + val;
  }
  if (route == "/fw/act" && !strcmp(d["do"] | "", "ha.connect")) { *kind = HA_LINK; return "Home Assistant link changed"; }
  return "";
}

bool http(const char *method, const std::string &path, const std::string &body, const std::string &auth, bool editor, int *code, std::string *out) {
  if (path != "/fw/keys" && path.rfind("/fw/keys/", 0) != 0) return false;
  State &s = S();
  sweep();
  const bool post = !strcmp(method, "POST");
  Doc in, o;
  if (post && (body.empty() || deserializeJson(in, body) || !in.is<ArduinoJson::JsonObject>())) { *code = 400; *out = line(nullptr, "The body is not a JSON object."); return true; }
  *code = 200;
  if (path == "/fw/keys" && !post) {  // names and times only: never a key or a hash
    o["on"] = on(), o["max"] = MAX_KEYS, o["waiting"] = waiting();
    ArduinoJson::JsonArray a = o["keys"].to<ArduinoJson::JsonArray>();
    for (const Key &k : s.keys) {
      ArduinoJson::JsonObject x = a.add<ArduinoJson::JsonObject>();
      x["id"] = k.id, x["name"] = k.name, x["made"] = k.made, x["used"] = k.used, x["ends"] = k.ends, x["idle"] = k.idle > 0;
      x["used_words"] = used_words(k), x["ends_words"] = ends_words(k);
    }
    *out = answer(o);
  } else if (path == "/fw/keys/request" && post) {
    std::string name;
    for (char c : std::string(in["name"] | "")) if ((unsigned char) c >= 0x20) name += c;
    while (!name.empty() && name.front() == ' ') name.erase(0, 1);
    while (!name.empty() && name.back() == ' ') name.pop_back();
    const std::string life = in["expires"] | "30d";
    if (!on()) { *code = 403; *out = line("off", OFF_LINE); }
    else if (name.empty() || name.size() > (size_t) NAME_LEN) { *code = 400; *out = line(nullptr, "Send your name, up to 60 characters: {\"name\": \"Claude Code on Sam's laptop\"}."); }
    else if (life != "1d" && life != "30d" && life != "never") { *code = 400; *out = line(nullptr, "expires is 1d, 30d or never (30d when left out)."); }
    else if (!clock_set()) { *code = 503; *out = line(nullptr, "The panel's clock is not set yet. Try again in a minute."); }
    else if ((int) s.keys.size() >= MAX_KEYS) { *code = 409; *out = line(nullptr, "This panel has " + std::to_string(MAX_KEYS) + " keys already. Revoke one in Settings > API keys, then ask again."); }
    else if (waiting() >= MAX_WAITING) { *code = 429; *out = line(nullptr, "Too many requests are waiting on the panel. Try again in two minutes."); }
    else {
      Req r;
      r.id = hex(6), r.name = name, r.asked = lv_tick_get();
      r.life = life == "1d" ? 86400LL : life == "never" ? 0LL : IDLE_S;
      s.reqs.push_back(r);
      bump();
      ensure_tick();
      ask_next();
      o["ok"] = true, o["id"] = r.id, o["state"] = "waiting", o["wait_s"] = ASK_MS / 1000;
      o["next"] = "POST /fw/keys/poll {\"id\": \"" + r.id + "\"} every 2 s until the state is not waiting";
      o["say"] = "Tap Allow on the panel's screen to let " + name + " in.";
      *out = answer(o);
    }
  } else if (path == "/fw/keys/poll" && post) {
    Req *r = req(in["id"] | "");
    if (!r) { *code = 404; *out = line(nullptr, "No such request (an answer is kept ten minutes). Ask again: POST /fw/keys/request."); }
    else if (r->st == WAITING) { o["ok"] = true, o["state"] = "waiting", o["left_s"] = (ASK_MS - std::min<uint32_t>(ASK_MS, lv_tick_elaps(r->asked))) / 1000; *out = answer(o); }
    else if (r->st == ALLOWED) {
      o["ok"] = true, o["state"] = "allowed", o["key"] = r->key, o["name"] = r->name, o["expires"] = r->life ? (r->life == IDLE_S ? "30d" : "1d") : "never";
      o["use"] = "Send Authorization: Bearer <key> on every POST. This is the only time the panel shows it.";
      r->key.clear(), r->st = TAKEN;
      *out = answer(o);
    } else if (r->st == TAKEN) { *code = 410; *out = line(nullptr, "That key was handed out already. If it was lost, ask for a new one."); }
    else { o["ok"] = false, o["state"] = "denied", o["error"] = r->why; *out = answer(o); }
  } else if (path == "/fw/keys/revoke" && post) {
    std::string who;
    if (allow(auth, editor, &who, code, out)) {
      const std::string id = in["id"] | "";
      if (id.empty()) { *code = 400; *out = line(nullptr, "Send the key's id, or \"all\": {\"id\": \"all\"}."); }
      else { o["ok"] = true, o["revoked"] = revoke(id); *out = answer(o); }
    }
  } else {
    *code = 404;
    *out = line(nullptr, "Key routes: GET /fw/keys, POST /fw/keys/request {name, expires?}, POST /fw/keys/poll {id}, POST /fw/keys/revoke {id | \"all\"}.");
  }
  return true;
}
}  // namespace keys
}  // namespace fw
