// Host stress test of the panel's image fetch (firmware/fw_images.h, the code the panel runs: worker task, queue, pump,
// hand-off to the LVGL task) on simulated ESP-IDF (shim/): FreeRTOS tasks = threads, esp_http_client = a fake whose
// behaviour comes from the URL, heap_caps with failure injection, time SPEED x faster than real. Built twice by run.sh:
// ASan + UBSan, and TSan (data races between the worker and the "LVGL" main thread).
//   fetch_test <fixtures dir>   (rockbottom.png + photo.jpg from firmware/tests/imgdec)
// Checks: every wanted URL ends ready or failed (never loading for ever), the right ones ready, one download at a time,
// no deadlock (a watchdog thread: the main loop must keep beating), works again after the worker's idle exit (the
// 2026-10-26 crash: the worker deleted itself holding the lock), and alloc / task / low-memory failures only fail images.
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>
#include <new>
#include <random>
#include <set>
#include <string>
#include <strings.h>
#include <thread>
#include <unistd.h>
#include <vector>

// ---------------------------------------------------------------- simulation knobs
static constexpr int SPEED = 50;  // simulated ms per real ms
static std::atomic<double> P_ALLOC{0}, P_LOW{0}, P_TASK{0}, P_NEW{0};
static std::mutex rng_m;
static std::mt19937 rng(12345);
static bool roll(double p) {
  if (p <= 0) return false;
  std::lock_guard<std::mutex> l(rng_m);
  return std::uniform_real_distribution<double>(0, 1)(rng) < p;
}
bool sim_verbose = false;
static const auto T0 = std::chrono::steady_clock::now();
static int64_t sim_us() { return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - T0).count() * SPEED; }
static void sim_sleep_ms(int64_t ms) { std::this_thread::sleep_for(std::chrono::microseconds(ms * 1000 / SPEED)); }

// ---------------------------------------------------------------- heap
static std::atomic<long> allocs{0}, alloc_fails{0};
void *heap_caps_malloc(size_t n, unsigned) {
  if (roll(P_ALLOC)) { alloc_fails++; return nullptr; }
  allocs++;
  return malloc(n);
}
void heap_caps_free(void *p) { free(p); }
size_t heap_caps_get_free_size(unsigned caps) { return roll(P_LOW) ? 9000 : (caps & (1 << 10)) ? 30u << 20 : 200u << 10; }
size_t heap_caps_get_largest_free_block(unsigned caps) { return roll(P_LOW) ? 4000 : (caps & (1 << 10)) ? 28u << 20 : 100u << 10; }
#define FW_IMGDEC_ALLOC(n) heap_caps_malloc(n, 0)
#define FW_IMGDEC_FREE(p) heap_caps_free(p)
// a nothrow new that can fail (fw_images.h: new (std::nothrow) Done)
void *operator new(size_t n, const std::nothrow_t &) noexcept {
  if (roll(P_NEW)) return nullptr;
  return ::operator new(n);  // (paired with the normal delete)
}
int64_t esp_timer_get_time() { return sim_us(); }

// ---------------------------------------------------------------- FreeRTOS
#include "freertos/task.h"
struct SimTask {
  std::mutex m;
  std::condition_variable cv;
  uint32_t notes = 0;
};
static thread_local SimTask *self_task = nullptr;
static std::atomic<int> tasks_made{0}, tasks_gone{0}, tasks_live{0};
BaseType_t xTaskCreatePinnedToCore(TaskFunction_t f, const char *, uint32_t, void *arg, UBaseType_t, TaskHandle_t *out, BaseType_t) {
  if (roll(P_TASK)) return 0;
  SimTask *t = new SimTask;  // never freed: a deleted task may still be referenced by a racing notify (the bug class we test)
  *out = t;
  tasks_made++; tasks_live++;
  std::thread([f, arg, t] { self_task = t; f(arg); }).detach();
  return pdPASS;
}
uint32_t ulTaskNotifyTake(BaseType_t clear, TickType_t wait) {
  SimTask *t = self_task;
  std::unique_lock<std::mutex> l(t->m);
  t->cv.wait_for(l, std::chrono::microseconds((int64_t) wait * 1000 / SPEED), [t] { return t->notes > 0; });
  const uint32_t n = t->notes;
  if (clear) t->notes = 0; else if (n) t->notes--;
  return n;
}
void xTaskNotifyGive(TaskHandle_t t) {
  std::lock_guard<std::mutex> l(t->m);
  t->notes++;
  t->cv.notify_all();
}
void vTaskDelete(TaskHandle_t t) {
  if (t) { fprintf(stderr, "unexpected vTaskDelete(other)\n"); abort(); }
  tasks_gone++; tasks_live--;
  for (;;) pause();  // gone: nothing on its stack unwinds (a lock held here stays held, as on the panel)
}
void vTaskDelay(TickType_t t) { sim_sleep_ms(t); }
UBaseType_t uxTaskGetStackHighWaterMark(TaskHandle_t) { return 4096; }

// ---------------------------------------------------------------- HTTP: behaviour from the URL https://t/<kind>/<n>
#include "esp_http_client.h"
static std::vector<uint8_t> PNG, JPG;
static std::atomic<int> open_now{0}, open_max{0}, opens{0};
struct esp_http_client {
  esp_http_client_config_t c;
  std::string url, kind;
  std::vector<uint8_t> body;
  size_t at = 0;
  int status = 0, hops = 0;
  int64_t len = -1;
  bool is_open = false;
};
static std::string kind_of(const std::string &u) {
  const size_t a = u.find("://t/");
  if (a == std::string::npos) return "err";
  const size_t b = u.find('/', a + 5);
  return u.substr(a + 5, b - a - 5);
}
esp_http_client_handle_t esp_http_client_init(const esp_http_client_config_t *c) {
  if (roll(P_ALLOC)) return nullptr;
  auto *h = new esp_http_client;
  h->c = *c;
  h->url = c->url;
  return h;
}
esp_err_t esp_http_client_set_header(esp_http_client_handle_t, const char *, const char *) { return ESP_OK; }
static void hdr(esp_http_client_handle_t h, const char *k, const char *v) {
  esp_http_client_event_t e{};
  e.event_id = HTTP_EVENT_ON_HEADER;
  e.client = h;
  e.user_data = h->c.user_data;
  e.header_key = (char *) k;
  e.header_value = (char *) v;
  h->c.event_handler(&e);
}
esp_err_t esp_http_client_open(esp_http_client_handle_t h, int) {
  if (h->is_open) { fprintf(stderr, "open twice\n"); abort(); }
  const int now = ++open_now;
  for (int m = open_max; now > m && !open_max.compare_exchange_weak(m, now);) {}
  opens++;
  h->is_open = true;
  h->kind = kind_of(h->url);
  sim_sleep_ms(30);  // connect + TLS
  if (h->kind == "hang") { sim_sleep_ms(h->c.timeout_ms); return ESP_FAIL; }
  if (h->kind == "err" || roll(P_ALLOC)) return ESP_FAIL;
  return ESP_OK;
}
int64_t esp_http_client_fetch_headers(esp_http_client_handle_t h) {
  const std::string &k = h->kind;
  h->at = 0; h->len = -1; h->body.clear(); h->status = 200;
  if (k == "404") { h->status = 404; return 0; }
  if (k == "500") { h->status = 500; return 0; }
  if (k == "bot") { h->status = 403; hdr(h, "cf-mitigated", "challenge"); return 0; }
  if (k == "loop" || (k == "redir" && h->hops == 0)) { h->status = 302; return 0; }
  if (k == "html") { hdr(h, "Content-Type", "text/html; charset=utf-8"); h->body.assign(300, 'x'); memcpy(h->body.data(), "<html>", 6); h->len = 300; return h->len; }
  if (k == "big") { h->len = 50ll << 20; return h->len; }
  if (k == "jpg") { hdr(h, "Content-Type", "image/jpeg"); h->body = JPG; }
  else { hdr(h, "Content-Type", "image/png"); h->body = PNG; }
  if (k == "trunc") h->body.resize(h->body.size() / 2);  // the length says more is coming: it never does
  h->len = k == "nolen" ? -1 : (int64_t) (k == "trunc" ? PNG.size() : h->body.size());
  if (k == "garbage") for (size_t i = 16; i < h->body.size(); i++) h->body[i] ^= (uint8_t) (i * 7);
  return h->len;
}
int esp_http_client_get_status_code(esp_http_client_handle_t h) { return h->status; }
esp_err_t esp_http_client_set_redirection(esp_http_client_handle_t h) { h->hops++; return ESP_OK; }
int esp_http_client_read(esp_http_client_handle_t h, char *buf, int n) {
  if (h->kind == "slow") { sim_sleep_ms(400); }
  if (h->kind == "drop" && h->at > 100) return -1;
  if (h->at >= h->body.size()) { if (h->kind == "trunc") sim_sleep_ms(50); return 0; }
  const int k = std::min<int>(n, std::min<size_t>(h->kind == "slow" ? 16 : 1500, h->body.size() - h->at));
  memcpy(buf, h->body.data() + h->at, k);
  h->at += k;
  return k;
}
bool esp_http_client_is_complete_data_received(esp_http_client_handle_t h) { return h->kind != "trunc" && h->at >= h->body.size(); }
esp_err_t esp_http_client_close(esp_http_client_handle_t h) {
  if (h->is_open) { h->is_open = false; open_now--; }
  return ESP_OK;
}
esp_err_t esp_http_client_cleanup(esp_http_client_handle_t h) { esp_http_client_close(h); delete h; return ESP_OK; }

// ---------------------------------------------------------------- the fake image cache (the "LVGL" main thread only)
#include "lvgl.h"
static std::thread::id lvgl_thread;
static lv_timer_cb_t pump_cb = nullptr;
lv_timer_t *lv_timer_create(lv_timer_cb_t cb, uint32_t, void *) { pump_cb = cb; return (lv_timer_t *) &pump_cb; }
uint32_t lv_tick_get() { return (uint32_t) (sim_us() / 1000); }
#include "images.h"
static int fails = 0, checks = 0;
#define CHECK(c, ...) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d  ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
namespace fw::img {
static const Budget B = {3u << 20, 2048 * 2048, 8000, "waveshare", 1u << 20};
const Budget &budget() { return B; }
struct P { uint32_t since; bool told; };
static std::map<std::string, P> pending;
struct V { int st = 0; std::string err; };  // 0 loading, 1 ready, 2 failed
static std::map<std::string, V> views;
static void on_lvgl() { if (std::this_thread::get_id() != lvgl_thread) { printf("FAIL: fw::img called off the LVGL thread\n"); abort(); } }
std::vector<std::string> wants() {
  on_lvgl();
  std::vector<std::string> o;
  for (auto &kv : pending) if (!kv.second.told) { kv.second.told = true; o.push_back(kv.first); }
  return o;
}
void put(const char *src, int w, int h, const uint8_t *rgba) {
  on_lvgl();
  CHECK(w > 0 && h > 0 && rgba, "put %s %dx%d", src, w, h);
  unsigned sum = 0;
  for (size_t i = 0; i < (size_t) w * h * 4; i++) sum += rgba[i];  // every pixel readable (ASan)
  (void) sum;
  pending.erase(src);
  auto it = views.find(src);
  if (it != views.end()) { it->second.st = 1; it->second.err.clear(); }
}
void fail(const char *src, const char *why) {
  on_lvgl();
  pending.erase(src);
  auto it = views.find(src);
  if (it != views.end() && it->second.st == 0) { it->second.st = 2; it->second.err = why; }
}
bool waiting(const char *src) { on_lvgl(); return pending.count(src) > 0; }
void alive(const char *src) { on_lvgl(); auto it = pending.find(src); if (it != pending.end()) it->second.since = lv_tick_get(); }
static void timeouts() {
  std::vector<std::string> late;
  for (auto &kv : pending) if (lv_tick_get() - kv.second.since >= (uint32_t) B.timeout_ms) late.push_back(kv.first);
  for (auto &u : late) fail(u.c_str(), "timeout");
}
static void want(const std::string &u) { views[u] = V{}; pending[u] = P{lv_tick_get(), false}; }
static void leave() { views.clear(); pending.clear(); }
}  // namespace fw::img

#include "../../fw_images.h"

// ---------------------------------------------------------------- driver
static std::atomic<int64_t> beat{0};
static void tick() {  // one LVGL timer round: 100 simulated ms
  beat = sim_us();
  fw::img::timeouts();
  if (pump_cb) pump_cb(nullptr);
  sim_sleep_ms(100);
}
static void run_sim_ms(int64_t ms) { for (const int64_t end = sim_us() + ms * 1000; sim_us() < end;) tick(); }
// run until nothing is loading (or the limit): returns the simulated ms it took
static int64_t settle(int64_t limit_ms) {
  const int64_t t0 = sim_us();
  for (;;) {
    int loading = 0;
    for (auto &kv : fw::img::views) loading += kv.second.st == 0;
    if (!loading || sim_us() - t0 > limit_ms * 1000) break;
    tick();
  }
  return (sim_us() - t0) / 1000;
}
static std::vector<uint8_t> load(const std::string &p) {
  std::vector<uint8_t> v;
  FILE *f = fopen(p.c_str(), "rb");
  if (!f) { printf("missing fixture %s\n", p.c_str()); exit(2); }
  uint8_t b[8192];
  size_t k;
  while ((k = fread(b, 1, sizeof b, f)) > 0) v.insert(v.end(), b, b + k);
  fclose(f);
  return v;
}
static const char *KINDS[] = {"ok", "ok", "ok", "jpg", "nolen", "redir", "404", "500", "bot", "html", "big", "trunc", "drop", "garbage", "err", "loop", "ok", "ok", "slow", "hang"};
static const std::set<std::string> GOOD = {"ok", "jpg", "nolen", "redir"};
static std::vector<std::string> batch(const char *tag, int n, bool with_slow = true) {
  std::vector<std::string> v;
  for (int i = 0; i < n; i++) {
    const char *k = KINDS[i % (with_slow ? 20 : 18)];
    v.push_back(std::string("https://t/") + k + "/" + tag + std::to_string(i) + ".png");
  }
  return v;
}
struct Tally { int ready = 0, failed = 0, loading = 0, good_ready = 0, good = 0, too_many = 0; };
static Tally tally() {
  Tally t;
  for (auto &kv : fw::img::views) {
    const bool g = GOOD.count(kind_of(kv.first)) > 0;
    t.good += g;
    if (kv.second.st == 0) t.loading++;
    if (kv.second.st == 1) { t.ready++; t.good_ready += g; CHECK(g, "a bad URL came out ready: %s", kv.first.c_str()); }
    if (kv.second.st == 2) { t.failed++; t.too_many += kv.second.err == "too many pictures"; }
  }
  return t;
}

int main(int argc, char **argv) {
  if (argc < 2) { printf("usage: fetch_test <fixtures dir>\n"); return 2; }
  sim_verbose = getenv("VERBOSE") != nullptr;
  PNG = load(std::string(argv[1]) + "/rockbottom.png");
  JPG = load(std::string(argv[1]) + "/photo.jpg");
  lvgl_thread = std::this_thread::get_id();
  std::thread([] {  // watchdog: the "LVGL task" must keep running (a deadlocked pump = the panel's task WDT reset)
    for (;;) {
      std::this_thread::sleep_for(std::chrono::milliseconds(200));
      if (beat && sim_us() - beat > 30000ll * 1000) { printf("FAIL: the LVGL loop stopped (deadlock) at %lld ms\n", (long long) (sim_us() / 1000)); fflush(stdout); _exit(1); }
    }
  }).detach();
  fw::device::images::setup();
  CHECK(pump_cb, "setup made the pump timer");

  {  // 1. 100 mixed URLs at once (the TBOI grid x3): all settle, the good ones as pictures, one download at a time
    for (auto &u : batch("a", 100)) fw::img::want(u);
    const int64_t ms = settle(20 * 60 * 1000);
    const Tally t = tally();
    printf("100 mixed: %d ready, %d failed, %d loading in %lld sim ms; max downloads at once %d\n", t.ready, t.failed, t.loading, (long long) ms, open_max.load());
    CHECK(t.loading == 0, "%d still loading", t.loading);
    CHECK(t.good_ready == t.good, "good %d of %d ready", t.good_ready, t.good);
    CHECK(open_max == 1, "one download at a time (max %d)", open_max.load());
    CHECK(t.too_many == 0, "100 fit the queue (%d too many)", t.too_many);
  }
  {  // 2. idle past 20 s: the worker task exits; then a new batch must still load (2026-10-26: deadlocked here)
    const int gone = tasks_gone;
    run_sim_ms(25000);
    CHECK(tasks_gone > gone, "the idle worker exited");
    fw::img::leave();
    for (auto &u : batch("b", 10, false)) fw::img::want(u);
    settle(5 * 60 * 1000);
    const Tally t = tally();
    CHECK(t.loading == 0 && t.good_ready == t.good, "after the idle exit: %d loading, good %d of %d", t.loading, t.good_ready, t.good);
    printf("after idle exit: %d ready, %d failed; tasks made %d, gone %d\n", t.ready, t.failed, tasks_made.load(), tasks_gone.load());
  }
  {  // 3. too many at once: the excess fails "too many pictures" at once, the rest load
    fw::img::leave();
    run_sim_ms(300);
    const auto v = batch("c", 200, false);
    for (auto &u : v) fw::img::want(u);
    settle(20 * 60 * 1000);
    const Tally t = tally();
    printf("200 at once: %d ready, %d failed (%d too many), %d loading\n", t.ready, t.failed, t.too_many, t.loading);
    CHECK(t.loading == 0, "%d still loading", t.loading);
    CHECK(t.too_many == 200 - (int) fw::device::images::MAX_QUEUE, "excess = too many pictures (%d)", t.too_many);
  }
  {  // 4. leave the screen mid-download, come back: nothing waits for the old URLs, the new ones load
    fw::img::leave();
    for (auto &u : batch("d", 60)) fw::img::want(u);
    run_sim_ms(700);
    fw::img::leave();
    run_sim_ms(1500);
    for (auto &u : batch("e", 20, false)) fw::img::want(u);
    settle(10 * 60 * 1000);
    const Tally t = tally();
    CHECK(t.loading == 0 && t.good_ready == t.good, "after leaving: %d loading, good %d of %d", t.loading, t.good_ready, t.good);
    printf("leave + return: %d ready, %d failed\n", t.ready, t.failed);
  }
  {  // 5. failure injection: allocations, nothrow new, task creation, low memory. Images may fail; nothing may crash or hang
    P_ALLOC = 0.03; P_NEW = 0.05; P_TASK = 0.2; P_LOW = 0.05;
    for (int round = 0; round < 3; round++) {
      fw::img::leave();
      for (auto &u : batch(("f" + std::to_string(round) + "_").c_str(), 100)) fw::img::want(u);
      settle(30 * 60 * 1000);
      const Tally t = tally();
      printf("injected round %d: %d ready, %d failed, %d loading (alloc fails so far %ld)\n", round, t.ready, t.failed, t.loading, alloc_fails.load());
      CHECK(t.loading == 0, "round %d: %d still loading", round, t.loading);
      CHECK(t.ready > 0, "round %d: some pictures still come through", round);
      run_sim_ms(round == 1 ? 25000 : 500);  // one idle exit under injection too
    }
    P_ALLOC = 0; P_NEW = 0; P_TASK = 0; P_LOW = 0;
  }
  fw::img::leave();
  run_sim_ms(1000);  // drain the last results
  beat = 0;  // done: the watchdog stands down
  CHECK(open_now == 0, "no download left open (%d)", open_now.load());
  printf("%d checks, %d failed; downloads %d, max at once %d\n", checks, fails, opens.load(), open_max.load());
  fflush(stdout);
  _exit(fails ? 1 : 0);  // (the worker thread may still be parked; don't run static destructors under it)
}
