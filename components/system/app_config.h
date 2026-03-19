#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/spi_master.h"
#include "app_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =========================
 * 项目模板信息
 * ========================= */
#define APP_PROJECT_NAME                  "codex_project_tep"
#define APP_PROJECT_DISPLAY_NAME          "ESP32 Cloud OTA Template"
#define APP_PROJECT_VERSION               "v1.8.0"
#define APP_PROJECT_TARGET                "ESP32-S3"

/* =========================
 * 当前学习阶段说明
 * ========================= */
#define APP_PROJECT_STAGE_NAME            "v1.8.0 Cloud Version Check"

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
#define APP_XL9555_BEEP_ACTIVE_LEVEL      0

// XL9555 引脚编号按 0~15 映射：IO0_0~IO0_7 = 0~7, IO1_0~IO1_7 = 8~15
#define APP_XL9555_BEEP_PIN               3    // IO0_3
#define APP_XL9555_LCD_CTRL0_PIN          11   // IO1_3
#define APP_XL9555_LCD_CTRL1_PIN          10   // IO1_2
#define APP_XL9555_KEY0_PIN               15   // IO1_7
#define APP_XL9555_KEY1_PIN               14   // IO1_6
#define APP_XL9555_KEY2_PIN               13   // IO1_5
#define APP_XL9555_KEY3_PIN               12   // IO1_4

/* =========================
 * LCD / SPI 配置
 * ========================= */
#define APP_LCD_RST_PIN                   APP_XL9555_LCD_CTRL1_PIN
#define APP_LCD_PWR_PIN                   APP_XL9555_LCD_CTRL0_PIN
#define APP_LCD_WIDTH                     320
#define APP_LCD_HEIGHT                    240
#define APP_LCD_SPI_HOST                  SPI2_HOST
#define APP_LCD_SPI_MOSI_GPIO             GPIO_NUM_11
#define APP_LCD_SPI_MISO_GPIO             GPIO_NUM_NC
#define APP_LCD_SPI_DC_GPIO               GPIO_NUM_13
#define APP_LCD_SPI_SCLK_GPIO             GPIO_NUM_12
#define APP_LCD_SPI_CS_GPIO               GPIO_NUM_21
#define APP_LCD_SPI_CLOCK_HZ              (20 * 1000 * 1000)
#define APP_LCD_SPI_QUEUE_SIZE            8
#define APP_LCD_SPI_MAX_TRANSFER_SZ       (APP_LCD_WIDTH * 40 * 2)
#define APP_LCD_ROTATION_LANDSCAPE        1
#define APP_LCD_X_OFFSET                  0
#define APP_LCD_Y_OFFSET                  0
#define APP_LCD_PWR_ON_LEVEL              1
#define APP_LCD_PWR_OFF_LEVEL             0
#define APP_LCD_RST_ACTIVE_LEVEL          0
#define APP_LCD_RST_INACTIVE_LEVEL        1

/* =========================
 * Wi-Fi 配置
 * ========================= */
#define APP_WIFI_STA_SSID                 "LV-HOME"
#define APP_WIFI_STA_PASSWORD             "lv666666"
#define APP_WIFI_MAX_RETRY                5
#define APP_WIFI_CONNECT_TIMEOUT_MS       15000

/* =========================
 * HTTP 配置
 * ========================= */
#define APP_HTTP_TEST_URL                 "https://httpbin.org/json"
#define APP_HTTP_TIMEOUT_MS               20000
#define APP_HTTP_AUTO_START               1
#define APP_HTTP_RETRY_INTERVAL_MS        5000

/* =========================
 * OTA 配置
 * ========================= */
#define APP_OTA_AUTO_CHECK                1
#define APP_OTA_AUTO_UPGRADE              0
#define APP_OTA_CHECK_INTERVAL_MS         10000
#define APP_OTA_VERSION_URL               "http://1406834977-7btxur47w5.ap-guangzhou.tencentscf.com/version"

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
 * 蜂鸣器服务配置
 * ========================= */
#define APP_BEEP_SHORT_ON_MS              60
#define APP_BEEP_SHORT_OFF_MS             80
#define APP_BEEP_LONG_ON_MS               180
#define APP_BEEP_TEST_INTERVAL_MS         1000


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
