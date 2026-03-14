#ifndef BUTTON_SERVICE_H
#define BUTTON_SERVICE_H

#include "app_types.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t button_service_init(QueueHandle_t event_queue);
void button_service_process(void);

#ifdef __cplusplus
}
#endif

#endif
