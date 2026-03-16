#include "app_event_task.h"

#include "app_config.h"
#include "app_types.h"
#include "beep_service.h"
#include "led_service.h"
#include "esp_log.h"
#include "freertos/task.h"

static const char *TAG = "APP_EVENT";
static QueueHandle_t s_event_queue = NULL;
static uint32_t s_event_recv_count = 0;
static uint32_t s_event_handle_count = 0;

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
        case BTN_FUNC:
            return "BTN_FUNC";
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

static beep_pattern_t app_event_get_beep_pattern_for_button_event(button_event_t event)
{
    switch (event) {
        case BUTTON_EVENT_SHORT:
            return BEEP_PATTERN_SHORT;
        case BUTTON_EVENT_DOUBLE:
            return BEEP_PATTERN_DOUBLE;
        case BUTTON_EVENT_LONG:
            return BEEP_PATTERN_LONG;
        case BUTTON_EVENT_NONE:
        default:
            return BEEP_PATTERN_NONE;
    }
}

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

/* 处理功能键事件 */
static void app_event_handle_function_button(button_event_t button_event)
{
    // KEY3 不再映射成第 4 路 LED，而是作为“功能键”去管理蜂鸣器相关状态。
    // 这样业务键和系统功能键职责分开，后面扩展菜单或模式切换会更自然。
    switch (button_event) {
        case BUTTON_EVENT_SHORT: {
            bool enabled = !beep_service_is_enabled(); // 切换蜂鸣器开关状态
            (void)beep_service_set_enabled(enabled);    // 同步设置生效状态
            // 无论是开还是关，都强制发一个提示音，让用户有明确的反馈，知道按键操作成功了。
            (void)beep_service_play_force(enabled ? BEEP_PATTERN_DOUBLE : BEEP_PATTERN_SHORT);// 开启时发双声，关闭时发单声，方便区分两种状态。
            ESP_LOGI(TAG, "function key short -> beep enabled=%d", enabled);
            break;
        }

        case BUTTON_EVENT_LONG:
            (void)beep_service_set_test_mode(false);
            (void)beep_service_set_enabled(false);
            (void)beep_service_play_force(BEEP_PATTERN_LONG);
            ESP_LOGI(TAG, "function key long -> beep all disabled");
            break;

        case BUTTON_EVENT_DOUBLE: {
            bool test_mode = !beep_service_is_test_mode_enabled();
            (void)beep_service_set_test_mode(test_mode);
            (void)beep_service_play_force(BEEP_PATTERN_DOUBLE);
            ESP_LOGI(TAG, "function key double -> beep test_mode=%d", test_mode);
            break;
        }

        case BUTTON_EVENT_NONE:
        default:
            break;
    }
}

// 事件任务负责把统一事件消息翻译成具体业务动作。
// v1.3.1 开始，业务键会驱动 LED + 蜂鸣反馈，功能键则负责蜂鸣器开关与测试模式。
static void app_event_handle_button_message(const app_event_msg_t *msg)
{
    button_id_t button_id = (button_id_t)msg->param1;
    button_event_t button_event = (button_event_t)msg->param2;

    s_event_handle_count++;
    ESP_LOGI(TAG, "handle src=%s type=%s btn=%s evt=%s ts=%lld handle=%lu",
             app_event_source_to_string(msg->source),
             app_event_type_to_string(msg->type),
             app_event_button_to_string(button_id),
             app_event_button_event_to_string(button_event),
             (long long)msg->timestamp_ms,
             (unsigned long)s_event_handle_count);

    // 如果按键是功能键，就不走 LED 主线了，单独处理蜂鸣器相关状态；
    if (button_id == BTN_FUNC) {
        app_event_handle_function_button(button_event);// 功能键单独处理
        return;
    }

    if (button_id >= BTN_SYS && button_id <= BTN_ERR) {
        // 三个业务键继续沿用“按键事件 -> LED 模式变化”的主线，
        // 同时顺手补一条蜂鸣反馈，让同一个输入事件可以驱动多种输出。
        led_id_t led_id = (led_id_t)button_id;
        led_mode_t cur_mode = led_service_get_mode(led_id);
        led_mode_t next_mode = app_event_get_next_mode_for_button_event(cur_mode, button_event);

        ESP_LOGI(TAG, "business key -> %s: %s -> %s",
                 app_event_led_to_string(led_id),
                 app_event_mode_to_string(cur_mode),
                 app_event_mode_to_string(next_mode));

        if (next_mode != cur_mode) {
            led_service_set_mode(led_id, next_mode);
        }

        // 按键事件也驱动蜂鸣器反馈，增强交互体验。检查按键目前的事件类型，短按/长按/双击分别对应不同的提示节奏。
        (void)beep_service_play(app_event_get_beep_pattern_for_button_event(button_event));
        return;
    }

    ESP_LOGW(TAG, "unknown button_id=%ld", (long)button_id);
}

static void app_event_task(void *param)
{
    (void)param;

    while (1) {
        app_event_msg_t msg;

        // 事件任务使用阻塞接收，没消息时不空转，适合作为统一事件分发入口。
        if (xQueueReceive(s_event_queue, &msg, portMAX_DELAY) != pdPASS) {
            continue;
        }

        s_event_recv_count++;
        ESP_LOGI(TAG, "recv src=%s type=%s p1=%ld p2=%ld ts=%lld recv=%lu",
                 app_event_source_to_string(msg.source),
                 app_event_type_to_string(msg.type),
                 (long)msg.param1,
                 (long)msg.param2,
                 (long long)msg.timestamp_ms,
                 (unsigned long)s_event_recv_count);
        // 目前事件任务只处理按键事件，其他类型的事件先不处理；后续如果有更多事件来源和类型，再在这里统一分发。
        if (msg.source == APP_EVENT_SOURCE_BUTTON && msg.type == APP_EVENT_TYPE_BUTTON) {
            app_event_handle_button_message(&msg);
        } else {
            ESP_LOGW(TAG, "unhandled event src=%s type=%s",
                     app_event_source_to_string(msg.source),
                     app_event_type_to_string(msg.type));
        }
    }
}

esp_err_t app_event_task_start(QueueHandle_t event_queue)
{
    if (event_queue == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

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
