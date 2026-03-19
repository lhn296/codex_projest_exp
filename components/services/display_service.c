#include "display_service.h"

#include <stdio.h>
#include <string.h>

#include "app_config.h"
#include "bsp_lcd.h"
#include "esp_log.h"

static const char *TAG = "DISPLAY";

#define DISPLAY_COLOR_BLACK    0x0000
#define DISPLAY_COLOR_WHITE    0xFFFF
#define DISPLAY_COLOR_YELLOW   0xFFE0
#define DISPLAY_COLOR_CYAN     0x07FF
#define DISPLAY_COLOR_GREEN    0x07E0
#define DISPLAY_COLOR_RED      0xF800

#define DISPLAY_TEXT_SCALE_TITLE  2
#define DISPLAY_TEXT_SCALE_BODY   2
#define DISPLAY_TEXT_SCALE_SMALL  1
#define DISPLAY_TEXT_SCALE_STATUS 1

// 首页布局常量集中放在这里，后面如果想微调页面，只需要改这一组参数。
#define DISPLAY_HEADER_X            8
#define DISPLAY_HEADER_Y            8
#define DISPLAY_PROJECT_INFO_X      8
#define DISPLAY_PROJECT_INFO_Y      32
#define DISPLAY_PROJECT_INFO_STEP   18
#define DISPLAY_LED_PANEL_X         8
#define DISPLAY_LED_PANEL_Y         76
#define DISPLAY_LED_PANEL_STEP      10
#define DISPLAY_BEEP_PANEL_X        164
#define DISPLAY_BEEP_PANEL_Y        76
#define DISPLAY_BEEP_PANEL_STEP     10
#define DISPLAY_HTTP_PANEL_X        8
#define DISPLAY_HTTP_PANEL_Y        116
#define DISPLAY_HTTP_PANEL_STEP     10
#define DISPLAY_OTA_PANEL_X         164
#define DISPLAY_OTA_PANEL_Y         116
#define DISPLAY_OTA_PANEL_STEP      10
#define DISPLAY_WIFI_PANEL_X        8
#define DISPLAY_WIFI_PANEL_Y        154
#define DISPLAY_WIFI_PANEL_STEP     10
#define DISPLAY_EVENT_PANEL_X       8
#define DISPLAY_EVENT_PANEL_Y       220
#define DISPLAY_EVENT_VALUE_X       56

#define DISPLAY_HEADER_AREA_W       220
#define DISPLAY_HEADER_AREA_H       20
#define DISPLAY_PROJECT_AREA_W      304
#define DISPLAY_PROJECT_AREA_H      42
#define DISPLAY_LED_AREA_W          148
#define DISPLAY_LED_AREA_H          32
#define DISPLAY_BEEP_AREA_W         148
#define DISPLAY_BEEP_AREA_H         20
#define DISPLAY_HTTP_AREA_W         148
#define DISPLAY_HTTP_AREA_H         32
#define DISPLAY_OTA_AREA_W          148
#define DISPLAY_OTA_AREA_H          20
#define DISPLAY_WIFI_AREA_W         304
#define DISPLAY_WIFI_AREA_H         32
#define DISPLAY_EVENT_AREA_W        280
#define DISPLAY_EVENT_AREA_H        10

typedef struct {
    bool inited;                   // 显示服务是否已完成初始化。
    bool full_refresh_pending;     // 是否需要整页刷新，通常用于首次绘制或全局布局变化。
    bool header_dirty;             // 标题区是否需要重绘。
    bool project_info_dirty;       // 项目信息区是否需要重绘。
    bool led_panel_dirty;          // LED 状态区是否需要重绘。
    bool beep_panel_dirty;         // 蜂鸣器状态区是否需要重绘。
    bool http_panel_dirty;         // HTTP 状态区是否需要重绘。
    bool ota_panel_dirty;          // OTA 状态区是否需要重绘。
    bool wifi_panel_dirty;         // Wi-Fi 状态区是否需要重绘。
    bool event_panel_dirty;        // 最近事件区是否需要重绘。
    char version[24];              // 当前显示的版本字符串。
    char stage[48];                // 当前显示的阶段说明字符串。
    button_id_t last_button_id;    // 最近一次按键事件对应的按键编号。
    button_event_t last_button_event; // 最近一次按键事件类型。
    led_mode_t led_modes[LED_ID_MAX]; // 三路 LED 当前模式缓存。
    bool beep_enabled;             // 当前蜂鸣提示开关状态。
    bool beep_test_mode;           // 当前蜂鸣器测试模式状态。
    wifi_state_t wifi_state;       // 当前 Wi-Fi 状态缓存。
    char wifi_ip[20];              // 当前显示的 IPv4 字符串。
    int wifi_rssi;                 // 当前显示的 Wi-Fi 信号强度，单位 dBm。
    uint8_t wifi_channel;          // 当前显示的 AP 信道编号。
    bool http_success;             // 最近一次 HTTP 请求是否成功。
    int http_status_code;          // 最近一次 HTTP 响应状态码。
    char http_message[64];         // 最近一次 HTTP 结果摘要。
    ota_state_t ota_state;         // 当前 OTA 状态缓存。
    char ota_message[64];          // 当前 OTA 说明文本。
} display_service_ctx_t;

static display_service_ctx_t s_display = {
    .inited = false,
    .full_refresh_pending = false,
    .header_dirty = false,
    .project_info_dirty = false,
    .led_panel_dirty = false,
    .beep_panel_dirty = false,
    .http_panel_dirty = false,
    .ota_panel_dirty = false,
    .wifi_panel_dirty = false,
    .event_panel_dirty = false,
    .version = {0},
    .stage = {0},
    .last_button_id = BTN_SYS,
    .last_button_event = BUTTON_EVENT_NONE,
    .led_modes = {LED_MODE_OFF, LED_MODE_OFF, LED_MODE_OFF},
    .beep_enabled = true,
    .beep_test_mode = false,
    .wifi_state = WIFI_STATE_IDLE,
    .wifi_ip = "0.0.0.0",
    .wifi_rssi = -127,
    .wifi_channel = 0,
    .http_success = false,
    .http_status_code = 0,
    .http_message = "IDLE",
    .ota_state = OTA_STATE_IDLE,
    .ota_message = "IDLE",
};

/**
 * @brief 把整页首页标记为需要重绘
 *
 * 这种方式适合初始化或页面布局整体变化时使用。
 */
static void display_service_mark_full_refresh(void)
{
    s_display.full_refresh_pending = true;
    s_display.header_dirty = true;
    s_display.project_info_dirty = true;
    s_display.led_panel_dirty = true;
    s_display.beep_panel_dirty = true;
    s_display.http_panel_dirty = true;
    s_display.ota_panel_dirty = true;
    s_display.wifi_panel_dirty = true;
    s_display.event_panel_dirty = true;
}

/**
 * @brief 清除首页某个局部区域
 *
 * 局部刷新不能直接在旧内容上叠新内容，否则字符串变短时会残留旧字符。
 * 所以每次局部重画前，先把目标区域用背景色擦干净。
 */
static esp_err_t display_service_clear_region(uint16_t x, uint16_t y, uint16_t width, uint16_t height)
{
    return bsp_lcd_fill_rect(x, y, width, height, DISPLAY_COLOR_BLACK);
}

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
 * @brief 把 Wi-Fi 状态转成便于显示的文本
 */
static const char *display_service_wifi_state_to_string(wifi_state_t state)
{
    switch (state) {
        case WIFI_STATE_IDLE:
            return "IDLE";
        case WIFI_STATE_CONNECTING:
            return "CONNECTING";
        case WIFI_STATE_CONNECTED:
            return "CONNECTED";
        case WIFI_STATE_GOT_IP:
            return "GOT_IP";
        case WIFI_STATE_DISCONNECTED:
            return "DISCONNECTED";
        case WIFI_STATE_ERROR:
        default:
            return "ERROR";
    }
}

/**
 * @brief 把 OTA 状态转成便于显示的文本
 */
static const char *display_service_ota_state_to_string(ota_state_t state)
{
    switch (state) {
        case OTA_STATE_IDLE:
            return "IDLE";
        case OTA_STATE_CHECK:
            return "CHECK";
        case OTA_STATE_READY:
            return "READY";
        case OTA_STATE_DOWNLOADING:
            return "DOWN";
        case OTA_STATE_VERIFY:
            return "VERIFY";
        case OTA_STATE_SUCCESS:
            return "SUCCESS";
        case OTA_STATE_FAIL:
        default:
            return "FAIL";
    }
}

/**
 * @brief 绘制一行“标签 : 值”格式的文本
 *
 * 这个小封装专门用来统一首页中各状态项的显示风格，
 * 让上层绘制函数更关注“这行显示什么”，而不是每次自己去拼格式。
 */
static esp_err_t display_service_draw_line(uint16_t x, uint16_t y, uint16_t color, uint8_t scale, const char *label, const char *value)
{
    char line[64];
    snprintf(line, sizeof(line), "%s : %s", label, value);
    return bsp_lcd_draw_string_scaled(x, y, line, color, DISPLAY_COLOR_BLACK, scale);
}

/**
 * @brief 绘制首页标题区
 *
 * 标题区主要负责显示项目名称，目的是让屏幕一上电就能明确看到当前工程身份。
 */
static esp_err_t display_service_draw_header(void)
{
    esp_err_t ret = display_service_clear_region(
        DISPLAY_HEADER_X,
        DISPLAY_HEADER_Y,
        DISPLAY_HEADER_AREA_W,
        DISPLAY_HEADER_AREA_H);
    if (ret != ESP_OK) {
        return ret;
    }

    return bsp_lcd_draw_string_scaled(
        DISPLAY_HEADER_X,
        DISPLAY_HEADER_Y,
        "CODEX PROJECT",
        DISPLAY_COLOR_YELLOW,
        DISPLAY_COLOR_BLACK,
        DISPLAY_TEXT_SCALE_TITLE);
}

/**
 * @brief 绘制项目信息区
 *
 * 这一块固定显示版本号和当前阶段，后面也适合继续扩展成项目基本信息区。
 */
static esp_err_t display_service_draw_project_info(void)
{
    esp_err_t ret = display_service_clear_region(
        DISPLAY_PROJECT_INFO_X,
        DISPLAY_PROJECT_INFO_Y,
        DISPLAY_PROJECT_AREA_W,
        DISPLAY_PROJECT_AREA_H);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = display_service_draw_line(
        DISPLAY_PROJECT_INFO_X,
        DISPLAY_PROJECT_INFO_Y,
        DISPLAY_COLOR_WHITE,
        DISPLAY_TEXT_SCALE_SMALL,
        "VER",
        s_display.version);
    if (ret != ESP_OK) {
        return ret;
    }

    return display_service_draw_line(
        DISPLAY_PROJECT_INFO_X,
        DISPLAY_PROJECT_INFO_Y + DISPLAY_PROJECT_INFO_STEP,
        DISPLAY_COLOR_WHITE,
        DISPLAY_TEXT_SCALE_SMALL,
        "STAGE",
        s_display.stage);
}

/**
 * @brief 绘制 LED 状态区
 *
 * 三路 LED 状态统一放在同一个 panel 中，后面调试业务键时可以很直观看到模式变化。
 */
static esp_err_t display_service_draw_led_panel(void)
{
    esp_err_t ret = display_service_clear_region(
        DISPLAY_LED_PANEL_X,
        DISPLAY_LED_PANEL_Y,
        DISPLAY_LED_AREA_W,
        DISPLAY_LED_AREA_H);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = display_service_draw_line(
        DISPLAY_LED_PANEL_X,
        DISPLAY_LED_PANEL_Y,
        DISPLAY_COLOR_CYAN,
        DISPLAY_TEXT_SCALE_STATUS,
        display_service_led_to_string(LED_ID_SYS),
        display_service_mode_to_string(s_display.led_modes[LED_ID_SYS]));
    if (ret != ESP_OK) {
        return ret;
    }

    ret = display_service_draw_line(
        DISPLAY_LED_PANEL_X,
        DISPLAY_LED_PANEL_Y + DISPLAY_LED_PANEL_STEP,
        DISPLAY_COLOR_CYAN,
        DISPLAY_TEXT_SCALE_STATUS,
        display_service_led_to_string(LED_ID_NET),
        display_service_mode_to_string(s_display.led_modes[LED_ID_NET]));
    if (ret != ESP_OK) {
        return ret;
    }

    return display_service_draw_line(
        DISPLAY_LED_PANEL_X,
        DISPLAY_LED_PANEL_Y + DISPLAY_LED_PANEL_STEP * 2,
        DISPLAY_COLOR_CYAN,
        DISPLAY_TEXT_SCALE_STATUS,
        display_service_led_to_string(LED_ID_ERR),
        display_service_mode_to_string(s_display.led_modes[LED_ID_ERR]));
}

/**
 * @brief 绘制蜂鸣器状态区
 *
 * 这里集中显示蜂鸣器的两个关键状态：提示是否开启、测试模式是否开启。
 */
static esp_err_t display_service_draw_beep_panel(void)
{
    esp_err_t ret = display_service_clear_region(
        DISPLAY_BEEP_PANEL_X,
        DISPLAY_BEEP_PANEL_Y,
        DISPLAY_BEEP_AREA_W,
        DISPLAY_BEEP_AREA_H);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = display_service_draw_line(
        DISPLAY_BEEP_PANEL_X,
        DISPLAY_BEEP_PANEL_Y,
        DISPLAY_COLOR_GREEN,
        DISPLAY_TEXT_SCALE_STATUS,
        "BEEP",
        s_display.beep_enabled ? "ON" : "OFF");
    if (ret != ESP_OK) {
        return ret;
    }

    return display_service_draw_line(
        DISPLAY_BEEP_PANEL_X,
        DISPLAY_BEEP_PANEL_Y + DISPLAY_BEEP_PANEL_STEP,
        DISPLAY_COLOR_GREEN,
        DISPLAY_TEXT_SCALE_STATUS,
        "TEST",
        s_display.beep_test_mode ? "ON" : "OFF");
}

/**
 * @brief 绘制 Wi-Fi 状态区
 *
 * 从 v1.5.0 开始，LCD 首页正式承担联网状态输出窗口的角色。
 */
static esp_err_t display_service_draw_wifi_panel(void)
{
    char signal_line[32];

    esp_err_t ret = display_service_clear_region(
        DISPLAY_WIFI_PANEL_X,
        DISPLAY_WIFI_PANEL_Y,
        DISPLAY_WIFI_AREA_W,
        DISPLAY_WIFI_AREA_H);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = display_service_draw_line(
        DISPLAY_WIFI_PANEL_X,
        DISPLAY_WIFI_PANEL_Y,
        DISPLAY_COLOR_WHITE,
        DISPLAY_TEXT_SCALE_SMALL,
        "WIFI",
        display_service_wifi_state_to_string(s_display.wifi_state));
    if (ret != ESP_OK) {
        return ret;
    }

    ret = display_service_draw_line(
        DISPLAY_WIFI_PANEL_X,
        DISPLAY_WIFI_PANEL_Y + DISPLAY_WIFI_PANEL_STEP,
        DISPLAY_COLOR_WHITE,
        DISPLAY_TEXT_SCALE_SMALL,
        "IP",
        s_display.wifi_ip);
    if (ret != ESP_OK) {
        return ret;
    }

    if (s_display.wifi_rssi <= -120) {
        snprintf(signal_line, sizeof(signal_line), "--");
    } else if (s_display.wifi_channel > 0) {
        snprintf(signal_line, sizeof(signal_line), "%d dBm CH%u", s_display.wifi_rssi, s_display.wifi_channel);
    } else {
        snprintf(signal_line, sizeof(signal_line), "%d dBm", s_display.wifi_rssi);
    }

    return display_service_draw_line(
        DISPLAY_WIFI_PANEL_X,
        DISPLAY_WIFI_PANEL_Y + DISPLAY_WIFI_PANEL_STEP * 2,
        DISPLAY_COLOR_WHITE,
        DISPLAY_TEXT_SCALE_SMALL,
        "RSSI",
        signal_line);
}

/**
 * @brief 绘制 HTTP 状态区
 *
 * HTTP 区主要显示三类信息：
 * 1. 请求是否成功
 * 2. HTTP 状态码
 * 3. 从 JSON 提取出来的摘要消息
 */
static esp_err_t display_service_draw_http_panel(void)
{
    char code_text[16];
    const char *http_state_text = NULL;
    uint16_t http_state_color = DISPLAY_COLOR_WHITE;

    esp_err_t ret = display_service_clear_region(
        DISPLAY_HTTP_PANEL_X,
        DISPLAY_HTTP_PANEL_Y,
        DISPLAY_HTTP_AREA_W,
        DISPLAY_HTTP_AREA_H);
    if (ret != ESP_OK) {
        return ret;
    }

    // 请求发起后，HTTP 区先显示 REQUESTING。
    // 这里把它单独显示成 REQ，避免用户在真正拿到结果前先看到 FAIL。
    if (strcmp(s_display.http_message, "REQUESTING") == 0) {
        http_state_text = "REQ";
        http_state_color = DISPLAY_COLOR_YELLOW;
    } else if (s_display.http_success) {
        http_state_text = "OK";
        http_state_color = DISPLAY_COLOR_GREEN;
    } else {
        http_state_text = "FAIL";
        http_state_color = DISPLAY_COLOR_RED;
    }

    ret = display_service_draw_line(
        DISPLAY_HTTP_PANEL_X,
        DISPLAY_HTTP_PANEL_Y,
        http_state_color,
        DISPLAY_TEXT_SCALE_SMALL,
        "HTTP",
        http_state_text);
    if (ret != ESP_OK) {
        return ret;
    }

    snprintf(code_text, sizeof(code_text), "%d", s_display.http_status_code);
    ret = display_service_draw_line(
        DISPLAY_HTTP_PANEL_X,
        DISPLAY_HTTP_PANEL_Y + DISPLAY_HTTP_PANEL_STEP,
        DISPLAY_COLOR_WHITE,
        DISPLAY_TEXT_SCALE_SMALL,
        "CODE",
        code_text);
    if (ret != ESP_OK) {
        return ret;
    }

    return display_service_draw_line(
        DISPLAY_HTTP_PANEL_X,
        DISPLAY_HTTP_PANEL_Y + DISPLAY_HTTP_PANEL_STEP * 2,
        DISPLAY_COLOR_WHITE,
        DISPLAY_TEXT_SCALE_SMALL,
        "MSG",
        s_display.http_message);
}

/**
 * @brief 绘制 OTA 状态区
 *
 * 这里不展示完整升级流程细节，只展示最核心的：
 * - 当前 OTA 状态
 * - 当前说明消息
 *
 * 这样页面既能保留信息密度，又不至于太拥挤。
 */
static esp_err_t display_service_draw_ota_panel(void)
{
    esp_err_t ret = display_service_clear_region(
        DISPLAY_OTA_PANEL_X,
        DISPLAY_OTA_PANEL_Y,
        DISPLAY_OTA_AREA_W,
        DISPLAY_OTA_AREA_H);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = display_service_draw_line(
        DISPLAY_OTA_PANEL_X,
        DISPLAY_OTA_PANEL_Y,
        DISPLAY_COLOR_WHITE,
        DISPLAY_TEXT_SCALE_SMALL,
        "OTA",
        display_service_ota_state_to_string(s_display.ota_state));
    if (ret != ESP_OK) {
        return ret;
    }

    return display_service_draw_line(
        DISPLAY_OTA_PANEL_X,
        DISPLAY_OTA_PANEL_Y + DISPLAY_OTA_PANEL_STEP,
        DISPLAY_COLOR_WHITE,
        DISPLAY_TEXT_SCALE_SMALL,
        "MSG",
        s_display.ota_message);
}

/**
 * @brief 绘制最近一次按键事件区
 *
 * 这块信息对调试非常有帮助，因为它能直接告诉我们“系统最后一次识别到的事件是什么”。
 */
static esp_err_t display_service_draw_last_event_panel(void)
{
    char line[64];
    snprintf(line, sizeof(line), "%s / %s",
             display_service_button_to_string(s_display.last_button_id),
             display_service_event_to_string(s_display.last_button_event));

    esp_err_t ret = display_service_clear_region(
        DISPLAY_EVENT_PANEL_X,
        DISPLAY_EVENT_PANEL_Y,
        DISPLAY_EVENT_AREA_W,
        DISPLAY_EVENT_AREA_H);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = bsp_lcd_draw_string_scaled(
        DISPLAY_EVENT_PANEL_X,
        DISPLAY_EVENT_PANEL_Y,
        "LAST :",
        DISPLAY_COLOR_RED,
        DISPLAY_COLOR_BLACK,
        DISPLAY_TEXT_SCALE_SMALL);
    if (ret != ESP_OK) {
        return ret;
    }

    return bsp_lcd_draw_string_scaled(
        DISPLAY_EVENT_VALUE_X,
        DISPLAY_EVENT_PANEL_Y,
        line,
        DISPLAY_COLOR_RED,
        DISPLAY_COLOR_BLACK,
        DISPLAY_TEXT_SCALE_SMALL);
}

/**
 * @brief 初始化显示服务
 *
 * 这一层不关心屏幕底层怎么初始化，只要求 `bsp_lcd_init()` 成功后，
 * 把当前版本、阶段和默认状态写进显示缓存，然后触发首页绘制。
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

    // 初始化时先把项目基本信息和默认状态写进缓存，后面首页刷新直接按缓存内容绘制。
    snprintf(s_display.version, sizeof(s_display.version), "%s", APP_PROJECT_VERSION);
    snprintf(s_display.stage, sizeof(s_display.stage), "%s", APP_PROJECT_STAGE_NAME);
    s_display.beep_enabled = true;
    s_display.beep_test_mode = false;
    s_display.wifi_state = WIFI_STATE_IDLE;
    snprintf(s_display.wifi_ip, sizeof(s_display.wifi_ip), "%s", "0.0.0.0");
    s_display.wifi_rssi = -127;
    s_display.wifi_channel = 0;
    s_display.http_success = false;
    s_display.http_status_code = 0;
    snprintf(s_display.http_message, sizeof(s_display.http_message), "%s", "IDLE");
    s_display.ota_state = OTA_STATE_IDLE;
    snprintf(s_display.ota_message, sizeof(s_display.ota_message), "%s", "IDLE");
    display_service_mark_full_refresh();
    s_display.inited = true;

    ESP_LOGI(TAG, "display service ready");
    // 初始化完成后立即刷新首页，让用户一上电就能看到完整状态页。
    return display_service_refresh_home();
}

/**
 * @brief 全屏清空为黑色背景
 *
 * 这个接口只负责清屏，不修改显示缓存里的业务状态。
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
    s_display.project_info_dirty = true;
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
    s_display.project_info_dirty = true;
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
    s_display.event_panel_dirty = true;
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
    s_display.led_panel_dirty = true;
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
    s_display.beep_panel_dirty = true;
    return ESP_OK;
}

/**
 * @brief 更新 Wi-Fi 状态显示缓存
 */
esp_err_t display_service_show_wifi_status(wifi_state_t state)
{
    if (!s_display.inited) {
        return ESP_ERR_INVALID_STATE;
    }

    s_display.wifi_state = state;
    s_display.wifi_panel_dirty = true;
    return ESP_OK;
}

/**
 * @brief 更新 IP 地址显示缓存
 */
esp_err_t display_service_show_wifi_ip(const char *ip_string)
{
    if (!s_display.inited || ip_string == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    snprintf(s_display.wifi_ip, sizeof(s_display.wifi_ip), "%s", ip_string);
    s_display.wifi_panel_dirty = true;
    return ESP_OK;
}

/**
 * @brief 更新 Wi-Fi 信号强度和信道显示缓存
 */
esp_err_t display_service_show_wifi_signal(int rssi, uint8_t channel)
{
    if (!s_display.inited) {
        return ESP_ERR_INVALID_STATE;
    }

    s_display.wifi_rssi = rssi;
    s_display.wifi_channel = channel;
    s_display.wifi_panel_dirty = true;
    return ESP_OK;
}

/**
 * @brief 更新 HTTP 结果显示缓存
 *
 * 这里只更新缓存并打脏标记，真正绘制仍然在 display_service_process() 里统一完成。
 */
esp_err_t display_service_show_http_result(bool success, int status_code, const char *message)
{
    if (!s_display.inited || message == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    s_display.http_success = success;
    s_display.http_status_code = status_code;
    snprintf(s_display.http_message, sizeof(s_display.http_message), "%s", message);
    s_display.http_panel_dirty = true;
    return ESP_OK;
}

/**
 * @brief 更新 OTA 结果显示缓存
 *
 * OTA 服务变化后只需要调用这一层，不需要自己关心 LCD 区域坐标和刷新顺序。
 */
esp_err_t display_service_show_ota_status(ota_state_t state, const char *message)
{
    if (!s_display.inited || message == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    s_display.ota_state = state;
    snprintf(s_display.ota_message, sizeof(s_display.ota_message), "%s", message);
    s_display.ota_panel_dirty = true;
    return ESP_OK;
}

/**
 * @brief 按当前缓存内容重绘首页
 *
 * 当前版本仍然保留“整页重绘”作为主策略，但已经把页面拆成多个区域函数，
 * 后面如果要升级成局部刷新，就可以沿这些区域函数继续扩展。
 */
esp_err_t display_service_refresh_home(void)
{
    if (!s_display.inited) {
        return ESP_ERR_INVALID_STATE;
    }

    // 整页刷新主要用于初始化或页面需要整体重建时。
    // 这里先清全屏，再按固定区域顺序重画所有内容。
    esp_err_t ret = display_service_clear();
    if (ret != ESP_OK) {
        return ret;
    }
    // 更新头部信息区
    ret = display_service_draw_header();
    if (ret != ESP_OK) {
        return ret;
    }
    // 更新项目信息区
    ret = display_service_draw_project_info();
    if (ret != ESP_OK) {
        return ret;
    }
    // 更新 LED 状态区
    ret = display_service_draw_led_panel();
    if (ret != ESP_OK) {
        return ret;
    }
    // 更新蜂鸣器状态区
    ret = display_service_draw_beep_panel();
    if (ret != ESP_OK) {
        return ret;
    }
    // 更新网络状态区
    ret = display_service_draw_wifi_panel();
    if (ret != ESP_OK) {
        return ret;
    }
    // 更新 HTTP 状态区
    ret = display_service_draw_http_panel();
    if (ret != ESP_OK) {
        return ret;
    }
    // 更新 OTA 状态区
    ret = display_service_draw_ota_panel();
    if (ret != ESP_OK) {
        return ret;
    }
    // 更新最近一次按键事件区
    ret = display_service_draw_last_event_panel();
    if (ret != ESP_OK) {
        return ret;
    }

    s_display.full_refresh_pending = false;
    s_display.header_dirty = false;
    s_display.project_info_dirty = false;
    s_display.led_panel_dirty = false;
    s_display.beep_panel_dirty = false;
    s_display.http_panel_dirty = false;
    s_display.ota_panel_dirty = false;
    s_display.wifi_panel_dirty = false;
    s_display.event_panel_dirty = false;
    return ESP_OK;
}

/**
 * @brief 显示服务周期处理
 *
 * 当前版本优先使用“分区 dirty + 局部刷新”：
 * 1. 如果需要整页重建，就走 full refresh
 * 2. 否则只重画发生变化的区域
 */
void display_service_process(void)
{
    if (!s_display.inited) {
        return;
    }

    if (s_display.full_refresh_pending) {
        (void)display_service_refresh_home();
        return;
    }

    
    if (s_display.header_dirty) {
        if (display_service_draw_header() == ESP_OK) {
            s_display.header_dirty = false;
        }
    }

    if (s_display.project_info_dirty) {
        if (display_service_draw_project_info() == ESP_OK) {
            s_display.project_info_dirty = false;
        }
    }

    if (s_display.led_panel_dirty) {
        if (display_service_draw_led_panel() == ESP_OK) {
            s_display.led_panel_dirty = false;
        }
    }

    if (s_display.beep_panel_dirty) {
        if (display_service_draw_beep_panel() == ESP_OK) {
            s_display.beep_panel_dirty = false;
        }
    }

    if (s_display.wifi_panel_dirty) {
        if (display_service_draw_wifi_panel() == ESP_OK) {
            s_display.wifi_panel_dirty = false;
        }
    }

    if (s_display.http_panel_dirty) {
        if (display_service_draw_http_panel() == ESP_OK) {
            s_display.http_panel_dirty = false;
        }
    }

    if (s_display.ota_panel_dirty) {
        if (display_service_draw_ota_panel() == ESP_OK) {
            s_display.ota_panel_dirty = false;
        }
    }

    if (s_display.event_panel_dirty) {
        if (display_service_draw_last_event_panel() == ESP_OK) {
            s_display.event_panel_dirty = false;
        }
    }
}
