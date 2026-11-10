// Image sources + cache (core/images.h). Platform-neutral: the WASM host and (piece 2) the panel feed put()/fail().
#include "images.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <map>
#include <new>
#include <string>
#include <vector>

extern "C" void lv_image_cache_drop(const void *src);  // LVGL 9 (misc/cache/instance/lv_image_cache.h, not in lvgl.h)

namespace fw::img {
namespace {
// Per-board caps (memory-bounded, Guition much lower). Every view keeps its own box's pixels on every board (the idle
// art 480 x 480 on the Guition, 720 x 720 on the Waveshare, the CD's and the card's at their sizes): never a fixed
// smaller size scaled up when drawn (a 320 px art blown up to 480 looked pixelated). Only the panel's decode, when the
// largest free PSRAM block can't hold a view at its box size, keeps it a step smaller (firmware/fw_imgdec.h fit), and
// LVGL then scales it up smoothly (fx::Pen::image: bilinear, never nearest).
// Guition 4848S040 (ESP32-S3, 8 MB PSRAM shared with code, LVGL's buffers, fonts, Wi-Fi + one TLS session; ~1.2 MB
// free on the Media screen): 1 MB = the idle screen's full-screen art twice (480 x 480 RGB565 = 450 KB: the shown one +
// the next track's while it comes in) and the Home card's (47 px, 7 KB); the Media screen's (CD 164 px RGB565A8 = 79 KB,
// twice) fit beside one of them.
// Waveshare P4 (32 MB PSRAM): 3 MB = a full 720 x 720 still (1 MB) twice, or one plus a 64-cell grid of 72 px
// thumbnails (1 MB).
#ifdef FW_1X
constexpr Budget B = {1024u * 1024u, 1024 * 1024, 8000, "guition-4848s040", 256u * 1024u};
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
  bool slot = false;  // data is a reserved slot (img_scale.h Slots): given back, never freed
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
  if (v->slot) slot_give(v->data);  // a reserved slot: back to the pool
  else FW_IMG_FREE(v->data);  // made by img_scale.h (FW_IMG_MALLOC)
  v->data = nullptr, v->slot = false;
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

Box box_of(const View &v) { Box b; b.w = v.w, b.h = v.h, b.r = v.r, b.shade = v.shade, b.s = v.s, b.nearest = v.nearest; return b; }
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
bool fail_px(View *v, const char *why) {
  refused++;
  if (!v->data) { v->st = failed; v->err = why; }
  else v->err = std::string(why) + " (kept the last image)";
  return false;
}
// the view takes these pixels (px.data = nullptr after): within the cap, its old pixels go only once the new are in
bool adopt(View *v, Pixels &px) {
  const size_t bytes = px.size + sizeof(View), had = v->bytes;
  if (!room(bytes > had ? bytes - had : 0, v)) return fail_px(v, "over budget");
  free_px(v);
  v->data = px.data, v->slot = px.slot;
  px.data = nullptr, px.slot = false;
  v->bytes = bytes;
  total += bytes;
  peak = std::max(peak, total);
  const size_t n = (size_t) px.w * px.h;
  v->dsc = {};
  v->dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
  v->dsc.header.cf = px.alpha ? LV_COLOR_FORMAT_RGB565A8 : LV_COLOR_FORMAT_RGB565;
  v->dsc.header.w = (uint32_t) px.w;
  v->dsc.header.h = (uint32_t) px.h;
  v->dsc.header.stride = (uint32_t) px.w * 2;
  v->dsc.data_size = (uint32_t) (n * 2 + (px.alpha ? n : 0));
  v->dsc.data = v->data;
  v->st = ready;
  v->err.clear();
  return true;
}
// a whole RGBA8888 source (the WASM host's decode) -> the view's pixels, one pass (img_scale.h). Every allocation may
// fail (the panel: a full PSRAM, no exceptions) = the view fails "no memory" and the component shows its fallback
void fill(View *v, const uint8_t *rgba, int sw, int sh) {
  bool see = false;  // an A8 plane only for a see-through source (an opaque one can go into a reserved slot)
  for (size_t i = 3, n = (size_t) sw * sh * 4; i < n && !see; i += 4) see = rgba[i] < 255;
  Sink k;
  if (!k.plan(box_of(*v), sw, sh, 0, see) || !k.begin()) { fail_px(v, "no memory"); return; }
  for (int y = 0; y < sh; y++) k.row(rgba + (size_t) y * sw * 4);
  Pixels px;
  if (!k.finish(px)) { fail_px(v, "no memory"); return; }
  adopt(v, px);
  free_pixels(px);
}
}  // namespace

const Budget &budget() { return B; }

namespace {
// a screen-size opaque cover (the idle art: square corners, at least half the screen, at most all of it): its pixels go
// into the reserved slots (img_scale.h Slots), made for it
bool screen_size(int w, int h, Scale s, int radius) {
  lv_display_t *d = lv_display_get_default();
  if (!d || s != cover || radius) return false;
  const size_t px = (size_t) w * h, scr = (size_t) lv_display_get_horizontal_resolution(d) * lv_display_get_vertical_resolution(d);
  return px <= scr && px * 2 >= scr;
}
// the screen-size views that want a slot: on screen (owned), or holding one
int slot_views() {
  int n = 0;
  for (View *v : views) n += (v->owners > 0 || v->slot) && screen_size(v->w, v->h, v->s, v->r) && !v->nearest;
  return n;
}
// the slots follow those views (img_scale.h Slots): a slot one of them no longer needs goes, retried while a decode
// still holds it; none at all once no screen-size picture is on screen (the idle art left)
lv_timer_t *trim_t = nullptr;
void trim(lv_timer_t *) {
  if (slots_trim(slot_views()) <= slot_views() && trim_t) { lv_timer_delete(trim_t); trim_t = nullptr; }
}
void trim_later() {
  if (!trim_t && slots().n.load() > slot_views()) trim_t = lv_timer_create(trim, 500, nullptr);
}
int want_screen(int k) {
  lv_display_t *d = lv_display_get_default();
  return d ? slots_want(lv_display_get_horizontal_resolution(d), lv_display_get_vertical_resolution(d), k) : 0;
}
}  // namespace

int reserve_screen() { return want_screen(Slots::N); }

int acquire(void *owner, const char *src_c, int w, int h, Scale s, int radius, Notify cb, bool nearest, int shade) {
  release(owner);
  const std::string src = sized(src_c ? src_c : "", w, h), k = key(src);
  w = std::max(1, w), h = std::max(1, h), shade = std::max(0, shade);
  if (screen_size(w, h, s, radius) && !nearest) {
    want_screen(slot_views() + 1);  // a slot for this one (shown, or the next track's beside the shown one)
    // the screen's picture: what other screens left in the cache (the Media screen's CD art, kept for a return) goes
    // now, so its memory is free for this one
    std::vector<View *> left;
    for (View *x : views)
      if (!x->owners && x->data && !(x->key == k && x->w == w && x->h == h)) left.push_back(x);
    for (View *x : left) { dropped++; erase(x); }
  }
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
  trim_later();
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
  trim_later();
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
std::vector<Box> boxes(const char *src_c) {
  const std::string k = key(src_c ? src_c : "");
  std::vector<Box> out;
  for (const View *v : views)
    if (v->key == k && std::find(out.begin(), out.end(), box_of(*v)) == out.end()) out.push_back(box_of(*v));
  return out;
}
void put(const char *src_c, std::vector<Pixels> &px) {
  const std::string src = src_c ? src_c : "", k = key(src);
  pending.erase(std::remove_if(pending.begin(), pending.end(), [&](const Pending &p) { return p.key == k; }), pending.end());
  bool again = false;
  for (View *v : views) {
    if (v->key != k) continue;
    auto it = std::find_if(px.begin(), px.end(), [&](const Pixels &p) { return p.data && p.box == box_of(*v); });
    if (it != px.end()) adopt(v, *it);
    else if (!v->data) again = true;  // came (or changed size) while the picture was decoding: fetched again for it
  }
  if (again) refresh(src.c_str());
  notify(k);
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
// Apple Music's artwork (mzstatic.com: the size is in the URL's last part, "600x600bb.jpg", or a template "{w}x{h}bb.jpg";
// also URL-encoded inside another URL's query): asked at the box's size, its long side, at most ART_MAX, so the 720 idle
// art comes from a 720 source and a small box downloads a small one. Anything else as it is
constexpr int ART_MAX = 1000;
std::string sized(const std::string &url, int w, int h) {
  std::string low = url;
  for (char &c : low) c = (char) tolower((unsigned char) c);
  if (low.find("mzstatic.com") == std::string::npos) return url;
  const int side = std::clamp(std::max(w, h), 16, ART_MAX);
  auto num = [&](size_t a, size_t &b) {  // digits or {w} / {h} from a: b = past them
    b = a;
    if (low.compare(a, 3, "{w}") == 0 || low.compare(a, 3, "{h}") == 0) return b = a + 3, true;
    while (b < low.size() && isdigit((unsigned char) low[b])) b++;
    return b > a;
  };
  for (size_t x = low.rfind('x'); x != std::string::npos && x > 0; x = low.rfind('x', x - 1)) {
    size_t a = x;  // back over the width
    if (x >= 3 && low.compare(x - 3, 3, "{w}") == 0) a = x - 3;
    else while (a > 0 && isdigit((unsigned char) low[a - 1])) a--;
    const bool after_slash = a > 0 && (low[a - 1] == '/' || (a >= 3 && low.compare(a - 3, 3, "%2f") == 0));
    size_t e;
    if (a == x || !after_slash || !num(x + 1, e)) continue;
    size_t d = e;  // the crop letters ("bb", "cc", "sr"), then the extension
    while (d < low.size() && isalpha((unsigned char) low[d])) d++;
    if (d >= low.size() || low[d] != '.') continue;
    bool ext = false;
    for (const char *t : {".jpg", ".jpeg", ".png", ".webp"}) ext |= low.compare(d, strlen(t), t) == 0;
    if (!ext) continue;
    return url.substr(0, a) + std::to_string(side) + "x" + std::to_string(side) + url.substr(e);
  }
  return url;
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
  if (st == ready && i != s->on_) {  // the new one is fully in: shown from now, and only then does the old one go
    const int old = s->on_;
    s->on_ = i;
    if (s->changed_) s->changed_(s->ctx_);
    drop(s->owner(old));
    s->src_[old].clear();
    return;
  }
  if (st == failed) {
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
  int sw, sh;  // the pixels a view of this box keeps (img_scale.h stored: its box)
  Box bx;
  bx.w = w, bx.h = h;
  stored(bx, w, h, 0, sw, sh);
  if (n != on_ && !src_[on_].empty() && 2 * ((size_t) sw * sh * (radius > 0 ? 3 : 2) + sizeof(View)) > B.cache_bytes) {  // two would
    drop(owner(on_));                                                                 // not fit: the old goes first
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
           "{\"board\":\"%s\",\"cap\":%u,\"bytes\":%u,\"peak\":%u,\"views\":%u,\"shown\":%d,\"evicted\":%u,\"dropped\":%u,\"refused\":%u,\"pending\":%u,\"fetches\":%u,\"slots\":%d,\"slots_free\":%d}",
           B.board, (unsigned) B.cache_bytes, (unsigned) total, (unsigned) peak, (unsigned) views.size(), shown(), evicted, dropped,
           refused, (unsigned) pending.size(), fetches, slots().n.load(), slots_free());
  return b;
}
}  // namespace fw::img
