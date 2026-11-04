#pragma once
// host shim: a fake HTTP client whose behaviour comes from the URL (sim.h)
#include <cstdint>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_HTTP_EAGAIN 0x7007
typedef enum { HTTP_EVENT_ERROR, HTTP_EVENT_ON_CONNECTED, HTTP_EVENT_HEADER_SENT, HTTP_EVENT_ON_HEADER, HTTP_EVENT_ON_DATA, HTTP_EVENT_ON_FINISH, HTTP_EVENT_DISCONNECTED } esp_http_client_event_id_t;
struct esp_http_client;
typedef struct esp_http_client *esp_http_client_handle_t;
typedef struct {
  esp_http_client_event_id_t event_id;
  esp_http_client_handle_t client;
  void *data;
  int data_len;
  void *user_data;
  char *header_key;
  char *header_value;
} esp_http_client_event_t;
typedef esp_err_t (*http_event_handle_cb)(esp_http_client_event_t *evt);
typedef struct {
  const char *url;
  int timeout_ms;
  const char *user_agent;
  int buffer_size, buffer_size_tx;
  http_event_handle_cb event_handler;
  void *user_data;
  bool disable_auto_redirect;
  esp_err_t (*crt_bundle_attach)(void *conf);
} esp_http_client_config_t;
esp_http_client_handle_t esp_http_client_init(const esp_http_client_config_t *c);
esp_err_t esp_http_client_set_header(esp_http_client_handle_t h, const char *k, const char *v);
esp_err_t esp_http_client_open(esp_http_client_handle_t h, int write_len);
int64_t esp_http_client_fetch_headers(esp_http_client_handle_t h);
int esp_http_client_get_status_code(esp_http_client_handle_t h);
esp_err_t esp_http_client_set_redirection(esp_http_client_handle_t h);
int esp_http_client_read(esp_http_client_handle_t h, char *buf, int len);
bool esp_http_client_is_complete_data_received(esp_http_client_handle_t h);
esp_err_t esp_http_client_close(esp_http_client_handle_t h);
esp_err_t esp_http_client_cleanup(esp_http_client_handle_t h);
