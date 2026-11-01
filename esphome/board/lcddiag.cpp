// See lcddiag.h.
#include "lcddiag.h"
#include <driver/gpio.h>
#include <esp_rom_sys.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace panel {
namespace lcd {

static constexpr gpio_num_t CS = GPIO_NUM_39, SCK = GPIO_NUM_48, SDA = GPIO_NUM_47;
static int HP = 5;  // half period us

// ESPHome 2026.9.1 GUITION-4848S040 init with transform mirror_x/y false (copied from the generated main.cpp,
// set_init_sequence): records cmd, n, data.. ; (ms, 0xFF) = delay.
static const uint8_t INIT[] = {
    1, 0, 255, 5, 119, 1, 0, 0, 16, 192, 2, 59, 0, 193, 2, 13, 2, 194, 2, 49, 5, 176, 16, 0, 17, 24, 14, 17, 6, 7, 8,
    7, 34, 4, 18, 15, 170, 49, 24, 177, 16, 0, 17, 25, 14, 18, 7, 8, 8, 8, 34, 4, 17, 17, 169, 50, 24, 255, 5, 119, 1,
    0, 0, 17, 176, 1, 96, 177, 1, 50, 178, 1, 7, 179, 1, 128, 181, 1, 73, 183, 1, 133, 184, 1, 33, 193, 1, 120, 194,
    1, 120, 224, 3, 0, 27, 2, 225, 11, 8, 160, 0, 0, 7, 160, 0, 0, 0, 68, 68, 226, 12, 17, 17, 68, 68, 237, 160, 0, 0,
    236, 160, 0, 0, 227, 4, 0, 0, 17, 17, 228, 2, 68, 68, 229, 16, 10, 233, 216, 160, 12, 235, 216, 160, 14, 237, 216,
    160, 16, 239, 216, 160, 230, 4, 0, 0, 17, 17, 231, 2, 68, 68, 232, 16, 9, 232, 216, 160, 11, 234, 216, 160, 13,
    236, 216, 160, 15, 238, 216, 160, 235, 7, 2, 0, 228, 228, 136, 0, 64, 236, 2, 60, 0, 237, 16, 171, 137, 118, 84,
    2, 255, 255, 255, 255, 255, 255, 32, 69, 103, 152, 186, 255, 5, 119, 1, 0, 0, 19, 229, 1, 228, 255, 5, 119, 1, 0,
    0, 16, 205, 1, 8, 205, 1, 0, 58, 1, 102, 54, 1, 0, 199, 1, 0, 32, 0, 0, 255, 17, 0, 10, 255, 41, 0, 10, 255};

static void pins_out() {
  gpio_reset_pin(CS);
  gpio_set_direction(CS, GPIO_MODE_OUTPUT);
  gpio_set_level(CS, 1);
  gpio_set_direction(SCK, GPIO_MODE_OUTPUT);
  gpio_set_level(SCK, 0);
  gpio_set_direction(SDA, GPIO_MODE_OUTPUT);
}
static inline void dly() { esp_rom_delay_us(HP); }

static void bit_out(int b) {
  gpio_set_level(SDA, b);
  dly();
  gpio_set_level(SCK, 1);  // panel samples on rising edge
  dly();
  gpio_set_level(SCK, 0);
}
static void word9(bool data, uint8_t v) {
  bit_out(data);
  for (int i = 7; i >= 0; i--) bit_out((v >> i) & 1);
}
static void command(uint8_t cmd, const uint8_t *d, int n) {
  gpio_set_level(CS, 0);
  dly();
  word9(false, cmd);
  for (int i = 0; i < n; i++) word9(true, d[i]);
  dly();
  gpio_set_level(CS, 1);
  dly();
}
static void cmd0(uint8_t c) { command(c, nullptr, 0); }
static void page(uint8_t p) {
  const uint8_t d[] = {0x77, 0x01, 0x00, 0x00, p};
  command(0xFF, d, 5);
}

// command, then `bits` clocks reading SDA (SDA released, pulled down or up)
static uint32_t read_reg(uint8_t cmd, int bits, bool pull_up) {
  gpio_set_level(CS, 0);
  dly();
  word9(false, cmd);
  gpio_set_direction(SDA, GPIO_MODE_INPUT);
  gpio_set_pull_mode(SDA, pull_up ? GPIO_PULLUP_ONLY : GPIO_PULLDOWN_ONLY);
  dly();
  uint32_t v = 0;
  for (int i = 0; i < bits; i++) {
    gpio_set_level(SCK, 1);
    dly();
    v = (v << 1) | (gpio_get_level(SDA) & 1);
    gpio_set_level(SCK, 0);
    dly();
  }
  gpio_set_level(CS, 1);
  gpio_set_pull_mode(SDA, GPIO_FLOATING);
  gpio_set_direction(SDA, GPIO_MODE_OUTPUT);
  dly();
  return v;
}

static void send_init(int after_reset, int after_slpout, int after_dispon) {
  size_t i = 0, n = sizeof(INIT);
  while (i + 1 < n) {
    uint8_t c = INIT[i++], x = INIT[i++];
    if (x == 0xFF) {
      vTaskDelay(pdMS_TO_TICKS(c));
      continue;
    }
    int k = x & 0x7F;
    if (c == 0x11) vTaskDelay(pdMS_TO_TICKS(120));
    command(c, INIT + i, k);
    i += k;
    if (c == 0x01) vTaskDelay(pdMS_TO_TICKS(after_reset));
    if (c == 0x11) vTaskDelay(pdMS_TO_TICKS(after_slpout));
    if (c == 0x29) vTaskDelay(pdMS_TO_TICKS(after_dispon));
  }
}

std::string run(const std::string &cmd, int val, int us, const std::string &arg) {
  HP = us > 0 ? us : 5;
  pins_out();
  char buf[400];
  if (cmd == "read") {
    struct R { uint8_t c; int bits; } regs[] = {{0x04, 25}, {0x0A, 8}, {0x0B, 8}, {0x0C, 8}, {0x0D, 8},
                                               {0x0F, 8}, {0xDA, 8}, {0xDB, 8}, {0xDC, 8}};
    int o = snprintf(buf, sizeof buf, "read us=%d:", HP);
    page(0x00);  // reads are command set 1 (CMD2 off)
    for (auto &r : regs) {
      uint32_t lo = read_reg(r.c, r.bits, false), hi = read_reg(r.c, r.bits, true);
      o += snprintf(buf + o, sizeof buf - o, " %02X=%0*lX/%0*lX", r.c, (r.bits + 3) / 4, (unsigned long) lo,
                    (r.bits + 3) / 4, (unsigned long) hi);
      if (o >= (int) sizeof buf - 40) break;
    }
    page(0x10);  // back to the BK0 page the init leaves selected
    return buf;
  }
  if (cmd == "off") { cmd0(0x28); return "display off (0x28) sent"; }
  if (cmd == "on") { cmd0(0x29); return "display on (0x29) sent"; }
  if (cmd == "inv_on") { cmd0(0x21); return "invert on (0x21) sent"; }
  if (cmd == "inv_off") { cmd0(0x20); return "invert off (0x20) sent"; }
  if (cmd == "init") { send_init(0, 10, 0); snprintf(buf, sizeof buf, "init re-sent us=%d", HP); return buf; }
  if (cmd == "init_slow") {
    send_init(150, 200, 50);
    snprintf(buf, sizeof buf, "init_slow re-sent us=%d (150/200/50 ms)", HP);
    return buf;
  }
  if (cmd == "defaults") {
    cmd0(0x01);
    vTaskDelay(pdMS_TO_TICKS(150));
    cmd0(0x11);
    vTaskDelay(pdMS_TO_TICKS(200));
    const uint8_t pf = 0x66;
    command(0x3A, &pf, 1);
    cmd0(0x29);
    return "defaults: SWRESET + SLPOUT + COLMOD + DISPON sent";
  }
  if (cmd == "vcom" || cmd == "vop" || cmd == "cd") {
    uint8_t v = (uint8_t) val;
    if (cmd == "cd") {
      page(0x10);
      command(0xCD, &v, 1);
    } else {
      page(0x11);
      command(cmd == "vcom" ? 0xB1 : 0xB0, &v, 1);
      page(0x10);
    }
    snprintf(buf, sizeof buf, "%s = 0x%02X sent", cmd.c_str(), v);
    return buf;
  }
  if (cmd == "raw") {
    std::vector<uint8_t> b;
    const char *p = arg.c_str();
    char *e;
    while (*p) {
      long x = strtol(p, &e, 16);
      if (e == p) { p++; continue; }
      b.push_back((uint8_t) x);
      p = e;
    }
    if (b.empty()) return "raw: no bytes";
    command(b[0], b.data() + 1, (int) b.size() - 1);
    snprintf(buf, sizeof buf, "raw cmd %02X + %d byte(s) sent", b[0], (int) b.size() - 1);
    return buf;
  }
  return "unknown cmd (read off on inv_on inv_off init init_slow defaults vcom vop cd raw)";
}

}  // namespace lcd
}  // namespace panel
