#include "ota_service.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "app_config.h"
#include "display_service.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "http_service.h"
#include "wifi_service.h"

static const char *TAG = "OTA_SVC";

typedef struct {
    bool inited;                      // OTA 服务是否已完成初始化。
    bool checked_once;                // 当前上电后是否已经做过一次自动检查。
    bool has_update;                  // 当前是否检测到可升级版本。
    ota_state_t state;                // 当前 OTA 高层状态。
    int64_t last_check_time_ms;       // 最近一次执行版本检查的时间戳。
    char target_version[24];          // 最近一次检测到的目标版本号。
    char firmware_url[160];           // 最近一次检测到的固件下载地址。
    char message[64];                 // 当前状态说明文本，供 LCD 和日志复用。
} ota_service_ctx_t;

static ota_service_ctx_t s_ota = {
    .inited = false,
    .checked_once = false,
    .has_update = false,
    .state = OTA_STATE_IDLE,
    .last_check_time_ms = 0,
    .target_version = {0},
    .firmware_url = {0},
    .message = "IDLE",
};

/**
 * @brief 把 OTA 状态转成更易读的日志文本
 */
static const char *ota_service_state_to_string(ota_state_t state);

/**
 * @brief 把 OTA 状态同步到显示服务
 */
static void ota_service_sync_display(void)
{
    (void)display_service_show_ota_status(s_ota.state, s_ota.message);
}

/**
 * @brief 统一更新 OTA 状态
 */
static void ota_service_set_state(ota_state_t state, const char *message)
{
    s_ota.state = state;
    snprintf(s_ota.message, sizeof(s_ota.message), "%s", message != NULL ? message : "NONE");
    ota_service_sync_display();
    ESP_LOGI(TAG, "ota state -> %s msg=%s", ota_service_state_to_string(state), s_ota.message);
}

/**
 * @brief 把 OTA 状态转成更易读的日志文本
 */
static const char *ota_service_state_to_string(ota_state_t state)
{
    switch (state) {
        case OTA_STATE_IDLE:
            return "IDLE";
        case OTA_STATE_CHECK:
            return "CHECK";
        case OTA_STATE_READY:
            return "READY";
        case OTA_STATE_DOWNLOADING:
            return "DOWNLOADING";
        case OTA_STATE_SUCCESS:
            return "SUCCESS";
        case OTA_STATE_FAIL:
        default:
            return "FAIL";
    }
}

/**
 * @brief 比较当前版本与目标版本
 *
 * 当前先采用最简单的字符串比较规则：
 * 只要目标版本与当前版本不同，就认为“存在新版本”。
 * 这样更适合当前学习阶段，后面再升级成更严格的语义版本比较。
 */
static bool ota_service_compare_version(const char *current_version, const char *target_version)
{
    if (current_version == NULL || target_version == NULL) {
        return false;
    }

    return strcmp(current_version, target_version) != 0;
}

/**
 * @brief 执行一次 OTA 版本检查
 *
 * 当前版本先做“可复用骨架”：
 * 1. 联网成功后进入检查状态
 * 2. 使用配置中的目标版本和固件地址模拟版本检查结果
 * 3. 如果发现新版本，则进入 READY
 * 4. 自动升级入口先保留，后续接真实服务器时再补下载执行
 */
static void ota_service_check_once(void)
{
    s_ota.last_check_time_ms = esp_timer_get_time() / 1000;
    ota_service_set_state(OTA_STATE_CHECK, "CHECK");

    snprintf(s_ota.target_version, sizeof(s_ota.target_version), "%s", APP_OTA_SERVER_VERSION);
    snprintf(s_ota.firmware_url, sizeof(s_ota.firmware_url), "%s", APP_OTA_FIRMWARE_URL);

    s_ota.has_update = ota_service_compare_version(APP_PROJECT_VERSION, s_ota.target_version);
    if (!s_ota.has_update) { // 如果没有检测到新版本，直接更新状态并返回，不进入 READY 状态了。
        ota_service_set_state(OTA_STATE_IDLE, "NO_UPDATE");
        ESP_LOGI(TAG, "ota no update, current=%s target=%s", APP_PROJECT_VERSION, s_ota.target_version);
        return;
    }

    ota_service_set_state(OTA_STATE_READY, s_ota.target_version);// 如果检测到新版本，进入 READY 状态，显示目标版本号。
    ESP_LOGI(TAG, "ota update ready, current=%s target=%s url=%s",
             APP_PROJECT_VERSION,
             s_ota.target_version,
             s_ota.firmware_url);

    if (APP_OTA_AUTO_UPGRADE) {
        // 当前版本先保留自动升级入口，后面接真实 OTA 下载链时继续扩展。
        ota_service_set_state(OTA_STATE_FAIL, "AUTO_DISABLED");
    }
}

/**
 * @brief 初始化 OTA 服务
 */
esp_err_t ota_service_init(void)
{
    if (s_ota.inited) {
        return ESP_OK;
    }

    snprintf(s_ota.target_version, sizeof(s_ota.target_version), "%s", APP_PROJECT_VERSION);
    snprintf(s_ota.firmware_url, sizeof(s_ota.firmware_url), "%s", "");
    ota_service_set_state(OTA_STATE_IDLE, "IDLE");
    s_ota.inited = true;
    ESP_LOGI(TAG, "ota service ready, auto_check=%d auto_upgrade=%d target=%s",
             APP_OTA_AUTO_CHECK,
             APP_OTA_AUTO_UPGRADE,
             APP_OTA_SERVER_VERSION);
    return ESP_OK;
}

/**
 * @brief OTA 周期处理
 */
void ota_service_process(void)
{
    
    if (!s_ota.inited || !APP_OTA_AUTO_CHECK) {
        return;
    }

    if (s_ota.checked_once) {
        return;
    }

    // OTA 检查至少要求网络已经可用，后面再接真实服务器时还会依赖 HTTP 版本接口。
    if (wifi_service_get_state() != WIFI_STATE_GOT_IP) {// 如果还没有联网成功，就先不检查了，等下次周期再检查。
        return;
    }

    // 当前版本先要求 HTTP 基础服务已经准备好，保持网络应用层的依赖关系清晰。
    if (!http_service_is_ready()) {
        return;
    }

    ota_service_check_once();// 执行一次版本检查，后续再接真实服务器时继续扩展检查内容和流程。
    s_ota.checked_once = true;
}

bool ota_service_is_ready(void)
{
    return s_ota.inited;
}

ota_state_t ota_service_get_state(void)
{
    return s_ota.state;
}

const char *ota_service_get_message(void)
{
    return s_ota.message;
}

bool ota_service_has_update(void)
{
    return s_ota.has_update;
}
