#ifndef APP_EVENT_TASK_H
#define APP_EVENT_TASK_H

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t app_event_task_start(QueueHandle_t event_queue);

#ifdef __cplusplus
}
#endif

#endif // APP_EVENT_TASK_H
