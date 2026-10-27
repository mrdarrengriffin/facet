// font_store.cpp: downloaded theme fonts (core/font_store.h). The check is pure C++ (fuzzed natively); the rest runs
// on the LVGL task: a get() from the apply path starts the read from flash or the fetch, a landed file is checked,
// stored and turned into TinyTTF fonts (one per px), then every look repaints live.
#include "font_store.h"
#include <stdio.h>
#include <string.h>
#include <algorithm>
#include <map>
#include <memory>

namespace fx {
namespace fonts {
namespace {
uint32_t be16(const uint8_t *p) { return (uint32_t) p[0] << 8 | p[1]; }
uint32_t be32(const uint8_t *p) { return (uint32_t) p[0] << 24 | (uint32_t) p[1] << 16 | (uint32_t) p[2] << 8 | p[3]; }
struct Tab { uint32_t off = 0, len = 0; bool ok = false; };
// the glyph of code point c in a cmap subtable (format 4 or 12), 0 = none; every read bounds-checked against [s, e)
uint32_t cmap_glyph(const uint8_t *s, const uint8_t *e, int fmt, uint32_t c) {
  if (fmt == 12) {
    if (e - s < 16) return 0;
    const uint32_t ng = be32(s + 12);
    if (ng > (uint32_t) ((e - s - 16) / 12)) return 0;
    for (uint32_t i = 0; i < ng; i++) {
      const uint8_t *g = s + 16 + 12 * i;
      if (c >= be32(g) && c <= be32(g + 4)) return be32(g + 8) + (c - be32(g));
    }
    return 0;
  }
  if (c > 0xFFFF || e - s < 14) return 0;
  const uint32_t segx2 = be16(s + 6), seg = segx2 / 2;
  if (!seg || 16 + 4 * (size_t) segx2 > (size_t) (e - s)) return 0;
  const uint8_t *ends = s + 14, *starts = ends + segx2 + 2, *delta = starts + segx2, *range = delta + segx2;
  for (uint32_t i = 0; i < seg; i++) {
    if (c > be16(ends + 2 * i)) continue;
    const uint32_t st = be16(starts + 2 * i);
    if (c < st) return 0;
    const uint32_t ro = be16(range + 2 * i), d = be16(delta + 2 * i);
    if (!ro) return (c + d) & 0xFFFF;
    const uint8_t *g = range + 2 * i + ro + 2 * (c - st);
    if (g < s || g + 2 > e) return 0;
    const uint32_t gi = be16(g);
    return gi ? (gi + d) & 0xFFFF : 0;
  }
  return 0;
}
// does a name string say SIL Open Font License (ASCII or UTF-16BE, any case)?
bool says_ofl(const uint8_t *p, size_t n, bool utf16) {
  std::string a;
  for (size_t i = utf16 ? 1 : 0; i < n; i += utf16 ? 2 : 1) a += (char) tolower(p[i]);
  return a.find("open font license") != std::string::npos || a.find("scripts.sil.org/ofl") != std::string::npos ||
         a.find("openfontlicense.org") != std::string::npos;
}
}  // namespace

bool check(const uint8_t *p, size_t n, const std::vector<uint32_t> &need, bool allow_missing, std::string &why, int *missing) {
  if (missing) *missing = 0;
  if (!p || n < 12) { why = "not a font file (too short)"; return false; }
  if (n > MAX_BYTES) { why = "over the size cap (" + std::to_string(n / 1024) + " KB > " + std::to_string(MAX_BYTES / 1024) + " KB)"; return false; }
  const uint32_t ver = be32(p);
  if (ver == 0x4F54544F) { why = "a CFF (OpenType PostScript) font: only TrueType outlines are drawn"; return false; }
  if (ver != 0x00010000 && ver != 0x74727565) { why = "not a TrueType font file"; return false; }
  const uint32_t nt = be16(p + 4);
  if (!nt || nt > 64 || 12 + 16 * (size_t) nt > n) { why = "corrupt (table directory)"; return false; }
  Tab cmap, head, hhea, hmtx, loca, glyf, maxp, name;
  for (uint32_t i = 0; i < nt; i++) {
    const uint8_t *r = p + 12 + 16 * i;
    const uint32_t off = be32(r + 8), len = be32(r + 12);
    if (off > n || len > n - off) { why = "corrupt (a table outside the file)"; return false; }
    Tab t{off, len, true};
    if (!memcmp(r, "cmap", 4)) cmap = t; else if (!memcmp(r, "head", 4)) head = t; else if (!memcmp(r, "hhea", 4)) hhea = t;
    else if (!memcmp(r, "hmtx", 4)) hmtx = t; else if (!memcmp(r, "loca", 4)) loca = t; else if (!memcmp(r, "glyf", 4)) glyf = t;
    else if (!memcmp(r, "maxp", 4)) maxp = t; else if (!memcmp(r, "name", 4)) name = t;
  }
  for (auto [t, nm] : {std::pair<Tab *, const char *>{&cmap, "cmap"}, {&head, "head"}, {&hhea, "hhea"}, {&hmtx, "hmtx"},
                       {&loca, "loca"}, {&glyf, "glyf"}, {&maxp, "maxp"}, {&name, "name"}})
    if (!t->ok) { why = std::string("corrupt (no ") + nm + " table)"; return false; }
  if (head.len < 54 || maxp.len < 6 || hhea.len < 36) { why = "corrupt (head, maxp or hhea too short)"; return false; }
  const uint32_t upem = be16(p + head.off + 18), locfmt = be16(p + head.off + 50), ng = be16(p + maxp.off + 4);
  if (upem < 16 || upem > 16384 || locfmt > 1 || !ng) { why = "corrupt (head or maxp values)"; return false; }
  const uint32_t nhm = be16(p + hhea.off + 34);
  if (!nhm || nhm > ng || hmtx.len < 4 * nhm) { why = "corrupt (horizontal metrics)"; return false; }
  const uint32_t le = locfmt ? 4 : 2;
  if (loca.len < le * (ng + 1)) { why = "corrupt (loca shorter than the glyph count)"; return false; }
  uint32_t prev = 0;
  for (uint32_t g = 0; g <= ng; g++) {  // every outline inside glyf, ascending: the rasteriser then never reads outside
    const uint8_t *q = p + loca.off + le * g;
    const uint32_t o = locfmt ? be32(q) : be16(q) * 2;
    if (o < prev || o > glyf.len) { why = "corrupt (a glyph outside glyf)"; return false; }
    prev = o;
  }
  // licence: SIL OFL only, as the file itself says (copyright 0, licence 13, licence URL 14)
  const uint8_t *nm = p + name.off;
  if (name.len < 6) { why = "corrupt (name)"; return false; }
  const uint32_t nn = be16(nm + 2), so = be16(nm + 4);
  if (6 + 12 * (size_t) nn > name.len || so > name.len) { why = "corrupt (name records)"; return false; }
  bool ofl = false;
  for (uint32_t i = 0; i < nn && !ofl; i++) {
    const uint8_t *r = nm + 6 + 12 * i;
    const uint32_t plat = be16(r), id = be16(r + 6), len = be16(r + 8), off = be16(r + 10);
    if (id != 0 && id != 13 && id != 14) continue;
    if ((size_t) so + off + len > name.len) { why = "corrupt (a name outside the table)"; return false; }
    ofl = says_ofl(nm + so + off, len, plat == 0 || plat == 3);
  }
  if (!ofl) { why = "its licence is not the SIL Open Font License (the file's name table does not say OFL)"; return false; }
  // cmap: the Unicode subtable (3,10 format 12 first, else 3,1 / 0,x format 4)
  const uint8_t *cm = p + cmap.off, *ce = cm + cmap.len;
  if (cmap.len < 4) { why = "corrupt (cmap)"; return false; }
  const uint32_t ns = be16(cm + 2);
  if (4 + 8 * (size_t) ns > cmap.len) { why = "corrupt (cmap records)"; return false; }
  const uint8_t *sub = nullptr;
  int fmt = 0;
  for (int pass = 0; pass < 2 && !sub; pass++)
    for (uint32_t i = 0; i < ns; i++) {
      const uint8_t *r = cm + 4 + 8 * i;
      const uint32_t plat = be16(r), enc = be16(r + 2), off = be32(r + 4);
      if (off + 4 > cmap.len) continue;
      const int f = (int) be16(cm + off);
      const bool uni = plat == 0 || (plat == 3 && (enc == 1 || enc == 10));
      if (uni && ((pass == 0 && f == 12) || (pass == 1 && f == 4))) { sub = cm + off; fmt = f; break; }
    }
  if (!sub) { why = "no Unicode character map (format 4 or 12)"; return false; }
  int miss = 0, ascii = 0;
  uint32_t first = 0;
  for (uint32_t c : need) {
    const uint32_t g = cmap_glyph(sub, ce, fmt, c);
    if (g && g < ng) continue;
    if (c >= 0x20 && c < 0x7F) ascii++;
    if (!miss) first = c;
    miss++;
  }
  if (missing) *missing = miss;
  char b[96];
  if (ascii) { snprintf(b, sizeof b, "lacks %d printable ASCII characters (U+%04X ...)", ascii, (unsigned) first); why = b; return false; }
  if (miss && !allow_missing) { snprintf(b, sizeof b, "lacks %d characters the text uses (U+%04X ...; or fallback: true)", miss, (unsigned) first); why = b; return false; }
  return true;
}

std::string key_of(const std::string &url) {
  uint64_t h = 1469598103934665603ull;
  for (unsigned char c : url) { h ^= c; h *= 1099511628211ull; }
  char b[24];
  snprintf(b, sizeof b, "f%016llx", (unsigned long long) h);
  return b;
}

namespace {
struct Entry {
  Status st;
  std::vector<uint8_t> data;                 // the checked file TinyTTF reads (kept while its fonts live)
  std::map<int, lv_font_t *> fonts;          // per px
  std::vector<uint32_t> need;
  bool fallback_ok = false;
  uint32_t retry_ms = RETRY_MS, retry_at = 0;
};
Fetch *net = nullptr;
Store *disk = nullptr;
std::map<std::string, std::unique_ptr<Entry>> entries;
uint32_t gen = 0;
void (*changed)() = nullptr;
lv_timer_t *retry_timer = nullptr;

void bump() { gen++; if (changed) changed(); }
void free_fonts(Entry &e) {
#if LV_USE_TINY_TTF
  for (auto &f : e.fonts) lv_tiny_ttf_destroy(f.second);
#endif
  e.fonts.clear();
}
void start(const std::string &url);
void retry_tick(lv_timer_t *) {
  const uint32_t now = lv_tick_get();
  bool any = false;
  for (auto &it : entries)
    if (it.second->st.state == WAITING) {
      if ((int32_t) (now - it.second->retry_at) >= 0) start(it.first); else any = true;
    }
  if (!any && retry_timer) { lv_timer_delete(retry_timer); retry_timer = nullptr; }
}
void landed(const std::string &id, int http, std::vector<uint8_t> &&body) {
  auto it = entries.find(id);
  if (it == entries.end()) return;  // reset() since: forget it
  Entry &e = *it->second;
  e.st.bytes = body.size();
  if (http == 0) {  // offline / timed out: keep the fallback, try again later (backing off)
    e.st.state = WAITING; e.st.msg = "offline: the fallback draws until the file arrives";
    e.retry_at = lv_tick_get() + e.retry_ms; e.retry_ms = std::min(e.retry_ms * 2, RETRY_MAX_MS);
    if (!retry_timer) retry_timer = lv_timer_create(retry_tick, 1000, nullptr);
    return;
  }
  std::string why;
  if (http == 413 || body.size() > MAX_BYTES) why = "over the size cap (" + std::to_string(MAX_BYTES / 1024) + " KB)";
  else if (http != 200) why = "the server answered " + std::to_string(http);
  else check(body.data(), body.size(), e.need, e.fallback_ok, why);
  if (!why.empty()) { e.st.state = REFUSED; e.st.msg = why; bump(); return; }
  if (disk && !disk->write(key_of(e.st.url), body)) e.st.msg = "ready, but not kept in flash (write failed): fetched again at the next boot";
  e.data = std::move(body);
  e.st.state = READY;
  bump();
}
void start(const std::string &id) {
  Entry &e = *entries[id];
  e.st.state = LOADING; e.st.msg = "";
  if (!net) { e.st.state = WAITING; e.st.msg = "this panel has no network for fonts"; return; }
  e.st.fetches++;
  net->get(e.st.url, MAX_BYTES, [id](int http, std::vector<uint8_t> &&b) { landed(id, http, std::move(b)); });
}
}  // namespace

void use(Fetch *f, Store *s) { net = f; disk = s; }
uint32_t generation() { return gen; }
void on_change(void (*cb)()) { changed = cb; }

const lv_font_t *get(const char *url, const char *family, int px, const std::vector<uint32_t> &need, bool fallback_ok,
                     const lv_font_t *fallback) {
  if (!url || !*url || px < 1) return nullptr;
  // one entry per (url, fallback): the file's verdict depends on whether missing glyphs may fall back
  const std::string id = std::string(url) + (fallback_ok ? "#fallback" : "");
  auto &slot = entries[id];
  if (!slot) {  // the first ask: from flash (checked again: flash can be corrupt), else the network
    slot.reset(new Entry);
    Entry &e = *slot;
    e.st.url = url; e.st.family = family ? family : ""; e.need = need; e.fallback_ok = fallback_ok;
    std::vector<uint8_t> b;
    std::string why;
    if (disk && disk->read(key_of(url), b)) {
      if (check(b.data(), b.size(), need, fallback_ok, why)) { e.data = std::move(b); e.st.bytes = e.data.size(); e.st.state = READY; e.st.from_store = true; }
      else disk->remove(key_of(url));  // a corrupt stored file: drop it and fetch again
    }
    if (e.st.state != READY) start(id);
  }
  Entry &e = *slot;
  if (e.st.state != READY) return nullptr;
#if LV_USE_TINY_TTF
  auto f = e.fonts.find(px);
  if (f != e.fonts.end()) return f->second;
  lv_font_t *t = lv_tiny_ttf_create_data_ex(e.data.data(), e.data.size(), px, LV_FONT_KERNING_NONE, 128);
  if (!t) { e.st.state = REFUSED; e.st.msg = "TinyTTF could not open it"; return nullptr; }
  t->fallback = fallback;
  e.fonts[px] = t;
  e.st.sizes = (int) e.fonts.size();
  return t;
#else
  (void) fallback;
  e.st.state = REFUSED; e.st.msg = "this panel's build has no TinyTTF (LV_USE_TINY_TTF)";
  return nullptr;
#endif
}

void status(std::vector<Status> &out) {
  out.clear();
  for (auto &it : entries) out.push_back(it.second->st);
}
void reset() {
  for (auto &it : entries) free_fonts(*it.second);
  entries.clear();
  if (retry_timer) { lv_timer_delete(retry_timer); retry_timer = nullptr; }
  bump();
}

#ifndef ESP_PLATFORM
namespace {
struct Resp { int status; std::vector<uint8_t> body; uint32_t delay; };
std::map<std::string, Resp> served;
bool offline = false;
struct Pending { uint32_t at; int status; std::vector<uint8_t> body; std::function<void(int, std::vector<uint8_t> &&)> done; };
std::vector<Pending> pending;
lv_timer_t *sim_timer = nullptr;
void sim_tick(lv_timer_t *) {
  const uint32_t now = lv_tick_get();
  for (size_t i = 0; i < pending.size();) {
    if ((int32_t) (now - pending[i].at) < 0) { i++; continue; }
    Pending p = std::move(pending[i]);
    pending.erase(pending.begin() + (long) i);
    p.done(p.status, std::move(p.body));
  }
  if (pending.empty() && sim_timer) { lv_timer_delete(sim_timer); sim_timer = nullptr; }
}
struct SimFetch : Fetch {
  void get(const std::string &url, size_t cap, std::function<void(int, std::vector<uint8_t> &&)> done) override {
    auto it = served.find(url);
    Pending p{lv_tick_get() + (it == served.end() ? 50 : it->second.delay), 404, {}, std::move(done)};
    if (offline) p.status = 0;
    else if (it != served.end()) {
      p.status = it->second.status;
      if (it->second.body.size() > cap) p.status = 413; else p.body = it->second.body;
    }
    pending.push_back(std::move(p));
    if (!sim_timer) sim_timer = lv_timer_create(sim_tick, 10, nullptr);
  }
} sim_net;
struct SimStore : Store {
  std::map<std::string, std::vector<uint8_t>> files;
  bool read(const std::string &k, std::vector<uint8_t> &out) override { auto it = files.find(k); if (it == files.end()) return false; out = it->second; return true; }
  bool write(const std::string &k, const std::vector<uint8_t> &d) override { files[k] = d; return true; }
  void remove(const std::string &k) override { files.erase(k); }
} sim_disk;
}  // namespace
void sim_install() { use(&sim_net, &sim_disk); }
void sim_serve(const std::string &url, int status, std::vector<uint8_t> body, uint32_t delay_ms) { served[url] = {status, std::move(body), delay_ms}; }
void sim_offline(bool on) { offline = on; }
void sim_store_clear() { sim_disk.files.clear(); }
int sim_store_files() { return (int) sim_disk.files.size(); }
bool sim_store_corrupt(const std::string &url) {
  auto it = sim_disk.files.find(key_of(url));
  if (it == sim_disk.files.end() || it->second.size() < 64) return false;
  for (size_t i = 12; i < 60; i++) it->second[i] ^= 0x5A;  // the table directory
  return true;
}
#endif
}  // namespace fonts
}  // namespace fx
