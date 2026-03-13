#ifndef LED_SERVICE_H
#define LED_SERVICE_H

#include "esp_err.h"
#include "app_types.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t led_service_init(void);
esp_err_t led_service_on(led_id_t led_id);
esp_err_t led_service_off(led_id_t led_id);
esp_err_t led_service_toggle(led_id_t led_id);

esp_err_t led_service_set_mode(led_id_t led_id, led_mode_t mode);
led_mode_t led_service_get_mode(led_id_t led_id);

void led_service_process(void);

#ifdef __cplusplus
}
#endif

#endif // LED_SERVICE_H