#include "app_event_task.h"
#include "app_config.h"
#include "app_types.h"
#include "led_service.h"
#include "esp_log.h"
#include "freertos/task.h"

static const char *TAG = "APP_EVENT";
static QueueHandle_t s_event_queue = NULL;
static uint32_t s_event_recv_count = 0;
static uint32_t s_event_handle_count = 0;


// 打印日志函数，方便后续调试和观察事件流转情况。实际项目中可以根据需要调整日志级别和内容。
static const char *app_event_source_to_string(app_event_source_t source)
{
    switch (source) {
        case APP_EVENT_SOURCE_BUTTON:
            return "BUTTON";
        case APP_EVENT_SOURCE_SYSTEM:
            return "SYSTEM";
        case APP_EVENT_SOURCE_MAX:
        default:
            return "UNKNOWN_SOURCE";
    }
}

static const char *app_event_type_to_string(app_event_type_t type)
{
    switch (type) {
        case APP_EVENT_TYPE_BUTTON:
            return "BUTTON_EVENT";
        case APP_EVENT_TYPE_STATUS:
            return "STATUS_EVENT";
        case APP_EVENT_TYPE_MAX:
        default:
            return "UNKNOWN_TYPE";
    }
}

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

static const char *app_event_button_event_to_string(button_event_t event)
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
// 打印日志函数end

/* 根据按键事件类型计算 LED 的下一个模式，当前版本的规则是：
 * 1. 短按：模式切换到下一个，形成循环
 * 2. 长按：模式切换到常灭
 * 3. 双击：模式切换到快闪
 * 4. 无事件：保持当前模式不变
 */
static led_mode_t app_event_get_next_mode_for_button_event(led_mode_t cur_mode, button_event_t event)
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

// 处理按钮事件消息的函数，负责把统一事件消息中的按键事件翻译成具体的 LED 模式切换动作。
static void app_event_handle_button_message(const app_event_msg_t *msg)
{
    button_id_t button_id = (button_id_t)msg->param1;
    button_event_t button_event = (button_event_t)msg->param2;
    led_id_t led_id = (led_id_t)button_id;
    led_mode_t cur_mode = led_service_get_mode(led_id);
    led_mode_t next_mode = app_event_get_next_mode_for_button_event(cur_mode, button_event);

    s_event_handle_count++;
    ESP_LOGI(TAG, "handle src=%s type=%s btn=%s evt=%s ts=%lld -> %s: %s -> %s handle=%lu",
             app_event_source_to_string(msg->source),
             app_event_type_to_string(msg->type),
             app_event_button_to_string(button_id),
             app_event_button_event_to_string(button_event),
             (long long)msg->timestamp_ms,
             app_event_led_to_string(led_id),
             app_event_mode_to_string(cur_mode),
             app_event_mode_to_string(next_mode),
             (unsigned long)s_event_handle_count);

    if (next_mode != cur_mode) {
        led_service_set_mode(led_id, next_mode);
    }
}

/* 事件任务负责把统一事件消息翻译成具体业务动作，是 v1.2.1 的核心解耦点。 */
// 事件任务通过接收主任务创建的事件队列来获取事件消息，当前版本只处理按键事件，
// 后续可以扩展处理系统事件等。
static void app_event_task(void *param)
{
    (void)param;

    while (1) {
        app_event_msg_t msg;

        // 等待接受事件消息，当前版本的事件任务设计为阻塞式等待，这样没有消息时不会空转浪费 CPU。
        if (xQueueReceive(s_event_queue, &msg, portMAX_DELAY) != pdPASS) {
            continue;
        }

        s_event_recv_count++; // 统计接收到的事件数量，方便观察事件流转情况。
        ESP_LOGI(TAG, "recv src=%s type=%s p1=%ld p2=%ld ts=%lld recv=%lu",
                 app_event_source_to_string(msg.source),
                 app_event_type_to_string(msg.type),
                 (long)msg.param1,
                 (long)msg.param2,
                 (long long)msg.timestamp_ms,
                 (unsigned long)s_event_recv_count);

        // v1.2.1 开始先按统一事件头做分发，当前版本只处理 BUTTON 分支。
        if (msg.source == APP_EVENT_SOURCE_BUTTON && msg.type == APP_EVENT_TYPE_BUTTON) {
            app_event_handle_button_message(&msg); // 处理按键事件消息，负责把统一事件消息中的按键事件翻译成具体的 LED 模式切换动作。
        } else {
            ESP_LOGW(TAG, "unhandled event src=%s type=%s",
                     app_event_source_to_string(msg.source),
                     app_event_type_to_string(msg.type));
        }
    }
}

// 创建事件任务的函数，供主任务调用。事件任务创建成功后会一直运行，等待和处理事件队列中的消息。
esp_err_t app_event_task_start(QueueHandle_t event_queue)
{
    if (event_queue == NULL) {
        return ESP_ERR_INVALID_ARG; // 参数检查，事件队列必须由主任务创建好传进来
    }

    // 把主任务创建好的队列句柄保存下来，后面事件任务就围绕这条队列工作。
    s_event_queue = event_queue;

    BaseType_t ret = xTaskCreate(
        app_event_task,
        APP_EVENT_TASK_NAME,
        APP_EVENT_TASK_STACK_SIZE,
        NULL,
        APP_EVENT_TASK_PRIORITY,
        NULL
    );

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "failed to create app_event_task");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "app_event_task created, queue_len=%d", APP_EVENT_QUEUE_LENGTH);
    return ESP_OK;
}
