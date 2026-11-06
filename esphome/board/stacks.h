// The web server task's stack (#152): ESPHome starts ESP-IDF's HTTP server with HTTPD_DEFAULT_CONFIG's 4 KB + 256 B,
// and the heaviest pages (/fw/info, /fw/report, the screenshot, the editor) left ~660 B of it, then overflowed it on
// the 4848S040. No Kconfig sets that size, so the link wraps httpd_start (workbench.yaml:
// -Wl,--wrap=httpd_start) and raises it to FACET_HTTPD_STACK. It stays in internal RAM (config.task_caps): handlers
// save settings and decks to flash, and a PSRAM stack can't be touched while the flash cache is off.
// /fw/info "stacks.httpd" (firmware/fw_stacks.h) reads the size from facet_httpd_stack_size.
#pragma once
#include "esp_http_server.h"

#ifndef FACET_HTTPD_STACK
#define FACET_HTTPD_STACK 8192
#endif

extern "C" {
int facet_httpd_stack_size = 0;
esp_err_t __real_httpd_start(httpd_handle_t *handle, const httpd_config_t *config);
esp_err_t __wrap_httpd_start(httpd_handle_t *handle, const httpd_config_t *config) {
  httpd_config_t c = *config;
  if (c.stack_size < FACET_HTTPD_STACK) c.stack_size = FACET_HTTPD_STACK;
  facet_httpd_stack_size = (int) c.stack_size;
  return __real_httpd_start(handle, &c);
}
}
