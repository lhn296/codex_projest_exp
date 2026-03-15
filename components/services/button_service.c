#include "button_service.h"
#include "bsp_button.h"
#include "esp_log.h"
#include "app_config.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

static const char *TAG = "BTN_SERVICE";
static QueueHandle_t s_button_event_queue = NULL;
static uint32_t s_event_send_ok_count = 0;
static uint32_t s_event_send_fail_count = 0;

/* 按钮对象结构体 */
typedef struct {
    button_id_t btn_id;
    bool raw_state; // 当前原始状态（未去抖动） 
    bool stable_state; // 稳定状态（去抖动后）
    bool long_reported; // 是否已报告过长按事件
    bool irq_tracking; // 中断触发后进入跟踪期，直到这次按键完整结束
    uint8_t click_count; // 连续点击计数
    int64_t raw_change_time_ms;// 原始状态上次变化时间
    int64_t press_start_time_ms;// 按下开始时间
    int64_t last_release_time_ms;// 上次释放时间
} button_obj_t;

static button_obj_t s_btn_objs[BTN_MAX];//  按钮对象数组，保存每个按钮的状态信息

static int64_t button_service_get_time_ms(void)
{
    return esp_timer_get_time() / 1000;
}

static const char *button_service_source_to_string(app_event_source_t source)
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

static const char *button_service_type_to_string(app_event_type_t type)
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

static bool button_service_should_poll(const button_obj_t *btn)
{
    // 中断模式下不再无脑扫描所有按键，只对“正在发生动作”的按键继续跟踪。
    return btn->irq_tracking ||
           btn->raw_state ||
           btn->stable_state ||
           (btn->click_count > 0);
}

static esp_err_t button_service_publish_event(button_id_t btn_id, button_event_t event, int64_t now_ms)
{
    if (s_button_event_queue == NULL) {
        ESP_LOGE(TAG, "button event queue is not ready");
        return ESP_ERR_INVALID_STATE;
    }

    // v1.2.1 开始统一使用通用事件消息格式，按键事件只是其中一种来源。
    app_event_msg_t msg = {
        .source = APP_EVENT_SOURCE_BUTTON,
        .type = APP_EVENT_TYPE_BUTTON,
        .param1 = (int32_t)btn_id,
        .param2 = (int32_t)event,
        .timestamp_ms = now_ms,
    };

    // 这里使用非阻塞发送：如果队列满了，宁可丢这次消息并打印日志，
    // 也不要把按键状态机卡住。
    if (xQueueSend(s_button_event_queue, &msg, 0) != pdPASS) {
        s_event_send_fail_count++;
        ESP_LOGW(TAG, "queue full, drop src=%s type=%s p1=%ld p2=%ld fail=%lu",
                 button_service_source_to_string(msg.source),
                 button_service_type_to_string(msg.type),
                 (long)msg.param1,
                 (long)msg.param2,
                 (unsigned long)s_event_send_fail_count);
        return ESP_ERR_TIMEOUT;
    }

    s_event_send_ok_count++;
    ESP_LOGI(TAG, "enqueue src=%s type=%s p1=%ld p2=%ld ts=%lld ok=%lu",
             button_service_source_to_string(msg.source),
             button_service_type_to_string(msg.type),
             (long)msg.param1,
             (long)msg.param2,
             (long long)msg.timestamp_ms,
             (unsigned long)s_event_send_ok_count);
    return ESP_OK;
}

// v1.2.0 起，按键服务不再直接操作 LED，而是统一把事件投递到队列。
static void button_service_handle_event(button_id_t btn_id, button_event_t event)
{
    button_service_publish_event(btn_id, event, button_service_get_time_ms());
}

/**
 * @brief 初始化按钮服务
 * @return ESP_OK 成功，否则返回错误码
 */
esp_err_t button_service_init(QueueHandle_t event_queue)
{
    if (event_queue == NULL) {
        ESP_LOGE(TAG, "event_queue is null");
        return ESP_ERR_INVALID_ARG;
    }

    // 由 app_main_task 创建队列，再把句柄注入给 button_service，
    // 这样服务层本身不负责系统资源创建，只负责使用。
    s_button_event_queue = event_queue;

    for (int i = 0; i < APP_BUTTON_COUNT; i++) {
        esp_err_t ret = bsp_button_init((button_id_t)i);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "bsp_button_init failed, btn_id=%d, ret=0x%x", i, ret);
            return ret;
        }

        s_btn_objs[i].btn_id = (button_id_t)i;
        s_btn_objs[i].raw_state = false;
        s_btn_objs[i].stable_state = false;
        s_btn_objs[i].long_reported = false;
        s_btn_objs[i].irq_tracking = false;
        s_btn_objs[i].click_count = 0;
        s_btn_objs[i].raw_change_time_ms = 0;
        s_btn_objs[i].press_start_time_ms = 0;
        s_btn_objs[i].last_release_time_ms = 0;
    }

    ESP_LOGI(TAG,
             "Button service init done, mode=interrupt+unified-event debounce=%dms long=%dms double=%dms",
             APP_BUTTON_DEBOUNCE_MS,
             APP_BUTTON_LONG_PRESS_MS,
             APP_BUTTON_DOUBLE_CLICK_MS);
    return ESP_OK;
}

/**
 * @brief 周期扫描按键，识别事件并发送到事件队列
 */
void button_service_process(void)
{
    int64_t now_ms = button_service_get_time_ms();

    for (int i = 0; i < APP_BUTTON_COUNT; i++) {
        button_obj_t *btn = &s_btn_objs[i];
        bool press_irq = bsp_button_consume_press_irq(btn->btn_id);

        if (press_irq) {
            // 下降沿中断只负责告诉服务层“某个按键开始有动作了”，
            // 真正的消抖、长按和双击判断仍留在普通上下文中处理。
            btn->irq_tracking = true;

            // 下降沿意味着“可能开始按下”，先把原始状态推进到按下候选态，
            // 后面仍要经过 debounce 时间才能成为稳定按下。
            if (!btn->raw_state && !btn->stable_state) {// 只有在当前完全未按下的情况下，才允许原始状态推进到按下候选态。
                btn->raw_state = true;
                btn->raw_change_time_ms = now_ms;
            }
        }
        // 没有中断且当前按键状态完全静止（不管是按下还是未按），就不必浪费 CPU 去轮询了。
        if (!button_service_should_poll(btn)) {
            continue;
        }

        // 即使已经用了 GPIO 中断，后续仍然要靠读取电平来完成
        // 消抖、释放判断、长按计时和双击窗口判断。
        bool state = bsp_button_read(btn->btn_id);// 读取当前按钮状态

        if (state != btn->raw_state) {
            btn->raw_state = state;
            btn->raw_change_time_ms = now_ms;
        }
        
        // 原始状态连续保持到达消抖阈值，才允许升级为稳定状态。
        if (btn->raw_state != btn->stable_state &&
            (now_ms - btn->raw_change_time_ms) >= APP_BUTTON_DEBOUNCE_MS) { 
            btn->stable_state = btn->raw_state;

            // 稳定按下只记录起点和上下文；真正的长按由后续时间判断触发。
            if (btn->stable_state) {
                btn->irq_tracking = true;
                btn->press_start_time_ms = now_ms;
                btn->long_reported = false;
            } else {
                // 稳定释放后先不急着报单击，要给双击留出判断窗口。
                if (!btn->long_reported) {
                    btn->click_count++;
                    btn->last_release_time_ms = now_ms;

                    if (btn->click_count >= 2) {
                        button_service_handle_event(btn->btn_id, BUTTON_EVENT_DOUBLE);
                        btn->click_count = 0;
                    }
                } else {
                    btn->click_count = 0;
                }
            }   
        }

        // 长按只触发一次；一旦触发，这次按键就不再参与单击/双击判定。
        if (btn->stable_state &&
            !btn->long_reported &&
            (now_ms - btn->press_start_time_ms) >= APP_BUTTON_LONG_PRESS_MS) {
            button_service_handle_event(btn->btn_id, BUTTON_EVENT_LONG);
            btn->long_reported = true;
            btn->click_count = 0;
        }

        // 单击必须等双击窗口超时后才能确认，否则会和双击抢事件。
        if (!btn->stable_state &&
            btn->click_count == 1 &&
            (now_ms - btn->last_release_time_ms) >= APP_BUTTON_DOUBLE_CLICK_MS) {
            button_service_handle_event(btn->btn_id, BUTTON_EVENT_SHORT);
            btn->click_count = 0;
        }

        // 当这次按键动作完全结束后，退出跟踪态，回到“等下一次中断唤醒”。
        if (!btn->stable_state &&
            !btn->raw_state &&
            btn->click_count == 0 &&
            (now_ms - btn->raw_change_time_ms) >= APP_BUTTON_DEBOUNCE_MS) {
            btn->irq_tracking = false;
        }
    }
}
