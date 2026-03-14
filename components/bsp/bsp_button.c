#include "bsp_button.h"
#include "app_config.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include <stdint.h>

static const char *TAG = "BSP_BTN";

/**
 * @brief 按钮对象结构体
 */
typedef struct {
    gpio_num_t gpio_num;
    bool inited;
    volatile bool press_irq_pending; // ISR 只置位这个标志，业务层再来消费
} bsp_button_obj_t;

/* 按钮对象数组 */
static bsp_button_obj_t s_buttons[BTN_MAX] = {
    [BTN_SYS] = {APP_BTN_SYS_GPIO, false, false},
    [BTN_NET] = {APP_BTN_NET_GPIO, false, false},
    [BTN_ERR] = {APP_BTN_ERR_GPIO, false, false},
};

static bool s_isr_service_inited = false;

static inline bool bsp_button_is_valid_id(button_id_t btn_id)
{
    return (btn_id >= 0) && (btn_id < APP_BUTTON_COUNT);
}

/* ISR 必须尽量短小，这里只做“哪个按键触发了下降沿”的标记。 */
static void IRAM_ATTR bsp_button_gpio_isr_handler(void *arg)
{
    uintptr_t btn_index = (uintptr_t)arg; //    

    if (btn_index < BTN_MAX) {
        s_buttons[btn_index].press_irq_pending = true;
    }
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
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "gpio_config failed, btn_id=%d, gpio=%d, ret=0x%x",
                 btn_id, btn->gpio_num, ret);
        return ret;
    }

    /* GPIO ISR 服务是全局资源，只需要安装一次。 */
    if (!s_isr_service_inited) {
        ret = gpio_install_isr_service(0); // 默认配置，使用 GPIO_INTR_FLAG_DEFAULT
        if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
            ESP_LOGE(TAG, "gpio_install_isr_service failed, ret=0x%x", ret);
            return ret;
        }

        s_isr_service_inited = true;
    }

    /* 每个按键各自绑定一个下降沿中断处理入口。 */
    ret = gpio_isr_handler_add(btn->gpio_num, bsp_button_gpio_isr_handler, (void *)(uintptr_t)btn_id);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "gpio_isr_handler_add failed, btn_id=%d, gpio=%d, ret=0x%x",
                 btn_id, btn->gpio_num, ret);
        return ret;
    }

    btn->inited = true;
    btn->press_irq_pending = false;

    ESP_LOGI(TAG, "Button init success, btn_id=%d, gpio=%d, active_level=%d, intr=negedge",
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

/**
 * @brief 消费按键按下中断
 * @param btn_id 按键编号
 * @return true=有按下中断, false=无按下中断
 */
bool bsp_button_consume_press_irq(button_id_t btn_id)
{
    if (!bsp_button_is_valid_id(btn_id) || !s_buttons[btn_id].inited) {
        return false;
    }

    /* 服务层读取后立即清零，形成一次性的“中断通知”。 */
    bool pending = s_buttons[btn_id].press_irq_pending;
    s_buttons[btn_id].press_irq_pending = false;
    return pending;
}
