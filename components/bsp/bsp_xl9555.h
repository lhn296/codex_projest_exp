#ifndef BSP_XL9555_H
#define BSP_XL9555_H

#include <stdbool.h>

#include "driver/gpio.h"
#include "esp_err.h"
#include "app_types.h"
#include "xl9555.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t bsp_xl9555_init(void);
esp_err_t bsp_xl9555_keys_init(void);
bool bsp_xl9555_key_read(button_id_t btn_id);
esp_err_t bsp_xl9555_beep_init(void);
esp_err_t bsp_xl9555_beep_set(bool on);
esp_err_t bsp_xl9555_int_init(gpio_isr_t isr_handler, void *arg);
int bsp_xl9555_int_get_level(void);
esp_err_t bsp_xl9555_lcd_ctrl_init(void);
esp_err_t bsp_xl9555_lcd_ctrl_write(xl9555_pin_t pin, bool level);

#ifdef __cplusplus
}
#endif

#endif
