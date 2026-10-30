// The panel's own Home Assistant link (core/device.h "ha_link"): HA's address + token, and the entity lists the
// pickers offer, read from GET <url>/api/states without ever holding the reply whole. Same file on the panels and in
// WASM; the transport (an HTTP GET) is the platform's (firmware: esp_http_client in a task; WASM: the host's fetch).
// The token is never logged and never shown back (token_hint = the last 4 characters).
// Scan: a streaming reader of the /api/states array. It keeps a stack of container kinds (32 deep), whether an
// object expects a key, the key at depth 2 (an entity's field) and depth 3 (an attribute), and three strings capped
// at CAP bytes (entity_id, attributes.friendly_name, attributes.icon); every other value is skipped as it streams.
// At each entity's end, one whose domain is wanted is kept. Held bytes = the capped strings + the kept rows.
#include "device.h"
#include <cstring>

namespace fw {
namespace dev {
namespace ha_link {
namespace {
constexpr size_t CAP = 160;  // a captured string's most bytes (ids and names are shorter; longer ones are cut)
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
  buf_.reserve(CAP), id_.reserve(CAP), name_.reserve(CAP), icon_.reserve(CAP);
  held_ = peak_ = 4 * CAP + 2 * 64;
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
}
void Scan::obj_end() {  // an entity's object closes: keep it when its domain is wanted
  const size_t dot = id_.find('.');
  if (dot != std::string::npos && dot > 0) {
    const std::string dom = id_.substr(0, dot);
    auto it = out.find(dom);
    if (it == out.end() && want.empty()) it = out.emplace(dom, std::vector<Entity>()).first;
    if (it != out.end()) {
      it->second.push_back({id_, name_.empty() ? id_ : name_, icon_});
      held_ += sizeof(Entity) + id_.size() + (name_.empty() ? id_.size() : name_.size()) + icon_.size();
      if (held_ > peak_) peak_ = held_;
    }
  }
  id_.clear(), name_.clear(), icon_.clear();
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
        else if (depth_ == 3 && attrs_ && key3_ == "friendly_name") cap_ = NAME;
        else if (depth_ == 3 && attrs_ && key3_ == "icon") cap_ = ICON;
        else cap_ = NONE;
        break;
      }
      case '{':
      case '[':
        if (depth_ >= DEPTH) { bad_ = true; break; }
        if (depth_ == 2 && c == '{' && key2_ == "attributes") attrs_ = true, key3_.clear();
        if (depth_ == 1 && c == '{') key2_.clear(), id_.clear(), name_.clear(), icon_.clear();
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
void configure(const std::string &url, const std::string &token) {
  State &s = S();
  if (url != s.url) s.cache.clear(), s.count = 0;
  s.url = url;
  while (!s.url.empty() && s.url.back() == '/') s.url.pop_back();
  if (!token.empty()) s.token = token;
  if (s.store) s.store(s.url, s.token);
  if (s.st != LOADING) s.st = ready() ? IDLE : OFF;
  s.why.clear();
  bump();
}
void set_proxy(bool on) { S().proxy = on; if (S().st == OFF && ready()) S().st = IDLE; bump(); }
const std::string &url() { return S().url; }
bool has_token() { return !S().token.empty(); }
std::string token_hint() {
  const std::string &t = S().token;
  return t.size() < 8 ? std::string(t.empty() ? "" : "set") : "\xE2\x80\xA6" + t.substr(t.size() - 4);
}
bool ready() { return !S().url.empty() && (S().proxy || !S().token.empty()); }
const char *fetch(const std::string &domains) {
  State &s = S();
  if (!ready()) return "Home Assistant is not set up: its address and a token first";
  if (s.st == LOADING) return "already reading Home Assistant";
  if (!s.get) return "this build cannot reach Home Assistant";
  delete s.active;
  s.active = new Scan(domains);
  s.st = LOADING;
  s.why.clear();
  bump();
  s.get(++s.cur, s.url + "/api/states", s.token);
  return "";
}
St status() { return S().st; }
const std::string &why() { return S().why; }
uint32_t rev() { return S().rev; }
size_t peak() { return S().peak; }
int count() { return S().count; }
const std::vector<Entity> *list(const std::string &domain) {
  auto it = S().cache.find(domain);
  return it == S().cache.end() ? nullptr : &it->second;
}
void on_get(GetSink f) { S().get = f; }
void chunk(uint32_t id, const char *p, size_t n) {
  State &s = S();
  if (id == s.cur && s.active) s.active->feed(p, n);
}
void done(uint32_t id, bool ok, const std::string &why, Scan *parsed) {
  State &s = S();
  if (id != s.cur || s.st != LOADING) { delete parsed; return; }
  Scan *sc = parsed ? parsed : s.active;
  if (parsed) delete s.active;
  s.active = nullptr;
  if (ok && sc && !sc->bad()) {
    s.count = 0;
    for (auto &kv : sc->out) { s.count += (int) kv.second.size(); s.cache[kv.first] = std::move(kv.second); }
    s.peak = sc->peak();
    s.st = OK;
  } else {
    s.st = FAIL;
    s.why = !ok ? (why.empty() ? "no reply" : why) : "the reply was not the entity list";
  }
  delete sc;
  bump();
}
}  // namespace ha_link
}  // namespace dev
}  // namespace fw
