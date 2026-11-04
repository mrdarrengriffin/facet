#pragma once
// host shim: FreeRTOS tasks on std::thread (sim.h); 1 tick = 1 simulated ms
#include <cstdint>
typedef uint32_t TickType_t;
typedef int BaseType_t;
typedef unsigned UBaseType_t;
#define pdTRUE 1
#define pdFALSE 0
#define pdPASS 1
#define pdMS_TO_TICKS(ms) ((TickType_t) (ms))
#define portNUM_PROCESSORS 2
#define tskNO_AFFINITY 0x7fffffff
