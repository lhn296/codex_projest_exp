#include "app_event_task.h"
#include "app_config.h"
#include "app_types.h"
#include "led_service.h"
#include "esp_log.h"
#include "freertos/task.h"

static const char *TAG = "APP_EVENT";
static QueueHandle_t s_event_queue = NULL;

static const char *app_event_button_to_string(button_id_t btn_id)
{
    switch (btn_id) {
        case BTN_SYS:
            return "BTN_SYS";
        case BTN_NET:
            return "BTN_NET";
        case BTN_ERR:
            return "BTN_ERR";
        default:
            return "BTN_UNKNOWN";
    }
}

static const char *app_event_led_to_string(led_id_t led_id)
{
    switch (led_id) {
        case LED_ID_SYS:
            return "SYS_LED";
        case LED_ID_NET:
            return "NET_LED";
        case LED_ID_ERR:
            return "ERR_LED";
        default:
            return "UNKNOWN_LED";
    }
}

static const char *app_event_event_to_string(button_event_t event)
{
    switch (event) {
        case BUTTON_EVENT_SHORT:
            return "SHORT";
        case BUTTON_EVENT_LONG:
            return "LONG";
        case BUTTON_EVENT_DOUBLE:
            return "DOUBLE";
        case BUTTON_EVENT_NONE:
        default:
            return "NONE";
    }
}

static const char *app_event_mode_to_string(led_mode_t mode)
{
    switch (mode) {
        case LED_MODE_OFF:
            return "OFF";
        case LED_MODE_ON:
            return "ON";
        case LED_MODE_BLINK_SLOW:
            return "BLINK_SLOW";
        case LED_MODE_BLINK_FAST:
            return "BLINK_FAST";
        default:
            return "UNKNOWN";
    }
}

static led_mode_t app_event_get_next_mode_for_event(led_mode_t cur_mode, button_event_t event)
{
    switch (event) {
        case BUTTON_EVENT_SHORT:
            return (led_mode_t)((cur_mode + 1) % LED_MODE_MAX);
        case BUTTON_EVENT_LONG:
            return LED_MODE_OFF;
        case BUTTON_EVENT_DOUBLE:
            return LED_MODE_BLINK_FAST;
        case BUTTON_EVENT_NONE:
        default:
            return cur_mode;
    }
}

/* 事件任务负责把“按键事件”翻译成具体业务动作，是 v1.2.0 的核心解耦点。 */
static void app_event_task(void *param)
{
    (void)param;

    while (1) {
        app_button_msg_t msg;

        // 事件任务使用阻塞式等待，这样没有消息时不会空转浪费 CPU。
        if (xQueueReceive(s_event_queue, &msg, portMAX_DELAY) != pdPASS) {
            continue;
        }

        led_id_t led_id = (led_id_t)msg.button_id;
        led_mode_t cur_mode = led_service_get_mode(led_id);
        led_mode_t next_mode = app_event_get_next_mode_for_event(cur_mode, msg.event);

        // 到了这里，按键服务已经把“发生了什么”说清楚了，
        // 事件任务只负责决定“收到这个事件后怎么处理业务”。
        ESP_LOGI(TAG, "recv %s %s at %lldms -> %s: %s -> %s",
                 app_event_button_to_string(msg.button_id),
                 app_event_event_to_string(msg.event),
                 (long long)msg.timestamp_ms,
                 app_event_led_to_string(led_id),
                 app_event_mode_to_string(cur_mode),
                 app_event_mode_to_string(next_mode));

        if (next_mode != cur_mode) {
            led_service_set_mode(led_id, next_mode);
        }
    }
}

esp_err_t app_event_task_start(QueueHandle_t event_queue)
{
    if (event_queue == NULL) {
        return ESP_ERR_INVALID_ARG; // 参数检查，事件队列必须由主任务创建好传进来
    }

    // 把主任务创建好的队列句柄保存下来，后面事件任务就围绕这条队列工作。
    s_event_queue = event_queue;

    // 创建事件任务，优先级比主任务低，避免抢占导致主任务初始化工作被打断。
    BaseType_t ret = xTaskCreate(
        app_event_task,
        APP_EVENT_TASK_NAME,
        APP_EVENT_TASK_STACK_SIZE,
        NULL,
        APP_EVENT_TASK_PRIORITY,// 事件任务优先级要比主任务低，避免抢占导致主任务初始化工作被打断
        NULL
    ); // 创建事件任务

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "failed to create app_event_task");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "app_event_task created, queue_len=%d", APP_EVENT_QUEUE_LENGTH);
    return ESP_OK;
}
