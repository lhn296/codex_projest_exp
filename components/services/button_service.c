#include "button_service.h"
#include "bsp_button.h"
#include "led_service.h"
#include "esp_log.h"
#include "app_config.h"
#include "esp_timer.h"

static const char *TAG = "BTN_SERVICE";

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

static const char *button_service_button_to_string(button_id_t btn_id)
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

static const char *button_service_led_to_string(led_id_t led_id)
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

static const char *button_service_mode_to_string(led_mode_t mode)
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

static const char *button_service_event_to_string(button_event_t event)
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

static bool button_service_should_poll(const button_obj_t *btn)
{
    // 中断模式下不再无脑扫描所有按键，只对“正在发生动作”的按键继续跟踪。
    return btn->irq_tracking ||
           btn->raw_state ||
           btn->stable_state ||
           (btn->click_count > 0);
}

// 根据当前 LED 模式和按钮事件，计算下一个 LED 模式
static led_mode_t button_service_get_next_mode_for_event(led_mode_t cur_mode, button_event_t event)
{
    switch (event) {
        case BUTTON_EVENT_SHORT:
            return (led_mode_t)((cur_mode + 1) % LED_MODE_MAX); // 短按切换到下一个模式
        case BUTTON_EVENT_LONG:
            return LED_MODE_OFF;    // 长按切换到常灭
        case BUTTON_EVENT_DOUBLE:
            return LED_MODE_BLINK_FAST; // 双击切换到快闪
        case BUTTON_EVENT_NONE:
        default:
            return cur_mode;
    }
}

// 处理按钮事件，根据事件类型和当前 LED 模式决定是否切换 LED 模式
static void button_service_handle_event(button_id_t btn_id, button_event_t event)
{
    led_id_t led_id = (led_id_t)btn_id;
    led_mode_t cur_mode = led_service_get_mode(led_id);
    led_mode_t next_mode = button_service_get_next_mode_for_event(cur_mode, event);

    ESP_LOGI(TAG, "%s %s -> %s: %s -> %s",
             button_service_button_to_string(btn_id),
             button_service_event_to_string(event),
             button_service_led_to_string(led_id),
             button_service_mode_to_string(cur_mode),
             button_service_mode_to_string(next_mode));

    if (next_mode != cur_mode) {
        led_service_set_mode(led_id, next_mode);
    }
}

/**
 * @brief 初始化按钮服务
 * @return ESP_OK 成功，否则返回错误码
 */
esp_err_t button_service_init(void)
{
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
             "Button service init done, mode=interrupt debounce=%dms long=%dms double=%dms",
             APP_BUTTON_DEBOUNCE_MS,
             APP_BUTTON_LONG_PRESS_MS,
             APP_BUTTON_DOUBLE_CLICK_MS);
    return ESP_OK;
}

/**
 * @brief 周期扫描按键，识别事件并更新 LED 模式
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
