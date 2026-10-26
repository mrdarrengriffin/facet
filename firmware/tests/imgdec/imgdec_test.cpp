// Host test of the panel picture decode (firmware/fw_imgdec.h): real PNG / JPEG files through the same code the panel
// runs, under ASan + UBSan (run.sh). Usage: imgdec_test <fixtures dir>. Exit 0 = every check passed.
#include <cstdio>
#include <cstdlib>
#include <random>
#include <string>
#include <vector>
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
// budgets as on the boards (renderer core/images.cpp): Waveshare 2048 x 2048 px, Guition 1024 x 1024
constexpr long WS_PX = 2048L * 2048, GU_PX = 1024L * 1024;
constexpr size_t MEM = 24u << 20;

static fw::imgdec::Out dec(const std::vector<uint8_t> &b, long px = WS_PX, size_t mem = MEM) {
  fw::imgdec::Out o;
  fw::imgdec::decode(b.data(), b.size(), px, mem, o);
  return o;
}

int main(int argc, char **argv) {
  if (argc < 2) { printf("usage: imgdec_test <fixtures dir>\n"); return 2; }
  D = argv[1];
  using fw::imgdec::release;
  {  // the Rock Bottom item sprite: 32 x 32 RGBA PNG with see-through edges
    auto o = dec(load("rockbottom.png"));
    CHECK(o.rgba && o.w == 32 && o.h == 32, "rockbottom %dx%d err=%s", o.w, o.h, o.err.c_str());
    int clear = 0, solid = 0;
    for (int i = 0; o.rgba && i < 32 * 32; i++) { clear += o.rgba[i * 4 + 3] == 0; solid += o.rgba[i * 4 + 3] == 255; }
    CHECK(clear > 100 && solid > 100, "rockbottom alpha clear=%d solid=%d", clear, solid);
    release(o);
  }
  {  // baseline JPEG: size + colours close to the source gradient
    auto o = dec(load("photo.jpg"));
    CHECK(o.rgba && o.w == 640 && o.h == 480, "photo.jpg %dx%d err=%s", o.w, o.h, o.err.c_str());
    if (o.rgba) {
      long d = 0;
      for (int y = 8; y < 480; y += 37)
        for (int x = 8; x < 640; x += 41) {
          const uint8_t *p = o.rgba + ((size_t) y * 640 + x) * 4;
          d += abs(p[0] - (x * 255) / 640) + abs(p[1] - (y * 255) / 480);
          CHECK(p[3] == 255, "jpeg alpha");
        }
      CHECK(d / (13 * 15 * 2) < 10, "photo.jpg colour off by %ld", d / (13 * 15 * 2));
    }
    release(o);
  }
  {  // descaled while decoding: 640 x 480 with room for 100 x 100 -> 1/8 = 80 x 60
    auto o = dec(load("photo.jpg"), 100 * 100);
    CHECK(o.rgba && o.w == 80 && o.h == 60, "photo.jpg 1/8 %dx%d err=%s", o.w, o.h, o.err.c_str());
    release(o);
    o = dec(load("photo.jpg"), 320 * 240);
    CHECK(o.rgba && o.w == 320 && o.h == 240, "photo.jpg 1/2 %dx%d", o.w, o.h);
    release(o);
  }
  {  // 8000 x 6000 JPEG: Waveshare 1/4 = 2000 x 1500; Guition 1/8 = 1000 x 750; tighter = too big
    auto o = dec(load("huge-flat.jpg"));
    CHECK(o.rgba && o.w == 2000 && o.h == 1500, "huge jpg ws %dx%d err=%s", o.w, o.h, o.err.c_str());
    release(o);
    o = dec(load("huge-flat.jpg"), GU_PX);
    CHECK(o.rgba && o.w == 1000 && o.h == 750, "huge jpg gu %dx%d err=%s", o.w, o.h, o.err.c_str());
    release(o);
    o = dec(load("huge-flat.jpg"), 300 * 300);
    CHECK(!o.rgba && o.err == "too big", "huge jpg tight err=%s", o.err.c_str());
    o = dec(load("huge-flat.jpg"), WS_PX, 1u << 20);  // memory cap: 1/8 still needs 4.5 MB
    CHECK(!o.rgba && o.err == "too big", "huge jpg mem err=%s", o.err.c_str());
  }
  {
    auto o = dec(load("grey.jpg"));
    CHECK(o.rgba && o.w == 640 && o.h == 480 && o.rgba[0] == o.rgba[1] && o.rgba[1] == o.rgba[2], "grey.jpg err=%s", o.err.c_str());
    release(o);
    o = dec(load("progressive.jpg"));
    CHECK(!o.rgba && o.err.find("progressive") != std::string::npos, "progressive err=%s", o.err.c_str());
  }
  for (const char *n : {"photo.png", "rgba.png", "palette.png"}) {
    auto o = dec(load(n));
    CHECK(o.rgba && o.w == 640 && o.h == 480, "%s %dx%d err=%s", n, o.w, o.h, o.err.c_str());
    release(o);
  }
  {
    auto o = dec(load("grey16.png"));
    CHECK(o.rgba && o.w == 64 && o.h == 48, "grey16 err=%s", o.err.c_str());
    release(o);
    o = dec(load("huge-flat.png"));  // 25 Mpx: refused from the header, nothing allocated
    CHECK(!o.rgba && o.err == "too big", "huge png err=%s", o.err.c_str());
    o = dec(load("rgba.png"), WS_PX, 1u << 20);  // 640 x 480 RGBA needs ~1.2 MB
    CHECK(!o.rgba && o.err == "too big", "png mem err=%s", o.err.c_str());
  }
  {  // not pictures / not supported
    auto o = dec(load("page.html"));
    CHECK(!o.rgba && o.err == "not a picture", "html err=%s", o.err.c_str());
    o = dec(load("photo.gif"));
    CHECK(!o.rgba && o.err == "GIF isn't supported on the panel", "gif err=%s", o.err.c_str());
    o = dec(load("photo.webp"));
    CHECK(!o.rgba && o.err == "WebP isn't supported on the panel", "webp err=%s", o.err.c_str());
    std::vector<uint8_t> tiny = {0x89, 'P', 'N'};
    o = dec(tiny);
    CHECK(!o.rgba && o.err == "not a picture", "tiny err=%s", o.err.c_str());
  }
  {  // truncated files: PNG = broken, JPEG = what came
    auto b = load("photo.png");
    b.resize(b.size() / 2);
    auto o = dec(b);
    CHECK(!o.rgba && o.err.rfind("broken picture", 0) == 0, "half png err=%s", o.err.c_str());
    b = load("photo.jpg");
    b.resize(b.size() / 2);
    o = dec(b);
    CHECK((o.rgba && o.w == 640) || o.err.rfind("broken", 0) == 0, "half jpg err=%s", o.err.c_str());
    release(o);
  }
  {  // the download cap: big.jpg (several MB) streamed in 4 KB reads, as the panel's fetch does
    auto b = load("big.jpg");
    CHECK(b.size() > 1024 * 1024, "big.jpg only %zu bytes", b.size());
    for (size_t cap : {(size_t) 1024 * 1024, (size_t) 256 * 1024}) {
      fw::imgdec::Body body(cap);
      CHECK(body.expect(-1), "expect");
      bool ok = true;
      for (size_t at = 0; ok && at < b.size(); at += 4096) ok = body.add(b.data() + at, std::min<size_t>(4096, b.size() - at));
      CHECK(!ok && body.n <= cap, "big.jpg cap %zu: ok=%d n=%zu", cap, ok, body.n);
      CHECK(body.err == (cap == 1024 * 1024 ? "too big (over 1 MB)" : "too big (over 256 KB)"), "cap err=%s", body.err.c_str());
      fw::imgdec::Body pre(cap);  // Content-Length over the cap: refused before reading
      CHECK(!pre.expect((long long) b.size()) && pre.b == nullptr, "expect over cap");
    }
    fw::imgdec::Body ok(1024 * 1024);  // the sprite fits, and decodes from the body
    auto s = load("rockbottom.png");
    CHECK(ok.expect((long long) s.size()) && ok.add(s.data(), s.size()), "small body");
    fw::imgdec::Out o;
    CHECK(fw::imgdec::decode(ok.b, ok.n, GU_PX, MEM, o) && o.w == 32, "body decode");
    release(o);
  }
  {  // damaged bytes: never a crash or a sanitizer report (each result is just ok or a reason)
    std::mt19937 rng(1);
    for (const char *n : {"photo.jpg", "rockbottom.png", "palette.png", "grey.jpg"}) {
      const auto src = load(n);
      for (int i = 0; i < 150; i++) {
        auto b = src;
        const int hits = 1 + (int) (rng() % 12);
        for (int k = 0; k < hits; k++) b[16 + rng() % (b.size() - 16)] = (uint8_t) rng();
        if (rng() % 4 == 0) b.resize(16 + rng() % (b.size() - 16));
        auto o = dec(b, GU_PX, 8u << 20);
        CHECK(o.rgba ? o.w > 0 && o.h > 0 : !o.err.empty(), "fuzz %s %d", n, i);
        release(o);
      }
    }
  }
  printf("imgdec: %d checks, %d failed\n", checks, fails);
  return fails ? 1 : 0;
}
