// Bins art (the Bins art component on the contract, components/bins_art.cpp): the panel lab's "Faithful" wheelie bin
// drawn through the Pen from colour token names (fg:bin-<colour>, fg:bin-<colour>-shade), sized from layout:bin-hero (or layout:bin-card)
// and stroke:edge, re-sized on a tuned token or a theme switch (LV_EVENT_STYLE_CHANGED). Core: the component only
// passes the colours.
#include "chart.h"
#include "fx.h"

namespace fw::art {
namespace {
struct Art {
  lv_obj_t *s = nullptr;
  std::vector<std::string> cols;
  const char *size = "layout:bin-hero";  // the bin height token
  bool busy = false;
};
void art_free(lv_event_t *e) { delete (Art *) lv_event_get_user_data(e); }
Art *art_of(lv_obj_t *s) {
  if (!s) return nullptr;
  for (uint32_t i = 0, n = lv_obj_get_event_count(s); i < n; i++) {
    lv_event_dsc_t *d = lv_obj_get_event_dsc(s, i);
    if (lv_event_dsc_get_cb(d) == art_free) return (Art *) lv_event_dsc_get_user_data(d);
  }
  return nullptr;
}
int bin_h(const Art *A) { return fx::size(A->size); }
int bin_w(int h) { return (h * 4 / 5 + 3) / 4 * 4; }
int step_of(int w) { return (w * 55 / 100) / 4 * 4; }
void one(const fx::Pen &p, int x0, int y0, int h, const std::string &col) {
  const std::string c = "fg:bin-" + col, k = "fg:bin-" + col + "-shade";
  const bool edge = col == "black" || col == "grey";
  const int w = bin_w(h), e = fx::size(h >= fx::size("layout:bin-hero") ? "stroke:edge" : "stroke:hair");  // hair on smaller bins
  const int hw = w * 36 / 100, hh = std::max(2, h / 10), lid = std::max(3, h * 13 / 100), o = std::max(1, w * 65 / 1000);
  const int rb = std::max(2, h / 10), band = std::max(2, h * 6 / 100), sw = std::max(2, w * 95 / 1000), top = hh + lid;
  auto part = [&](int x, int y, int pw, int ph, const std::string &ink, int r, bool edged) {
    p.rect(x0 + x, y0 + y, pw, ph, ink.c_str(), r);
    if (edged) p.rect(x0 + x, y0 + y, pw, ph, "fg:line-hi", r, e);
  };
  part((w - hw) / 2, 0, hw, hh + rb, k, rb, edge);  // the handle (its lower half under the lid)
  part(o, top - rb, w - 2 * o, h - top + rb, c, rb, edge);  // the body
  const int eb = edge ? e : 0;
  part(o + eb, top, w - 2 * o - 2 * eb, band, k, 0, false);  // the band under the lid
  const int sh = (h - top - band) * 55 / 100, sy = top + band + (h - top - band - sh) / 2;
  for (int i = -1; i <= 1; i++) part(w / 2 + i * (w * 228 / 1000) - sw / 2, sy, sw, sh, k, sw / 2, false);  // the slots
  part(0, hh, w, lid, k, std::max(1, lid / 5), edge);  // the slab lid over the body's top corners
}
void draw(const fx::Pen &p) {
  const Art *A = art_of(p.obj);
  if (!A) return;
  const int h = bin_h(A), step = step_of(bin_w(h)), L = -(p.w / 2), T = -(p.h / 2);
  for (int i = (int) A->cols.size() - 1; i >= 0; i--) one(p, L + i * step, T, h, A->cols[i]);  // the first in front
}
void size(Art *A) {
  const int h = bin_h(A), w = bin_w(h), n = std::max<int>(1, A->cols.size());
  lv_obj_set_size(A->s, w + step_of(w) * (n - 1), h);
}
void retune(lv_event_t *e) {
  Art *A = (Art *) lv_event_get_user_data(e);
  if (A->busy || lv_event_get_target(e) != A->s) return;
  A->busy = true;
  size(A);
  lv_obj_invalidate(A->s);
  A->busy = false;
}
}  // namespace

lv_obj_t *bins(lv_obj_t *parent, const std::vector<std::string> &cols, const char *tok) {
  auto *A = new Art();
  A->cols = cols, A->size = tok;
  A->s = fx::strip(parent, draw, "free");  // a strip: the Pen gets the box size (w, h)
  lv_obj_add_event_cb(A->s, art_free, LV_EVENT_DELETE, A);
  size(A);
  lv_obj_add_event_cb(A->s, retune, LV_EVENT_STYLE_CHANGED, A);
  return A->s;
}
bool bins_set(lv_obj_t *s, const std::vector<std::string> &cols, const char *tok) {
  Art *A = art_of(s);
  if (!A) return false;
  A->cols = cols, A->size = tok;
  size(A);
  lv_obj_invalidate(s);
  return true;
}
}  // namespace fw::art
