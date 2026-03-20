#include "menu_service.h"

#include <stdio.h>
#include <string.h>

#include "app_config.h"
#include "config_service.h"
#include "display_service.h"
#include "esp_log.h"
#include "esp_system.h"
#include "wifi_service.h"

static const char *TAG = "MENU_SVC";

typedef enum {
    MENU_PAGE_HOME = 0,   // 菜单首页，显示版本/配置来源/Wi-Fi 状态。
    MENU_PAGE_CONFIG,     // 配置摘要页，显示 SSID / HTTP / OTA 关键配置。
    MENU_PAGE_ACTION,     // 动作页，执行 RESET / RELOAD / REBOOT。
    MENU_PAGE_MAX,
} menu_page_t;

typedef struct {
    bool inited;          // 菜单服务是否已初始化。
    bool visible;         // 当前菜单是否显示中。
    menu_page_t page;     // 当前菜单页。
    uint8_t action_index; // ACTION 页当前选中的动作索引。
} menu_service_ctx_t;

static menu_service_ctx_t s_menu = {
    .inited = false,
    .visible = false,
    .page = MENU_PAGE_HOME,
    .action_index = 0,
};

/**
 * @brief 把 Wi-Fi 状态转成菜单页里更紧凑的文本
 *
 * 菜单页的可显示空间比首页更有限，所以这里使用更短的状态词。
 */
static const char *menu_service_wifi_state_to_string(wifi_state_t state)
{
    switch (state) {
        case WIFI_STATE_IDLE:
            return "IDLE";
        case WIFI_STATE_CONNECTING:
            return "CONNECT";
        case WIFI_STATE_CONNECTED:
            return "LINKED";
        case WIFI_STATE_GOT_IP:
            return "GOT_IP";
        case WIFI_STATE_DISCONNECTED:
            return "DISC";
        case WIFI_STATE_ERROR:
        default:
            return "ERROR";
    }
}

/**
 * @brief 把长字符串截成适合菜单显示的短文本
 *
 * HTTP / OTA URL 往往比较长，菜单页只需要看“当前值大概是不是对的”，
 * 所以这里保留前半段并追加 `...`，避免菜单页排版被长 URL 撑坏。
 */
static void menu_service_copy_short_text(char *dst, size_t dst_size, const char *src, size_t keep_chars)
{
    if (dst == NULL || dst_size == 0) {
        return;
    }

    if (src == NULL) {
        dst[0] = '\0';
        return;
    }

    size_t src_len = strlen(src);
    if (src_len <= keep_chars) {
        (void)snprintf(dst, dst_size, "%s", src);
        return;
    }

    (void)snprintf(dst, dst_size, "%.*s...", (int)keep_chars, src);
}

/**
 * @brief 把当前配置来源摘要同步回首页显示缓存
 *
 * 菜单中的 RESET / RELOAD 等动作可能改变配置来源，
 * 所以动作执行后要顺手更新首页中的 `CFG : ...` 显示内容。
 */
static void menu_service_sync_home_summary(void)
{
    (void)display_service_show_config_source(config_service_get_source_summary());
}

/**
 * @brief 按当前菜单页状态重新组织显示内容
 *
 * 这个函数负责把菜单状态机里的“当前页 + 当前选中项”
 * 转成 `display_service_show_menu_page()` 能直接绘制的三行文本。
 */
static esp_err_t menu_service_render(void)
{
    char line1[64];
    char line2[64];
    char line3[64];
    char short_http[32];
    char short_ota[32];
    const app_runtime_config_t *cfg = config_service_get();
    int selected_index = -1;

    if (cfg == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    switch (s_menu.page) {
        case MENU_PAGE_HOME:
            (void)snprintf(line1, sizeof(line1), "VER %s", APP_PROJECT_VERSION);
            (void)snprintf(line2, sizeof(line2), "CFG %s", config_service_get_source_summary());
            (void)snprintf(line3, sizeof(line3), "WIFI %s", menu_service_wifi_state_to_string(wifi_service_get_state()));
            return display_service_show_menu_page("MENU HOME", line1, line2, line3, selected_index);

        case MENU_PAGE_CONFIG:
            menu_service_copy_short_text(short_http, sizeof(short_http), cfg->http_test_url, 22);
            menu_service_copy_short_text(short_ota, sizeof(short_ota), cfg->ota_version_url, 22);
            (void)snprintf(line1, sizeof(line1), "SSID %s", cfg->wifi_ssid);
            (void)snprintf(line2, sizeof(line2), "HTTP %s", short_http);
            (void)snprintf(line3, sizeof(line3), "OTA %s", short_ota);
            return display_service_show_menu_page("MENU CONFIG", line1, line2, line3, selected_index);

        case MENU_PAGE_ACTION:
        default:
            selected_index = (int)s_menu.action_index;
            (void)snprintf(line1, sizeof(line1), "RESET_CFG");
            (void)snprintf(line2, sizeof(line2), "RELOAD_CFG");
            (void)snprintf(line3, sizeof(line3), "REBOOT");
            return display_service_show_menu_page("MENU ACTION", line1, line2, line3, selected_index);
    }
}

/**
 * @brief 进入菜单
 *
 * 进入菜单时总是回到 HOME 页，并重置动作页选中项，
 * 这样每次用户从首页进入菜单时都有一个稳定起点。
 */
static void menu_service_enter(void)
{
    s_menu.visible = true;
    s_menu.page = MENU_PAGE_HOME;
    s_menu.action_index = 0;
    (void)menu_service_render();
    ESP_LOGI(TAG, "menu entered");
}

/**
 * @brief 退出菜单并恢复首页显示
 */
static void menu_service_exit(void)
{
    s_menu.visible = false;
    (void)display_service_hide_menu();
    ESP_LOGI(TAG, "menu exited");
}

/**
 * @brief 切到上一页
 *
 * 三个页面构成一个简单循环：
 * HOME <- CONFIG <- ACTION <- HOME
 */
static void menu_service_prev_page(void)
{
    if (s_menu.page == MENU_PAGE_HOME) {
        s_menu.page = MENU_PAGE_ACTION;
    } else {
        s_menu.page = (menu_page_t)(s_menu.page - 1);
    }
    (void)menu_service_render();
}

/**
 * @brief 切到下一页
 */
static void menu_service_next_page(void)
{
    s_menu.page = (menu_page_t)((s_menu.page + 1) % MENU_PAGE_MAX);
    (void)menu_service_render();
}

/**
 * @brief 执行动作页当前选中的动作
 *
 * 当前动作页只保留三个非常基础、很适合做模板的动作：
 * - RESET_CFG：恢复默认值并重新加载
 * - RELOAD_CFG：从 NVS 重新加载
 * - REBOOT：软件重启
 */
static void menu_service_execute_action(void)
{
    esp_err_t ret = ESP_OK;

    if (s_menu.action_index == 0) {
        ret = config_service_reset_to_default();
        if (ret == ESP_OK) {
            ret = config_service_load();
        }
        menu_service_sync_home_summary();
        ESP_LOGI(TAG, "menu action RESET_CFG ret=0x%x", ret);
        (void)menu_service_render();
        return;
    }

    if (s_menu.action_index == 1) {
        ret = config_service_load();
        menu_service_sync_home_summary();
        ESP_LOGI(TAG, "menu action RELOAD_CFG ret=0x%x", ret);
        (void)menu_service_render();
        return;
    }

    ESP_LOGI(TAG, "menu action REBOOT");
    esp_restart();
}

/**
 * @brief 初始化菜单服务
 *
 * 当前初始化只做状态机默认值准备，不依赖额外硬件资源。
 */
esp_err_t menu_service_init(void)
{
    if (s_menu.inited) {
        return ESP_OK;
    }

    s_menu.visible = false;
    s_menu.page = MENU_PAGE_HOME;
    s_menu.action_index = 0;
    s_menu.inited = true;
    ESP_LOGI(TAG, "menu service ready");
    return ESP_OK;
}

/**
 * @brief 菜单周期处理入口
 *
 * 当前 v2.2.0 第一版菜单没有异步动画、闪烁光标或超时退出需求，
 * 所以这里暂时留空，后面如果要扩展再往这里加周期逻辑。
 */
void menu_service_process(void)
{
}

/**
 * @brief 判断菜单当前是否可见
 */
bool menu_service_is_visible(void)
{
    return s_menu.visible;
}

/**
 * @brief 处理按键事件；若菜单消费了该事件则返回 true
 *
 * 这是菜单状态机的核心入口：
 * 1. 菜单隐藏时，只关心 `KEY3 长按进入菜单`
 * 2. 菜单显示时，由菜单优先接管按键，避免原来的 LED / BEEP 逻辑继续生效
 * 3. 普通页和 ACTION 页的导航规则不同：
 *    - HOME / CONFIG：KEY0/KEY1 切页
 *    - ACTION：KEY0/KEY1 切动作项
 */
bool menu_service_handle_button_event(button_id_t button_id, button_event_t button_event)
{
    if (!s_menu.inited) {
        return false;
    }

    if (!s_menu.visible) {
        // 菜单未显示时，只有功能键长按才作为“进入菜单”入口。
        if (button_id == BTN_FUNC && button_event == BUTTON_EVENT_LONG) {
            menu_service_enter();
            return true;
        }
        return false;
    }

    // 菜单显示中时，功能键短按充当“返回/退出”。
    if (button_id == BTN_FUNC && button_event == BUTTON_EVENT_SHORT) {
        if (s_menu.page == MENU_PAGE_HOME) {
            menu_service_exit();
        } else {
            s_menu.page = MENU_PAGE_HOME;
            (void)menu_service_render();// 切回 HOME 页时顺便刷新显示，避免用户在 CONFIG 页修改了配置但没看到更新。
        }
        return true;
    }

    // 菜单显示期间，其余按键事件默认由菜单接管，避免继续触发业务灯效或蜂鸣器逻辑。
    if (button_event != BUTTON_EVENT_SHORT) {// 目前只处理短按，长按和双击先不做特殊区分。
        return true;
    }

    switch (button_id) {
        case BTN_SYS:
            // ACTION 页里，KEY0 表示“上一项”；其他页里表示“上一页”。
            if (s_menu.page == MENU_PAGE_ACTION) {// ACTION 页的上一项和下一项切换规则稍微特殊一点，避免用户在 HOME/CONFIG 页切来切去时不小心把 ACTION 页选成了 REBOOT。    
                s_menu.action_index = (uint8_t)((s_menu.action_index + 2) % 3);
                (void)menu_service_render();
            } else {
                menu_service_prev_page();// HOME/CONFIG 页的上一页切换规则比较简单，直接循环就好。
            }
            return true;

        case BTN_NET:
            // ACTION 页里，KEY1 表示“下一项”；其他页里表示“下一页”。
            if (s_menu.page == MENU_PAGE_ACTION) {
                s_menu.action_index = (uint8_t)((s_menu.action_index + 1) % 3);
                (void)menu_service_render();
            } else {
                menu_service_next_page();
            }
            return true;

        case BTN_ERR:
            // KEY2 统一作为确认键：
            // - HOME -> CONFIG
            // - CONFIG -> ACTION
            // - ACTION -> 执行动作
            if (s_menu.page == MENU_PAGE_HOME) {
                s_menu.page = MENU_PAGE_CONFIG;
                (void)menu_service_render();
            } else if (s_menu.page == MENU_PAGE_CONFIG) {
                s_menu.page = MENU_PAGE_ACTION;
                (void)menu_service_render();
            } else {
                menu_service_execute_action();
            }
            return true;

        case BTN_FUNC:
            return true;

        default:
            return false;
    }
}
