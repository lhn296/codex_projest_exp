#ifndef DISPLAY_SERVICE_H
#define DISPLAY_SERVICE_H

#include <stdbool.h>

#include "app_types.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t display_service_init(void);
esp_err_t display_service_clear(void);// 这个接口单纯清屏，不修改显示缓存里的状态信息，后续刷新时会按缓存内容重画。
esp_err_t display_service_show_version(const char *version);// 版本信息通常在开机动画里展示，后续也可能在设置界面显示。
esp_err_t display_service_show_stage(const char *stage);// 阶段信息通常在开机动画里展示，后续也可能在设置界面显示。
esp_err_t display_service_show_last_button(button_id_t button_id, button_event_t button_event);// 最近一次按键事件通常在首页展示，后续也可能在调试界面显示。    
esp_err_t display_service_show_led_status(led_id_t led_id, led_mode_t mode);
esp_err_t display_service_show_beep_status(bool enabled, bool test_mode);
esp_err_t display_service_refresh_home(void);
void display_service_process(void);

#ifdef __cplusplus
}
#endif

#endif
