#ifndef DISPLAY_SERVICE_H
#define DISPLAY_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "app_types.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t display_service_init(void);
esp_err_t display_service_clear(void);
esp_err_t display_service_show_version(const char *version);
esp_err_t display_service_show_stage(const char *stage);
esp_err_t display_service_show_last_button(button_id_t button_id, button_event_t button_event);
esp_err_t display_service_show_led_status(led_id_t led_id, led_mode_t mode);
esp_err_t display_service_show_beep_status(bool enabled, bool test_mode);
esp_err_t display_service_show_wifi_status(wifi_state_t state);
esp_err_t display_service_show_wifi_ip(const char *ip_string);
esp_err_t display_service_show_wifi_signal(int rssi, uint8_t channel);
esp_err_t display_service_refresh_home(void);
void display_service_process(void);

#ifdef __cplusplus
}
#endif

#endif
