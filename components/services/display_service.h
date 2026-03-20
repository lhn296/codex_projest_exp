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
esp_err_t display_service_show_config_source(const char *source);
esp_err_t display_service_show_last_button(button_id_t button_id, button_event_t button_event);
esp_err_t display_service_show_led_status(led_id_t led_id, led_mode_t mode);
esp_err_t display_service_show_beep_status(bool enabled, bool test_mode);
esp_err_t display_service_show_wifi_status(wifi_state_t state);
esp_err_t display_service_show_wifi_ip(const char *ip_string);
esp_err_t display_service_show_wifi_signal(int rssi, uint8_t channel);
esp_err_t display_service_show_http_result(bool success, int status_code, const char *message);
esp_err_t display_service_show_ota_status(ota_state_t state, const char *message);
// 显示一个全屏菜单覆盖页，title + 三行内容，selected_index 用于高亮当前项。
esp_err_t display_service_show_menu_page(const char *title,
                                         const char *line1,
                                         const char *line2,
                                         const char *line3,
                                         int selected_index);
// 关闭菜单覆盖页并恢复首页刷新。
esp_err_t display_service_hide_menu(void);
esp_err_t display_service_refresh_home(void);
void display_service_process(void);

#ifdef __cplusplus
}
#endif

#endif
