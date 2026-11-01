#pragma once
// "Restart reason" text sensor (Waveshare; the Guition has its own in s3_reset.h because of its deep-sleep hop)
#include <esp_system.h>
#include <string>

namespace panel::reset {
inline std::string reason() {
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON: return "power on";
    case ESP_RST_SW: return "software (OTA / restart)";
    case ESP_RST_PANIC: return "crash (panic)";
    case ESP_RST_INT_WDT: return "interrupt watchdog";
    case ESP_RST_TASK_WDT: return "task watchdog";
    case ESP_RST_WDT: return "watchdog";
    case ESP_RST_BROWNOUT: return "brownout";
    case ESP_RST_DEEPSLEEP: return "deep sleep";
    case ESP_RST_USB: return "USB";
    default: return "other (" + std::to_string((int) esp_reset_reason()) + ")";
  }
}
}  // namespace panel::reset
