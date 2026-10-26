// Image sources + cache (board facet-171, notes image-component.md, piece 1): the one place an image component gets
// its pixels. A view = (src URL, box w x h, scale, radius) -> the pixels scaled to that box, RGB565 (+A8 when anything
// is see-through: a rounded corner, a transparent PNG), dithered to 565 (pixel art: nearest, never dithered).
// Sources: the host fetches each wanted URL and hands the DECODED pixels to put() (RGBA8888, any size up to
// budget().src_max_px) or says fail(). WASM: the browser decodes (the hub proxy for CORS), core/main.cpp
// fw_img_put / fw_img_fail; the panel (piece 2): its own HTTP fetch + JPEG/PNG decoder feeds the same put().
// Cache: every view's pixels count against ONE per-board byte cap (budget().cache_bytes: Guition 512 KB, Waveshare
// 3 MB); a new view that doesn't fit first evicts views nothing shows (least recently used first), and if it still
// doesn't fit it is refused (the component shows its fallback): the cap is never exceeded. Views nothing shows stay
// while other images are on screen (a grid rebinding its items gets them back), and all go once no image is shown
// (the screen was left). A failed refresh of a URL keeps the pixels it had (the last good image).
// Pending URLs time out after budget().timeout_ms (failed: "timeout"). Single-threaded (LVGL task).
#pragma once
#include <string>
#include <vector>
#include "lvgl.h"

namespace fw::img {
enum Scale : uint8_t { cover, contain, pixel };
enum State : uint8_t { loading, ready, failed };
struct Budget {
  size_t cache_bytes;  // decoded pixels of every view, at most
  int src_max_px;      // a source image this big (w x h) at most; bigger = failed "too big"
  int timeout_ms;      // a wanted URL that hasn't come in by then = failed "timeout"
  const char *board;
};
const Budget &budget();
// a view's owner hears about every change of its view (pixels in, failed) through this
using Notify = void (*)(void *owner);
// take a view for owner (any stable key: a component keeps two, the image shown + the one coming) (one per owner: a second acquire releases the first); returns its id (> 0)
int acquire(void *owner, const char *src, int w, int h, Scale s, int radius, Notify cb);
void release(void *owner);
State state(void *owner);
const lv_image_dsc_t *pixels(void *owner);  // nullptr until ready (a failed refresh keeps the last good)
const char *error(void *owner);             // "" | "404" | "timeout" | "too big" | "over budget" ...
// host side
std::vector<std::string> wants();  // URLs waiting for bytes, each listed once until put/fail
void put(const char *src, int w, int h, const uint8_t *rgba);
void fail(const char *src, const char *why);
void refresh(const char *src);  // fetch a URL again (its views keep their pixels until the new ones come; a failure keeps them)
std::string stats();  // JSON {board, cap, bytes, peak, views, shown, evicted, dropped, refused, pending, fetches}
}  // namespace fw::img
