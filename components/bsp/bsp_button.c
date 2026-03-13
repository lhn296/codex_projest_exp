#include "bsp_button.h"
#include "app_config.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "BSP_BTN";

/**
 * @brief 按钮对象结构体
 */
typedef struct {
    gpio_num_t gpio_num;
    bool inited;
} bsp_button_obj_t;

/* 按钮对象数组 */
static bsp_button_obj_t s_buttons[BTN_MAX] = {
    [BTN_SYS] = {APP_BTN_SYS_GPIO, false},
    [BTN_NET] = {APP_BTN_NET_GPIO, false},
    [BTN_ERR] = {APP_BTN_ERR_GPIO, false},
};

static inline bool bsp_button_is_valid_id(button_id_t btn_id)
{
    return (btn_id >= 0) && (btn_id < APP_BUTTON_COUNT);
}

/**
 * @brief 初始化按键硬件
 * @param btn_id 按键编号
 * @return ESP_OK 成功，否则返回错误码
 */
esp_err_t bsp_button_init(button_id_t btn_id)
{
    if (!bsp_button_is_valid_id(btn_id)) {
        ESP_LOGE(TAG, "invalid btn_id=%d", btn_id);
        return ESP_ERR_INVALID_ARG;
    }

    bsp_button_obj_t *btn = &s_buttons[btn_id];// 获取按键对象
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << btn->gpio_num),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "gpio_config failed, btn_id=%d, gpio=%d, ret=0x%x",
                 btn_id, btn->gpio_num, ret);
        return ret;
    }

    btn->inited = true;
    ESP_LOGI(TAG, "Button init success, btn_id=%d, gpio=%d, active_level=%d",
             btn_id, btn->gpio_num, APP_BUTTON_ACTIVE_LEVEL);
    return ESP_OK;
}

/**
 * @brief 读取按键状态
 * @param btn_id 按键编号
 * @return true=按下, false=未按下
 */
bool bsp_button_read(button_id_t btn_id)
{
    if (!bsp_button_is_valid_id(btn_id) || !s_buttons[btn_id].inited) {
        return false;
    }

    return gpio_get_level(s_buttons[btn_id].gpio_num) == APP_BUTTON_ACTIVE_LEVEL;
}
