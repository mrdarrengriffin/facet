#pragma once
#include <cstdio>
extern bool sim_verbose;
#define ESP_LOGW(tag, fmt, ...) do { if (sim_verbose) fprintf(stderr, "[W][%s] " fmt "\n", tag, ##__VA_ARGS__); } while (0)
#define ESP_LOGI(tag, fmt, ...) do { if (sim_verbose) fprintf(stderr, "[I][%s] " fmt "\n", tag, ##__VA_ARGS__); } while (0)
