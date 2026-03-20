#include "app_event_task.h"

#include "app_config.h"
#include "app_types.h"
#include "beep_service.h"
#include "display_service.h"
#include "led_service.h"
#include "menu_service.h"
#include "esp_log.h"
#include "freertos/task.h"

static const char *TAG = "APP_EVENT";
static QueueHandle_t s_event_queue = NULL;
static uint32_t s_event_recv_count = 0;
static uint32_t s_event_handle_count = 0;

/**
 * @brief 把事件来源转成日志文本
 */
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

/**
 * @brief 把事件类型转成日志文本
 */
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

/**
 * @brief 把按键编号转成日志文本
 */
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

/**
 * @brief 把按键事件转成日志文本
 */
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

/**
 * @brief 把 LED 编号转成日志文本
 */
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

/**
 * @brief 把 LED 模式转成日志文本
 */
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

/**
 * @brief 按按键事件类型映射蜂鸣器提示模式
 */
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

/**
 * @brief 按按键事件类型推导 LED 的下一个模式
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

/**
 * @brief 把蜂鸣器当前状态同步到显示服务
 */
static void app_event_update_display_for_beep(void)
{
    (void)display_service_show_beep_status(
        beep_service_is_enabled(),
        beep_service_is_test_mode_enabled());
}

/**
 * @brief 处理功能键事件
 *
 * BTN_FUNC 不参与 LED 模式切换，而是专门负责蜂鸣器相关功能控制。
 */
static void app_event_handle_function_button(button_event_t button_event)
{
    switch (button_event) {
        case BUTTON_EVENT_SHORT: {
            // 单击功能键：切换蜂鸣器提示开关，并发一个确认音。
            bool enabled = !beep_service_is_enabled();
            (void)beep_service_set_enabled(enabled);
            (void)beep_service_play_force(enabled ? BEEP_PATTERN_DOUBLE : BEEP_PATTERN_SHORT);
            app_event_update_display_for_beep();
            ESP_LOGI(TAG, "function key short -> beep enabled=%d", enabled);
            break;
        }

        case BUTTON_EVENT_LONG:
            // 长按功能键：关闭蜂鸣器提示，同时退出测试模式。
            (void)beep_service_set_test_mode(false);
            (void)beep_service_set_enabled(false);
            (void)beep_service_play_force(BEEP_PATTERN_LONG);
            app_event_update_display_for_beep();
            ESP_LOGI(TAG, "function key long -> beep all disabled");
            break;

        case BUTTON_EVENT_DOUBLE: {
            // 双击功能键：切换蜂鸣器测试模式。
            bool test_mode = !beep_service_is_test_mode_enabled();
            (void)beep_service_set_test_mode(test_mode);
            (void)beep_service_play_force(BEEP_PATTERN_DOUBLE);
            app_event_update_display_for_beep();
            ESP_LOGI(TAG, "function key double -> beep test_mode=%d", test_mode);
            break;
        }

        case BUTTON_EVENT_NONE:
        default:
            break;
    }
}

/**
 * @brief 处理按键事件消息
 *
 * 统一事件任务的职责不是自己去扫按键，而是把 button_service 发布过来的事件
 * 翻译成具体业务动作。v1.4.0 起，这里同时负责同步 LED、BEEP 和 LCD 状态。
 */
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

    // 不管是业务键还是功能键，最近一次按键事件都同步到 LCD 首页。
    (void)display_service_show_last_button(button_id, button_event);

    // 菜单模式下优先由菜单服务接管按键，避免继续触发原来的业务键/功能键逻辑。
    if (menu_service_handle_button_event(button_id, button_event)) {
        return;
    }

    if (button_id == BTN_FUNC) {
        app_event_handle_function_button(button_event);
        return;
    }

    if (button_id >= BTN_SYS && button_id <= BTN_ERR) {
        // 业务键仍然沿用“按键 -> LED 模式变化”的主线，同时附带蜂鸣反馈和显示更新。
        led_id_t led_id = (led_id_t)button_id;
        led_mode_t cur_mode = led_service_get_mode(led_id);
        led_mode_t next_mode = app_event_get_next_mode_for_button_event(cur_mode, button_event);

        ESP_LOGI(TAG, "business key -> %s: %s -> %s",
                 app_event_led_to_string(led_id),
                 app_event_mode_to_string(cur_mode),
                 app_event_mode_to_string(next_mode));

        if (next_mode != cur_mode) {
            // 只有模式真的变化时才去切换 LED，避免无意义重复设置。
            (void)led_service_set_mode(led_id, next_mode);
        }

        // 业务结果同步到显示服务和蜂鸣器服务，让多个输出端保持一致。
        (void)display_service_show_led_status(led_id, next_mode);
        (void)beep_service_play(app_event_get_beep_pattern_for_button_event(button_event));
        app_event_update_display_for_beep();
        return;
    }

    ESP_LOGW(TAG, "unknown button_id=%ld", (long)button_id);
}

/**
 * @brief 统一事件任务主体
 *
 * 这个任务阻塞等待事件队列，收到消息后再按来源和类型分发到具体处理函数。
 */
static void app_event_task(void *param)
{
    (void)param;

    while (1) {
        app_event_msg_t msg;

        // 阻塞接收可以避免没消息时空转耗 CPU。
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

        // 当前版本主要处理按键事件，后面如果扩更多来源，就继续在这里统一分发。
        if (msg.source == APP_EVENT_SOURCE_BUTTON && msg.type == APP_EVENT_TYPE_BUTTON) {
            app_event_handle_button_message(&msg);
        } else {
            ESP_LOGW(TAG, "unhandled event src=%s type=%s",
                     app_event_source_to_string(msg.source),
                     app_event_type_to_string(msg.type));
        }
    }
}

/**
 * @brief 创建统一事件任务
 */
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
