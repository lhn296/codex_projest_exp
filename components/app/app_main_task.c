#include "app_main_task.h"
#include "app_config.h"
#include "led_service.h"
#include "button_service.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"

static const char *TAG = "APP_MAIN";

static void app_main_task_apply_default_led_modes(void)
{
    led_service_set_mode(LED_ID_SYS, APP_LED_SYS_DEFAULT_MODE);
    led_service_set_mode(LED_ID_NET, APP_LED_NET_DEFAULT_MODE);
    led_service_set_mode(LED_ID_ERR, APP_LED_ERR_DEFAULT_MODE);
}

static void app_main_task(void *param)
{
    (void)param;

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

    ESP_LOGI(TAG, "%s started", APP_PROJECT_DISPLAY_NAME);
    ESP_LOGI(TAG, "Default LED modes: SYS=%d NET=%d ERR=%d",
             APP_LED_SYS_DEFAULT_MODE,
             APP_LED_NET_DEFAULT_MODE,
             APP_LED_ERR_DEFAULT_MODE);

    while (1) {
        led_service_process();// 处理 LED 状态切换，必须在主循环调用以实现 LED 的闪烁效果
        button_service_process();// 处理按键事件，必须在主循环调用以检测按键状态并触发相应的 LED 模式切换
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
