// Image sources + cache (core/images.h). Platform-neutral: the WASM host and (piece 2) the panel feed put()/fail().
#include "images.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <string>
#include <vector>

extern "C" void lv_image_cache_drop(const void *src);  // LVGL 9 (misc/cache/instance/lv_image_cache.h, not in lvgl.h)

namespace fw::img {
namespace {
// Per-board caps (notes image-component.md §3 + Darren 2026-10-26: memory-bounded, Guition much lower).
// Guition 4848S040 (ESP32-S3, 8 MB PSRAM shared with LVGL's buffers, fonts, Wi-Fi + one TLS session): 512 KB = a
// content-wide 16:9 photo (440 x 248 RGB565 = 218 KB) plus a 4 x 2 grid of 96 px cells (RGB565A8 = 221 KB).
// Waveshare P4 (32 MB PSRAM): 3 MB = a full 720 x 720 still (1 MB) plus a 64-cell grid of 72 px thumbnails (1 MB).
#ifdef FW_1X
constexpr Budget B = {512u * 1024u, 1024 * 1024, 8000, "guition"};
#else
constexpr Budget B = {3u * 1024u * 1024u, 2048 * 2048, 8000, "waveshare"};
#endif

struct View {
  std::string src, err;
  int w = 0, h = 0, r = 0;
  Scale s = cover;
  State st = loading;
  lv_image_dsc_t dsc{};
  uint8_t *data = nullptr;
  size_t bytes = 0;  // counted against the cap (pixels + this record)
  int owners = 0;
  uint32_t used = 0;
};
struct Own { View *v; Notify cb; };
struct Pending { std::string url; uint32_t since; bool told; };

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
void notify(const std::string &src) {
  std::vector<std::pair<void *, Notify>> who;
  for (auto &kv : owned)
    if (kv.second.v->src == src && kv.second.cb) who.push_back({kv.first, kv.second.cb});
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
bool url_ok(const std::string &s) { return !s.rfind("http://", 0) || !s.rfind("https://", 0); }

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
void scale(const View &v, const uint8_t *src, int sw, int sh, int ow, int oh, uint16_t *col, uint8_t *alpha, bool &see_through) {
  double k = 1, x0 = 0, y0 = 0;
  if (v.s == cover) {
    k = std::max((double) ow / sw, (double) oh / sh);
    x0 = (sw - ow / k) / 2; y0 = (sh - oh / k) / 2;
  } else if (v.s == contain) k = (double) ow / sw;
  const int rad = std::min(v.r, std::min(ow, oh) / 2);
  see_through = false;
  for (int y = 0; y < oh; y++)
    for (int x = 0; x < ow; x++) {
      double r = 0, g = 0, b = 0, a = 0;
      if (v.s == pixel) {
        const int sx = std::min(sw - 1, x * sw / ow), sy = std::min(sh - 1, y * sh / oh);
        const uint8_t *p = src + ((size_t) sy * sw + sx) * 4;
        a = p[3]; r = p[0] * a; g = p[1] * a; b = p[2] * a;
      } else if (k >= 1) {  // up: bilinear
        const double fx = std::clamp(x0 + (x + 0.5) / k - 0.5, 0.0, sw - 1.0), fy = std::clamp(y0 + (y + 0.5) / k - 0.5, 0.0, sh - 1.0);
        const int ix = (int) fx, iy = (int) fy, jx = std::min(ix + 1, sw - 1), jy = std::min(iy + 1, sh - 1);
        const double tx = fx - ix, ty = fy - iy;
        const int xs[2] = {ix, jx}, ys[2] = {iy, jy};
        for (int q = 0; q < 4; q++) {
          const double wq = (q & 1 ? tx : 1 - tx) * (q & 2 ? ty : 1 - ty);
          const uint8_t *p = src + ((size_t) ys[q >> 1] * sw + xs[q & 1]) * 4;
          a += p[3] * wq; r += p[0] * p[3] * wq; g += p[1] * p[3] * wq; b += p[2] * p[3] * wq;
        }
      } else {  // down: box average over the source pixels this one covers
        const int sx0 = (int) std::floor(x0 + x / k), sx1 = std::max(sx0 + 1, (int) std::ceil(x0 + (x + 1) / k));
        const int sy0 = (int) std::floor(y0 + y / k), sy1 = std::max(sy0 + 1, (int) std::ceil(y0 + (y + 1) / k));
        int n = 0;
        for (int sy = std::max(0, sy0); sy < std::min(sh, sy1); sy++)
          for (int sx = std::max(0, sx0); sx < std::min(sw, sx1); sx++, n++) {
            const uint8_t *p = src + ((size_t) sy * sw + sx) * 4;
            a += p[3]; r += p[0] * p[3]; g += p[1] * p[3]; b += p[2] * p[3];
          }
        if (n) { a /= n; r /= n; g /= n; b /= n; }
      }
      const double an = a > 0 ? a : 1;
      int R = (int) (r / an), G = (int) (g / an), Bc = (int) (b / an), A = (int) std::lround(a);
      if (rad > 0) {  // rounded corner: anti-aliased coverage of the corner circle
        const double cx = x < rad ? rad : x >= ow - rad ? ow - rad : x + 0.5, cy = y < rad ? rad : y >= oh - rad ? oh - rad : y + 0.5;
        const double d = std::hypot(x + 0.5 - cx, y + 0.5 - cy);
        A = (int) std::lround(A * std::clamp(rad - d + 0.5, 0.0, 1.0));
      }
      if (v.s != pixel) {  // ordered dither to 565 (photos band otherwise); pixel art stays exact
        const int t = BAYER[(x & 3) + (y & 3) * 4];
        R = std::min(255, R + t / 2); G = std::min(255, G + t / 4); Bc = std::min(255, Bc + t / 2);
      }
      col[(size_t) y * ow + x] = (uint16_t) (((R >> 3) << 11) | ((G >> 2) << 5) | (Bc >> 3));
      alpha[(size_t) y * ow + x] = (uint8_t) std::clamp(A, 0, 255);
      see_through |= A < 255;
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
  // scale into scratch first (alpha known after), then keep only the planes needed
  std::vector<uint16_t> col(px);
  std::vector<uint8_t> al(px);
  bool see = false;
  scale(*v, rgba, sw, sh, ow, oh, col.data(), al.data(), see);
  const size_t data = px * 2 + (see ? px : 0), bytes = data + sizeof(View);
  const size_t had = v->bytes;
  if (!room(bytes > had ? bytes - had : 0, v)) {
    refused++;
    if (!v->data) { v->st = failed; v->err = "over budget"; }
    else v->err = "over budget (kept the last image)";
    return;
  }
  uint8_t *d = (uint8_t *) lv_malloc(data);
  if (!d) { refused++; if (!v->data) { v->st = failed; v->err = "no memory"; } return; }
  memcpy(d, col.data(), px * 2);
  if (see) memcpy(d + px * 2, al.data(), px);
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

int acquire(void *owner, const char *src_c, int w, int h, Scale s, int radius, Notify cb) {
  release(owner);
  const std::string src = src_c ? src_c : "";
  View *v = nullptr;
  for (View *x : views)
    if (x->src == src && x->w == w && x->h == h && x->s == s && x->r == radius) { v = x; break; }
  if (!v) {
    v = new View;
    v->src = src; v->w = std::max(1, w); v->h = std::max(1, h); v->s = s; v->r = radius;
    views.push_back(v);
    if (src.empty()) { v->st = failed; v->err = "no src"; }
    else if (!url_ok(src)) { v->st = failed; v->err = "not a URL"; }
    else {
      bool have = false;
      for (auto &p : pending) have |= p.url == src;
      if (!have) {
        pending.push_back({src, lv_tick_get(), false});
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
  const std::string src = src_c ? src_c : "";
  if (w <= 0 || h <= 0 || !rgba) { fail(src_c, "bad image"); return; }
  if ((long) w * h > B.src_max_px) { fail(src_c, "too big"); return; }
  pending.erase(std::remove_if(pending.begin(), pending.end(), [&](const Pending &p) { return p.url == src; }), pending.end());
  for (size_t i = 0; i < views.size(); i++)
    if (views[i]->src == src) fill(views[i], rgba, w, h);
  notify(src);
}
void fail(const char *src_c, const char *why) {
  const std::string src = src_c ? src_c : "";
  pending.erase(std::remove_if(pending.begin(), pending.end(), [&](const Pending &p) { return p.url == src; }), pending.end());
  for (View *v : views)
    if (v->src == src) {
      if (v->data) v->err = std::string(why ? why : "failed") + " (kept the last image)";  // a failed refresh
      else { v->st = failed; v->err = why ? why : "failed"; }
    }
  notify(src);
}
void refresh(const char *src_c) {
  const std::string src = src_c ? src_c : "";
  if (!url_ok(src)) return;
  for (auto &p : pending)
    if (p.url == src) return;
  pending.push_back({src, lv_tick_get(), false});
  fetches++;
  if (!watch) watch = lv_timer_create(timeouts, 250, nullptr);
}
std::string stats() {
  char b[320];
  snprintf(b, sizeof b,
           "{\"board\":\"%s\",\"cap\":%u,\"bytes\":%u,\"peak\":%u,\"views\":%u,\"shown\":%d,\"evicted\":%u,\"dropped\":%u,\"refused\":%u,\"pending\":%u,\"fetches\":%u}",
           B.board, (unsigned) B.cache_bytes, (unsigned) total, (unsigned) peak, (unsigned) views.size(), shown(), evicted, dropped,
           refused, (unsigned) pending.size(), fetches);
  return b;
}
}  // namespace fw::img
