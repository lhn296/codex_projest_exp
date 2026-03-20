#include "ota_service.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "app_config.h"
#include "config_service.h"
#include "cJSON.h"
#include "display_service.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
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
    int last_http_status_code;        // 最近一次版本接口访问的 HTTP 状态码。
    size_t downloaded_bytes;          // 最近一次 OTA 下载写入的固件字节数。
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
    .last_http_status_code = 0,
    .downloaded_bytes = 0,
};

/**
 * @brief 把 OTA 状态转成更易读的日志文本
 */
static const char *ota_service_state_to_string(ota_state_t state);

/**
 * @brief 下载固件并写入 OTA 分区
 *
 * 这是 v1.9.0 的核心流程：
 * 1. 打开云端固件下载地址
 * 2. 获取下一个可更新分区
 * 3. 边下载边调用 esp_ota_write 写入分区
 * 4. 写完后结束 OTA、切换启动分区并重启
 */
static esp_err_t ota_service_download_and_apply(void);

/**
 * @brief 从云端版本 JSON 中解析 OTA 需要的字段
 *
 * 当前最关心三个字段：
 * 1. version：云端最新版本号
 * 2. url：固件下载地址
 * 3. message：给日志和 LCD 用的说明文字
 */
static esp_err_t ota_service_parse_cloud_payload(const char *json_text)
{
    if (json_text == NULL || json_text[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *root = cJSON_Parse(json_text);
    if (root == NULL) {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL) {
            ESP_LOGW(TAG, "ota json parse error near: %.48s", error_ptr);
        }
        return ESP_FAIL;
    }

    const cJSON *version = cJSON_GetObjectItemCaseSensitive(root, "version");
    const cJSON *url = cJSON_GetObjectItemCaseSensitive(root, "url");
    const cJSON *message = cJSON_GetObjectItemCaseSensitive(root, "message");

    if (!cJSON_IsString(version) || version->valuestring == NULL) {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_RESPONSE;
    }

    snprintf(s_ota.target_version, sizeof(s_ota.target_version), "%s", version->valuestring);
    snprintf(s_ota.firmware_url, sizeof(s_ota.firmware_url), "%s",
             (cJSON_IsString(url) && url->valuestring != NULL) ? url->valuestring : "");
    snprintf(s_ota.message, sizeof(s_ota.message), "%s",
             (cJSON_IsString(message) && message->valuestring != NULL) ? message->valuestring : version->valuestring);

    cJSON_Delete(root);
    return ESP_OK;
}

/**
 * @brief 把 OTA 状态同步到显示服务
 *
 * OTA 服务不直接负责页面绘制，它只维护升级状态并把结果同步给显示层。
 */
static void ota_service_sync_display(void)
{
    (void)display_service_show_ota_status(s_ota.state, s_ota.message);
}

/**
 * @brief 统一更新 OTA 状态
 *
 * 这里统一负责：
 * 1. 更新内部 OTA 状态
 * 2. 更新说明文本
 * 3. 同步日志和 LCD
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
 * @brief 比较当前版本与目标版本
 *
 * 当前采用最常见的主版本.次版本.补丁版本比较规则：
 * 例如：
 * - v1.9.1 < v2.0.0
 * - v2.0.0 > v1.9.1
 *
 * 只有目标版本真正比当前版本更高时，才认为“存在新版本”。
 */
static bool ota_service_compare_version(const char *current_version, const char *target_version)
{
    if (current_version == NULL || target_version == NULL) {
        return false;
    }

    int current_major = 0;
    int current_minor = 0;
    int current_patch = 0;
    int target_major = 0;
    int target_minor = 0;
    int target_patch = 0;

    int current_count = sscanf(current_version, "v%d.%d.%d", &current_major, &current_minor, &current_patch);
    if (current_count != 3) {
        current_count = sscanf(current_version, "%d.%d.%d", &current_major, &current_minor, &current_patch);
    }

    int target_count = sscanf(target_version, "v%d.%d.%d", &target_major, &target_minor, &target_patch);
    if (target_count != 3) {
        target_count = sscanf(target_version, "%d.%d.%d", &target_major, &target_minor, &target_patch);
    }

    // 如果版本字符串不符合预期格式，回退成保守策略：只在完全不相等时提示，但打日志提醒后续修正。
    if (current_count != 3 || target_count != 3) {
        ESP_LOGW(TAG, "version parse fallback, current=%s target=%s", current_version, target_version);
        return strcmp(current_version, target_version) != 0;
    }

    if (target_major != current_major) {
        return target_major > current_major;
    }

    if (target_minor != current_minor) {
        return target_minor > current_minor;
    }

    if (target_patch != current_patch) {
        return target_patch > current_patch;
    }

    return false;
}

/**
 * @brief 下载固件并写入 OTA 分区
 */
static esp_err_t ota_service_download_and_apply(void)
{
    if (s_ota.firmware_url[0] == '\0') {
        ota_service_set_state(OTA_STATE_FAIL, "URL_EMPTY");
        return ESP_ERR_INVALID_ARG;
    }

    // 真实 OTA 不能覆盖当前正在运行的分区，必须写到“下一个升级分区”。
    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);// 获取下一个可更新分区，后续的写入都针对这个分区进行。
    if (update_partition == NULL) {
        ota_service_set_state(OTA_STATE_FAIL, "NO_PARTITION");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG,
             "auto upgrade start, current=%s target=%s url=%s",
             APP_PROJECT_VERSION,
             s_ota.target_version,
             s_ota.firmware_url);
    ESP_LOGI(TAG,
             "target ota partition: label=%s address=0x%08x size=%u",
             update_partition->label,
             (unsigned)update_partition->address,
             (unsigned)update_partition->size);

    // 进入下载流程前先切到 DOWNLOADING 状态，给用户和日志明确的反馈。
    esp_http_client_config_t config = {
        .url = s_ota.firmware_url,
        .timeout_ms = APP_HTTP_TIMEOUT_MS,
        .method = HTTP_METHOD_GET,
        .addr_type = HTTP_ADDR_TYPE_INET,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);// 创建 HTTP 客户端对象，后续的 open/read/close 都围绕这个句柄进行。
    if (client == NULL) {
        ota_service_set_state(OTA_STATE_FAIL, "HTTP_INIT_FAIL");
        return ESP_FAIL;
    }

    esp_ota_handle_t ota_handle = 0;
    bool ota_begun = false;
    s_ota.downloaded_bytes = 0;
    ota_service_set_state(OTA_STATE_DOWNLOADING, "DOWNLOADING");

    // 打开固件下载连接，后面通过 read + write 方式边下载边写分区。
    esp_err_t ret = esp_http_client_open(client, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "firmware open failed, ret=0x%x", ret);
        ota_service_set_state(OTA_STATE_FAIL, "OPEN_FAIL");
        esp_http_client_cleanup(client);
        return ret;
    }

    ESP_LOGI(TAG, "firmware url opened");

    int content_length = esp_http_client_fetch_headers(client);
    int status_code = esp_http_client_get_status_code(client);
    if (status_code != 200) {
        ESP_LOGE(TAG, "firmware http status invalid, status=%d", status_code);
        ota_service_set_state(OTA_STATE_FAIL, "HTTP_STATUS");
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "firmware response ready, status=%d content_length=%d", status_code, content_length);

    // OTA 写入开始后，后面的每一块数据都写到 update_partition。
    ret = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &ota_handle);// 开始 OTA 升级，拿到 OTA 句柄后才能调用 esp_ota_write 进行写入。
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin failed, ret=0x%x", ret);
        ota_service_set_state(OTA_STATE_FAIL, "OTA_BEGIN");
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ret;
    }
    ota_begun = true;
    ESP_LOGI(TAG, "ota begin success");

    uint8_t buffer[APP_OTA_WRITE_BUFFER_SIZE];
    size_t next_progress_log_bytes = 32 * 1024;
    while (true) {
        // 每次从网络读取一小块固件，再立刻写入 OTA 分区。
        int once_read_len = esp_http_client_read(client, (char *)buffer, sizeof(buffer));
        if (once_read_len < 0) {
            ESP_LOGE(TAG, "firmware read failed");
            ret = ESP_FAIL;
            ota_service_set_state(OTA_STATE_FAIL, "READ_FAIL");
            break;
        }

        if (once_read_len == 0) {
            // 返回 0 说明固件数据已经读完，不是错误。
            ret = ESP_OK;
            break;
        }

        ret = esp_ota_write(ota_handle, buffer, once_read_len);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_write failed, ret=0x%x", ret);
            ota_service_set_state(OTA_STATE_FAIL, "WRITE_FAIL");
            break;
        }

        s_ota.downloaded_bytes += (size_t)once_read_len;
        if (s_ota.downloaded_bytes >= next_progress_log_bytes) {
            ESP_LOGI(TAG, "ota downloading... bytes=%u", (unsigned)s_ota.downloaded_bytes);
            next_progress_log_bytes += 32 * 1024;
        }
    }

    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    if (ret != ESP_OK) {
        if (ota_begun) {
            esp_ota_abort(ota_handle);
        }
        return ret;
    }

    ota_service_set_state(OTA_STATE_VERIFY, "VERIFY"); // 下载完成后先切到 VERIFY 状态，给用户和日志明确的反馈。
    ESP_LOGI(TAG, "firmware download finished, total_bytes=%u", (unsigned)s_ota.downloaded_bytes);

    ret = esp_ota_end(ota_handle);// 结束 OTA 升级，完成校验并释放 OTA 句柄，后续才能切换启动分区。
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_end failed, ret=0x%x", ret);
        ota_service_set_state(OTA_STATE_FAIL, "OTA_END");
        return ret;
    }
    ESP_LOGI(TAG, "ota end success");

    ret = esp_ota_set_boot_partition(update_partition);//   设置新的启动分区，后续重启后就会从这个分区启动。
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "set boot partition failed, ret=0x%x", ret);
        ota_service_set_state(OTA_STATE_FAIL, "SET_BOOT");
        return ret;
    }
    ESP_LOGI(TAG, "set boot partition success, next=%s", update_partition->label);

    ESP_LOGI(TAG,
             "ota download success, bytes=%u content_length=%d target_partition=%s",
             (unsigned)s_ota.downloaded_bytes,
             content_length,
             update_partition->label);

    ota_service_set_state(OTA_STATE_SUCCESS, "REBOOTING");// 升级成功后切到 SUCCESS 状态，并提示正在重启。

    // 留一点时间给串口和 LCD 完成最后一次状态输出。
    ESP_LOGI(TAG, "reboot to new firmware in 1 second");
    vTaskDelay(pdMS_TO_TICKS(1000));// 1 秒后重启，重启后新固件就会从 update_partition 启动。
    esp_restart();// 重启后新固件就会从 update_partition 启动。
    return ESP_OK;
}

/**
 * @brief 执行一次 OTA 版本检查
 *
 * 当前版本先做真实云端版本检查：
 * 1. 联网成功后进入检查状态
 * 2. 通过 HTTP 访问真实云端版本接口
 * 3. 解析 version / url / message
 * 4. 如果发现新版本，则进入 READY
 * 5. 如果开启自动升级，则继续执行真实 OTA 下载与切换主链
 */
static void ota_service_check_once(void)
{
    const app_runtime_config_t *cfg = config_service_get();
    const char *version_url = (cfg != NULL) ? cfg->ota_version_url : APP_OTA_VERSION_URL;

    // 先记录本次检查时间，并把状态切到 CHECK，便于日志和屏幕同步显示。
    s_ota.last_check_time_ms = esp_timer_get_time() / 1000;
    ota_service_set_state(OTA_STATE_CHECK, "CHECK");

    if (version_url[0] == '\0') {
        ota_service_set_state(OTA_STATE_FAIL, "URL_EMPTY");
        return;
    }

    // 先访问真实云端版本接口，后面再从响应正文中解析 OTA 元数据。
    esp_err_t ret = http_service_request_get(version_url);
    s_ota.last_http_status_code = http_service_get_status_code();
    if (ret != ESP_OK || !http_service_is_success()) {
        ota_service_set_state(OTA_STATE_FAIL, "HTTP_FAIL");
        ESP_LOGE(TAG, "ota version request failed, ret=0x%x status=%d",
                 ret,
                 s_ota.last_http_status_code);
        return;
    }

    // 云端版本接口返回的正文继续交给 OTA 层解析，这样 OTA 能直接消费 version/url/message。
    ret = ota_service_parse_cloud_payload(http_service_get_response_body());
    if (ret != ESP_OK) {
        ota_service_set_state(OTA_STATE_FAIL, "JSON_FAIL");
        ESP_LOGE(TAG, "ota cloud payload parse failed, ret=0x%x", ret);
        return;
    }

    // 当前阶段先采用最简单规则：目标版本和当前版本不同，就认为存在更新。
    s_ota.has_update = ota_service_compare_version(APP_PROJECT_VERSION, s_ota.target_version);
    if (!s_ota.has_update) {
        // 没有新版本时直接回到 IDLE / NO_UPDATE，不进入 READY。
        ota_service_set_state(OTA_STATE_IDLE, "NO_UPDATE");
        ESP_LOGI(TAG, "ota no update, current=%s target=%s", APP_PROJECT_VERSION, s_ota.target_version);
        return;
    }

    // 检测到新版本后进入 READY，优先显示云端 message，没有 message 时再回退版本号。
    ota_service_set_state(OTA_STATE_READY, s_ota.message[0] != '\0' ? s_ota.message : s_ota.target_version);
    ESP_LOGI(TAG, "ota update ready, current=%s target=%s url=%s message=%s",
             APP_PROJECT_VERSION,
             s_ota.target_version,
             s_ota.firmware_url,
             s_ota.message);

    if (APP_OTA_AUTO_UPGRADE) {
        // 开启自动升级后，直接进入真实下载与切换链。
        (void)ota_service_download_and_apply();
    }
}

/**
 * @brief 初始化 OTA 服务
 *
 * 当前先做“真实 OTA 升级基础版”：
 * - 初始化状态缓存
 * - 同步默认显示状态
 * - 为真实 OTA 下载链保留统一入口
 */
esp_err_t ota_service_init(void)
{
    if (s_ota.inited) {
        return ESP_OK;
    }

    const app_runtime_config_t *cfg = config_service_get();
    const char *version_url = (cfg != NULL) ? cfg->ota_version_url : APP_OTA_VERSION_URL;

    // 检查前默认把目标版本设成当前版本，避免未初始化时显示脏值。
    snprintf(s_ota.target_version, sizeof(s_ota.target_version), "%s", APP_PROJECT_VERSION);
    snprintf(s_ota.firmware_url, sizeof(s_ota.firmware_url), "%s", "");
    s_ota.downloaded_bytes = 0;
    ota_service_set_state(OTA_STATE_IDLE, "IDLE");
    s_ota.inited = true;
    ESP_LOGI(TAG, "ota service ready, auto_check=%d auto_upgrade=%d version_url=%s",
             APP_OTA_AUTO_CHECK,
             APP_OTA_AUTO_UPGRADE,
             version_url);
    return ESP_OK;
}

/**
 * @brief OTA 周期处理
 *
 * 当前版本先做一次性自动检查：
 * 1. 等 Wi-Fi 拿到 IP
 * 2. 等 HTTP 服务准备好
 * 3. 执行一次版本检查
 */
void ota_service_process(void)
{
    if (!s_ota.inited || !APP_OTA_AUTO_CHECK) {
        return;
    }

    if (s_ota.checked_once) {
        return;
    }

    // OTA 检查至少要求网络已经可用。
    if (wifi_service_get_state() != WIFI_STATE_GOT_IP) {
        return;
    }

    // 当前版本先要求 HTTP 基础服务已经准备好，保持网络应用层的依赖关系清晰。
    if (!http_service_is_ready()) {
        return;
    }

    // 到这里说明联网和 HTTP 基础都准备好了，可以执行一次 OTA 检查。
    ota_service_check_once();
    s_ota.checked_once = true;
}

/**
 * @brief 判断 OTA 服务是否已初始化
 */
bool ota_service_is_ready(void)
{
    return s_ota.inited;
}

/**
 * @brief 获取当前 OTA 状态
 */
ota_state_t ota_service_get_state(void)
{
    return s_ota.state;
}

/**
 * @brief 获取当前 OTA 状态说明文本
 */
const char *ota_service_get_message(void)
{
    return s_ota.message;
}

/**
 * @brief 判断当前是否检测到新版本
 */
bool ota_service_has_update(void)
{
    return s_ota.has_update;
}
