#include "app_main_task.h"
#include "app_event_task.h"
#include "app_config.h"
#include "led_service.h"
#include "button_service.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"

static const char *TAG = "APP_MAIN";
static QueueHandle_t s_button_event_queue = NULL;

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

    // v1.2.0 的第一步是先把系统级事件通道准备好，
    // 后面的输入服务和事件任务都会围绕这条队列通信。
    s_button_event_queue = xQueueCreate(APP_EVENT_QUEUE_LENGTH, sizeof(app_event_msg_t));
    if (s_button_event_queue == NULL) {
        ESP_LOGE(TAG, "xQueueCreate failed, queue_len=%d", APP_EVENT_QUEUE_LENGTH);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "button event queue ready, queue_len=%d", APP_EVENT_QUEUE_LENGTH);

    // 先准备输出能力，再准备输入能力，方便后面按键事件一产生就能立刻看到灯效反馈。
    esp_err_t ret = led_service_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "led_service_init failed, ret=0x%x", ret);
        vTaskDelete(NULL);
        return;
    }

    // 启动事件任务，准备好事件通路，后续按键服务投递的事件才能被处理。
    ret = app_event_task_start(s_button_event_queue);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "app_event_task_start failed, ret=0x%x", ret);
        vTaskDelete(NULL);
        return;
    }

    // button_service 不创建队列，而是拿到现成的队列句柄后只负责发消息。
    ret = button_service_init(s_button_event_queue);
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
    ESP_LOGI(TAG, "Event flow: button_service -> unified_event_queue -> app_event_task -> led_service");

    while (1) {
        // v1.2.0 开始，按键服务只负责识别事件并投递到队列；
        // 真正的业务动作改由 app_event_task 接收消息后处理。
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
