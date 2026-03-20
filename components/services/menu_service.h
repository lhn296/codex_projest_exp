#ifndef MENU_SERVICE_H
#define MENU_SERVICE_H

#include <stdbool.h>

#include "app_types.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// 初始化菜单服务，准备菜单状态机与默认显示状态。
esp_err_t menu_service_init(void);

// 菜单周期处理入口，当前版本预留给后续扩展。
void menu_service_process(void);

// 判断菜单当前是否可见。
bool menu_service_is_visible(void);

// 处理按键事件；如果事件被菜单消费，则返回 true。
bool menu_service_handle_button_event(button_id_t button_id, button_event_t button_event);

#ifdef __cplusplus
}
#endif

#endif
