#include "app_main_task.h"
#include "app_config.h"
#include "led_service.h"
#include "button_service.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"

static const char *TAG = "APP_MAIN";

/* 启动时打印当前版本使用的外部 GPIO 映射，方便上板核对接线。 */
static void app_main_task_log_gpio_mapping(void)
{
    ESP_LOGI(TAG, "External LED mapping:");
    ESP_LOGI(TAG, "  SYS -> GPIO%d active_level=%d default_mode=%d",
             APP_SYS_LED_GPIO, APP_SYS_LED_ACTIVE_LEVEL, APP_LED_SYS_DEFAULT_MODE);
    ESP_LOGI(TAG, "  NET -> GPIO%d active_level=%d default_mode=%d",
             APP_NET_LED_GPIO, APP_NET_LED_ACTIVE_LEVEL, APP_LED_NET_DEFAULT_MODE);
    ESP_LOGI(TAG, "  ERR -> GPIO%d active_level=%d default_mode=%d",
             APP_ERR_LED_GPIO, APP_ERR_LED_ACTIVE_LEVEL, APP_LED_ERR_DEFAULT_MODE);

    ESP_LOGI(TAG, "External button mapping:");
    ESP_LOGI(TAG, "  BTN_SYS -> GPIO%d active_level=%d intr=negedge",
             APP_BTN_SYS_GPIO, APP_BUTTON_ACTIVE_LEVEL);
    ESP_LOGI(TAG, "  BTN_NET -> GPIO%d active_level=%d intr=negedge",
             APP_BTN_NET_GPIO, APP_BUTTON_ACTIVE_LEVEL);
    ESP_LOGI(TAG, "  BTN_ERR -> GPIO%d active_level=%d intr=negedge",
             APP_BTN_ERR_GPIO, APP_BUTTON_ACTIVE_LEVEL);
}

/* 默认模式集中在配置层，这里只负责把配置应用到运行时。 */
static void app_main_task_apply_default_led_modes(void)
{
    led_service_set_mode(LED_ID_SYS, APP_LED_SYS_DEFAULT_MODE);
    led_service_set_mode(LED_ID_NET, APP_LED_NET_DEFAULT_MODE);
    led_service_set_mode(LED_ID_ERR, APP_LED_ERR_DEFAULT_MODE);
}

static void app_main_task(void *param)
{
    (void)param;

    // 先准备输出能力，再准备输入能力，方便后面按键事件一产生就能立刻看到灯效反馈。
    esp_err_t ret = led_service_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "led_service_init failed, ret=0x%x", ret);
        vTaskDelete(NULL);
        return;
    }

    ret = button_service_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "button_service_init failed, ret=0x%x", ret);
        vTaskDelete(NULL);
        return;
    }

    app_main_task_apply_default_led_modes();
    app_main_task_log_gpio_mapping();

    ESP_LOGI(TAG, "%s started", APP_PROJECT_DISPLAY_NAME);
    ESP_LOGI(TAG, "Default LED modes: SYS=%d NET=%d ERR=%d",
             APP_LED_SYS_DEFAULT_MODE,
             APP_LED_NET_DEFAULT_MODE,
             APP_LED_ERR_DEFAULT_MODE);

    while (1) {
        // v1.1.2 改成“GPIO 下降沿中断通知 + 主循环状态机处理”的混合模式。
        // LED 仍然需要周期驱动来实现闪烁；按键服务则在这里处理 ISR 之后的完整状态机。
        led_service_process();
        button_service_process();
        vTaskDelay(pdMS_TO_TICKS(APP_LED_SERVICE_TASK_PERIOD_MS));
    }
}

void app_main_task_start(void)
{
    BaseType_t ret = xTaskCreate(
        app_main_task,
        APP_MAIN_TASK_NAME,
        APP_MAIN_TASK_STACK_SIZE,
        NULL,
        APP_MAIN_TASK_PRIORITY,
        NULL
    );

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create app_main_task");
    } else {
        ESP_LOGI(TAG, "app_main_task created successfully");
    }
}
