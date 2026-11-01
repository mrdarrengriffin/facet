// ST7701S 3-wire SPI diagnostics (Guition 4848S040 only): bit-banged on the init pins (CS IO39, SCK IO48,
// SDA IO47) after boot, so the panel's control path can be tested at runtime without touching the RGB bus.
// API action facet_lcd{cmd, val, us} (HA esphome.guition_4848s040_facet_lcd), result in text sensor "LCD diag".
//   read       reads 0x04 (ID, dummy + 24 bits), 0x0A power mode, 0x0B MADCTL, 0x0C pixel format, 0x0D, 0x0F, 0xDA-0xDC;
//              each twice, SDA pulled down then up: same value both times = the panel drives SDA (link alive).
//   off / on   0x28 display off / 0x29 on (visible: proves writes land)
//   inv_on / inv_off   0x21 / 0x20
//   init       re-sends the ESPHome GUITION-4848S040 init (copy of the generated sequence)
//   init_slow  same, 150 ms after SWRESET, 200 ms after SLPOUT, 50 ms after DISPON
//   defaults   SWRESET + SLPOUT + COLMOD 0x66 + DISPON only (register defaults; compare with the fault look)
//   vcom N     page 1 0xB1 = N (VCOM; stock 0x32)   vop N  page 1 0xB0 = N (VOP; stock 0x60)
//   cd N       page 0 0xCD = N (stock 0x00)
//   raw HEX    "cmd b1 b2 .." hex bytes written as one command (val ignored)
// `us` = SPI half-period in microseconds (default 5 = ~100 kHz; the boot init runs at 2 MHz hardware-paced).
#pragma once
#include <string>

namespace panel {
namespace lcd {

std::string run(const std::string &cmd, int val, int us, const std::string &arg);

}  // namespace lcd
}  // namespace panel
