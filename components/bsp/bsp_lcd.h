#ifndef BSP_LCD_H
#define BSP_LCD_H

#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t bsp_lcd_init(void);
esp_err_t bsp_lcd_clear(uint16_t color);
esp_err_t bsp_lcd_fill_rect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color);
esp_err_t bsp_lcd_draw_string(uint16_t x, uint16_t y, const char *text, uint16_t fg_color, uint16_t bg_color);
esp_err_t bsp_lcd_draw_string_scaled(uint16_t x, uint16_t y, const char *text, uint16_t fg_color, uint16_t bg_color, uint8_t scale);

#ifdef __cplusplus
}
#endif

#endif
