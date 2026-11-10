// The panel's own Home Assistant link (core/device.h "ha_link"): HA's address + token, and the entity lists the
// pickers offer, read from GET <url>/api/states without ever holding the reply whole. Same file on the panels and in
// WASM; the transport (an HTTP GET) is the platform's (firmware: esp_http_client in a task; WASM: the host's fetch).
// The token is never logged and never shown back (token_hint = the last 4 characters).
// Scan: a streaming reader of the /api/states array. It keeps a stack of container kinds (32 deep), whether an
// object expects a key, the key at depth 2 (an entity's field) and depth 3 (an attribute), and three strings capped
// at CAP bytes (entity_id, attributes.friendly_name, attributes.icon, attributes.device_class); every other value is
// skipped as it streams. At each entity's end, one whose domain is wanted (and, a search, that matches) is kept. Held
// bytes = the capped strings + the kept rows.
#include "device.h"
#include "sax.h"
#include <memory>
#include <cstring>

namespace fw {
namespace dev {
namespace ha_link {
namespace {
constexpr size_t CAP = 160;  // a captured string's most bytes (ids and names are shorter; longer ones are cut)
// what a read keeps: at most PER entities a domain and BUDGET counted bytes in all (the heap adds its own per block; a
// house runs to thousands of sensors: kept whole they ran the 480 panel out of heap mid-read); the rest is counted,
// left_out(domain)
constexpr size_t PER = 500, BUDGET = 96 * 1024;
struct State {
  std::string url, token;
  bool proxy = false;
  void (*store)(const std::string &, const std::string &) = nullptr;
  GetSink get = nullptr;
  uint32_t cur = 0, rev = 1;
  Scan *active = nullptr;
  St st = OFF;
  std::string why;
  size_t peak = 0;
  int count = 0;
  std::map<std::string, std::vector<Entity>> cache;
  std::map<std::string, uint32_t> read_at;  // per cached domain: when it was read (lv_tick)
  std::map<std::string, int> left;         // per cached domain: entities the read did not keep (PER, BUDGET)
  size_t (*largest)() = nullptr;           // on_largest
  std::string index_key, index;            // the editor's index (index_text): its domains, its lines until taken
  int index_left = 0;
  bool index_ready = false;
  std::string found_key;                   // the last search's (domains|dc|q), its matches, how many more
  std::vector<Hit> found;
  int found_more = 0;
  WsSink ws = nullptr;
  uint32_t ws_next = WS_ID;
  struct Call { std::function<void(const char *, size_t)> feed; std::function<void(bool, const std::string &)> end; };
  std::map<uint32_t, Call> calls;  // WS reads waiting for their reply
  uint32_t check = 0;              // the link check in flight (configure), 0 = none
  bool checked = false;
  std::string page;
  std::map<std::string, std::vector<RegRow>> reg;  // per domain, once all three registry reads answered
  std::string reg_busy;                              // the domain being read ("" = none)
  uint32_t reg_failed = 0;                           // when the last registry read failed (0 = never): again after 30 s
};
State &S() { static State s; return s; }
void bump() { S().rev++; }
void put_utf8(std::string &b, uint32_t c) {
  if (c < 0x80) b += (char) c;
  else if (c < 0x800) { b += (char) (0xC0 | (c >> 6)); b += (char) (0x80 | (c & 0x3F)); }
  else if (c < 0x10000) { b += (char) (0xE0 | (c >> 12)); b += (char) (0x80 | ((c >> 6) & 0x3F)); b += (char) (0x80 | (c & 0x3F)); }
  else { b += (char) (0xF0 | (c >> 18)); b += (char) (0x80 | ((c >> 12) & 0x3F)); b += (char) (0x80 | ((c >> 6) & 0x3F)); b += (char) (0x80 | (c & 0x3F)); }
}
}  // namespace

Scan::Scan(const std::string &domains) {
  for (size_t i = 0; i < domains.size();) {
    size_t j = domains.find(',', i);
    if (j == std::string::npos) j = domains.size();
    std::string d = domains.substr(i, j - i);
    while (!d.empty() && d[0] == ' ') d.erase(0, 1);
    while (!d.empty() && d.back() == ' ') d.pop_back();
    if (!d.empty()) { want.push_back(d); out[d]; }
    i = j + 1;
  }
  buf_.reserve(CAP), id_.reserve(CAP), name_.reserve(CAP), icon_.reserve(CAP), dc_.reserve(32);
  held_ = peak_ = 4 * CAP + 2 * 64 + 32;
}
Scan::Scan(const std::string &domains, const Find &f) : Scan(domains) {
  search = true, f_ = f;
  for (char &c : f_.q) if (c >= 'A' && c <= 'Z') c = (char) (c - 'A' + 'a');
}
namespace {
bool holds(const std::string &hay, const std::string &q) {  // ASCII case-insensitive; q is lower case
  if (q.empty()) return true;
  if (hay.size() < q.size()) return false;
  for (size_t i = 0; i + q.size() <= hay.size(); i++) {
    size_t k = 0;
    while (k < q.size()) {
      char c = hay[i + k];
      if (c >= 'A' && c <= 'Z') c = (char) (c - 'A' + 'a');
      if (c != q[k]) break;
      k++;
    }
    if (k == q.size()) return true;
  }
  return false;
}
}  // namespace
bool Scan::match(const std::string &) const {
  if (!f_.dc.empty()) {
    bool in = false;
    for (size_t i = 0; i < f_.dc.size() && !in;) {
      size_t j = f_.dc.find(',', i);
      if (j == std::string::npos) j = f_.dc.size();
      in = f_.dc.compare(i, j - i, dc_) == 0 && j > i;
      i = j + 1;
    }
    if (!in) return false;
  }
  return holds(id_, f_.q) || holds(name_, f_.q);
}
void Scan::add(char c) {
  if (cap_ != NONE && buf_.size() < CAP) buf_ += c;
}
void Scan::str_end() {
  const Cap c = cap_;
  cap_ = NONE;
  if (c == KEY) { (depth_ == 2 ? key2_ : key3_) = buf_.substr(0, 64); return; }
  if (c == ID) id_ = buf_;
  else if (c == NAME) name_ = buf_;
  else if (c == ICON) icon_ = buf_;
  else if (c == DC) dc_ = buf_.substr(0, 32);
  else if (c == ST) st_ = buf_.substr(0, 32);
  else if (c == UNIT) unit_ = buf_.substr(0, 16);
}
void Scan::obj_end() {  // an entity's object closes: keep it when its domain is wanted (and, a search, it matches)
  const size_t dot = id_.find('.');
  if (dot != std::string::npos && dot > 0) {
    const std::string dom = id_.substr(0, dot);
    auto it = out.find(dom);
    const bool ok = (it != out.end() || want.empty()) && (!search || match(dom));
    if (ok && it == out.end()) it = out.emplace(dom, std::vector<Entity>()).first;
    const size_t add = sizeof(Entity) + id_.size() + (name_.empty() ? id_.size() : name_.size()) + icon_.size() + dc_.size();
    if (!ok) ;  // not wanted, or not a match
    else if (index) {  // one line, straight into the one string
      const size_t n = id_.size() + name_.size() + dc_.size() + st_.size() + unit_.size() + 5;
      if (lines.size() + n + 16 > lines.capacity()) more++;  // the string was sized once (index_text): never grown here
      else {
        auto field = [&](const std::string &v, char end) { for (char c : v) lines += (c == '\t' || c == '\n') ? ' ' : c; lines += end; };
        field(id_, '\t'), field(name_.empty() ? id_ : name_, '\t'), field(dc_, '\t'), field(st_, '\t'), field(unit_, '\n');
        peak_ = std::max(peak_, held_ + lines.capacity());
      }
    } else if (search && kept_ >= FIND_MAX) more++;
    else if (search) {  // a match: kept with its value, in Home Assistant's order
      hits.push_back({{id_, name_.empty() ? id_ : name_, icon_, dc_}, st_, unit_});
      kept_++;
      held_ += add + sizeof(Hit) - sizeof(Entity) + st_.size() + unit_.size();
      if (held_ > peak_) peak_ = held_;
    }
    else if (it->second.size() >= PER || held_ + add > BUDGET) left[dom]++;
    else {
      it->second.push_back({id_, name_.empty() ? id_ : name_, icon_, dc_});
      kept_++;
      held_ += add;
      if (held_ > peak_) peak_ = held_;
    }
  }
  id_.clear(), name_.clear(), icon_.clear(), dc_.clear(), st_.clear(), unit_.clear();
}
void Scan::feed(const char *p, size_t n) {
  for (size_t i = 0; i < n && !bad_; i++) {
    const char c = p[i];
    if (in_str_) {
      if (hex_ >= 0) {  // \uXXXX
        const int v = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
        if (v < 0) { bad_ = true; break; }
        cp_ = cp_ * 16 + (uint32_t) v;
        if (++hex_ < 4) continue;
        hex_ = -1;
        if (cp_ >= 0xD800 && cp_ < 0xDC00) { hi_ = cp_; continue; }
        uint32_t u = cp_;
        if (cp_ >= 0xDC00 && cp_ < 0xE000 && hi_) u = 0x10000 + ((hi_ - 0xD800) << 10) + (cp_ - 0xDC00);
        hi_ = 0;
        if (cap_ != NONE && buf_.size() + 4 <= CAP) put_utf8(buf_, u);
        continue;
      }
      if (esc_) {
        esc_ = false;
        if (c == 'u') { hex_ = 0, cp_ = 0; continue; }
        add(c == 'n' ? '\n' : c == 't' ? '\t' : c == 'r' ? '\r' : c == 'b' ? '\b' : c == 'f' ? '\f' : c);
        continue;
      }
      if (c == '\\') { esc_ = true; continue; }
      if (c == '"') { in_str_ = false; str_end(); continue; }
      add(c);
      continue;
    }
    switch (c) {
      case '"': {
        in_str_ = true;
        buf_.clear();
        const bool obj = depth_ > 0 && kind_[depth_ - 1] == '{';
        if (obj && expect_key_[depth_ - 1]) cap_ = depth_ == 2 || (depth_ == 3 && attrs_) ? KEY : NONE;
        else if (depth_ == 2 && obj && key2_ == "entity_id") cap_ = ID;
        else if (depth_ == 2 && obj && key2_ == "state" && (search || index)) cap_ = ST;
        else if (depth_ == 3 && attrs_ && key3_ == "friendly_name") cap_ = NAME;
        else if (depth_ == 3 && attrs_ && key3_ == "icon") cap_ = ICON;
        else if (depth_ == 3 && attrs_ && key3_ == "device_class") cap_ = DC;
        else if (depth_ == 3 && attrs_ && key3_ == "unit_of_measurement" && (search || index)) cap_ = UNIT;
        else cap_ = NONE;
        break;
      }
      case '{':
      case '[':
        if (depth_ >= DEPTH) { bad_ = true; break; }
        if (depth_ == 2 && c == '{' && key2_ == "attributes") attrs_ = true, key3_.clear();
        if (depth_ == 1 && c == '{') key2_.clear(), id_.clear(), name_.clear(), icon_.clear(), dc_.clear(), st_.clear(), unit_.clear();
        kind_[depth_] = c;
        expect_key_[depth_] = c == '{';
        depth_++;
        break;
      case '}':
      case ']':
        if (depth_ <= 0 || kind_[depth_ - 1] != (c == '}' ? '{' : '[')) { bad_ = true; break; }
        depth_--;
        if (depth_ == 2 && attrs_) attrs_ = false;
        if (depth_ == 1 && c == '}') obj_end();
        if (depth_ == 2) key2_.clear();
        break;
      case ':':
        if (depth_ > 0) expect_key_[depth_ - 1] = false;
        break;
      case ',':
        if (depth_ > 0 && kind_[depth_ - 1] == '{') expect_key_[depth_ - 1] = true;
        break;
      default:
        break;  // numbers, true / false / null, white space: skipped
    }
  }
}

void on_store(void (*f)(const std::string &, const std::string &)) { S().store = f; }
void restore(const std::string &url, const std::string &token) {
  S().url = url, S().token = token;
  S().st = ready() ? IDLE : OFF;
  bump();
}
// a link just set up: the units from Home Assistant's country (GET /api/config "country": GB, US, LR, MM drive in
// miles = miles and mph, else km and km/h). Once per link, so a unit chosen afterwards stays; a reply without a
// country changes nothing.
void units_from_config() {
  auto *buf = new std::string;
  get("config", [buf](const char *p, size_t n) { if (buf->size() < 8192) buf->append(p, std::min(n, 8192 - buf->size())); },
      [buf](bool ok, const std::string &) {
        ArduinoJson::JsonDocument d, f;
        f["country"] = true;
        std::string c;
        if (ok && !deserializeJson(d, *buf, ArduinoJson::DeserializationOption::Filter(f))) c = d["country"] | "";
        delete buf;
        if (c.size() != 2) return;
        const bool miles = c == "GB" || c == "US" || c == "LR" || c == "MM";
        fw::dev::set("distanceUnit", miles ? "mi" : "km", fw::dev::HOST);
        fw::dev::set("speedUnit", miles ? "mph" : "kmh", fw::dev::HOST);
      });
}
void configure(const std::string &url, const std::string &token) {
  State &s = S();
  if (url != s.url) s.cache.clear(), s.read_at.clear(), s.reg.clear(), s.count = 0;
  s.url = url;
  while (!s.url.empty() && s.url.back() == '/') s.url.pop_back();
  if (!token.empty()) s.token = token;
  if (s.store) s.store(s.url, s.token);
  if (s.st != LOADING) s.st = ready() ? IDLE : OFF;
  s.why.clear();
  s.checked = false, s.check = 0;
  // the check: GET <url>/api/ (HA answers 200 with the token, 401 without); a newer configure supersedes it
  if (s.st == IDLE && (s.check = get("", nullptr, nullptr))) {
    const uint32_t id = s.check;
    s.calls[id].end = [id](bool ok, const std::string &why) {
      State &s = S();
      if (id != s.check) return;
      s.check = 0, s.checked = ok;
      if (s.st == CHECKING) s.st = ok ? (s.count ? OK : IDLE) : FAIL;
      if (!ok) s.why = why;
      if (ok) units_from_config();
      bump();
    };
    s.st = CHECKING;
  }
  bump();
}
Link kept() { return {S().url, S().token}; }
void put_back(const Link &l) {
  S().token = l.token;
  configure(l.url, "");  // stores it (on_store) and checks it again
}
void set_proxy(bool on) { S().proxy = on; if (S().st == OFF && ready()) S().st = IDLE; bump(); }
const std::string &url() { return S().url; }
bool has_token() { return !S().token.empty(); }
std::string token_hint() {
  const std::string &t = S().token;
  return t.size() < 8 ? std::string(t.empty() ? "" : "set") : "\xE2\x80\xA6" + t.substr(t.size() - 4);
}
bool ready() { return !S().url.empty() && (S().proxy || !S().token.empty()); }
namespace {
// a read of /api/states needs room for the HTTP client, the parse and what it keeps: refused, with a reason, when the
// largest free block is under LOW (the 480 panel with every feed on Home runs close to it)
constexpr size_t LOW = 96 * 1024;
const char *cannot() {
  State &s = S();
  if (!ready()) return "Home Assistant is not set up: its address and a token first";
  if (s.largest && s.largest() < LOW) return "the panel is short of memory: close a screen or two and try again";
  if (s.st == LOADING) return "already reading Home Assistant";
  if (!s.get) return "this build cannot reach Home Assistant";
  return nullptr;
}
std::string find_key(const std::string &domains, const Find &f) {
  std::string q = f.q;
  for (char &c : q) if (c >= 'A' && c <= 'Z') c = (char) (c - 'A' + 'a');
  return domains + '|' + f.dc + '|' + q;
}
}  // namespace
const char *find(const std::string &domains, const Find &f) {
  State &s = S();
  if (const char *no = cannot()) return no;
  delete s.active;
  s.active = new Scan(domains, f);
  s.found.clear(), s.found.shrink_to_fit(), s.found_key = find_key(domains, f), s.found_more = 0;  // never two searches held
  s.st = LOADING;
  s.why.clear();
  bump();
  s.get(++s.cur, s.url + "/api/states", s.token);
  return "";
}
const std::vector<Hit> *found(const std::string &domains, const Find &f, int *more) {
  State &s = S();
  if (s.found_key != find_key(domains, f) || (s.st == LOADING && s.active && s.active->search)) return nullptr;
  if (s.st == FAIL && s.found.empty() && !s.found_more) return nullptr;
  if (more) *more = s.found_more;
  return &s.found;
}
std::string find_json(const std::string &domains, const Find &f) {
  State &s = S();
  auto esc = [](std::string &o, const std::string &v) {
    o += '"';
    for (char c : v) {
      if (c == '"' || c == '\\') o += '\\', o += c;
      else if ((unsigned char) c < 0x20) o += ' ';
      else o += c;
    }
    o += '"';
  };
  int more = 0;
  const std::vector<Hit> *l = found(domains, f, &more);
  std::string o;
  if (!l) {
    if (!ready()) return R"({"state":"off","found":[],"more":0})";
    if (s.st == FAIL && s.found_key == find_key(domains, f)) {
      o = R"({"state":"failed","why":)";
      esc(o, s.why);
      return o + R"(,"found":[],"more":0})";
    }
    if (s.st != LOADING && s.st != CHECKING) {
      const char *no = find(domains, f);
      if (*no) { o = R"({"state":"failed","why":)"; esc(o, no); return o + R"(,"found":[],"more":0})"; }
    }
    return R"({"state":"loading","found":[],"more":0})";
  }
  size_t need = 48;
  for (const auto &x : *l) need += x.e.id.size() + x.e.name.size() + x.e.dc.size() + x.st.size() + x.unit.size() + 48;
  o.reserve(need);
  o = R"({"state":"ok","found":[)";
  for (size_t i = 0; i < l->size(); i++) {
    const Hit &x = (*l)[i];
    if (i) o += ',';
    o += "{\"id\":", esc(o, x.e.id), o += ",\"name\":", esc(o, x.e.name), o += ",\"dc\":", esc(o, x.e.dc);
    o += ",\"st\":", esc(o, x.st), o += ",\"unit\":", esc(o, x.unit), o += '}';
  }
  return o + "],\"more\":" + std::to_string(more) + '}';
}
void index_text(const std::string &domains, std::string &out) {
  State &s = S();
  if (s.index_ready && s.index_key == domains) {
    out.clear();
    out.swap(s.index);  // the browser holds it now: no copy, nothing left here
    out += "#ok\t" + std::to_string(s.index_left) + "\n";
    s.index_ready = false, s.index_key.clear();
    return;
  }
  if (!ready()) { out = "#off\n"; return; }
  if (s.st != LOADING && s.largest && s.largest() < LOW) { out = std::string("#failed\t") + cannot() + "\n"; return; }
  if (s.st == FAIL && s.index_key == domains && !s.index_ready) { out = "#failed\t" + s.why + "\n"; s.index_key.clear(); return; }
  if (s.st != LOADING && s.st != CHECKING && s.index_key != domains) {
    if (!cannot()) {
      delete s.active;
      std::string().swap(s.index);
      // one allocation, before the read, never grown: half the largest free block at most (the 480 panel: a doubling
      // string ran it out of memory mid-read)
      const size_t room = s.largest ? s.largest() / 2 : INDEX_MAX;
      s.active = new Scan(domains);
      s.active->index = true;
      s.active->lines.reserve(std::min(INDEX_MAX, room));
      s.index_key = domains, s.index_ready = false, s.index_left = 0;
      s.st = LOADING;
      s.why.clear();
      bump();
      s.get(++s.cur, s.url + "/api/states", s.token);
    }
  }
  out = "#loading\n";
}
const char *fetch(const std::string &domains) {
  State &s = S();
  if (const char *no = cannot()) return no;
  delete s.active;
  s.active = new Scan(domains);
  // the lists being read again go now: the old and the new copies are never held at once
  for (const std::string &w : s.active->want) s.count -= s.cache.count(w) ? (int) s.cache[w].size() : 0, s.cache.erase(w), s.left.erase(w);
  if (s.active->want.empty()) s.cache.clear(), s.left.clear(), s.count = 0;
  s.st = LOADING;
  s.why.clear();
  bump();
  s.get(++s.cur, s.url + "/api/states", s.token);
  return "";
}
St status() { return S().st; }
const std::string &why() { return S().why; }
bool checked() { return S().checked; }
void set_page(const std::string &u) { if (u != S().page) S().page = u, bump(); }
const std::string &page() { return S().page; }
namespace {
Pairing (*pair_state)() = nullptr;
bool (*pair_reset)() = nullptr;
}  // namespace
void on_pairing(Pairing (*state)(), bool (*reset)()) { pair_state = state, pair_reset = reset, bump(); }
Pairing pairing() { return pair_state ? pair_state() : NO_PAIRING; }
bool reset_pairing() {
  if (pairing() != PAIRED || !pair_reset || !pair_reset()) return false;
  bump();
  return true;
}
const char *pairing_word(Pairing p) {
  switch (p) {
    case UNPAIRED: return "unpaired";
    case PAIRED: return "paired";
    case FIXED: return "fixed";
    default: return "none";
  }
}
uint32_t rev() { return S().rev; }
size_t peak() { return S().peak; }
int count() { return S().count; }
void forget(const std::string &domain) { State &s = S(); auto it = s.cache.find(domain); if (it == s.cache.end()) return; s.count -= (int) it->second.size(); s.cache.erase(it); s.read_at.erase(domain); s.left.erase(domain); }
int left_out(const std::string &domain) { auto it = S().left.find(domain); return it == S().left.end() ? 0 : it->second; }
const std::vector<Entity> *list(const std::string &domain) {
  auto it = S().cache.find(domain);
  return it == S().cache.end() ? nullptr : &it->second;
}
bool stale(const std::string &domain) {
  auto it = S().read_at.find(domain);
  return it != S().read_at.end() && lv_tick_diff(lv_tick_get(), it->second) > STALE_MS;
}
void on_get(GetSink f) { S().get = f; }
void on_largest(size_t (*f)()) { S().largest = f; }
void on_ws(WsSink f) { S().ws = f; }
uint32_t ws(const std::string &cmd, std::function<void(const char *, size_t)> feed, std::function<void(bool, const std::string &)> end) {
  State &s = S();
  if (!ready() || !s.ws) return 0;
  const uint32_t id = s.ws_next++;
  s.calls[id] = {std::move(feed), std::move(end)};
  s.ws(id, s.url, s.token, cmd);
  return id;
}
uint32_t get(const std::string &path, std::function<void(const char *, size_t)> feed, std::function<void(bool, const std::string &)> end) {
  State &s = S();
  if (!ready() || !s.get) return 0;
  const uint32_t id = s.ws_next++;
  s.calls[id] = {std::move(feed), std::move(end)};
  s.get(id, s.url + "/api/" + path, s.token);
  return id;
}
void chunk(uint32_t id, const char *p, size_t n) {
  State &s = S();
  if (id >= WS_ID) {
    auto it = s.calls.find(id);
    if (it != s.calls.end() && it->second.feed) it->second.feed(p, n);
    return;
  }
  if (id == s.cur && s.active) s.active->feed(p, n);
}
void done(uint32_t id, bool ok, const std::string &why, Scan *parsed) {
  State &s = S();
  if (id >= WS_ID) {
    delete parsed;
    auto it = s.calls.find(id);
    if (it == s.calls.end()) return;
    auto end = std::move(it->second.end);
    s.calls.erase(it);
    if (end) end(ok, ok ? "" : (why.empty() ? "no reply" : why));
    return;
  }
  if (id != s.cur || s.st != LOADING) { delete parsed; return; }
  Scan *sc = parsed ? parsed : s.active;
  if (parsed) delete s.active;
  s.active = nullptr;
  if (ok && sc && !sc->bad() && sc->index) {
    s.index = std::move(sc->lines);
    s.index_left = sc->more;
    s.index_ready = true;
    s.peak = sc->peak();
    s.st = OK;
  } else if (ok && sc && !sc->bad() && sc->search) {
    s.found = std::move(sc->hits);
    s.found_more = sc->more;
    s.peak = sc->peak();
    s.st = OK;
  } else if (ok && sc && !sc->bad()) {
    s.count = 0;
    const uint32_t now = lv_tick_get();
    for (auto &kv : sc->out) { s.count += (int) kv.second.size(); s.cache[kv.first] = std::move(kv.second); s.read_at[kv.first] = now; s.left[kv.first] = sc->left.count(kv.first) ? sc->left[kv.first] : 0; }
    s.peak = sc->peak();
    s.st = OK;
  } else {
    s.st = FAIL;
    s.why = !ok ? (why.empty() ? "no reply" : why) : "the reply was not the entity list";
  }
  delete sc;
  bump();
}
// ---- the registries (reg_fetch): entity -> device -> area, each its own WS read through a Sax
namespace {
struct RegRead {
  std::string domain;
  std::vector<RegRow> rows;
  std::map<std::string, std::string> dev_of, area_of_dev;  // entity -> device, device -> area id
  std::map<std::string, std::pair<std::string, int>> areas; // area id -> name, order
  std::string f_id, f_pl, f_area, f_dev;
  json::Sax sax;
};
std::unique_ptr<RegRead> R;
void reg_end(bool ok);
bool reg_read(const char *cmd, std::function<void(json::Sax &, const std::string &, bool)> leaf, std::function<void(json::Sax &)> close,
              std::function<void()> next) {
  R->sax = json::Sax();
  R->sax.leaf = std::move(leaf), R->sax.close = std::move(close);
  const uint32_t id = ws(std::string("{\"type\":\"") + cmd + "\"}", [](const char *p, size_t n) { if (R) R->sax.feed(p, n); },
                         [next](bool ok, const std::string &) {
                           if (!R) return;
                           if (!ok || R->sax.bad || !R->sax.ok) return reg_end(false);
                           next();
                         });
  return id != 0;
}
void reg_areas() {
  const bool ok = reg_read("config/area_registry/list",
      [](json::Sax &x, const std::string &v, bool) {
        if (x.depth != 3 || x.key(0) != "result") return;
        if (x.key(2) == "area_id") R->f_id = v;
        else if (x.key(2) == "name") R->f_pl = v;
      },
      [](json::Sax &x) {
        if (x.depth != 3) return;
        const int n = (int) R->areas.size();
        if (!R->f_id.empty()) R->areas[R->f_id] = {R->f_pl, n};
        R->f_id.clear(), R->f_pl.clear();
      },
      [] { reg_end(true); });
  if (!ok) reg_end(false);
}
void reg_devices() {
  const bool ok = reg_read("config/device_registry/list",
      [](json::Sax &x, const std::string &v, bool str) {
        if (x.depth != 3 || x.key(0) != "result") return;
        if (x.key(2) == "id") R->f_dev = v;
        else if (x.key(2) == "area_id" && str) R->f_area = v;
      },
      [](json::Sax &x) {
        if (x.depth != 3) return;
        if (!R->f_dev.empty() && !R->f_area.empty()) R->area_of_dev[R->f_dev] = R->f_area;
        R->f_dev.clear(), R->f_area.clear();
      },
      reg_areas);
  if (!ok) reg_end(false);
}
void reg_entities() {
  const bool ok = reg_read("config/entity_registry/list_for_display",
      [](json::Sax &x, const std::string &v, bool str) {
        if (x.depth != 4 || x.key(1) != "entities") return;
        const std::string &k = x.key(3);
        if (k == "ei") R->f_id = v;
        else if (k == "pl") R->f_pl = v;
        else if (k == "ai" && str) R->f_area = v;
        else if (k == "di" && str) R->f_dev = v;
      },
      [](json::Sax &x) {
        if (x.depth != 4) return;
        if (R->f_id.rfind(R->domain + ".", 0) == 0) {
          RegRow r;
          r.id = R->f_id, r.platform = R->f_pl, r.area = R->f_area;  // its own area for now; the device's fills in after
          if (r.area.empty() && !R->f_dev.empty()) R->dev_of[r.id] = R->f_dev;
          R->rows.push_back(std::move(r));
        }
        R->f_id.clear(), R->f_pl.clear(), R->f_area.clear(), R->f_dev.clear();
      },
      reg_devices);
  if (!ok) reg_end(false);
}
void reg_end(bool ok) {
  State &s = S();
  if (ok) {
    for (RegRow &r : R->rows) {
      if (r.area.empty()) {
        auto d = R->dev_of.find(r.id);
        if (d != R->dev_of.end()) { auto a = R->area_of_dev.find(d->second); if (a != R->area_of_dev.end()) r.area = a->second; }
      }
      auto a = R->areas.find(r.area);
      if (a != R->areas.end()) r.area_order = a->second.second, r.area = a->second.first;
      else r.area.clear();
    }
    s.reg[R->domain] = std::move(R->rows);
  } else s.reg_failed = lv_tick_get() | 1;
  s.reg_busy.clear();
  R.reset();
  bump();
}
}  // namespace
void reg_fetch(const std::string &domain, bool again) {
  State &s = S();
  if ((!again && s.reg.count(domain)) || !s.reg_busy.empty() || !ready() || !s.ws) return;
  if (s.reg_failed && lv_tick_diff(lv_tick_get(), s.reg_failed) < 30000) return;  // Home Assistant said no just now
  s.reg_busy = domain;
  R.reset(new RegRead());
  R->domain = domain;
  reg_entities();
}
const std::vector<RegRow> *reg(const std::string &domain) {
  auto it = S().reg.find(domain);
  return it == S().reg.end() ? nullptr : &it->second;
}
}  // namespace ha_link
}  // namespace dev
}  // namespace fw
