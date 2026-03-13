#ifndef BSP_LED_H
#define BSP_LED_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "app_types.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t bsp_led_init(led_id_t led_id);
esp_err_t bsp_led_set(led_id_t led_id, bool on);
esp_err_t bsp_led_toggle(led_id_t led_id);
bool bsp_led_get_state(led_id_t led_id);

#ifdef __cplusplus
}
#endif

#endif // BSP_LED_H