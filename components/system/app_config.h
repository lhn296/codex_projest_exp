#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "app_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =========================
 * 项目模板信息
 * ========================= */
#define APP_PROJECT_NAME                  "codex_project_tep"
#define APP_PROJECT_DISPLAY_NAME          "ESP32 XL9555 Board Input Learning"
#define APP_PROJECT_VERSION               "v1.3.0"
#define APP_PROJECT_TARGET                "ESP32-S3"

/* =========================
 * 当前学习阶段说明
 * ========================= */
#define APP_PROJECT_STAGE_NAME            "v1.3.0 XL9555 Driver Foundation"

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
 * I2C / XL9555 配置
 * ========================= */
#define APP_I2C_MASTER_PORT               I2C_NUM_0
#define APP_I2C_MASTER_SDA_GPIO           GPIO_NUM_41
#define APP_I2C_MASTER_SCL_GPIO           GPIO_NUM_42
#define APP_I2C_MASTER_FREQ_HZ            400000
#define APP_I2C_XFER_TIMEOUT_MS           100

#define APP_XL9555_I2C_ADDR               0x20
#define APP_XL9555_INT_GPIO               GPIO_NUM_39
#define APP_XL9555_KEY_ACTIVE_LEVEL       0

// XL9555 引脚编号按 0~15 映射：IO0_0~IO0_7 = 0~7, IO1_0~IO1_7 = 8~15
#define APP_XL9555_BEEP_PIN               3    // IO0_3
#define APP_XL9555_LCD_CTRL0_PIN          11   // IO1_3
#define APP_XL9555_LCD_CTRL1_PIN          10   // IO1_2
#define APP_XL9555_KEY0_PIN               15   // IO1_7
#define APP_XL9555_KEY1_PIN               14   // IO1_6
#define APP_XL9555_KEY2_PIN               13   // IO1_5
#define APP_XL9555_KEY3_PIN               12   // IO1_4

/* =========================
 * 按键服务配置
 * ========================= */
#define APP_BUTTON_COUNT                  BTN_MAX
#define APP_BUTTON_DEBOUNCE_MS            30
#define APP_BUTTON_LONG_PRESS_MS          800
#define APP_BUTTON_DOUBLE_CLICK_MS        300
#define APP_BUTTON_INT_ONLY_DEBUG         0
#define APP_BUTTON_IDLE_SCAN_PERIOD_MS    20


/* =========================
 * 任务配置
 * ========================= */
#define APP_MAIN_TASK_NAME                "app_main_task"
#define APP_MAIN_TASK_STACK_SIZE          4096
#define APP_MAIN_TASK_PRIORITY            5
#define APP_LED_SERVICE_TASK_PERIOD_MS    20
#define APP_EVENT_TASK_NAME               "app_event_task"
#define APP_EVENT_TASK_STACK_SIZE         4096
#define APP_EVENT_TASK_PRIORITY           6
#define APP_EVENT_QUEUE_LENGTH            8

#ifdef __cplusplus
}
#endif

#endif // APP_CONFIG_H
