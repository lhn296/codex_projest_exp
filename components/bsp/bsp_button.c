#include "bsp_button.h"
#include "bsp_xl9555.h"
#include "app_config.h"
#include "esp_log.h"
#include <stdint.h>

static const char *TAG = "BSP_BTN";
static volatile uint32_t s_xl9555_irq_count = 0;

/**
 * @brief 按钮对象结构体
 */
typedef struct {
    bool inited;                     // 当前按键对象是否已经完成初始化。
    volatile bool press_irq_pending; // 是否存在待处理的按下中断标记，由 ISR 置位、服务层消费。
} bsp_button_obj_t;

/* 按钮对象数组 */
static bsp_button_obj_t s_buttons[BTN_MAX] = {
    [BTN_SYS] = {false, false},
    [BTN_NET] = {false, false},
    [BTN_ERR] = {false, false},
    [BTN_FUNC] = {false, false},
};

static bool s_button_hw_inited = false;

static inline bool bsp_button_is_valid_id(button_id_t btn_id)
{
    return (btn_id >= 0) && (btn_id < APP_BUTTON_COUNT);
}

/* XL9555 只有一根 INT 线，任何板载按键动作都会通过这根线把服务层唤醒。 */
static void IRAM_ATTR bsp_button_xl9555_int_isr_handler(void *arg)
{
    (void)arg;
    s_xl9555_irq_count++;

    for (int i = 0; i < BTN_MAX; i++) {
        if (s_buttons[i].inited) {
            s_buttons[i].press_irq_pending = true;
        }
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

    esp_err_t ret = ESP_OK;
    if (!s_button_hw_inited) {
        // 先完成 XL9555 板级初始化，再让按键服务复用原有状态机。
        ret = bsp_xl9555_init();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "bsp_xl9555_init failed, ret=0x%x", ret);
            return ret;
        }
        
        ret = bsp_xl9555_keys_init();// 这个函数会把 XL9555 上的按键引脚配置成输入并启用内部上拉，同时把按键映射关系告诉驱动层，方便后续读取。
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "bsp_xl9555_keys_init failed, ret=0x%x", ret);
            return ret;
        }

        // 先把已知的输出资源切到输出模式，避免它们还停留在输入态时持续干扰 INT。
        ret = bsp_xl9555_beep_init();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "bsp_xl9555_beep_init failed, ret=0x%x", ret);
            return ret;
        }

        ret = bsp_xl9555_lcd_ctrl_init();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "bsp_xl9555_lcd_ctrl_init failed, ret=0x%x", ret);
            return ret;
        }

        ret = bsp_xl9555_int_init(bsp_button_xl9555_int_isr_handler, NULL);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "bsp_xl9555_int_init failed, ret=0x%x", ret);
            return ret;
        }

        // 如果初始化后 INT 仍然是低电平，说明这根线可能已经带着历史中断状态。
        // 这时先给日志留痕，后续上板时就能快速判断中断线是否被一直拉低。
        int int_level = bsp_xl9555_int_get_level();
        if (int_level == 0) {
            ESP_LOGW(TAG, "XL9555 INT line is low after init, check wiring or latch state");
        }

        s_button_hw_inited = true;
    }

    bsp_button_obj_t *btn = &s_buttons[btn_id];
    if (btn->inited) {
        return ESP_OK;
    }

    btn->inited = true;
    btn->press_irq_pending = false;

    ESP_LOGI(TAG, "Button init success, btn_id=%d, source=XL9555 INT_GPIO=%d int_level=%d irq_count=%lu",
             btn_id,
             APP_XL9555_INT_GPIO,
             bsp_xl9555_int_get_level(),
             (unsigned long)s_xl9555_irq_count);
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

    return bsp_xl9555_key_read(btn_id);
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

    /* 共用 INT 线时，把一次中断广播成所有按键的一次“开始检查”机会。 */
    bool pending = s_buttons[btn_id].press_irq_pending;
    s_buttons[btn_id].press_irq_pending = false;
    return pending;
}
