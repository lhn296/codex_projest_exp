#include "bsp_xl9555.h"

#include "app_config.h"
#include "esp_log.h"

static const char *TAG = "BSP_XL9555";

static bool s_xl9555_board_inited = false;
static bool s_xl9555_keys_inited = false;
static bool s_xl9555_beep_inited = false;
static bool s_xl9555_lcd_ctrl_inited = false;
static bool s_xl9555_int_inited = false;
static bool s_gpio_isr_service_inited = false;

static const xl9555_pin_t s_board_key_pins[] = {
    [BTN_SYS] = (xl9555_pin_t)APP_XL9555_KEY0_PIN,
    [BTN_NET] = (xl9555_pin_t)APP_XL9555_KEY1_PIN,
    [BTN_ERR] = (xl9555_pin_t)APP_XL9555_KEY2_PIN,
    [BTN_FUNC] = (xl9555_pin_t)APP_XL9555_KEY3_PIN,
};

static inline bool bsp_xl9555_valid_btn_id(button_id_t btn_id)
{
    return (btn_id >= 0) && (btn_id < BTN_MAX);
}

static inline bool bsp_xl9555_beep_output_level(bool on)
{
    return on ? APP_XL9555_BEEP_ACTIVE_LEVEL : !APP_XL9555_BEEP_ACTIVE_LEVEL;
}

/* 读取 XL9555 输入口来清除中断锁存状态，避免重复触发同一事件。 */
static esp_err_t bsp_xl9555_clear_int_latch(uint8_t *port0_value, uint8_t *port1_value)
{
    uint8_t local_port0 = 0;
    uint8_t local_port1 = 0;

    // 对 XL9555 这类扩展芯片来说，读取输入口通常可以释放当前的中断锁存状态。
    esp_err_t ret = xl9555_read_port(XL9555_PORT_0, &local_port0);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = xl9555_read_port(XL9555_PORT_1, &local_port1);
    if (ret != ESP_OK) {
        return ret;
    }

    if (port0_value != NULL) {
        *port0_value = local_port0;
    }
    if (port1_value != NULL) {
        *port1_value = local_port1;
    }

    return ESP_OK;
}

esp_err_t bsp_xl9555_init(void)
{
    if (s_xl9555_board_inited) {
        return ESP_OK;
    }

    esp_err_t ret = xl9555_init(APP_XL9555_I2C_ADDR);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "xl9555_init failed, ret=0x%x", ret);
        return ret;
    }

    s_xl9555_board_inited = true;
    ESP_LOGI(TAG, "board XL9555 ready, addr=0x%02X", APP_XL9555_I2C_ADDR);
    return ESP_OK;
}

esp_err_t bsp_xl9555_keys_init(void)
{
    esp_err_t ret = bsp_xl9555_init();
    if (ret != ESP_OK) {
        return ret;
    }

    if (s_xl9555_keys_inited) {
        return ESP_OK;
    }

    for (int i = 0; i < BTN_MAX; i++) {
        ret = xl9555_set_pin_mode(s_board_key_pins[i], XL9555_PIN_MODE_INPUT);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "key input config failed, key=%d ret=0x%x", i, ret);
            return ret;
        }
    }

    // 某些 IO 扩展芯片在上电或方向切换后，INT 可能已经处于待触发状态。
    // 这里主动把两个输入口都读一遍，先把历史中断状态清干净。
    uint8_t port0_value = 0;
    uint8_t port1_value = 0;
    ret = bsp_xl9555_clear_int_latch(&port0_value, &port1_value);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "clear key irq latch failed, ret=0x%x", ret);
        return ret;
    }

    s_xl9555_keys_inited = true;
    ESP_LOGI(TAG, "board keys ready, KEY0=%d KEY1=%d KEY2=%d KEY3=%d active_level=%d port0=0x%02X port1=0x%02X",
             APP_XL9555_KEY0_PIN,
             APP_XL9555_KEY1_PIN,
             APP_XL9555_KEY2_PIN,
             APP_XL9555_KEY3_PIN,
             APP_XL9555_KEY_ACTIVE_LEVEL,
             port0_value,
             port1_value);
    return ESP_OK;
}

bool bsp_xl9555_key_read(button_id_t btn_id)
{
    if (!s_xl9555_keys_inited || !bsp_xl9555_valid_btn_id(btn_id)) {
        return false;
    }

    bool level = false;
    if (xl9555_read_pin(s_board_key_pins[btn_id], &level) != ESP_OK) {
        return false;
    }

    return level == APP_XL9555_KEY_ACTIVE_LEVEL;
}

esp_err_t bsp_xl9555_beep_init(void)
{
    esp_err_t ret = bsp_xl9555_init();
    if (ret != ESP_OK) {
        return ret;
    }

    if (s_xl9555_beep_inited) {
        return ESP_OK;
    }

    ret = xl9555_set_pin_mode((xl9555_pin_t)APP_XL9555_BEEP_PIN, XL9555_PIN_MODE_OUTPUT);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = xl9555_write_pin((xl9555_pin_t)APP_XL9555_BEEP_PIN, bsp_xl9555_beep_output_level(false));
    if (ret != ESP_OK) {
        return ret;
    }

    s_xl9555_beep_inited = true;
    ESP_LOGI(TAG, "beep pin ready, pin=%d active_level=%d",
             APP_XL9555_BEEP_PIN,
             APP_XL9555_BEEP_ACTIVE_LEVEL);
    return ESP_OK;
}

esp_err_t bsp_xl9555_beep_set(bool on)
{
    if (!s_xl9555_beep_inited) {
        esp_err_t ret = bsp_xl9555_beep_init();
        if (ret != ESP_OK) {
            return ret;
        }
    }

    return xl9555_write_pin((xl9555_pin_t)APP_XL9555_BEEP_PIN, bsp_xl9555_beep_output_level(on));
}

esp_err_t bsp_xl9555_int_init(gpio_isr_t isr_handler, void *arg)
{
    if (isr_handler == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = bsp_xl9555_init();
    if (ret != ESP_OK) {
        return ret;
    }

    if (s_xl9555_int_inited) {
        return ESP_OK;
    }

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << APP_XL9555_INT_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        return ret;
    }

    if (!s_gpio_isr_service_inited) {
        ret = gpio_install_isr_service(0);
        if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
            return ret;
        }
        s_gpio_isr_service_inited = true;
    }

    ret = gpio_isr_handler_add(APP_XL9555_INT_GPIO, isr_handler, arg);
    if (ret != ESP_OK) {
        return ret;
    }

    s_xl9555_int_inited = true;
    ESP_LOGI(TAG, "XL9555 INT ready, gpio=%d intr=negedge pullup=external-or-board level=%d",
             APP_XL9555_INT_GPIO,
             gpio_get_level(APP_XL9555_INT_GPIO));
    return ESP_OK;
}

int bsp_xl9555_int_get_level(void)
{
    return gpio_get_level(APP_XL9555_INT_GPIO);
}

esp_err_t bsp_xl9555_lcd_ctrl_init(void)
{
    esp_err_t ret = bsp_xl9555_init();
    if (ret != ESP_OK) {
        return ret;
    }

    if (s_xl9555_lcd_ctrl_inited) {
        return ESP_OK;
    }

    ret = xl9555_set_pin_mode((xl9555_pin_t)APP_XL9555_LCD_CTRL0_PIN, XL9555_PIN_MODE_OUTPUT);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = xl9555_set_pin_mode((xl9555_pin_t)APP_XL9555_LCD_CTRL1_PIN, XL9555_PIN_MODE_OUTPUT);
    if (ret != ESP_OK) {
        return ret;
    }

    s_xl9555_lcd_ctrl_inited = true;
    ESP_LOGI(TAG, "lcd ctrl pins ready, pin0=%d pin1=%d",
             APP_XL9555_LCD_CTRL0_PIN,
             APP_XL9555_LCD_CTRL1_PIN);
    return ESP_OK;
}

esp_err_t bsp_xl9555_lcd_ctrl_write(xl9555_pin_t pin, bool level)
{
    if (!s_xl9555_lcd_ctrl_inited) {
        esp_err_t ret = bsp_xl9555_lcd_ctrl_init();
        if (ret != ESP_OK) {
            return ret;
        }
    }

    return xl9555_write_pin(pin, level);
}
