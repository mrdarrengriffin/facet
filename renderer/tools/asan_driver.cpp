// Native AddressSanitizer / UBSan driver for the renderer (tools/asan.sh): builds every given item (or, with no
// arguments, every registered foundation, component and screen with its default props, then each of its variants),
// runs 400 frames, presses / drags / releases, takes an update with empty props, frees the stage. Any memory error
// aborts with the sanitizer's report. Usage: fw-asan-<den> [id '<props json>' ...]
#include <cstdint>
#include <csignal>
#include <cstdio>
#include <unistd.h>
#include <string>
#include "ArduinoJson.h"
extern "C" {
void fw_init();
int fw_stage(int, int);
const char *fw_render(int, const char *, const char *, int, int);
void fw_tick(uint32_t);
const char *fw_meta();
const char *fw_events();
void fw_stage_free(int);
void fw_touch(int, int, int, int);
const char *fw_update(int, const char *, const char *, int, int);
}
static uint32_t t = 0;
static void tick(int n) { for (int i = 0; i < n; i++) { t += 16; fw_tick(t); fw_events(); } }
// a time bound per item: a render that never returns (a layout pass that never settles: the 2026-10-27 Layout page,
// hours at 100% CPU) fails the run with its name instead of holding the box's job queue. Normal items take seconds.
constexpr unsigned ITEM_S = 300;  // the slowest item today (icons, all) takes well under a minute under ASan
static const char *g_item = "";
static void on_alarm(int) {
  fprintf(stderr, "asan: %s did not finish within %d s (an endless loop, e.g. a layout that never settles)\n", g_item, ITEM_S);
  _exit(3);
}
// "taps:<comp>" = random taps for a screen's flows (Darren 2026-10-28: the Timer crashed the panel while tapping): 400 taps
// over the lower part of the stage (where a page's buttons and chips are), a few with time jumps so timers run out and
// the done / stop paths run too; deterministic (an LCG), so a report repeats
static void taps(const char *id, const char *props, int S) {
  uint32_t r = 12345;
  auto rnd = [&](int n) { r = r * 1103515245u + 12345u; return (int) ((r >> 8) % (uint32_t) n); };
  const int st = fw_stage(S, S);
  const char *e = fw_render(st, id, props, 0, 0);
  if (e && *e) fprintf(stderr, "   render: %s\n", e);
  tick(60);
  for (int i = 0; i < 400; i++) {
    const int x = S / 8 + rnd(S * 3 / 4), y = S * 2 / 3 + rnd(S / 3 - S / 30);
    fw_touch(st, x, y, 1); tick(6); fw_touch(st, x, y, 0); tick(30);
    if (rnd(10) == 0) { t += 1000u * (uint32_t) (30 + rnd(900)); tick(10); }  // a time jump: timers tick, some run out
  }
  fw_stage_free(st);
}
static void run(const char *id, const char *props, int S) {
  fprintf(stderr, "== %s %s\n", id, props);
  if (!strncmp(id, "taps:", 5)) { g_item = id; signal(SIGALRM, on_alarm); alarm(ITEM_S); taps(id + 5, props, S); alarm(0); return; }
  g_item = id;
  signal(SIGALRM, on_alarm);
  alarm(ITEM_S);
  for (int light = 0; light < 2; light++) {
    const int st = fw_stage(S, S);
    const char *e = fw_render(st, id, props, light, 0);
    if (e && *e) fprintf(stderr, "   render: %s\n", e);
    tick(400);
    fw_touch(st, S / 2, S / 4, 1); tick(5);
    fw_touch(st, S * 3 / 4, S / 2, 1); tick(5);
    fw_touch(st, S * 3 / 4, S / 2, 0); tick(60);
    fw_update(st, id, props, light, 0); tick(30);
    fw_update(st, id, "{}", light, 0); tick(60);
    fw_stage_free(st);
  }
  alarm(0);
}
int main(int argc, char **argv) {
  fw_init();
#ifdef FW_1X
  const int S = 480;
#else
  const int S = 720;
#endif
  if (argc > 1) {
    for (int a = 1; a + 1 < argc; a += 2) run(argv[a], argv[a + 1], S);
  } else {
    ArduinoJson::JsonDocument doc;
    deserializeJson(doc, fw_meta());
    for (ArduinoJson::JsonObject c : doc.as<ArduinoJson::JsonArray>()) {
      const std::string id = c["id"] | "";
      if (id == "device") continue;
      run(id.c_str(), "{}", S);
      ArduinoJson::JsonDocument meta;
      const char *m = c["meta"] | "";
      if (c["meta"].is<ArduinoJson::JsonObject>()) meta.set(c["meta"]); else deserializeJson(meta, m);
      for (ArduinoJson::JsonObject v : meta["variants"].as<ArduinoJson::JsonArray>()) {
        std::string p;
        serializeJson(v["props"], p);
        run(id.c_str(), p.c_str(), S);
      }
    }
  }
  fprintf(stderr, "asan: done, no reports\n");
  return 0;
}
