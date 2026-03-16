#ifndef LCD_ST7789V_H
#define LCD_ST7789V_H

#include <stdbool.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    spi_device_handle_t spi_handle;
    gpio_num_t dc_gpio;
    uint16_t width;
    uint16_t height;
    uint16_t x_offset;
    uint16_t y_offset;
    bool landscape;
} lcd_st7789v_config_t;

esp_err_t lcd_st7789v_init(const lcd_st7789v_config_t *config);
bool lcd_st7789v_is_ready(void);
esp_err_t lcd_st7789v_fill_screen(uint16_t color);
esp_err_t lcd_st7789v_fill_rect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color);
esp_err_t lcd_st7789v_draw_char(uint16_t x, uint16_t y, char ch, uint16_t fg_color, uint16_t bg_color);
esp_err_t lcd_st7789v_draw_string(uint16_t x, uint16_t y, const char *text, uint16_t fg_color, uint16_t bg_color);
esp_err_t lcd_st7789v_draw_char_scaled(uint16_t x, uint16_t y, char ch, uint16_t fg_color, uint16_t bg_color, uint8_t scale);
esp_err_t lcd_st7789v_draw_string_scaled(uint16_t x, uint16_t y, const char *text, uint16_t fg_color, uint16_t bg_color, uint8_t scale);

#ifdef __cplusplus
}
#endif

#endif
