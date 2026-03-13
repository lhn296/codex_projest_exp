#include "led_service.h"
#include "bsp_led.h"
#include "app_config.h"

#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "LED_SERVICE";

typedef struct {
    led_mode_t mode;// 当前工作模式
    int64_t last_toggle_time_ms;// 上次切换状态的时间戳，单位毫秒
    bool in_use;    // 是否在使用中
} led_service_obj_t;

static bool s_led_service_inited = false;
static led_service_obj_t s_led_objs[LED_ID_MAX];

static inline bool led_service_is_valid_id(led_id_t led_id)
{
    return (led_id >= 0) && (led_id < LED_ID_MAX);
}

static int64_t led_service_get_time_ms(void)
{
    return esp_timer_get_time() / 1000;
}

static const char *led_service_mode_to_string(led_mode_t mode)
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

static const char *led_service_id_to_string(led_id_t led_id)
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

static uint32_t led_service_get_blink_period_ms(led_mode_t mode)
{
    switch (mode) {
        case LED_MODE_BLINK_SLOW:
            return APP_LED_BLINK_SLOW_PERIOD_MS;
        case LED_MODE_BLINK_FAST:
            return APP_LED_BLINK_FAST_PERIOD_MS;
        case LED_MODE_OFF:
        case LED_MODE_ON:
        default:
            return 0;
    }
}

esp_err_t led_service_init(void)
{
    if (s_led_service_inited) {
        ESP_LOGW(TAG, "LED service already initialized");
        return ESP_OK;
    }

    for (int i = 0; i < LED_ID_MAX; i++) {
        esp_err_t ret = bsp_led_init((led_id_t)i); //将i的值转换为led_id_t类型 也就是每个LED的编号
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "bsp_led_init failed, led_id=%d, ret=0x%x", i, ret);
            return ret;
        }

        s_led_objs[i].mode = LED_MODE_OFF;
        s_led_objs[i].last_toggle_time_ms = led_service_get_time_ms();
        s_led_objs[i].in_use = true;
    }

    s_led_service_inited = true;
    ESP_LOGI(TAG, "Multi LED service init success");
    return ESP_OK;
}

esp_err_t led_service_on(led_id_t led_id)
{
    if (!s_led_service_inited) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!led_service_is_valid_id(led_id)) {
        return ESP_ERR_INVALID_ARG;
    }

    return bsp_led_set(led_id, true);
}

esp_err_t led_service_off(led_id_t led_id)
{
    if (!s_led_service_inited) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!led_service_is_valid_id(led_id)) {
        return ESP_ERR_INVALID_ARG;
    }

    return bsp_led_set(led_id, false);
}

esp_err_t led_service_toggle(led_id_t led_id)
{
    if (!s_led_service_inited) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!led_service_is_valid_id(led_id)) {
        return ESP_ERR_INVALID_ARG;
    }

    return bsp_led_toggle(led_id);
}

esp_err_t led_service_set_mode(led_id_t led_id, led_mode_t mode)
{
    if (!s_led_service_inited) {
        ESP_LOGE(TAG, "service not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (!led_service_is_valid_id(led_id)) {
        ESP_LOGE(TAG, "invalid led_id=%d", led_id);
        return ESP_ERR_INVALID_ARG;
    }

    switch (mode) {
        case LED_MODE_OFF:
            led_service_off(led_id);
            break;

        case LED_MODE_ON:
            led_service_on(led_id);
            break;

        case LED_MODE_BLINK_SLOW:
        case LED_MODE_BLINK_FAST:
            s_led_objs[led_id].last_toggle_time_ms = led_service_get_time_ms();
            break;

        default:
            ESP_LOGE(TAG, "invalid mode=%d", mode);
            return ESP_ERR_INVALID_ARG;
    }

    s_led_objs[led_id].mode = mode;

    ESP_LOGI(TAG, "%s mode changed to %s",
             led_service_id_to_string(led_id),
             led_service_mode_to_string(mode));

    return ESP_OK;
}

led_mode_t led_service_get_mode(led_id_t led_id)
{
    if (!led_service_is_valid_id(led_id)) {
        return LED_MODE_OFF;
    }

    return s_led_objs[led_id].mode;
}

void led_service_process(void)
{
    if (!s_led_service_inited) {
        return;
    }

    int64_t now_ms = led_service_get_time_ms();// 获取当前时间

    for (int i = 0; i < LED_ID_MAX; i++) {
        led_mode_t mode = s_led_objs[i].mode; // 获取当前 LED 模式     
        uint32_t period_ms = led_service_get_blink_period_ms(mode);// 获取闪烁周期
        // 如果周期为 0，表示不需要闪烁，直接跳过
        if (period_ms == 0) {
            continue;   
        }
        // 检查是否到了切换状态的时间,如果到了则切换 LED 状态并更新时间戳
        if ((now_ms - s_led_objs[i].last_toggle_time_ms) >= period_ms) {
            if (led_service_toggle((led_id_t)i) == ESP_OK) {
                s_led_objs[i].last_toggle_time_ms = now_ms;
            }
        }
    }
}