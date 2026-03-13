#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include "app_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =========================
 * 项目模板信息
 * ========================= */
#define APP_PROJECT_NAME                  "codex_project_tep"
#define APP_PROJECT_DISPLAY_NAME          "ESP32 Button to Multi LED Template"
#define APP_PROJECT_VERSION               "v1.0.0"
#define APP_PROJECT_TARGET                "ESP32-S3"

/* =========================
 * SYS LED 配置
 * ========================= */
#define APP_SYS_LED_GPIO                  GPIO_NUM_1
#define APP_SYS_LED_ACTIVE_LEVEL          0   // io控制状态： 1 表示高电平点亮，0 表示低电平点亮
#define APP_SYS_LED_DEFAULT_ON            1   // 是否默认点亮 LED，1=默认点亮，0=默认熄灭

/* =========================
 * NET LED 配置
 * ========================= */
#define APP_NET_LED_GPIO                  GPIO_NUM_19
#define APP_NET_LED_ACTIVE_LEVEL          1  // io控制状态： 1 表示高电平点亮，0 表示低电平点亮
#define APP_NET_LED_DEFAULT_ON            1  // 是否默认点亮 LED，1=默认点亮，0=默认熄灭

/* =========================
 * ERR LED 配置
 * ========================= */
#define APP_ERR_LED_GPIO                  GPIO_NUM_36
#define APP_ERR_LED_ACTIVE_LEVEL          1   // io控制状态： 1 表示高电平点亮，0 表示低电平点亮 
#define APP_ERR_LED_DEFAULT_ON            1   // 是否默认点亮 LED，1=默认点亮，0=默认熄灭

/* =========================
 * LED 模式配置
 * ========================= */
#define APP_LED_BLINK_SLOW_PERIOD_MS      500
#define APP_LED_BLINK_FAST_PERIOD_MS      150
#define APP_LED_SYS_DEFAULT_MODE          LED_MODE_BLINK_SLOW
#define APP_LED_NET_DEFAULT_MODE          LED_MODE_BLINK_FAST
#define APP_LED_ERR_DEFAULT_MODE          LED_MODE_OFF

/* =========================
 * 按键 GPIO 配置
 * ========================= */
#define APP_BTN_SYS_GPIO                   GPIO_NUM_0
#define APP_BTN_NET_GPIO                   GPIO_NUM_7
#define APP_BTN_ERR_GPIO                   GPIO_NUM_16
#define APP_BUTTON_ACTIVE_LEVEL            0
#define APP_BUTTON_COUNT                   BTN_MAX


/* =========================
 * 任务配置
 * ========================= */
#define APP_MAIN_TASK_NAME                "app_main_task"
#define APP_MAIN_TASK_STACK_SIZE          4096
#define APP_MAIN_TASK_PRIORITY            5
#define APP_LED_SERVICE_TASK_PERIOD_MS    20

#ifdef __cplusplus
}
#endif

#endif // APP_CONFIG_H
