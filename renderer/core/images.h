// Image sources + cache (notes image-component.md, piece 1): the one place an image component gets
// its pixels. A view = (src URL, box w x h, scale, radius) -> the pixels scaled to that box, RGB565 (+A8 when anything
// is see-through: a rounded corner, a transparent PNG), dithered to 565 (pixel art: nearest, never dithered).
// Scaling: core/img_scale.h (one pass, rows in, the view's pixels out at the view's own box size: an area filter down,
// bilinear up, never nearest except pixel art).
// A URL is http(s)://..., or a path on the panel's Home Assistant ("/api/media_player_proxy/...": the host fetches it
// from its HA link). key(): the picture a URL names, without HA's access token (token=..., which HA's proxy rotates
// every 5 minutes): views, fetches and the cache match by it, so a new token never fetches the same picture again.
// shade (px, 0 = none): the bottom shade px darken towards black, the upper half a smoothstep ramp, the lower half at
// SHADE_MIN, so light text in that lower half keeps its contrast over any picture.
// Sources: the host fetches each wanted URL and hands the DECODED pixels to put() (RGBA8888, any size up to
// budget().src_max_px) or says fail(). WASM: the browser decodes (through the workbench host for CORS), core/main.cpp
// fw_img_put / fw_img_fail. The panel (facet firmware/fw_images.h): its own HTTP fetch + JPEG/PNG decoder, which asks
// boxes() which views wait, decodes straight into their pixels (img_scale.h Sink) and hands them over with put(src, px).
// Cache: every view's pixels count against ONE per-board byte cap (budget().cache_bytes: Guition 640 KB, Waveshare
// 3 MB); a new view that doesn't fit first evicts views nothing shows (least recently used first), and if it still
// doesn't fit it is refused (the component shows its fallback): the cap is never exceeded. Views nothing shows stay
// while other images are on screen (a grid rebinding its items gets them back), and all go once no image is shown
// (the screen was left). A failed refresh of a URL keeps the pixels it had (the last good image).
// Pending URLs time out after budget().timeout_ms (failed: "timeout"; alive() restarts it). Single-threaded (LVGL task):
// a host thread never calls in here (the panel hands results over on an lv_timer).
#pragma once
#include <string>
#include <vector>
#include "lvgl.h"
#include "img_scale.h"

namespace fw::img {
enum State : uint8_t { loading, ready, failed };
struct Budget {
  size_t cache_bytes;  // decoded pixels of every view, at most
  int src_max_px;      // a source image this big (w x h) at most; bigger = failed "too big"
  int timeout_ms;      // a wanted URL that hasn't come in by then = failed "timeout"
  const char *board;
  size_t body_max;     // the panel's download of one picture, at most (compressed bytes; fw_images.h)
};
const Budget &budget();
// a view's owner hears about every change of its view (pixels in, failed) through this
using Notify = void (*)(void *owner);
// take a view for owner (any stable key: a component keeps two, the image shown + the one coming) (one per owner: a second acquire releases the first); returns its id (> 0)
// nearest = no blur when scaling up (any scale; pixel always is): sharp pixel art in cover / contain boxes, never dithered
int acquire(void *owner, const char *src, int w, int h, Scale s, int radius, Notify cb, bool nearest = false, int shade = 0);
std::string key(const std::string &url);  // the URL without its access token (token=...)
std::string sized(const std::string &url, int w, int h);  // Apple Music art (mzstatic.com) asked at the box size (capped); others as they are
bool same(const std::string &a, const std::string &b);  // the same picture (key)
void release(void *owner);
void drop(void *owner);  // release, and its pixels go at once when nothing else shows them (not kept for a return)
State state(void *owner);
const lv_image_dsc_t *pixels(void *owner);  // nullptr until ready (a failed refresh keeps the last good)
const char *error(void *owner);             // "" | "404" | "timeout" | "too big" | "over budget" ...
// host side
std::vector<std::string> wants();  // URLs waiting for bytes, each listed once until put/fail
void put(const char *src, int w, int h, const uint8_t *rgba);
// the panel: the boxes of the views that want this URL (each once), and their finished pixels, made off the LVGL task
// (img_scale.h Sink). put() takes the pixels it uses (data = nullptr after); the caller frees the rest (free_pixels).
// A view the pixels don't match (it came or changed size meanwhile) is asked for again
std::vector<Box> boxes(const char *src);
void put(const char *src, std::vector<Pixels> &px);
void fail(const char *src, const char *why);
bool waiting(const char *src);
// the host is still on it (queued behind others, or just started): its timeout counts again from now. The panel fetches
// one URL at a time, so a URL's budget().timeout_ms starts when its own download does, not when the view asked
void alive(const char *src);  // a view still wants this URL (false once put / fail / timeout, or the screen was left)
void refresh(const char *src);  // fetch a URL again (its views keep their pixels until the new ones come; a failure keeps them)
std::string stats();  // JSON {board, cap, bytes, peak, views, shown, evicted, dropped, refused, pending, fetches, slots, slots_free}
// the screen-size picture slots (img_scale.h Slots: the idle art, shown + coming), reserved once for this display (LVGL
// task; also done by the first screen-size view): how many there are
int reserve_screen();

// A picture that changes (a track's album art): the one shown stays until the next is fully in, then the two swap and
// only then do the old one's pixels go (only the current picture is kept, never a cache of past tracks; only if two
// would not fit the board's cap does the old one go first: neither board's boxes come near it); a
// failed or empty URL = none (the component draws its plain look: the old picture belongs to something else). A URL
// that differs only in its access token is the same picture (nothing fetched). changed(ctx) runs when what pixels()
// gives changes. Two views at most (shown + coming), each a cover-scaled box with an optional round mask and shade.
class Swap {
 public:
  Swap(void (*changed)(void *ctx), void *ctx) : changed_(changed), ctx_(ctx) {}
  ~Swap() { clear(false); }
  Swap(const Swap &) = delete;
  Swap &operator=(const Swap &) = delete;
  void set(const std::string &url, int w, int h, int radius = 0, int shade = 0);  // "" / "None" = none
  void clear(bool tell = true);
  const lv_image_dsc_t *pixels() const;  // nullptr = none (yet)
  const std::string &url() const { return src_[on_]; }

 private:
  static void in(void *owner);
  void *owner(int i) const { return (void *) ((uintptr_t) this | (uintptr_t) i); }  // aligned: i in the low bit
  void (*changed_)(void *);
  void *ctx_;
  std::string src_[2];
  int on_ = 0, w_ = 0, h_ = 0, r_ = 0, shade_ = 0;
};
}  // namespace fw::img
