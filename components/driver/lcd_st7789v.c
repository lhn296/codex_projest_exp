#include "lcd_st7789v.h"

#include <ctype.h>
#include <string.h>

#include "spi_bus.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "LCD_ST7789V";

typedef struct {
    lcd_st7789v_config_t cfg;
    bool inited;
} lcd_st7789v_ctx_t;

static lcd_st7789v_ctx_t s_lcd = {0};

#define LCD_ST7789V_CHAR_WIDTH   6
#define LCD_ST7789V_CHAR_HEIGHT  8
#define LCD_ST7789V_COLOR_BLACK  0x0000

/**
 * @brief 控制 LCD 的 DC 管脚电平
 *
 * DC 常用来区分“当前发的是命令”还是“当前发的是数据”。
 */
static esp_err_t lcd_st7789v_set_dc_level(int level)
{
    return gpio_set_level(s_lcd.cfg.dc_gpio, level);
}

/**
 * @brief 向 LCD 发送一条命令
 */
static esp_err_t lcd_st7789v_write_cmd(uint8_t cmd)
{
    // 发命令前先把 DC 拉到命令态。
    esp_err_t ret = lcd_st7789v_set_dc_level(0);
    if (ret != ESP_OK) {
        return ret;
    }
    return spi_bus_transmit_cmd(s_lcd.cfg.spi_handle, cmd);
}

/**
 * @brief 向 LCD 发送数据区
 */
static esp_err_t lcd_st7789v_write_data(const void *data, size_t data_bytes)
{
    // 发数据前把 DC 拉到数据态，后面的 SPI 字节就会被 LCD 当成像素或参数读取。
    esp_err_t ret = lcd_st7789v_set_dc_level(1);
    if (ret != ESP_OK) {
        return ret;
    }
    return spi_bus_transmit_data(s_lcd.cfg.spi_handle, data, data_bytes);
}

/**
 * @brief 发送“命令 + 1字节参数”的简化接口
 */
static esp_err_t lcd_st7789v_write_u8(uint8_t cmd, uint8_t value)
{
    esp_err_t ret = lcd_st7789v_write_cmd(cmd);
    if (ret != ESP_OK) {
        return ret;
    }
    return lcd_st7789v_write_data(&value, sizeof(value));
}

/**
 * @brief 设置 LCD 当前写入窗口
 *
 * 后面无论是清屏、填充矩形还是画字符，本质上都会先把窗口切到目标区域。
 */
static esp_err_t lcd_st7789v_set_window(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
    uint16_t xs = x1 + s_lcd.cfg.x_offset;
    uint16_t xe = x2 + s_lcd.cfg.x_offset;
    uint16_t ys = y1 + s_lcd.cfg.y_offset;
    uint16_t ye = y2 + s_lcd.cfg.y_offset;

    uint8_t col_data[4] = {
        (uint8_t)(xs >> 8), (uint8_t)(xs & 0xFF),
        (uint8_t)(xe >> 8), (uint8_t)(xe & 0xFF),
    };
    uint8_t row_data[4] = {
        (uint8_t)(ys >> 8), (uint8_t)(ys & 0xFF),
        (uint8_t)(ye >> 8), (uint8_t)(ye & 0xFF),
    };

    // 0x2A = CASET，设置列地址范围。
    esp_err_t ret = lcd_st7789v_write_cmd(0x2A);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = lcd_st7789v_write_data(col_data, sizeof(col_data));
    if (ret != ESP_OK) {
        return ret;
    }

    // 0x2B = RASET，设置行地址范围。
    ret = lcd_st7789v_write_cmd(0x2B);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = lcd_st7789v_write_data(row_data, sizeof(row_data));
    if (ret != ESP_OK) {
        return ret;
    }

    // 0x2C = RAMWR，后续发送的数据会写入刚才设定的窗口。
    return lcd_st7789v_write_cmd(0x2C);
}

/**
 * @brief 交换 RGB565 字节序
 *
 * SPI 一般按高字节在前发送，这里提前把颜色值调整成 LCD 更容易接收的顺序。
 */
static uint16_t lcd_st7789v_swap_color(uint16_t color)
{
    return (uint16_t)((color << 8) | (color >> 8));
}

/**
 * @brief 获取一个简化 5x7 字模
 *
 * 当前版本先用轻量点阵字体支撑“模板化显示”，不引入更重的字库。
 */
static bool lcd_st7789v_get_glyph(char input, uint8_t glyph[7])
{
    char c = (char)toupper((unsigned char)input);
    memset(glyph, 0, 7);

    switch (c) {
        case 'A': { uint8_t g[7] = {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11}; memcpy(glyph,g,7); return true; }
        case 'B': { uint8_t g[7] = {0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E}; memcpy(glyph,g,7); return true; }
        case 'C': { uint8_t g[7] = {0x0E,0x11,0x10,0x10,0x10,0x11,0x0E}; memcpy(glyph,g,7); return true; }
        case 'D': { uint8_t g[7] = {0x1C,0x12,0x11,0x11,0x11,0x12,0x1C}; memcpy(glyph,g,7); return true; }
        case 'E': { uint8_t g[7] = {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F}; memcpy(glyph,g,7); return true; }
        case 'F': { uint8_t g[7] = {0x1F,0x10,0x10,0x1E,0x10,0x10,0x10}; memcpy(glyph,g,7); return true; }
        case 'G': { uint8_t g[7] = {0x0E,0x11,0x10,0x17,0x11,0x11,0x0F}; memcpy(glyph,g,7); return true; }
        case 'H': { uint8_t g[7] = {0x11,0x11,0x11,0x1F,0x11,0x11,0x11}; memcpy(glyph,g,7); return true; }
        case 'I': { uint8_t g[7] = {0x1F,0x04,0x04,0x04,0x04,0x04,0x1F}; memcpy(glyph,g,7); return true; }
        case 'J': { uint8_t g[7] = {0x07,0x02,0x02,0x02,0x12,0x12,0x0C}; memcpy(glyph,g,7); return true; }
        case 'K': { uint8_t g[7] = {0x11,0x12,0x14,0x18,0x14,0x12,0x11}; memcpy(glyph,g,7); return true; }
        case 'L': { uint8_t g[7] = {0x10,0x10,0x10,0x10,0x10,0x10,0x1F}; memcpy(glyph,g,7); return true; }
        case 'M': { uint8_t g[7] = {0x11,0x1B,0x15,0x15,0x11,0x11,0x11}; memcpy(glyph,g,7); return true; }
        case 'N': { uint8_t g[7] = {0x11,0x19,0x15,0x13,0x11,0x11,0x11}; memcpy(glyph,g,7); return true; }
        case 'O': { uint8_t g[7] = {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E}; memcpy(glyph,g,7); return true; }
        case 'P': { uint8_t g[7] = {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10}; memcpy(glyph,g,7); return true; }
        case 'Q': { uint8_t g[7] = {0x0E,0x11,0x11,0x11,0x15,0x12,0x0D}; memcpy(glyph,g,7); return true; }
        case 'R': { uint8_t g[7] = {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11}; memcpy(glyph,g,7); return true; }
        case 'S': { uint8_t g[7] = {0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E}; memcpy(glyph,g,7); return true; }
        case 'T': { uint8_t g[7] = {0x1F,0x04,0x04,0x04,0x04,0x04,0x04}; memcpy(glyph,g,7); return true; }
        case 'U': { uint8_t g[7] = {0x11,0x11,0x11,0x11,0x11,0x11,0x0E}; memcpy(glyph,g,7); return true; }
        case 'V': { uint8_t g[7] = {0x11,0x11,0x11,0x11,0x11,0x0A,0x04}; memcpy(glyph,g,7); return true; }
        case 'W': { uint8_t g[7] = {0x11,0x11,0x11,0x15,0x15,0x15,0x0A}; memcpy(glyph,g,7); return true; }
        case 'X': { uint8_t g[7] = {0x11,0x11,0x0A,0x04,0x0A,0x11,0x11}; memcpy(glyph,g,7); return true; }
        case 'Y': { uint8_t g[7] = {0x11,0x11,0x0A,0x04,0x04,0x04,0x04}; memcpy(glyph,g,7); return true; }
        case 'Z': { uint8_t g[7] = {0x1F,0x01,0x02,0x04,0x08,0x10,0x1F}; memcpy(glyph,g,7); return true; }
        case '0': { uint8_t g[7] = {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E}; memcpy(glyph,g,7); return true; }
        case '1': { uint8_t g[7] = {0x04,0x0C,0x04,0x04,0x04,0x04,0x0E}; memcpy(glyph,g,7); return true; }
        case '2': { uint8_t g[7] = {0x0E,0x11,0x01,0x02,0x04,0x08,0x1F}; memcpy(glyph,g,7); return true; }
        case '3': { uint8_t g[7] = {0x1E,0x01,0x01,0x0E,0x01,0x01,0x1E}; memcpy(glyph,g,7); return true; }
        case '4': { uint8_t g[7] = {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02}; memcpy(glyph,g,7); return true; }
        case '5': { uint8_t g[7] = {0x1F,0x10,0x10,0x1E,0x01,0x01,0x1E}; memcpy(glyph,g,7); return true; }
        case '6': { uint8_t g[7] = {0x0E,0x10,0x10,0x1E,0x11,0x11,0x0E}; memcpy(glyph,g,7); return true; }
        case '7': { uint8_t g[7] = {0x1F,0x01,0x02,0x04,0x08,0x08,0x08}; memcpy(glyph,g,7); return true; }
        case '8': { uint8_t g[7] = {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E}; memcpy(glyph,g,7); return true; }
        case '9': { uint8_t g[7] = {0x0E,0x11,0x11,0x0F,0x01,0x01,0x0E}; memcpy(glyph,g,7); return true; }
        case ':': { uint8_t g[7] = {0x00,0x04,0x04,0x00,0x04,0x04,0x00}; memcpy(glyph,g,7); return true; }
        case '.': { uint8_t g[7] = {0x00,0x00,0x00,0x00,0x00,0x06,0x06}; memcpy(glyph,g,7); return true; }
        case '-': { uint8_t g[7] = {0x00,0x00,0x00,0x1F,0x00,0x00,0x00}; memcpy(glyph,g,7); return true; }
        case '_': { uint8_t g[7] = {0x00,0x00,0x00,0x00,0x00,0x00,0x1F}; memcpy(glyph,g,7); return true; }
        case '/': { uint8_t g[7] = {0x01,0x02,0x02,0x04,0x08,0x08,0x10}; memcpy(glyph,g,7); return true; }
        case '(': { uint8_t g[7] = {0x02,0x04,0x08,0x08,0x08,0x04,0x02}; memcpy(glyph,g,7); return true; }
        case ')': { uint8_t g[7] = {0x08,0x04,0x02,0x02,0x02,0x04,0x08}; memcpy(glyph,g,7); return true; }
        case '+': { uint8_t g[7] = {0x00,0x04,0x04,0x1F,0x04,0x04,0x00}; memcpy(glyph,g,7); return true; }
        case ' ': return true;
        default: {
            uint8_t g[7] = {0x1F,0x01,0x02,0x04,0x04,0x00,0x04};
            memcpy(glyph, g, 7);
            return false;
        }
    }
}

/**
 * @brief 连续写入同一种颜色
 *
 * 清屏和大块区域填充时，用这种方式比逐像素构造更直接。
 */
static esp_err_t lcd_st7789v_write_color_repeat(uint16_t color, size_t pixel_count)
{
    uint16_t buffer[64];
    uint16_t be_color = lcd_st7789v_swap_color(color);

    for (size_t i = 0; i < 64; i++) {
        buffer[i] = be_color;
    }

    // 分块发送，避免一次事务过大。
    while (pixel_count > 0) {
        size_t chunk_pixels = pixel_count > 64 ? 64 : pixel_count;
        esp_err_t ret = lcd_st7789v_write_data(buffer, chunk_pixels * sizeof(uint16_t));
        if (ret != ESP_OK) {
            return ret;
        }
        pixel_count -= chunk_pixels;
    }

    return ESP_OK;
}

/**
 * @brief 初始化 ST7789V 驱动
 *
 * 这里完成的事情包括：
 * 1. 配置 DC 管脚
 * 2. 发送基础初始化命令
 * 3. 设置像素格式和方向
 * 4. 最后清屏
 */
esp_err_t lcd_st7789v_init(const lcd_st7789v_config_t *config)
{
    if (config == NULL || config->spi_handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_lcd.inited) {
        return ESP_OK;
    }

    s_lcd.cfg = *config;

    // DC 是显示驱动层唯一直接操作的 GPIO，需要先配置为输出。
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << s_lcd.cfg.dc_gpio),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        return ret;
    }

    // 软件复位后给芯片一点稳定时间，再继续后续初始化序列。
    vTaskDelay(pdMS_TO_TICKS(120));
    ret = lcd_st7789v_write_cmd(0x01);
    if (ret != ESP_OK) {
        return ret;
    }
    vTaskDelay(pdMS_TO_TICKS(120));

    // 0x11 = Sleep Out，退出睡眠。
    ret = lcd_st7789v_write_cmd(0x11);
    if (ret != ESP_OK) {
        return ret;
    }
    vTaskDelay(pdMS_TO_TICKS(120));

    // 0x3A = COLMOD，0x55 表示 RGB565。
    ret = lcd_st7789v_write_u8(0x3A, 0x55);
    if (ret != ESP_OK) {
        return ret;
    }

    // 0x36 = MADCTL，用来控制横竖屏方向和扫描方向。
    ret = lcd_st7789v_write_u8(0x36, s_lcd.cfg.landscape ? 0xA0 : 0x00);
    if (ret != ESP_OK) {
        return ret;
    }

    // 0x21 = Display Inversion On，很多 ST7789 模块需要开反显才能正常显示。
    ret = lcd_st7789v_write_cmd(0x21);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = lcd_st7789v_write_cmd(0x13);
    if (ret != ESP_OK) {
        return ret;
    }

    // 0x29 = Display On，正式开屏。
    ret = lcd_st7789v_write_cmd(0x29);
    if (ret != ESP_OK) {
        return ret;
    }

    s_lcd.inited = true;
    ESP_LOGI(TAG, "LCD ST7789V ready, %ux%u landscape=%d dc=%d",
             s_lcd.cfg.width,
             s_lcd.cfg.height,
             s_lcd.cfg.landscape,
             s_lcd.cfg.dc_gpio);
    return lcd_st7789v_fill_screen(LCD_ST7789V_COLOR_BLACK);
}

/**
 * @brief 查询 LCD 驱动是否已经可用
 */
bool lcd_st7789v_is_ready(void)
{
    return s_lcd.inited;
}

/**
 * @brief 全屏填充纯色
 */
esp_err_t lcd_st7789v_fill_screen(uint16_t color)
{
    return lcd_st7789v_fill_rect(0, 0, s_lcd.cfg.width, s_lcd.cfg.height, color);
}

/**
 * @brief 填充一个矩形区域
 */
esp_err_t lcd_st7789v_fill_rect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color)
{
    if (!s_lcd.inited || width == 0 || height == 0) {
        return ESP_ERR_INVALID_STATE;
    }

    uint16_t x2 = (uint16_t)(x + width - 1);
    uint16_t y2 = (uint16_t)(y + height - 1);

    // 先切窗口，再把颜色数据铺满这个窗口。
    esp_err_t ret = lcd_st7789v_set_window(x, y, x2, y2);
    if (ret != ESP_OK) {
        return ret;
    }

    return lcd_st7789v_write_color_repeat(color, (size_t)width * height);
}

/**
 * @brief 按默认 1 倍尺寸绘制单字符
 */
esp_err_t lcd_st7789v_draw_char(uint16_t x, uint16_t y, char ch, uint16_t fg_color, uint16_t bg_color)
{
    return lcd_st7789v_draw_char_scaled(x, y, ch, fg_color, bg_color, 1);
}

/**
 * @brief 按指定缩放倍数绘制单字符
 *
 * 这层是后面“字体变大”的核心入口，scale=2 时大约等于把 6x8 字体放大成 12x16。
 */
esp_err_t lcd_st7789v_draw_char_scaled(uint16_t x, uint16_t y, char ch, uint16_t fg_color, uint16_t bg_color, uint8_t scale)
{
    if (!s_lcd.inited) {
        return ESP_ERR_INVALID_STATE;
    }
    if (scale == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    // 先取原始 5x7 点阵，再按 scale 把每个点复制成更大的像素块。
    uint8_t glyph[7];
    lcd_st7789v_get_glyph(ch, glyph);

    uint16_t scaled_w = (uint16_t)(LCD_ST7789V_CHAR_WIDTH * scale);
    uint16_t scaled_h = (uint16_t)(LCD_ST7789V_CHAR_HEIGHT * scale);
    uint16_t pixels[LCD_ST7789V_CHAR_WIDTH * LCD_ST7789V_CHAR_HEIGHT * 16];
    uint16_t fg = lcd_st7789v_swap_color(fg_color);
    uint16_t bg = lcd_st7789v_swap_color(bg_color);

    // 把字模中的每一个“开/关”点翻译成实际颜色块。
    for (int row = 0; row < 7; row++) {
        for (int col = 0; col < 5; col++) {
            bool on = (glyph[row] >> (4 - col)) & 0x01;
            for (uint8_t sy = 0; sy < scale; sy++) {
                for (uint8_t sx = 0; sx < scale; sx++) {
                    uint16_t draw_x = (uint16_t)(col * scale + sx);
                    uint16_t draw_y = (uint16_t)(row * scale + sy);
                    pixels[draw_y * scaled_w + draw_x] = on ? fg : bg;
                }
            }
        }
        for (uint8_t sy = 0; sy < scale; sy++) {
            for (uint8_t sx = 0; sx < scale; sx++) {
                uint16_t draw_x = (uint16_t)(5 * scale + sx);
                uint16_t draw_y = (uint16_t)(row * scale + sy);
                pixels[draw_y * scaled_w + draw_x] = bg;
            }
        }
    }

    for (uint8_t sy = 0; sy < scale; sy++) {
        for (uint16_t col = 0; col < scaled_w; col++) {
            uint16_t draw_y = (uint16_t)(7 * scale + sy);
            pixels[draw_y * scaled_w + col] = bg;
        }
    }

    // 字符同样按“先设窗口、再一次性写像素”的方式输出。
    esp_err_t ret = lcd_st7789v_set_window(x, y, x + scaled_w - 1, y + scaled_h - 1);
    if (ret != ESP_OK) {
        return ret;
    }

    return lcd_st7789v_write_data(pixels, scaled_w * scaled_h * sizeof(uint16_t));
}

/**
 * @brief 按默认 1 倍尺寸绘制字符串
 */
esp_err_t lcd_st7789v_draw_string(uint16_t x, uint16_t y, const char *text, uint16_t fg_color, uint16_t bg_color)
{
    return lcd_st7789v_draw_string_scaled(x, y, text, fg_color, bg_color, 1);
}

/**
 * @brief 按指定缩放倍数绘制字符串
 *
 * 这里负责处理：
 * 1. 自动换行
 * 2. 缩放后的字符间距
 * 3. 超出屏幕高度时停止绘制
 */
esp_err_t lcd_st7789v_draw_string_scaled(uint16_t x, uint16_t y, const char *text, uint16_t fg_color, uint16_t bg_color, uint8_t scale)
{
    if (!s_lcd.inited || text == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (scale == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    uint16_t cursor_x = x;
    uint16_t cursor_y = y;
    uint16_t char_w = (uint16_t)(LCD_ST7789V_CHAR_WIDTH * scale);
    uint16_t char_h = (uint16_t)(LCD_ST7789V_CHAR_HEIGHT * scale);

    while (*text != '\0') {
        if (*text == '\n') {
            // 换行符直接把光标移到下一行开头。
            cursor_x = x;
            cursor_y = (uint16_t)(cursor_y + char_h + 2);
            text++;
            continue;
        }

        if ((cursor_x + char_w) > s_lcd.cfg.width) {
            // 水平方向超出显示宽度时自动换行。
            cursor_x = x;
            cursor_y = (uint16_t)(cursor_y + char_h + 2);
        }

        if ((cursor_y + char_h) > s_lcd.cfg.height) {
            // 垂直方向放不下时直接停止，避免越界绘图。
            break;
        }

        esp_err_t ret = lcd_st7789v_draw_char_scaled(cursor_x, cursor_y, *text, fg_color, bg_color, scale);
        if (ret != ESP_OK) {
            return ret;
        }

        cursor_x = (uint16_t)(cursor_x + char_w);
        text++;
    }

    return ESP_OK;
}
