// Facet UI sounds on the Waveshare ESP32-P4-86 (facet_audio.h). Same I2S path as the approved sound tests
// (soundui/soundui.h: ONE port, MCLK 256 fs, ES8311 on DOUT 9) minus the mic: TX only, 24 kHz, and a mixer.
#include "facet_audio.h"
#include "facet_sounds_gen.h"

#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "driver/gpio.h"
#include "driver/i2s_std.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esphome/core/log.h"

#ifndef FACET_AUDIO_AMP_PIN
#define FACET_AUDIO_AMP_PIN 53
#endif
#ifndef FACET_AUDIO_AMP_IDLE_MS
#define FACET_AUDIO_AMP_IDLE_MS 30000
#endif

namespace facet_audio {
namespace {
const char *const TAG = "facet_audio";
constexpr uint32_t RATE = bank::RATE;
constexpr size_t FRAMES = 64;  // per I2S block: 2.7 ms at 24 kHz
constexpr int DMA_DESC = 3;
constexpr int VOICES = 6;      // mixed at once; a seventh replaces the oldest
constexpr int QLEN = 16;       // request queue (UI task -> audio task), power of two

struct Req { int16_t pack, type; uint32_t step; int32_t gain; };  // step Q16 (1.0 = 65536), gain Q15
struct Voice { const int16_t *p = nullptr; uint32_t n = 0; uint64_t pos = 0; uint32_t step = 0; int32_t gain = 0; uint32_t born = 0; };

i2s_chan_handle_t tx = nullptr;
TaskHandle_t task_h = nullptr;
bool ok = false;
Req ring[QLEN];
std::atomic<uint32_t> head{0}, tail{0};  // head: UI task writes, tail: audio task
std::atomic<bool> wake_req{false};
// counters (status())
std::atomic<uint32_t> plays{0}, steals{0}, drops{0}, tx_err{0}, amp_ons{0};
std::atomic<int64_t> last_req_us{0}, last_pick_us{0};
char st[192] = "not started";

void amp(bool on) {
#if FACET_AUDIO_AMP_PIN >= 0
  gpio_set_level((gpio_num_t) FACET_AUDIO_AMP_PIN, on ? 1 : 0);
#endif
}

void push(const Req &r) {
  const uint32_t h = head.load(std::memory_order_relaxed);
  if (h - tail.load(std::memory_order_acquire) >= QLEN) { drops++; return; }  // full: the audio task is behind
  ring[h & (QLEN - 1)] = r;
  head.store(h + 1, std::memory_order_release);
  last_req_us.store(esp_timer_get_time());
  if (task_h) xTaskNotifyGive(task_h);
}

void task(void *) {
  static int16_t buf[FRAMES * 2];
  static int32_t acc[FRAMES];
  Voice v[VOICES];
  uint32_t born = 0;
  bool amp_on = false;
  int64_t last_active = 0;
  for (;;) {
    bool any = false;
    for (auto &x : v) any |= x.p != nullptr;
    if (!any) {
      // idle: sleep until a request or a touch; while the amp is on, wake now and then to switch it off in time
      ulTaskNotifyTake(pdTRUE, amp_on ? pdMS_TO_TICKS(250) : portMAX_DELAY);
    }
    const int64_t now = esp_timer_get_time();
    if (wake_req.exchange(false)) {
      if (!amp_on) { amp(true); amp_on = true; amp_ons++; }
      last_active = now;
    }
    for (uint32_t t = tail.load(std::memory_order_relaxed); t != head.load(std::memory_order_acquire); t++) {
      const Req r = ring[t & (QLEN - 1)];
      tail.store(t + 1, std::memory_order_release);
      const bank::Clip &c = bank::CLIPS[r.pack][r.type];
      Voice *slot = nullptr;
      for (auto &x : v) if (!x.p) { slot = &x; break; }
      if (!slot) {  // all busy: the oldest makes way
        slot = &v[0];
        for (auto &x : v) if (x.born < slot->born) slot = &x;
        steals++;
      }
      *slot = Voice{bank::pcm() + c.off, c.n, 0, r.step, r.gain, born++};
      plays++;
      last_pick_us.store(now);
      if (!amp_on) { amp(true); amp_on = true; amp_ons++; }
      last_active = now;
      any = true;
    }
    if (!any) {
      if (amp_on && now - last_active > (int64_t) FACET_AUDIO_AMP_IDLE_MS * 1000) { amp(false); amp_on = false; }
      continue;
    }
    // mix one block
    memset(acc, 0, sizeof acc);
    for (auto &x : v) {
      if (!x.p) continue;
      for (size_t i = 0; i < FRAMES; i++) {
        const uint32_t k = (uint32_t) (x.pos >> 16);
        if (k + 1 >= x.n) { x.p = nullptr; break; }
        int32_t s = x.p[k];
        if (x.step != 65536) s += (int32_t) (((int64_t) (x.p[k + 1] - s) * (int64_t) (x.pos & 0xFFFF)) >> 16);
        acc[i] += (s * x.gain) >> 15;
        x.pos += x.step;
      }
    }
    for (size_t i = 0; i < FRAMES; i++) {
      const int32_t s = acc[i] > 32767 ? 32767 : acc[i] < -32768 ? -32768 : acc[i];
      buf[i * 2] = buf[i * 2 + 1] = (int16_t) s;
    }
    size_t done = 0;
    if (i2s_channel_write(tx, buf, sizeof buf, &done, pdMS_TO_TICKS(50)) != ESP_OK) tx_err++;
    last_active = esp_timer_get_time();
  }
}

int pack_index(const char *pack) {
  if (pack)
    for (int i = 0; i < bank::NPACKS; i++)
      if (!strcasecmp(pack, bank::PACKS[i])) return i;
  return bank::DEFAULT_PACK;
}
int type_index(const char *id) {
  if (id)
    for (int i = 0; i < bank::NTYPES; i++)
      if (!strcmp(id, bank::TYPES[i])) return i;
  return -1;
}
}  // namespace

void play(int pack, int type, float pos, float gain) {
  if (!ok || pack < 0 || pack >= bank::NPACKS || type < 0 || type >= bank::NTYPES || gain <= 0.0f) return;
  const bank::Clip &c = bank::CLIPS[pack][type];
  float rate = 1.0f;
  if (c.pitched && pos >= 0.0f) {
    const float p = pos > 1.0f ? 1.0f : pos;
    rate = powf(2.0f, (bank::PITCH_LOW + p * (bank::PITCH_HIGH - bank::PITCH_LOW)) / 12.0f);
  }
  Req r{(int16_t) pack, (int16_t) type, (uint32_t) lroundf(rate * 65536.0f), (int32_t) lroundf((gain > 1.0f ? 1.0f : gain) * 32767.0f)};
  push(r);
}

void ui_sound(const char *id, const char *detail, const char *sounds, const char *pack, const char *volume) {
  if (sounds && (!strcmp(sounds, "false") || !strcmp(sounds, "off"))) return;  // UI sounds off: nothing plays
  const float vol = volume ? (float) atof(volume) : 80.0f;
  if (vol <= 0.0f) return;
  const int t = type_index(id);
  if (t < 0) { ESP_LOGW(TAG, "unknown sound %s", id ? id : "(null)"); return; }
  const float pos = detail && *detail ? (float) atof(detail) : -1.0f;
  play(pack_index(pack), t, pos, vol / 100.0f);
}

void wake() {
  if (!ok) return;
  if (!wake_req.exchange(true) && task_h) xTaskNotifyGive(task_h);
}

const char *status() {
  if (ok) {
    const int64_t q = last_pick_us.load() - last_req_us.load();
    snprintf(st, sizeof st, "I2S 24 kHz, %u-frame blocks x %d DMA (%.1f ms), %d voices; plays %u, steals %u, drops %u, tx errors %u, amp ons %u, last pickup %.2f ms",
             (unsigned) FRAMES, DMA_DESC, FRAMES * DMA_DESC * 1000.0f / RATE, VOICES, (unsigned) plays.load(), (unsigned) steals.load(),
             (unsigned) drops.load(), (unsigned) tx_err.load(), (unsigned) amp_ons.load(), q >= 0 ? q / 1000.0f : 0.0f);
  }
  return st;
}

void setup(int port) {
#if FACET_AUDIO_AMP_PIN >= 0
  gpio_config_t g = {};
  g.pin_bit_mask = 1ULL << FACET_AUDIO_AMP_PIN;
  g.mode = GPIO_MODE_OUTPUT;
  gpio_config(&g);
  amp(false);
#endif
  i2s_chan_config_t cc = I2S_CHANNEL_DEFAULT_CONFIG((i2s_port_t) port, I2S_ROLE_MASTER);
  cc.auto_clear = true;  // nothing written = zeros (the DMA keeps running between sounds)
  cc.dma_desc_num = DMA_DESC;
  cc.dma_frame_num = FRAMES;
  esp_err_t e = i2s_new_channel(&cc, &tx, nullptr);
  if (e != ESP_OK) { snprintf(st, sizeof st, "i2s_new_channel failed: %s", esp_err_to_name(e)); ESP_LOGE(TAG, "%s", st); return; }
  i2s_std_config_t sc = {
      .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(RATE),
      .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
      .gpio_cfg = {.mclk = GPIO_NUM_13, .bclk = GPIO_NUM_12, .ws = GPIO_NUM_10, .dout = GPIO_NUM_9, .din = I2S_GPIO_UNUSED,
                   .invert_flags = {.mclk_inv = false, .bclk_inv = false, .ws_inv = false}},
  };
  sc.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;  // 6.144 MHz (ES8311 coefficient table: 24 kHz at 256 fs)
  if ((e = i2s_channel_init_std_mode(tx, &sc)) != ESP_OK || (e = i2s_channel_enable(tx)) != ESP_OK) {
    snprintf(st, sizeof st, "I2S init failed: %s", esp_err_to_name(e));
    ESP_LOGE(TAG, "%s", st);
    return;
  }
  ok = true;
  xTaskCreatePinnedToCore(task, "facet_audio", 4096, nullptr, 19, &task_h, tskNO_AFFINITY);
  size_t bytes = 0;
  for (int p = 0; p < bank::NPACKS; p++)
    for (int t = 0; t < bank::NTYPES; t++) bytes += bank::CLIPS[p][t].n * 2;
  ESP_LOGI(TAG, "%s; %d packs x %d sounds, %u KB in flash", status(), bank::NPACKS, bank::NTYPES, (unsigned) (bytes / 1024));
}
}  // namespace facet_audio
