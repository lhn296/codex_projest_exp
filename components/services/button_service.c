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
        s_btn_objs[i].click_count = 0;
        s_btn_objs[i].raw_change_time_ms = 0;
        s_btn_objs[i].press_start_time_ms = 0;
        s_btn_objs[i].last_release_time_ms = 0;
    }

    ESP_LOGI(TAG,
             "Button service init done, button_count=%d debounce=%dms long=%dms double=%dms",
             APP_BUTTON_COUNT,
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
        bool state = bsp_button_read((button_id_t)i);// 读取当前按钮状态
        button_obj_t *btn = &s_btn_objs[i];

        if (state != btn->raw_state) {
            btn->raw_state = state;
            btn->raw_change_time_ms = now_ms;
        }
        
        //现在的状态与稳定状态不同，并且已经过去抖动时间，认为状态已经稳定
        if (btn->raw_state != btn->stable_state &&
            (now_ms - btn->raw_change_time_ms) >= APP_BUTTON_DEBOUNCE_MS) { 
            btn->stable_state = btn->raw_state;
            //状态发生了变化，处理按下或释放事件
            if (btn->stable_state) {
                btn->press_start_time_ms = now_ms;
                btn->long_reported = false;// 按下时重置长按报告和点击计数
            } else {
                if (!btn->long_reported) {// 如果还没有报告过长按事件，说明这是一次短按或双击
                    btn->click_count++;// 增加点击计数
                    btn->last_release_time_ms = now_ms; // 更新上次释放时间

                    if (btn->click_count >= 2) {    // 如果点击计数达到2，认为是双击事件
                        button_service_handle_event(btn->btn_id, BUTTON_EVENT_DOUBLE);
                        btn->click_count = 0;// 重置点击计数
                    }
                } else {    // 如果已经报告过长按事件，重置点击计数
                    btn->click_count = 0;
                }
            }   
        }

        // 如果当前状态是按下，并且还没有报告过长按事件，并且已经超过长按时间，报告长按事件
        if (btn->stable_state &&
            !btn->long_reported &&
            (now_ms - btn->press_start_time_ms) >= APP_BUTTON_LONG_PRESS_MS) {
            button_service_handle_event(btn->btn_id, BUTTON_EVENT_LONG);
            btn->long_reported = true;
            btn->click_count = 0;
        }
        // 如果当前状态是释放，并且点击计数为1，并且已经超过双击时间，报告短按事件
        if (!btn->stable_state &&
            btn->click_count == 1 &&
            (now_ms - btn->last_release_time_ms) >= APP_BUTTON_DOUBLE_CLICK_MS) {
            button_service_handle_event(btn->btn_id, BUTTON_EVENT_SHORT);
            btn->click_count = 0;
        }
    }
}
