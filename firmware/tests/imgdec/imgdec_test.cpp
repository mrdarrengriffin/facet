// Host test of the panel picture decode (firmware/fw_imgdec.h + renderer core/img_scale.h): real PNG / JPEG files
// through the same code the panel runs, decoded straight into the views' boxes, under ASan + UBSan (run.sh).
// Usage: imgdec_test <fixtures dir>. Exit 0 = every check passed. Prints the memory a Guition album-art decode holds.
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <malloc.h>
#include <random>
#include <string>
#include <vector>

// every picture buffer counted: the most held at once (the decode's real reserve)
static long live = 0, live_max = 0;
static void count(void *p, long sign) {
  if (!p) return;
  live += sign * (long) malloc_usable_size(p);
  live_max = std::max(live_max, live);
}
static void *t_alloc(size_t n) { void *p = malloc(n); count(p, 1); return p; }
static void t_free(void *p) { count(p, -1); free(p); }
static void *t_realloc(void *p, size_t n) { count(p, -1); void *q = realloc(p, n); count(q ? q : p, 1); return q; }
#define FW_IMGDEC_ALLOC(n) t_alloc(n)
#define FW_IMGDEC_FREE(p) t_free(p)
#define FW_IMG_MALLOC(n) t_alloc(n)
#define FW_IMG_REALLOC(p, n) t_realloc(p, n)
#define FW_IMG_FREE(p) t_free(p)
#include "fw_imgdec.h"

static int fails = 0, checks = 0;
#define CHECK(c, ...) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d  ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static std::string D;
static std::vector<uint8_t> load(const char *name) {
  std::vector<uint8_t> v;
  FILE *f = fopen((D + "/" + name).c_str(), "rb");
  if (!f) { printf("missing fixture %s\n", name); exit(2); }
  uint8_t buf[8192];
  size_t k;
  while ((k = fread(buf, 1, sizeof buf, f)) > 0) v.insert(v.end(), buf, buf + k);
  fclose(f);
  return v;
}
// budgets as on the boards (renderer core/images.cpp): Waveshare 2048 x 2048 px, Guition 1024 x 1024; every view at its box size
constexpr long WS_PX = 2048L * 2048, GU_PX = 1024L * 1024;
constexpr size_t MEM = 24u << 20;
using fw::img::Box;
static Box box(int w, int h, int r = 0, fw::img::Scale s = fw::img::cover) { Box b; b.w = w, b.h = h, b.r = r, b.s = s; return b; }

static fw::imgdec::Result dec(const std::vector<uint8_t> &b, std::vector<Box> boxes = {box(640, 480)}, long px = WS_PX, size_t mem = MEM) {
  fw::imgdec::Result r;
  fw::imgdec::decode(b.data(), b.size(), px, mem, boxes, r);
  return r;
}
static double ms_since(std::chrono::steady_clock::time_point t) { return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t).count(); }
static bool ok(const fw::imgdec::Result &r, size_t n = 1) { return r.err.empty() && r.px.size() == n && r.px[0].data; }
static void rgb(const fw::img::Pixels &p, int x, int y, int &R, int &G, int &B) {
  const uint16_t q = ((const uint16_t *) p.data)[(size_t) y * p.w + x];
  R = (q >> 11) << 3, G = ((q >> 5) & 63) << 2, B = (q & 31) << 3;
}

int main(int argc, char **argv) {
  if (argc < 2) { printf("usage: imgdec_test <fixtures dir>\n"); return 2; }
  D = argv[1];
  using fw::imgdec::release;
  {  // a real 32 x 32 RGBA PNG with see-through edges (files/gem.png, the docs' demo gem): pixel art, RGB565 + A8
    auto r = dec(load("gem.png"), {box(32, 32, 0, fw::img::pixel)});
    CHECK(ok(r) && r.px[0].w == 32 && r.px[0].h == 32 && r.px[0].alpha, "gem err=%s", r.err.c_str());
    int clear = 0, solid = 0;
    for (int i = 0; ok(r) && i < 32 * 32; i++) { const uint8_t a = r.px[0].data[32 * 32 * 2 + i]; clear += a == 0; solid += a == 255; }
    CHECK(clear > 100 && solid > 100, "gem alpha clear=%d solid=%d", clear, solid);
    release(r);
  }
  {  // baseline JPEG at full size: colours close to the source gradient, opaque (RGB565 alone, the A8 plane never made)
    auto r = dec(load("photo.jpg"));
    CHECK(ok(r) && r.px[0].w == 640 && r.px[0].h == 480 && !r.px[0].alpha && r.px[0].size == 640 * 480 * 2 && r.sc == 0, "photo.jpg err=%s", r.err.c_str());
    if (ok(r)) {
      long d = 0;
      for (int y = 8; y < 480; y += 37)
        for (int x = 8; x < 640; x += 41) {
          int R, G, B;
          rgb(r.px[0], x, y, R, G, B);
          d += abs(R - (x * 255) / 640) + abs(G - (y * 255) / 480);
        }
      CHECK(d / (13 * 15 * 2) < 10, "photo.jpg colour off by %ld", d / (13 * 15 * 2));
    }
    release(r);
  }
  {  // the descale follows the box: the smallest step that still covers it, never below, never 1/8 (garbage in this
     // TJpgDec: its colours checked here at every step used)
    const int want[][3] = {{80, 60, 2}, {40, 30, 2}, {81, 60, 2}, {160, 120, 2}, {168, 168, 1}, {320, 240, 1}, {321, 240, 0}};
    for (auto &w : want) {
      auto r = dec(load("photo.jpg"), {box(w[0], w[1])});
      long d = 0, n = 0;
      for (int y = 2; ok(r) && y < r.px[0].h; y += std::max(1, r.px[0].h / 9))
        for (int x = 2; x < r.px[0].w; x += std::max(1, r.px[0].w / 9)) {
          int R, G, B;
          rgb(r.px[0], x, y, R, G, B);
          d += abs(R - x * 255 / r.px[0].w) + abs(G - y * 255 / r.px[0].h), n += 2;
        }
      CHECK(n && d / n < 12, "photo.jpg into %dx%d: colour off by %ld", w[0], w[1], n ? d / n : -1);
      CHECK(ok(r) && r.sc == w[2] && r.px[0].w == w[0] && r.px[0].h == w[1], "photo.jpg into %dx%d: 1/%d (want 1/%d) %dx%d err=%s", w[0], w[1], 1 << r.sc,
            1 << w[2], ok(r) ? r.px[0].w : 0, ok(r) ? r.px[0].h : 0, r.err.c_str());
      release(r);
    }
  }
  {  // 8000 x 6000 JPEG (a too-big source): Waveshare 1/4 = 2000 x 1500 into its box; the Guition (1 Mpx) can't go
     // below 1/4 = refused, nothing allocated
    auto r = dec(load("huge-flat.jpg"), {box(720, 720)});
    CHECK(ok(r) && r.sc == 2 && r.sw == 2000 && r.sh == 1500 && r.px[0].w == 720, "huge jpg ws 1/%d %dx%d err=%s", 1 << r.sc, r.sw, r.sh, r.err.c_str());
    release(r);
    const long base = live;
    r = dec(load("huge-flat.jpg"), {box(480, 480)}, GU_PX, MEM);
    CHECK(r.px.empty() && r.err == "too big" && live == base, "huge jpg gu err=%s", r.err.c_str());
    r = dec(load("huge-flat.jpg"), {box(100, 100)}, 300 * 300);
    CHECK(r.px.empty() && r.err == "too big", "huge jpg tight err=%s", r.err.c_str());
  }
  {  // the reserve: what the decode needs, against what is free (not a fixed amount): too little = refused, with numbers
    const long before = live;
    auto r = dec(load("cover.jpg"), {box(480, 480)}, GU_PX, 60 * 1024);
    CHECK(r.px.empty() && r.err.rfind("too big (needs ", 0) == 0, "cover in 60 KB (under even the smallest fallback): err=%s", r.err.c_str());
    CHECK(live == before, "nothing kept after a refusal (%ld B)", live - before);
    printf("refused: %s\n", r.err.c_str());
  }
  {  // the Guition's album art (an HA proxy cover: 640 x 640 4:4:4 q90): idle full screen (its 480 box, kept at 480), the
     // Media screen's CD (168, round) and the Home card's (47, round corner), in ONE decode
    const auto b = load("cover.jpg");
    live_max = live;
    const long base = live;
    auto t0 = std::chrono::steady_clock::now();
    auto r = dec(b, {box(480, 480)}, GU_PX, MEM);
    const double dms = ms_since(t0);
    const long peak = live_max - base;
    CHECK(ok(r) && r.sc == 0 && r.side == 0 && r.px[0].w == 480 && r.px[0].h == 480 && !r.px[0].alpha && r.px[0].size == 480 * 480 * 2, "idle art %dx%d 1/%d err=%s",
          ok(r) ? r.px[0].w : 0, ok(r) ? r.px[0].h : 0, 1 << r.sc, r.err.c_str());
    CHECK(peak <= 480 * 480 * 2 + 64 * 1024, "idle art decode held %ld KB", peak / 1024);
    CHECK((long) r.need >= peak - 1024, "the reserve (%u B) covers what was held (%ld B)", (unsigned) r.need, peak);
    printf("Guition idle art (640 cover -> 480 box, kept 480x480 RGB565): peak %ld KB, reserve %u KB, kept %u KB, host decode %.1f ms\n", peak / 1024,
           (unsigned) (r.need / 1024), (unsigned) (r.px[0].size / 1024), dms);
    release(r);
    // the reserve can't hold 480: kept two thirds of it (320, the JPEG's own 1/2 step), drawn scaled up smoothly
    live_max = live;
    r = dec(b, {box(480, 480)}, GU_PX, 300 * 1024);
    CHECK(ok(r) && r.side == 320 && r.sc == 1 && r.px[0].w == 320 && r.px[0].h == 320, "idle art in 300 KB: %dx%d side %d 1/%d err=%s",
          ok(r) ? r.px[0].w : 0, ok(r) ? r.px[0].h : 0, r.side, 1 << r.sc, r.err.c_str());
    CHECK(live_max - base <= 300 * 1024, "the fallback held %ld KB (300 KB free)", (live_max - base) / 1024);
    printf("Guition idle art, 300 KB free: the fallback %dx%d, peak %ld KB\n", ok(r) ? r.px[0].w : 0, ok(r) ? r.px[0].h : 0, (live_max - base) / 1024);
    release(r);
    live_max = live;
    r = dec(b, {box(480, 480), box(168, 168, 84), box(47, 47, 8)}, GU_PX, MEM);
    CHECK(ok(r, 3) && r.px[1].w == 168 && r.px[1].alpha && r.px[2].w == 47 && r.px[2].alpha, "three boxes err=%s", r.err.c_str());
    if (ok(r, 3)) {
      const uint8_t *a = r.px[1].data + 168 * 168 * 2;
      CHECK(a[0] == 0 && a[84 * 168 + 84] == 255, "the CD's round mask: corner %d, centre %d", a[0], a[84 * 168 + 84]);
    }
    printf("three boxes in one decode: peak %ld KB\n", (live_max - base) / 1024);
    release(r);
    live_max = live;
    r = dec(b, {box(168, 168, 84)}, GU_PX, MEM);
    printf("Guition Media CD art (168, round): 1/%d, peak %ld KB, kept %u KB\n", 1 << r.sc, (live_max - base) / 1024, ok(r) ? (unsigned) (r.px[0].size / 1024) : 0);
    release(r);
    live_max = live;
    t0 = std::chrono::steady_clock::now();
    r = dec(b, {box(720, 720)}, WS_PX, MEM);
    const double wms = ms_since(t0);
    CHECK(ok(r) && r.sc == 0 && r.px[0].w == 720, "Waveshare idle art 720 err=%s", r.err.c_str());
    printf("Waveshare idle art (640 cover -> 720 box): peak %ld KB, kept %u KB, host decode %.1f ms\n", (live_max - base) / 1024, ok(r) ? (unsigned) (r.px[0].size / 1024) : 0, wms);
    release(r);
    CHECK(live == base, "every buffer freed (%ld B)", live - base);
  }
  {  // quality: the album art at each board's idle box against a reference resample of the full-size decode (doubles:
     // an exact area average down to 480, bilinear up to 720). Pixelated art (a smaller copy blown up by nearest
     // neighbour, the 320 -> 480 look) scores far lower: worked out here too, so the bar is known to tell them apart
    const auto b = load("cover.jpg");
    auto full = dec(b, {box(640, 640)}, WS_PX, MEM);
    CHECK(ok(full) && full.px[0].w == 640, "cover at 1:1 err=%s", full.err.c_str());
    const int S = 640;
    std::vector<double> src((size_t) S * S * 3);
    for (int y = 0; ok(full) && y < S; y++)
      for (int x = 0; x < S; x++) { int R, G, B; rgb(full.px[0], x, y, R, G, B); double *q = &src[((size_t) y * S + x) * 3]; q[0] = R, q[1] = G, q[2] = B; }
    // the reference at n x n: area average (n < S) or bilinear with centres aligned (n > S)
    const auto ref = [&](int n) {
      std::vector<double> o((size_t) n * n * 3, 0);
      const double k = (double) S / n;
      for (int y = 0; y < n; y++)
        for (int x = 0; x < n; x++) {
          double *d = &o[((size_t) y * n + x) * 3];
          if (n < S) {
            double wsum = 0;
            for (int sy = (int) (y * k); sy < std::min(S, (int) std::ceil((y + 1) * k)); sy++)
              for (int sx = (int) (x * k); sx < std::min(S, (int) std::ceil((x + 1) * k)); sx++) {
                const double wy = std::min<double>(sy + 1, (y + 1) * k) - std::max<double>(sy, y * k), wx = std::min<double>(sx + 1, (x + 1) * k) - std::max<double>(sx, x * k);
                for (int c = 0; c < 3; c++) d[c] += src[((size_t) sy * S + sx) * 3 + c] * wx * wy;
                wsum += wx * wy;
              }
            for (int c = 0; c < 3; c++) d[c] /= wsum;
          } else {
            const double fx = std::clamp((x + 0.5) * k - 0.5, 0.0, S - 1.0), fy = std::clamp((y + 0.5) * k - 0.5, 0.0, S - 1.0);
            const int x0 = (int) fx, y0 = (int) fy, x1 = std::min(x0 + 1, S - 1), y1 = std::min(y0 + 1, S - 1);
            const double ax = fx - x0, ay = fy - y0;
            for (int c = 0; c < 3; c++)
              d[c] = (src[((size_t) y0 * S + x0) * 3 + c] * (1 - ax) + src[((size_t) y0 * S + x1) * 3 + c] * ax) * (1 - ay) +
                     (src[((size_t) y1 * S + x0) * 3 + c] * (1 - ax) + src[((size_t) y1 * S + x1) * 3 + c] * ax) * ay;
          }
        }
      return o;
    };
    // PSNR (dB) of an n x n picture given by px(x, y, rgb) against the reference
    const auto psnr = [&](const std::vector<double> &o, int n, auto px) {
      double e = 0;
      for (int y = 0; y < n; y++)
        for (int x = 0; x < n; x++) {
          int c[3];
          px(x, y, c);
          for (int i = 0; i < 3; i++) { const double d = c[i] - o[((size_t) y * n + x) * 3 + i]; e += d * d; }
        }
      e /= (double) n * n * 3;
      return e > 0 ? 10 * std::log10(255.0 * 255.0 / e) : 99.0;
    };
    for (const auto &bd : {std::make_pair("Guition", 480), std::make_pair("Waveshare", 720)}) {
      const int n = bd.second;
      const auto o = ref(n);
      auto r = dec(b, {box(n, n)}, n == 480 ? GU_PX : WS_PX, MEM);
      CHECK(ok(r) && r.px[0].w == n && r.px[0].h == n, "%s idle art %dx%d err=%s", bd.first, ok(r) ? r.px[0].w : 0, ok(r) ? r.px[0].h : 0, r.err.c_str());
      const double got = ok(r) ? psnr(o, n, [&](int x, int y, int *c) { rgb(r.px[0], x, y, c[0], c[1], c[2]); }) : 0;
      // the pixelated look: a two-thirds copy (320 / 480) blown up to n by nearest neighbour
      auto sm = dec(b, {box(n * 2 / 3, n * 2 / 3)}, WS_PX, MEM);
      const int m = ok(sm) ? sm.px[0].w : 1;
      const double blocky = ok(sm) ? psnr(o, n, [&](int x, int y, int *c) { rgb(sm.px[0], x * m / n, y * m / n, c[0], c[1], c[2]); }) : 0;
      printf("%s idle art %dx%d: PSNR %.1f dB against the reference (a %d px copy blown up by nearest: %.1f dB)\n", bd.first, n, n, got, m, blocky);
      CHECK(got >= 32, "%s idle art: PSNR %.1f dB (>= 32)", bd.first, got);
      CHECK(blocky < 32 && got - blocky >= 4, "%s: the bar tells pixelated art apart (%.1f vs %.1f dB)", bd.first, got, blocky);
      release(r);
      release(sm);
    }
    release(full);
  }
  {
    auto r = dec(load("grey.jpg"), {box(640, 480)});
    if (ok(r)) { int R, G, B; rgb(r.px[0], 10, 10, R, G, B); CHECK(abs(R - B) <= 8, "grey.jpg is grey: %d %d %d", R, G, B); }
    CHECK(ok(r), "grey.jpg err=%s", r.err.c_str());
    release(r);
    r = dec(load("progressive.jpg"));
    CHECK(r.px.empty() && r.err.find("progressive") != std::string::npos, "progressive err=%s", r.err.c_str());
  }
  for (const char *n : {"photo.png", "rgba.png", "palette.png"}) {
    auto r = dec(load(n), {box(168, 168, 20)});
    CHECK(ok(r) && r.px[0].w == 168 && r.px[0].h == 168 && r.px[0].alpha, "%s err=%s", n, r.err.c_str());
    release(r);
    r = dec(load(n), {box(640, 480)});
    CHECK(ok(r) && !r.px[0].alpha && r.px[0].size == 640 * 480 * 2, "%s opaque: the A8 plane given back (size %u)", n, ok(r) ? (unsigned) r.px[0].size : 0);
    release(r);
  }
  {
    auto r = dec(load("grey16.png"), {box(64, 48)});
    CHECK(ok(r), "grey16 err=%s", r.err.c_str());
    release(r);
    r = dec(load("huge-flat.png"));  // 25 Mpx: refused from the header, nothing allocated
    CHECK(r.px.empty() && r.err == "too big", "huge png err=%s", r.err.c_str());
    r = dec(load("rgba.png"), {box(640, 480)}, WS_PX, 200u * 1024);  // 640 x 480 RGB565 + A8 = 900 KB, and its fallbacks over 200 KB
    CHECK(r.px.empty() && r.err.rfind("too big (needs", 0) == 0, "png mem err=%s", r.err.c_str());
    release(r);
  }
  {  // not pictures / not supported
    auto r = dec(load("page.html"));
    CHECK(r.px.empty() && r.err == "not a picture", "html err=%s", r.err.c_str());
    r = dec(load("photo.gif"));
    CHECK(r.px.empty() && r.err == "GIF isn't supported on the panel", "gif err=%s", r.err.c_str());
    r = dec(load("photo.webp"));
    CHECK(r.px.empty() && r.err == "WebP isn't supported on the panel", "webp err=%s", r.err.c_str());
    std::vector<uint8_t> tiny = {0x89, 'P', 'N'};
    r = dec(tiny);
    CHECK(r.px.empty() && r.err == "not a picture", "tiny err=%s", r.err.c_str());
  }
  {  // truncated files are never pictures: PNG and JPEG both "cut short", nothing kept
    const long base = live;
    for (const char *n : {"photo.png", "photo.jpg", "cover.jpg"})
      for (int part : {2, 4, 10}) {
        auto b = load(n);
        b.resize(b.size() * (part - 1) / part);
        auto r = dec(b, {box(168, 168, 84)}, GU_PX, MEM);
        CHECK(r.px.empty() && r.err.find("cut short") != std::string::npos, "%s cut to %d/%d: err=%s", n, part - 1, part, r.err.c_str());
      }
    CHECK(live == base, "nothing kept after the cut files (%ld B)", live - base);
  }
  {  // the download cap: big.jpg (several MB) streamed in 4 KB reads, as the panel's fetch does
    auto b = load("big.jpg");
    CHECK(b.size() > 1024 * 1024, "big.jpg only %zu bytes", b.size());
    for (size_t cap : {(size_t) 1024 * 1024, (size_t) 256 * 1024}) {
      fw::imgdec::Body body(cap);
      CHECK(body.expect(-1), "expect");
      bool good = true;
      for (size_t at = 0; good && at < b.size(); at += 4096) good = body.add(b.data() + at, std::min<size_t>(4096, b.size() - at));
      CHECK(!good && body.n <= cap, "big.jpg cap %zu: ok=%d n=%zu", cap, good, body.n);
      CHECK(body.err == (cap == 1024 * 1024 ? "too big (over 1 MB)" : "too big (over 256 KB)"), "cap err=%s", body.err.c_str());
      fw::imgdec::Body pre(cap);  // Content-Length over the cap: refused before reading
      CHECK(!pre.expect((long long) b.size()) && pre.b == nullptr, "expect over cap");
    }
    fw::imgdec::Body body(1024 * 1024);  // the sprite fits, and decodes from the body
    auto s = load("gem.png");
    CHECK(body.expect((long long) s.size()) && body.add(s.data(), s.size()), "small body");
    fw::imgdec::Result r;
    CHECK(fw::imgdec::decode(body.b, body.n, GU_PX, MEM, {box(32, 32)}, r) && ok(r), "body decode");
    release(r);
  }
  {  // damaged bytes: never a crash or a sanitizer report (each result is just pixels or a reason), nothing leaks
    const long base = live;
    std::mt19937 rng(1);
    for (const char *n : {"photo.jpg", "gem.png", "palette.png", "grey.jpg", "cover.jpg"}) {
      const auto src = load(n);
      for (int i = 0; i < 150; i++) {
        auto b = src;
        const int hits = 1 + (int) (rng() % 12);
        for (int k = 0; k < hits; k++) b[16 + rng() % (b.size() - 16)] = (uint8_t) rng();
        if (rng() % 4 == 0) b.resize(16 + rng() % (b.size() - 16));
        auto r = dec(b, {box(1 + (int) (rng() % 500), 1 + (int) (rng() % 500), (int) (rng() % 40))}, GU_PX, 8u << 20);
        CHECK(ok(r) ? r.px[0].w > 0 && r.px[0].h > 0 : !r.err.empty() && r.px.empty(), "fuzz %s %d", n, i);
        release(r);
      }
    }
    CHECK(live == base, "fuzz: every buffer freed (%ld B)", live - base);
  }
  printf("imgdec: %d checks, %d failed\n", checks, fails);
  return fails ? 1 : 0;
}
