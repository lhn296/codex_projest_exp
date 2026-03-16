#include "display_service.h"

#include <stdio.h>
#include <string.h>

#include "app_config.h"
#include "bsp_lcd.h"
#include "esp_log.h"

static const char *TAG = "DISPLAY";

#define DISPLAY_COLOR_BLACK   0x0000
#define DISPLAY_COLOR_WHITE   0xFFFF
#define DISPLAY_COLOR_YELLOW  0xFFE0
#define DISPLAY_COLOR_CYAN    0x07FF
#define DISPLAY_COLOR_GREEN   0x07E0
#define DISPLAY_COLOR_RED     0xF800
#define DISPLAY_TEXT_SCALE_TITLE   2
#define DISPLAY_TEXT_SCALE_BODY    2
#define DISPLAY_TEXT_SCALE_SMALL   1

typedef struct {
    bool inited;
    bool dirty;
    char version[24];
    char stage[48];
    button_id_t last_button_id;
    button_event_t last_button_event;
    led_mode_t led_modes[LED_ID_MAX];
    bool beep_enabled;
    bool beep_test_mode;
} display_service_ctx_t;

static display_service_ctx_t s_display = {
    .inited = false,
    .dirty = false,
    .version = {0},
    .stage = {0},
    .last_button_id = BTN_SYS,
    .last_button_event = BUTTON_EVENT_NONE,
    .led_modes = {LED_MODE_OFF, LED_MODE_OFF, LED_MODE_OFF},
    .beep_enabled = true,
    .beep_test_mode = false,
};

/**
 * @brief 把按钮编号转成便于显示的文本
 */
static const char *display_service_button_to_string(button_id_t button_id)
{
    switch (button_id) {
        case BTN_SYS:
            return "BTN_SYS";
        case BTN_NET:
            return "BTN_NET";
        case BTN_ERR:
            return "BTN_ERR";
        case BTN_FUNC:
            return "BTN_FUNC";
        default:
            return "BTN_UNKNOWN";
    }
}

/**
 * @brief 把按键事件转成便于显示的文本
 */
static const char *display_service_event_to_string(button_event_t event)
{
    switch (event) {
        case BUTTON_EVENT_SHORT:
            return "SHORT";
        case BUTTON_EVENT_LONG:
            return "LONG";
        case BUTTON_EVENT_DOUBLE:
            return "DOUBLE";
        case BUTTON_EVENT_NONE:
        default:
            return "NONE";
    }
}

/**
 * @brief 把 LED 编号转成便于显示的文本
 */
static const char *display_service_led_to_string(led_id_t led_id)
{
    switch (led_id) {
        case LED_ID_SYS:
            return "SYS";
        case LED_ID_NET:
            return "NET";
        case LED_ID_ERR:
            return "ERR";
        default:
            return "UNK";
    }
}

/**
 * @brief 把 LED 模式转成便于显示的文本
 */
static const char *display_service_mode_to_string(led_mode_t mode)
{
    switch (mode) {
        case LED_MODE_OFF:
            return "OFF";
        case LED_MODE_ON:
            return "ON";
        case LED_MODE_BLINK_SLOW:
            return "SLOW";
        case LED_MODE_BLINK_FAST:
            return "FAST";
        default:
            return "UNKNOWN";
    }
}

/**
 * @brief 绘制一行“标签: 值”格式的文本
 *
 * 这个小封装的作用是把首页上很多状态项的显示风格统一起来。
 */
static esp_err_t display_service_draw_line(uint16_t x, uint16_t y, uint16_t color, const char *label, const char *value)
{
    char line[64];
    snprintf(line, sizeof(line), "%s: %s", label, value);
    return bsp_lcd_draw_string_scaled(x, y, line, color, DISPLAY_COLOR_BLACK, DISPLAY_TEXT_SCALE_BODY);
}

/**
 * @brief 初始化显示服务
 *
 * 这一层不关心屏幕具体怎么初始化，只要求 `bsp_lcd_init()` 成功后，
 * 把当前版本、阶段和默认状态写进显示缓存，然后绘制首页。
 */
esp_err_t display_service_init(void)
{
    if (s_display.inited) {
        return ESP_OK;
    }

    // 先让板级 LCD 链路准备好，后面的所有显示接口都建立在它之上。
    esp_err_t ret = bsp_lcd_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "bsp_lcd_init failed, ret=0x%x", ret);
        return ret;
    }

    // 初始化时把项目当前版本和阶段先写进显示缓存，方便上电就能看到基本信息。
    snprintf(s_display.version, sizeof(s_display.version), "%s", APP_PROJECT_VERSION);
    snprintf(s_display.stage, sizeof(s_display.stage), "%s", APP_PROJECT_STAGE_NAME);
    s_display.beep_enabled = true;
    s_display.beep_test_mode = false;
    s_display.dirty = true;
    s_display.inited = true;

    ESP_LOGI(TAG, "display service ready");
    return display_service_refresh_home();// 初始化完成后直接刷新首页，让用户一上电就看到完整的状态信息。
}

/**
 * @brief 全屏清空为黑色背景
 */
esp_err_t display_service_clear(void)
{
    if (!s_display.inited) {
        return ESP_ERR_INVALID_STATE;
    }

    return bsp_lcd_clear(DISPLAY_COLOR_BLACK);
}

/**
 * @brief 更新版本号显示缓存
 */
esp_err_t display_service_show_version(const char *version)
{
    if (!s_display.inited || version == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    snprintf(s_display.version, sizeof(s_display.version), "%s", version);
    s_display.dirty = true;
    return ESP_OK;
}

/**
 * @brief 更新阶段名显示缓存
 */
esp_err_t display_service_show_stage(const char *stage)
{
    if (!s_display.inited || stage == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    snprintf(s_display.stage, sizeof(s_display.stage), "%s", stage);
    s_display.dirty = true;
    return ESP_OK;
}

/**
 * @brief 更新最近一次按键事件显示缓存
 */
esp_err_t display_service_show_last_button(button_id_t button_id, button_event_t button_event)
{
    if (!s_display.inited) {
        return ESP_ERR_INVALID_STATE;
    }

    s_display.last_button_id = button_id;
    s_display.last_button_event = button_event;
    s_display.dirty = true;
    return ESP_OK;
}

/**
 * @brief 更新某一路 LED 的显示缓存
 */
esp_err_t display_service_show_led_status(led_id_t led_id, led_mode_t mode)
{
    if (!s_display.inited || led_id < 0 || led_id >= LED_ID_MAX) {
        return ESP_ERR_INVALID_ARG;
    }

    s_display.led_modes[led_id] = mode;
    s_display.dirty = true;
    return ESP_OK;
}

/**
 * @brief 更新蜂鸣器状态显示缓存
 */
esp_err_t display_service_show_beep_status(bool enabled, bool test_mode)
{
    if (!s_display.inited) {
        return ESP_ERR_INVALID_STATE;
    }

    s_display.beep_enabled = enabled;
    s_display.beep_test_mode = test_mode;
    s_display.dirty = true;
    return ESP_OK;
}

/**
 * @brief 按当前缓存内容重绘首页
 *
 * 当前版本采用最直接的“整页重绘”方式，先求稳定和易懂；
 * 后面如果要优化性能，再考虑局部刷新或脏区更新。
 */
esp_err_t display_service_refresh_home(void)
{
    if (!s_display.inited) {
        return ESP_ERR_INVALID_STATE;
    }

    char line[64];
    // 每次刷新首页时先清屏，再按固定布局从上到下重画所有状态信息。
    esp_err_t ret = display_service_clear();
    if (ret != ESP_OK) {
        return ret;
    }

    // 标题用更大的字号，优先让版本首页一上电就容易辨认。
    ret = bsp_lcd_draw_string_scaled(8, 8, "CODEX PROJECT", DISPLAY_COLOR_YELLOW, DISPLAY_COLOR_BLACK, DISPLAY_TEXT_SCALE_TITLE);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = display_service_draw_line(8, 24, DISPLAY_COLOR_WHITE, "VER", s_display.version);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = display_service_draw_line(8, 40, DISPLAY_COLOR_WHITE, "STAGE", s_display.stage);
    if (ret != ESP_OK) {
        return ret;
    }

    // 下面三行是 LED 状态区，方便把当前三路业务状态直接映射到屏幕。
    snprintf(line, sizeof(line), "%s=%s", display_service_led_to_string(LED_ID_SYS), display_service_mode_to_string(s_display.led_modes[LED_ID_SYS]));
    ret = bsp_lcd_draw_string_scaled(8, 88, line, DISPLAY_COLOR_CYAN, DISPLAY_COLOR_BLACK, DISPLAY_TEXT_SCALE_BODY);
    if (ret != ESP_OK) {
        return ret;
    }

    snprintf(line, sizeof(line), "%s=%s", display_service_led_to_string(LED_ID_NET), display_service_mode_to_string(s_display.led_modes[LED_ID_NET]));
    ret = bsp_lcd_draw_string_scaled(8, 108, line, DISPLAY_COLOR_CYAN, DISPLAY_COLOR_BLACK, DISPLAY_TEXT_SCALE_BODY);
    if (ret != ESP_OK) {
        return ret;
    }

    snprintf(line, sizeof(line), "%s=%s", display_service_led_to_string(LED_ID_ERR), display_service_mode_to_string(s_display.led_modes[LED_ID_ERR]));
    ret = bsp_lcd_draw_string_scaled(8, 128, line, DISPLAY_COLOR_CYAN, DISPLAY_COLOR_BLACK, DISPLAY_TEXT_SCALE_BODY);
    if (ret != ESP_OK) {
        return ret;
    }

    // 蜂鸣器状态区专门显示“开关”和“测试模式”两个关键状态。
    snprintf(line, sizeof(line), "BEEP=%s", s_display.beep_enabled ? "ON" : "OFF");
    ret = bsp_lcd_draw_string_scaled(8, 160, line, DISPLAY_COLOR_GREEN, DISPLAY_COLOR_BLACK, DISPLAY_TEXT_SCALE_BODY);
    if (ret != ESP_OK) {
        return ret;
    }

    snprintf(line, sizeof(line), "TEST=%s", s_display.beep_test_mode ? "ON" : "OFF");
    ret = bsp_lcd_draw_string_scaled(8, 180, line, DISPLAY_COLOR_GREEN, DISPLAY_COLOR_BLACK, DISPLAY_TEXT_SCALE_BODY);
    if (ret != ESP_OK) {
        return ret;
    }

    // 最后一行显示最近一次按键事件，后续调试时非常直观。
    snprintf(line, sizeof(line), "%s / %s",
             display_service_button_to_string(s_display.last_button_id),
             display_service_event_to_string(s_display.last_button_event));
    ret = bsp_lcd_draw_string_scaled(8, 212, "LAST:", DISPLAY_COLOR_RED, DISPLAY_COLOR_BLACK, DISPLAY_TEXT_SCALE_SMALL);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = bsp_lcd_draw_string_scaled(48, 212, line, DISPLAY_COLOR_RED, DISPLAY_COLOR_BLACK, DISPLAY_TEXT_SCALE_SMALL);
    if (ret != ESP_OK) {
        return ret;
    }

    s_display.dirty = false;
    return ESP_OK;
}

/**
 * @brief 显示服务周期处理
 *
 * 只要显示缓存被标记为 dirty，就在主循环里触发一次首页刷新。
 */
void display_service_process(void)
{
    if (!s_display.inited || !s_display.dirty) {
        return;
    }

    (void)display_service_refresh_home();
}
