#include "bsp_led.h"
#include "app_config.h"

#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"

static const char *TAG = "BSP_LED";
/* LED 对象 */
typedef struct {
    gpio_num_t gpio_num;          // 当前 LED 实际连接的 GPIO 编号。
    uint8_t active_level;         // 点亮 LED 所需的有效电平，0=低电平点亮，1=高电平点亮。
    bool default_on;              // 初始化后默认是否点亮。
    bool current_state;           // 当前 LED 逻辑状态，true=亮，false=灭。
    bool inited;                  // 当前 LED 硬件是否已经初始化完成。
} bsp_led_obj_t;

/* LED 对象数组 */
static bsp_led_obj_t s_leds[LED_ID_MAX] = {
    [LED_ID_SYS] = {// 系统状态灯
        .gpio_num = APP_SYS_LED_GPIO,
        .active_level = APP_SYS_LED_ACTIVE_LEVEL,
        .default_on = APP_SYS_LED_DEFAULT_ON,
        .current_state = false,
        .inited = false,
    },
    [LED_ID_NET] = {// 网络状态灯
        .gpio_num = APP_NET_LED_GPIO,
        .active_level = APP_NET_LED_ACTIVE_LEVEL,
        .default_on = APP_NET_LED_DEFAULT_ON,
        .current_state = false,
        .inited = false,
    },
    [LED_ID_ERR] = {// 错误状态灯
        .gpio_num = APP_ERR_LED_GPIO,
        .active_level = APP_ERR_LED_ACTIVE_LEVEL,
        .default_on = APP_ERR_LED_DEFAULT_ON,
        .current_state = false,
        .inited = false,
    },
};

/* 参数检查函数，检查 LED ID 是否有效 */
static inline bool bsp_led_is_valid_id(led_id_t led_id)
{
    return (led_id >= 0) && (led_id < LED_ID_MAX);
}

/*  将 LED 的逻辑状态转换为 GPIO 电平,根据 active_level 决定点亮时是高电平还是低电平
 *  on 表示要点亮 LED，如果 active_level 是 1，则输出高电平；如果 active_level 是 0，则输出低电平
 *  如果 on 是 false，则输出与 active_level 相反的电平
 *  例如，如果 active_level 是 1，表示高电平点亮，那么当 on 是 true 时输出 1，当 on 是 false 时输出 0
 *  反之，如果 active_level 是 0，表示低电平点亮，那么当 on 是 true 时输出 0，当 on 是 false 时输出 1
 *  这个函数的作用是将 LED 的 GPIO 电平转换为逻辑状态，只要 on 是 true 就点亮 LED，on 是false 就熄灭 LED 
 */
static inline int bsp_led_logic_to_level(uint8_t active_level, bool on)
{
    return on ? active_level : !active_level;
}

/**
 * @brief 初始化 LED 硬件
 * @param led_id LED 编号
 * @return ESP_OK 成功，否则返回错误码
 */
esp_err_t bsp_led_init(led_id_t led_id)
{
    // 参数检查 return (led_id >= 0) && (led_id < LED_ID_MAX);id是否合法范围内
    if (!bsp_led_is_valid_id(led_id)) {
        ESP_LOGE(TAG, "invalid led_id=%d", led_id);
        return ESP_ERR_INVALID_ARG;
    }

    bsp_led_obj_t *led = &s_leds[led_id];// 获取 LED 对象

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << led->gpio_num),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "gpio_config failed, led_id=%d, gpio=%d, ret=0x%x",
                 led_id, led->gpio_num, ret);
        return ret;
    }

    led->current_state = led->default_on;     // 设置当前状态为默认状态

    ret = gpio_set_level(led->gpio_num,
                         bsp_led_logic_to_level(led->active_level, led->current_state));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "gpio_set_level failed during init, led_id=%d, ret=0x%x",
                 led_id, ret);
        return ret;
    }

    led->inited = true; // 标记该 LED 已初始化

    ESP_LOGI(TAG,
             "LED init success, led_id=%d, gpio=%d, active_level=%d, default_on=%d",
             led_id, led->gpio_num, led->active_level, led->default_on);

    return ESP_OK;
}

/**
 * @brief 设置 LED 状态
 * @param led_id LED 编号
 * @param on true=亮, false=灭
 * @return ESP_OK 成功，否则返回错误码
 */
esp_err_t bsp_led_set(led_id_t led_id, bool on)
{
    if (!bsp_led_is_valid_id(led_id)) {
        ESP_LOGE(TAG, "invalid led_id=%d", led_id);
        return ESP_ERR_INVALID_ARG;
    }

    bsp_led_obj_t *led = &s_leds[led_id];

    if (!led->inited) {
        ESP_LOGE(TAG, "bsp_led_set failed, led_id=%d not initialized", led_id);
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = gpio_set_level(
        led->gpio_num,
        bsp_led_logic_to_level(led->active_level, on)
    );

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "gpio_set_level failed, led_id=%d, on=%d, ret=0x%x",
                 led_id, on, ret);
        return ret;
    }

    led->current_state = on;// 更新当前状态
    return ESP_OK;
}

/**
 * @brief 翻转 LED 状态
 * @param led_id LED 编号
 * @return ESP_OK 成功，否则返回错误码
 */
esp_err_t bsp_led_toggle(led_id_t led_id)
{
    if (!bsp_led_is_valid_id(led_id)) {
        ESP_LOGE(TAG, "invalid led_id=%d", led_id);
        return ESP_ERR_INVALID_ARG;
    }

    return bsp_led_set(led_id, !s_leds[led_id].current_state);
}

/**
 * @brief 获取 LED 当前状态
 * @param led_id LED 编号
 * @return true=亮, false=灭
 */
bool bsp_led_get_state(led_id_t led_id)
{
    if (!bsp_led_is_valid_id(led_id)) {
        ESP_LOGE(TAG, "invalid led_id=%d", led_id);
        return false;
    }

    return s_leds[led_id].current_state; // 返回当前状态
}
