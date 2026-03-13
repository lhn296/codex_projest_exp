#include "button_service.h"
#include "bsp_button.h"
#include "led_service.h"
#include "esp_log.h"
#include "app_config.h"

static const char *TAG = "BTN_SERVICE";

/* 按钮对象结构体 */
typedef struct {
    button_id_t btn_id;
    bool last_state;
} button_obj_t;

static button_obj_t s_btn_objs[BTN_MAX];//  按钮对象数组，保存每个按钮的状态信息

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
        s_btn_objs[i].last_state = false;
    }

    ESP_LOGI(TAG, "Button service init done, button_count=%d", APP_BUTTON_COUNT);
    return ESP_OK;
}

/**
 * @brief 周期扫描按键，识别事件并更新 LED 模式
 */
void button_service_process(void)
{
    for (int i = 0; i < APP_BUTTON_COUNT; i++) {
        bool state = bsp_button_read((button_id_t)i);// 读取当前按钮状态
        button_obj_t *btn = &s_btn_objs[i];
        if (state && !btn->last_state) {
            // 按下事件检测
            led_mode_t cur_mode = led_service_get_mode((led_id_t)i);// 获取当前 LED 模式
            led_mode_t next_mode = (cur_mode + 1) % LED_MODE_MAX; // 循环切换   
            led_service_set_mode((led_id_t)i, next_mode);
        }
        btn->last_state = state;// 更新按钮状态
    }
}
