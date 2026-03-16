#include "bsp_lcd.h"

#include "app_config.h"
#include "bsp_xl9555.h"
#include "lcd_st7789v.h"
#include "spi_bus.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "BSP_LCD";
static bool s_bsp_lcd_inited = false;
static spi_device_handle_t s_lcd_spi_handle = NULL;

/**
 * @brief 控制 LCD 供电脚
 *
 * 这里通过 XL9555 去控制板上的 LCD_PWR，不直接走 ESP32 GPIO。
 */
static esp_err_t bsp_lcd_power_set(bool on)
{
    return bsp_xl9555_lcd_ctrl_write((xl9555_pin_t)APP_LCD_PWR_PIN, on ? APP_LCD_PWR_ON_LEVEL : APP_LCD_PWR_OFF_LEVEL);
}

/**
 * @brief 控制 LCD 复位脚
 */
static esp_err_t bsp_lcd_reset_set(bool active)
{
    return bsp_xl9555_lcd_ctrl_write((xl9555_pin_t)APP_LCD_RST_PIN, active ? APP_LCD_RST_ACTIVE_LEVEL : APP_LCD_RST_INACTIVE_LEVEL);
}

/**
 * @brief 初始化板级 LCD 适配层
 *
 * 这一层负责把“这块板子的接线方式”翻译成统一 LCD 初始化流程：
 * 1. 先准备 XL9555 控制口
 * 2. 再做 LCD 上电/复位时序
 * 3. 最后初始化 SPI 和 ST7789V 驱动
 */
esp_err_t bsp_lcd_init(void)
{
    if (s_bsp_lcd_inited) {
        return ESP_OK;
    }

    // LCD 的 RST/PWR 挂在 XL9555 上，所以显示初始化前必须先确保 XL9555 可用。
    esp_err_t ret = bsp_xl9555_init();
    if (ret != ESP_OK) {
        return ret;
    }

    // 初始化 XL9555 上对应的 LCD 控制脚方向。
    ret = bsp_xl9555_lcd_ctrl_init();
    if (ret != ESP_OK) {
        return ret;
    }

    // 先执行一次标准上电时序：断电、保持复位、再上电、释放复位。
    ret = bsp_lcd_power_set(false);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = bsp_lcd_reset_set(true);
    if (ret != ESP_OK) {
        return ret;
    }

    vTaskDelay(pdMS_TO_TICKS(20));
    ret = bsp_lcd_power_set(true);
    if (ret != ESP_OK) {
        return ret;
    }
    vTaskDelay(pdMS_TO_TICKS(20));
    ret = bsp_lcd_reset_set(false);
    if (ret != ESP_OK) {
        return ret;
    }
    vTaskDelay(pdMS_TO_TICKS(120));

    // 建立当前项目的 SPI 总线模板，后面其他 SPI 外设也可以继续复用这层。
    spi_bus_config_ex_t bus_cfg = {
        .host = APP_LCD_SPI_HOST,
        .mosi_io_num = APP_LCD_SPI_MOSI_GPIO,
        .miso_io_num = APP_LCD_SPI_MISO_GPIO,
        .sclk_io_num = APP_LCD_SPI_SCLK_GPIO,
        .max_transfer_sz = APP_LCD_SPI_MAX_TRANSFER_SZ,
    };
    ret = spi_bus_init(&bus_cfg);
    if (ret != ESP_OK) {
        return ret;
    }

    spi_bus_device_config_ex_t dev_cfg = {
        .cs_io_num = APP_LCD_SPI_CS_GPIO,
        .clock_speed_hz = APP_LCD_SPI_CLOCK_HZ,
        .mode = 0,
        .queue_size = APP_LCD_SPI_QUEUE_SIZE,
        .command_bits = 0,
        .address_bits = 0,
    };
    // 在总线上注册 LCD 设备，后面 lcd_st7789v 都围绕这个 handle 发命令和数据。
    ret = spi_bus_register_device(&dev_cfg, &s_lcd_spi_handle);
    if (ret != ESP_OK) {
        return ret;
    }

    // 把板级参数整理成 LCD 驱动层能直接使用的配置结构。
    lcd_st7789v_config_t lcd_cfg = {
        .spi_handle = s_lcd_spi_handle,
        .dc_gpio = APP_LCD_SPI_DC_GPIO,
        .width = APP_LCD_WIDTH,
        .height = APP_LCD_HEIGHT,
        .x_offset = APP_LCD_X_OFFSET,
        .y_offset = APP_LCD_Y_OFFSET,
        .landscape = APP_LCD_ROTATION_LANDSCAPE,
    };
    ret = lcd_st7789v_init(&lcd_cfg);
    if (ret != ESP_OK) {
        return ret;
    }

    s_bsp_lcd_inited = true;
    ESP_LOGI(TAG, "LCD board ready, cs=%d dc=%d rst_pin=%d pwr_pin=%d",
             APP_LCD_SPI_CS_GPIO,
             APP_LCD_SPI_DC_GPIO,
             APP_LCD_RST_PIN,
             APP_LCD_PWR_PIN);
    return ESP_OK;
}

/**
 * @brief 清屏
 */
esp_err_t bsp_lcd_clear(uint16_t color)
{
    if (!s_bsp_lcd_inited) {
        return ESP_ERR_INVALID_STATE;
    }

    return lcd_st7789v_fill_screen(color);
}

/**
 * @brief 按默认字号绘制字符串
 */
esp_err_t bsp_lcd_draw_string(uint16_t x, uint16_t y, const char *text, uint16_t fg_color, uint16_t bg_color)
{
    if (!s_bsp_lcd_inited) {
        return ESP_ERR_INVALID_STATE;
    }

    return lcd_st7789v_draw_string(x, y, text, fg_color, bg_color);
}

/**
 * @brief 按指定缩放倍数绘制字符串
 *
 * 这一层是显示服务后面最常直接调用的接口，方便业务层只关心“显示多大”，
 * 而不用碰到底层 ST7789V 的字符绘制细节。
 */
esp_err_t bsp_lcd_draw_string_scaled(uint16_t x, uint16_t y, const char *text, uint16_t fg_color, uint16_t bg_color, uint8_t scale)
{
    if (!s_bsp_lcd_inited) {
        return ESP_ERR_INVALID_STATE;
    }

    return lcd_st7789v_draw_string_scaled(x, y, text, fg_color, bg_color, scale);
}
