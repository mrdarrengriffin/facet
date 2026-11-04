#pragma once
#include "freertos/FreeRTOS.h"
struct SimTask;
typedef SimTask *TaskHandle_t;
typedef void (*TaskFunction_t)(void *);
BaseType_t xTaskCreatePinnedToCore(TaskFunction_t f, const char *name, uint32_t stack, void *arg, UBaseType_t prio, TaskHandle_t *out, BaseType_t core);
uint32_t ulTaskNotifyTake(BaseType_t clear, TickType_t wait);
void xTaskNotifyGive(TaskHandle_t t);
void vTaskDelete(TaskHandle_t t);  // nullptr = this task: never returns (like FreeRTOS: nothing on its stack unwinds)
void vTaskDelay(TickType_t t);
UBaseType_t uxTaskGetStackHighWaterMark(TaskHandle_t t);
