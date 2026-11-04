// Image sources + cache (core/images.h). Platform-neutral: the WASM host and (piece 2) the panel feed put()/fail().
#include "images.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <new>
#include <string>
#include <vector>

extern "C" void lv_image_cache_drop(const void *src);  // LVGL 9 (misc/cache/instance/lv_image_cache.h, not in lvgl.h)

namespace fw::img {
namespace {
// Per-board caps (memory-bounded, Guition much lower).
// Guition 4848S040 (ESP32-S3, 8 MB PSRAM shared with LVGL's buffers, fonts, Wi-Fi + one TLS session): 640 KB = the
// idle screen's full-screen album art (480 x 480 RGB565 = 450 KB) while the Media screen's (CD 168 px RGB565A8 = 83 KB)
// and the Home card's (47 px, 7 KB) stay on their kept pages, with room to spare.
// Waveshare P4 (32 MB PSRAM): 3 MB = a full 720 x 720 still (1 MB) plus a 64-cell grid of 72 px thumbnails (1 MB).
#ifdef FW_1X
constexpr Budget B = {640u * 1024u, 1024 * 1024, 8000, "guition-4848s040", 256u * 1024u};
#else
constexpr Budget B = {3u * 1024u * 1024u, 2048 * 2048, 8000, "waveshare-p4-86", 1024u * 1024u};
#endif

struct View {
  std::string src, key, err;
  int w = 0, h = 0, r = 0;
  Scale s = cover;
  bool nearest = false;  // sampling: nearest (no blur up, no dither) | smooth
  int shade = 0;         // the bottom px darkened (images.h)
  State st = loading;
  lv_image_dsc_t dsc{};
  uint8_t *data = nullptr;
  size_t bytes = 0;  // counted against the cap (pixels + this record)
  int owners = 0;
  uint32_t used = 0;
};
struct Own { View *v; Notify cb; };
struct Pending { std::string url, key; uint32_t since; bool told; };

std::vector<View *> views;
std::map<void *, Own> owned;
std::vector<Pending> pending;
size_t total = 0, peak = 0;
uint32_t clock_ = 0, evicted = 0, dropped = 0, refused = 0, fetches = 0;
lv_timer_t *watch = nullptr, *sweep_t = nullptr;

void free_px(View *v) {
  if (!v->data) return;
  lv_image_cache_drop(&v->dsc);
  lv_free(v->data);
  v->data = nullptr;
  total -= v->bytes;
  v->bytes = 0;
}
void erase(View *v) {
  free_px(v);
  views.erase(std::find(views.begin(), views.end(), v));
  delete v;
}
int shown() {
  int n = 0;
  for (View *v : views) n += v->owners > 0;
  return n;
}
// no image on screen any more (the screen was left): every view goes
void sweep(lv_timer_t *) {
  sweep_t = nullptr;
  if (shown()) return;
  dropped += (uint32_t) views.size();
  while (!views.empty()) erase(views.back());
  pending.clear();
}
void notify(const std::string &k) {
  std::vector<std::pair<void *, Notify>> who;
  for (auto &kv : owned)
    if (kv.second.v->key == k && kv.second.cb) who.push_back({kv.first, kv.second.cb});
  for (auto &w : who)
    if (owned.count(w.first)) w.second(w.first);
}
void timeouts(lv_timer_t *) {
  const uint32_t now = lv_tick_get();
  std::vector<std::string> late;
  for (auto &p : pending)
    if (lv_tick_diff(now, p.since) >= (uint32_t) B.timeout_ms) late.push_back(p.url);
  for (auto &u : late) fail(u.c_str(), "timeout");
  if (pending.empty() && watch) { lv_timer_delete(watch); watch = nullptr; }
}
bool url_ok(const std::string &s) { return !s.rfind("http://", 0) || !s.rfind("https://", 0) || (s.size() > 1 && s[0] == '/' && s[1] != '/'); }

// ---- scaling: RGBA8888 source -> the view's box
void out_size(const View &v, int sw, int sh, int &ow, int &oh) {
  if (v.s == cover) { ow = v.w; oh = v.h; return; }
  if (v.s == contain) {
    const double k = std::min((double) v.w / sw, (double) v.h / sh);
    ow = std::max(1, (int) std::lround(sw * k));
    oh = std::max(1, (int) std::lround(sh * k));
    return;
  }
  if (sw <= v.w && sh <= v.h) {  // pixel: whole-number zoom (nearest), or a whole-number shrink when too big
    const int k = std::max(1, std::min(v.w / sw, v.h / sh));
    ow = sw * k; oh = sh * k;
  } else {
    const int d = (int) std::ceil(std::max((double) sw / v.w, (double) sh / v.h));
    ow = std::max(1, sw / d); oh = std::max(1, sh / d);
  }
}
constexpr uint8_t BAYER[16] = {0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};
// one output column (or row): its source taps. Box (down): the source span [a, b); bilinear (up): a, b and b's weight
// w (0-256); nearest: a alone (w = 0, b = a). Worked out once per column and row, so the pixel loop is integer only
// (the panels' FPUs are single precision: double maths there is software, ~1 s for a full screen)
struct Tap { int a, b, w; bool box; };
void taps(std::vector<Tap> &t, int on, int sn, double k, double o, bool pix, bool near) {
  t.resize((size_t) on);
  for (int x = 0; x < on; x++) {
    Tap &q = t[(size_t) x];
    q.box = false, q.w = 0;
    if (pix) q.a = q.b = std::min(sn - 1, x * sn / on);
    else if (near && k >= 1) q.a = q.b = std::clamp((int) std::floor(o + (x + 0.5) / k), 0, sn - 1);
    else if (k >= 1) {
      const double f = std::clamp(o + (x + 0.5) / k - 0.5, 0.0, sn - 1.0);
      q.a = (int) f, q.b = std::min(q.a + 1, sn - 1), q.w = (int) std::lround((f - q.a) * 256);
    } else {
      const int a = (int) std::floor(o + x / k), b = std::max(a + 1, (int) std::ceil(o + (x + 1) / k));
      q.a = std::clamp(a, 0, sn - 1), q.b = std::clamp(b, q.a + 1, sn), q.box = true;
    }
  }
}
void scale(const View &v, const uint8_t *src, int sw, int sh, int ow, int oh, uint16_t *col, uint8_t *alpha, bool &see_through) {
  double k = 1, x0 = 0, y0 = 0;
  if (v.s == cover) {
    k = std::max((double) ow / sw, (double) oh / sh);
    x0 = (sw - ow / k) / 2; y0 = (sh - oh / k) / 2;
  } else if (v.s == contain) k = (double) ow / sw;
  const bool pix = v.s == pixel, near = v.nearest;
  std::vector<Tap> xs, ys;
  taps(xs, ow, sw, k, x0, pix, near);
  taps(ys, oh, sh, k, y0, pix, near);
  const int rad = std::min(v.r, std::min(ow, oh) / 2);
  see_through = false;
  for (int y = 0; y < oh; y++) {
    const Tap &ty = ys[(size_t) y];
    int shade = 256;  // the shade: darker towards the bottom edge (images.h), so light text reads over it
    if (v.shade > 0 && y >= oh - v.shade) {
      const float t = std::min(1.0f, (y - (oh - v.shade) + 0.5f) / (v.shade / 2.0f));
      shade = (int) std::lround(256 * (1 - (1 - SHADE_MIN) * t * t * (3 - 2 * t)));
    }
    const bool yc = y < rad || y >= oh - rad;
    for (int x = 0; x < ow; x++) {
      const Tap &tx = xs[(size_t) x];
      uint32_t a = 0, r = 0, g = 0, b = 0, n = 1;  // premultiplied sums: R = r / a
      if (tx.box) {
        n = 0;
        for (int sy = ty.a; sy < ty.b; sy++) {
          const uint8_t *p = src + ((size_t) sy * sw + tx.a) * 4;
          for (int sx = tx.a; sx < tx.b; sx++, p += 4, n++) a += p[3], r += p[0] * p[3], g += p[1] * p[3], b += p[2] * p[3];
        }
      } else {  // bilinear (nearest: one tap at weight 65536): weights in 1/65536
        const int wx[2] = {256 - tx.w, tx.w}, wy[2] = {256 - ty.w, ty.w}, sx[2] = {tx.a, tx.b}, sy[2] = {ty.a, ty.b};
        for (int q = 0; q < 4; q++) {
          const uint32_t w = (uint32_t) (wx[q & 1] * wy[q >> 1]);
          if (!w) continue;
          const uint8_t *p = src + ((size_t) sy[q >> 1] * sw + sx[q & 1]) * 4;
          const uint32_t pa = p[3] * w;
          a += pa, r += (p[0] * pa) >> 8, g += (p[1] * pa) >> 8, b += (p[2] * pa) >> 8;
        }
        n = 65536;
        if (a) r = (uint32_t) (((uint64_t) r << 8) / a), g = (uint32_t) (((uint64_t) g << 8) / a), b = (uint32_t) (((uint64_t) b << 8) / a);
      }
      int R, G, Bc, A;
      if (tx.box) {
        R = a ? (int) (r / a) : 0, G = a ? (int) (g / a) : 0, Bc = a ? (int) (b / a) : 0;
        A = n ? (int) ((a + n / 2) / n) : 0;
      } else {
        R = (int) r, G = (int) g, Bc = (int) b;
        A = (int) ((a + 32768) >> 16);
      }
      if (rad > 0 && yc && (x < rad || x >= ow - rad)) {  // rounded corner: anti-aliased coverage of the corner circle
        const float cx = x < rad ? rad : ow - rad, cy = y < rad ? rad : oh - rad;
        const float d = std::sqrt((x + 0.5f - cx) * (x + 0.5f - cx) + (y + 0.5f - cy) * (y + 0.5f - cy));
        A = (int) std::lround(A * std::clamp(rad - d + 0.5f, 0.0f, 1.0f));
      }
      if (shade < 256) R = R * shade >> 8, G = G * shade >> 8, Bc = Bc * shade >> 8;
      if (!pix && !near) {  // ordered dither to 565 (photos band otherwise); pixel art stays exact
        const int t = BAYER[(x & 3) + (y & 3) * 4];
        R = std::min(255, R + t / 2); G = std::min(255, G + t / 4); Bc = std::min(255, Bc + t / 2);
      }
      col[(size_t) y * ow + x] = (uint16_t) (((R >> 3) << 11) | ((G >> 2) << 5) | (Bc >> 3));
      alpha[(size_t) y * ow + x] = (uint8_t) std::clamp(A, 0, 255);
      see_through |= A < 255;
    }
  }
}
// room for need more bytes: evict views nothing shows, least recently used first; false = it can't fit
bool room(size_t need, const View *keep) {
  while (total + need > B.cache_bytes) {
    View *lru = nullptr;
    for (View *v : views)
      if (v != keep && !v->owners && v->data && (!lru || v->used < lru->used)) lru = v;
    if (!lru) return false;
    evicted++;
    erase(lru);
  }
  return true;
}
void fill(View *v, const uint8_t *rgba, int sw, int sh) {
  int ow, oh;
  out_size(*v, sw, sh, ow, oh);
  const size_t px = (size_t) ow * oh;
  // scale into scratch first (alpha known after), then keep only the planes needed. Every allocation here may fail
  // (the panel: a full PSRAM, no exceptions) = the view fails "no memory" and the component shows its fallback
  uint8_t *scratch = (uint8_t *) lv_malloc(px * 3);
  if (!scratch) { refused++; if (!v->data) { v->st = failed; v->err = "no memory"; } return; }
  uint16_t *col = (uint16_t *) scratch;
  uint8_t *al = scratch + px * 2;
  bool see = false;
  scale(*v, rgba, sw, sh, ow, oh, col, al, see);
  const size_t data = px * 2 + (see ? px : 0), bytes = data + sizeof(View);
  const size_t had = v->bytes;
  if (!room(bytes > had ? bytes - had : 0, v)) {
    lv_free(scratch);
    refused++;
    if (!v->data) { v->st = failed; v->err = "over budget"; }
    else v->err = "over budget (kept the last image)";
    return;
  }
  uint8_t *d = (uint8_t *) lv_malloc(data);
  if (!d) { lv_free(scratch); refused++; if (!v->data) { v->st = failed; v->err = "no memory"; } return; }
  memcpy(d, col, px * 2);
  if (see) memcpy(d + px * 2, al, px);
  lv_free(scratch);
  free_px(v);
  v->data = d;
  v->bytes = bytes;
  total += bytes;
  peak = std::max(peak, total);
  v->dsc = {};
  v->dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
  v->dsc.header.cf = see ? LV_COLOR_FORMAT_RGB565A8 : LV_COLOR_FORMAT_RGB565;
  v->dsc.header.w = (uint32_t) ow;
  v->dsc.header.h = (uint32_t) oh;
  v->dsc.header.stride = (uint32_t) ow * 2;
  v->dsc.data_size = (uint32_t) data;
  v->dsc.data = d;
  v->st = ready;
  v->err.clear();
}
}  // namespace

const Budget &budget() { return B; }

int acquire(void *owner, const char *src_c, int w, int h, Scale s, int radius, Notify cb, bool nearest, int shade) {
  release(owner);
  const std::string src = src_c ? src_c : "", k = key(src);
  w = std::max(1, w), h = std::max(1, h), shade = std::max(0, shade);
  View *v = nullptr;
  for (View *x : views)
    if (x->key == k && x->w == w && x->h == h && x->s == s && x->r == radius && x->nearest == nearest && x->shade == shade) { v = x; break; }
  if (!v) {
    v = new (std::nothrow) View;
    if (!v) return 0;  // no memory: no view, state() = failed (the component shows its fallback)
    v->src = src; v->key = k; v->w = w; v->h = h; v->s = s; v->r = radius; v->nearest = nearest; v->shade = shade;
    views.push_back(v);
    if (src.empty()) { v->st = failed; v->err = "no src"; }
    else if (!url_ok(src)) { v->st = failed; v->err = "not a URL"; }
    else {
      bool have = false;
      for (auto &p : pending) have |= p.key == k;
      if (!have) {
        pending.push_back({src, k, lv_tick_get(), false});
        fetches++;
        if (!watch) watch = lv_timer_create(timeouts, 250, nullptr);
      }
    }
  }
  v->owners++;
  v->used = ++clock_;
  owned[owner] = {v, cb};
  return (int) clock_;
}
void release(void *owner) {
  auto it = owned.find(owner);
  if (it == owned.end()) return;
  View *v = it->second.v;
  v->owners--;
  owned.erase(it);
  if (!v->owners && !v->data) erase(v);  // nothing cached (loading / failed): nothing to keep
  if (!shown() && !sweep_t) {  // later, so a rebuild that releases then acquires again keeps its pixels
    sweep_t = lv_timer_create(sweep, 0, nullptr);
    lv_timer_set_repeat_count(sweep_t, 1);
  }
}
void drop(void *owner) {
  auto it = owned.find(owner);
  if (it == owned.end()) return;
  View *v = it->second.v;
  release(owner);
  if (std::find(views.begin(), views.end(), v) != views.end() && !v->owners) { dropped++; erase(v); }
}
State state(void *owner) { auto it = owned.find(owner); return it == owned.end() ? failed : it->second.v->st; }
const lv_image_dsc_t *pixels(void *owner) {
  auto it = owned.find(owner);
  return it != owned.end() && it->second.v->data ? &it->second.v->dsc : nullptr;
}
const char *error(void *owner) { auto it = owned.find(owner); return it == owned.end() ? "" : it->second.v->err.c_str(); }

std::vector<std::string> wants() {
  std::vector<std::string> out;
  for (auto &p : pending)
    if (!p.told) { p.told = true; out.push_back(p.url); }
  return out;
}
void put(const char *src_c, int w, int h, const uint8_t *rgba) {
  const std::string k = key(src_c ? src_c : "");
  if (w <= 0 || h <= 0 || !rgba) { fail(src_c, "bad image"); return; }
  if ((long) w * h > B.src_max_px) { fail(src_c, "too big"); return; }
  pending.erase(std::remove_if(pending.begin(), pending.end(), [&](const Pending &p) { return p.key == k; }), pending.end());
  for (size_t i = 0; i < views.size(); i++)
    if (views[i]->key == k) fill(views[i], rgba, w, h);
  notify(k);
}
void fail(const char *src_c, const char *why) {
  const std::string k = key(src_c ? src_c : "");
  pending.erase(std::remove_if(pending.begin(), pending.end(), [&](const Pending &p) { return p.key == k; }), pending.end());
  for (View *v : views)
    if (v->key == k) {
      if (v->data) v->err = std::string(why ? why : "failed") + " (kept the last image)";  // a failed refresh
      else { v->st = failed; v->err = why ? why : "failed"; }
    }
  notify(k);
}
void alive(const char *src_c) {
  const std::string k = key(src_c ? src_c : "");
  for (auto &p : pending)
    if (p.key == k) p.since = lv_tick_get();
}
bool waiting(const char *src_c) {
  const std::string k = key(src_c ? src_c : "");
  for (auto &p : pending)
    if (p.key == k) return true;
  return false;
}
long need_px(const char *src_c) {
  const std::string k = key(src_c ? src_c : "");
  long n = 0;
  for (const View *v : views)
    if (v->key == k) n = std::max(n, (long) v->w * v->h);
  return n;
}
void refresh(const char *src_c) {
  const std::string src = src_c ? src_c : "", k = key(src);
  if (!url_ok(src)) return;
  for (auto &p : pending)
    if (p.key == k) return;
  pending.push_back({src, k, lv_tick_get(), false});
  fetches++;
  if (!watch) watch = lv_timer_create(timeouts, 250, nullptr);
}
// HA's media_player_proxy URL carries the entity's access token (token=...), new every 5 minutes: not the picture
std::string key(const std::string &url) {
  const size_t q = url.find('?');
  if (q == std::string::npos) return url;
  std::string out = url.substr(0, q), sep = "?";
  size_t at = q + 1;
  while (at <= url.size()) {
    size_t e = url.find('&', at);
    if (e == std::string::npos) e = url.size();
    const std::string part = url.substr(at, e - at);
    if (!part.empty() && part.rfind("token=", 0) != 0) out += sep + part, sep = "&";
    at = e + 1;
  }
  return out;
}
bool same(const std::string &a, const std::string &b) { return a == b || key(a) == key(b); }

// ---- Swap: shown + coming
void Swap::in(void *own) {
  Swap *s = (Swap *) ((uintptr_t) own & ~(uintptr_t) 1);
  const int i = (int) ((uintptr_t) own & 1);
  const State st = state(own);
  if (st == loading) return;
  if (st == ready && i != s->on_) {
    drop(s->owner(s->on_));
    s->src_[s->on_].clear();
    s->on_ = i;
  } else if (st == failed) {
    s->clear();
    return;
  }
  if (s->changed_) s->changed_(s->ctx_);
}
void Swap::set(const std::string &url, int w, int h, int radius, int shade) {
  if (url.empty() || url == "None" || w < 1 || h < 1) { if (!src_[0].empty() || !src_[1].empty()) clear(); return; }
  const bool resized = w != w_ || h != h_ || radius != r_ || shade != shade_;
  if (!resized && same(src_[on_], url)) return;  // the same picture (or only its token moved on)
  const int n = resized ? on_ : 1 - on_;
  if (!resized && same(src_[n], url)) return;    // already coming
  if (resized) { drop(owner(1 - on_)); src_[1 - on_].clear(); }
  w_ = w, h_ = h, r_ = radius, shade_ = shade;
  drop(owner(n));  // what was coming (or, resized, what was shown at the old size)
  if (n != on_ && !src_[on_].empty() && 2 * (size_t) w * h * 3 > B.cache_bytes) {  // two would not fit (Guition, full
    drop(owner(on_));                                                               // screen): the old goes first
    src_[on_].clear();
    if (changed_) changed_(ctx_);
  }
  src_[n] = url;
  acquire(owner(n), url.c_str(), w, h, cover, radius, in, false, shade);
  if (src_[on_].empty()) on_ = n;  // nothing shown yet: this one, once it is in
  if (state(owner(n)) != loading) in(owner(n));  // in the cache already (or refused)
  else if (resized && changed_) changed_(ctx_);
}
void Swap::clear(bool tell) {
  const bool had = !src_[0].empty() || !src_[1].empty();
  for (int i = 0; i < 2; i++) drop(owner(i)), src_[i].clear();
  if (tell && had && changed_) changed_(ctx_);
}
const lv_image_dsc_t *Swap::pixels() const { return src_[on_].empty() ? nullptr : fw::img::pixels(owner(on_)); }
std::string stats() {
  char b[320];
  snprintf(b, sizeof b,
           "{\"board\":\"%s\",\"cap\":%u,\"bytes\":%u,\"peak\":%u,\"views\":%u,\"shown\":%d,\"evicted\":%u,\"dropped\":%u,\"refused\":%u,\"pending\":%u,\"fetches\":%u}",
           B.board, (unsigned) B.cache_bytes, (unsigned) total, (unsigned) peak, (unsigned) views.size(), shown(), evicted, dropped,
           refused, (unsigned) pending.size(), fetches);
  return b;
}
}  // namespace fw::img
