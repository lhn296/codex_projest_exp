#include "http_service.h"

#include <stdio.h>
#include <string.h>

#include "app_config.h"
#include "config_service.h"
#include "cJSON.h"
#include "display_service.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "wifi_service.h"

static const char *TAG = "HTTP_SVC";

typedef struct {
    bool inited;                    // HTTP 服务是否已完成初始化。
    bool request_in_progress;       // 当前是否有请求正在执行，避免重复发起。
    bool auto_request_done;         // 自动测试请求是否已经执行过一次。
    bool last_success;              // 最近一次请求是否成功。
    int status_code;                // 最近一次 HTTP 状态码。
    char message[64];               // 最近一次结果摘要，供 LCD 和日志复用。
    char response_body[512];        // 最近一次响应正文缓存。
    int64_t last_request_time_ms;   // 上次发起请求的时间戳。
} http_service_ctx_t;

static http_service_ctx_t s_http = {
    .inited = false,
    .request_in_progress = false,
    .auto_request_done = false,
    .last_success = false,
    .status_code = 0,
    .message = "IDLE",
    .response_body = {0},
    .last_request_time_ms = 0,
};

/**
 * @brief 把 HTTP 结果同步到显示服务
 *
 * HTTP 服务不直接关心 LCD 怎么画，它只负责把“最近一次请求结果”
 * 同步给显示缓存，真正的局部刷新仍然由 display_service 统一调度。
 */
static void http_service_sync_display(void)
{
    (void)display_service_show_http_result(s_http.last_success, s_http.status_code, s_http.message);
}

/**
 * @brief 统一更新 HTTP 结果缓存
 *
 * 这里统一收口最近一次请求的核心结果：
 * 1. 是否成功
 * 2. 状态码
 * 3. 给用户看的摘要文本
 */
static void http_service_set_result(bool success, int status_code, const char *message)
{
    s_http.last_success = success;
    s_http.status_code = status_code;
    snprintf(s_http.message, sizeof(s_http.message), "%s", message != NULL ? message : "NONE");
    http_service_sync_display();
}

/**
 * @brief 把常见 HTTP 状态码转成简短结果文本
 *
 * 当前主要用于“请求有响应但正文为空”这类场景下的 LCD 提示，
 * 方便先快速知道返回的大概类型。
 */
static const char *http_service_status_to_string(int status_code)
{
    switch (status_code) {
        case 200:
            return "HTTP_200";
        case 204:
            return "HTTP_204";
        case 301:
            return "HTTP_301";
        case 302:
            return "HTTP_302";
        case 400:
            return "HTTP_400";
        case 401:
            return "HTTP_401";
        case 403:
            return "HTTP_403";
        case 404:
            return "HTTP_404";
        case 500:
            return "HTTP_500";
        default:
            return "HTTP_STATUS";
    }
}

/**
 * @brief 从 JSON 文本中提取最基础的展示信息
 *
 * 当前先做轻量解析：
 * 1. 优先取顶层 message
 * 2. 如果没有，再取 slideshow.title
 * 3. 如果还没有，再取顶层 title
 * 4. 都没有就只保留 OK
 */
static void http_service_parse_json_summary(const char *json_text)
{
    // 先尝试把正文解析成 JSON 树，后面再从树里抽取关键摘要字段。
    cJSON *root = cJSON_Parse(json_text);
    if (root == NULL) {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL) {
            ESP_LOGW(TAG, "cJSON parse error near: %.48s", error_ptr);
        }
        ESP_LOGW(TAG, "json preview: %.160s", json_text);
        http_service_set_result(true, s_http.status_code, "JSON_PARSE_FAIL");
        return;
    }

    // 先看接口有没有直接返回 message，这对云端版本检查类接口最常见。
    const cJSON *message = cJSON_GetObjectItemCaseSensitive(root, "message");
    // 当前测试接口如果没有 message，再尝试从 slideshow.title 里取标题。
    const cJSON *title = cJSON_GetObjectItemCaseSensitive(root, "title");
    const cJSON *slideshow = cJSON_GetObjectItemCaseSensitive(root, "slideshow");
    const cJSON *slide_title = NULL;

    if (cJSON_IsObject(slideshow)) {
        slide_title = cJSON_GetObjectItemCaseSensitive(slideshow, "title");
    }

    if (cJSON_IsString(message) && message->valuestring != NULL) {
        http_service_set_result(true, s_http.status_code, message->valuestring);
    } else if (cJSON_IsString(slide_title) && slide_title->valuestring != NULL) {
        http_service_set_result(true, s_http.status_code, slide_title->valuestring);
    } else if (cJSON_IsString(title) && title->valuestring != NULL) {
        http_service_set_result(true, s_http.status_code, title->valuestring);
    } else {
        http_service_set_result(true, s_http.status_code, "OK");
    }

    cJSON_Delete(root);
}

/**
 * @brief 初始化 HTTP 服务
 *
 * 当前模板先把 HTTP 做成最基础的 GET 测试服务，后面再继续扩展：
 * - POST
 * - 自定义 header
 * - HTTPS
 * - 更完整的 JSON 解析
 */
esp_err_t http_service_init(void)
{
    if (s_http.inited) {
        return ESP_OK;
    }

    const app_runtime_config_t *cfg = config_service_get();
    const char *http_url = (cfg != NULL) ? cfg->http_test_url : APP_HTTP_TEST_URL;

    http_service_set_result(false, 0, "IDLE");
    s_http.inited = true;
    ESP_LOGI(TAG, "http service ready, url=%s timeout_ms=%d", http_url, APP_HTTP_TIMEOUT_MS);
    return ESP_OK;
}

/**
 * @brief 发起一次 HTTP GET 请求
 *
 * 当前函数分成这几步：
 * 1. 检查网络和请求状态
 * 2. 初始化 HTTP 客户端
 * 3. 显式 open + fetch_headers + read 正文
 * 4. 解析 JSON 摘要
 * 5. 更新显示和日志
 */
esp_err_t http_service_request_get(const char *url)
{
    if (!s_http.inited || url == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    // 请求发起前的检查项：
    // 1. 必须联网成功拿到 IP，才允许发请求。
    // 2. 如果上一个请求还没结束，也不允许发新请求，避免状态混乱。
    if (wifi_service_get_state() != WIFI_STATE_GOT_IP) {
        http_service_set_result(false, 0, "NO_WIFI");
        return ESP_ERR_INVALID_STATE;
    }

    // 当前请求还在进行中，不能发新请求。
    if (s_http.request_in_progress) {
        return ESP_ERR_INVALID_STATE;
    }

    s_http.request_in_progress = true;
    s_http.last_request_time_ms = esp_timer_get_time() / 1000; // 记录本次请求开始的时间戳，便于后续做超时分析或统计。
    http_service_set_result(false, 0, "REQUESTING"); // 请求发起后先把 LCD 状态切到 REQUESTING。

    // 配置 HTTP 客户端，当前只支持最基本的 URL 和超时设置，后续可以根据需要继续扩展。
    esp_http_client_config_t config = {
        .url = url,
        .timeout_ms = APP_HTTP_TIMEOUT_MS,
        .method = HTTP_METHOD_GET,
        // 某些云端地址会同时返回 IPv4 / IPv6，这里先强制走 IPv4，
        // 方便在当前 ESP32 网络环境下优先排除地址族导致的连接异常。
        .addr_type = HTTP_ADDR_TYPE_INET,
        // 云端版本接口后面大概率会走 HTTPS，这里直接挂系统证书包，
        // 这样像 Cloudflare Workers 这类 HTTPS 接口就能直接验证证书。
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

    // 先根据配置创建 HTTP 客户端对象，后面所有 open/read/close 都围绕这个句柄进行。
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        s_http.request_in_progress = false;
        http_service_set_result(false, 0, "CLIENT_INIT_FAIL");
        return ESP_FAIL;
    }


    // 这里改成显式 open + read 的方式，
    // 目的是确保响应正文由我们自己主动读取，而不是只拿到状态码。
    esp_err_t ret = esp_http_client_open(client, 0); // GET 请求没有请求体，所以发送长度填 0。
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_http_client_open failed, ret=0x%x", ret);
        esp_http_client_cleanup(client);
        s_http.request_in_progress = false;
        http_service_set_result(false, 0, "HTTP_FAIL");
        return ret;
    }

    // 先读响应头，拿到状态码和内容长度，再决定怎么读取正文。
    int content_length = esp_http_client_fetch_headers(client);
    s_http.status_code = esp_http_client_get_status_code(client);
    if (content_length < 0) {
        // 分块传输或长度未知时，仍然允许继续读，但上限不能超过本地缓冲区。
        content_length = (int)sizeof(s_http.response_body) - 1;
    }

    // 采用循环读取，避免只拿到部分正文导致 JSON 解析失败。
    int total_read_len = 0;
    while (total_read_len < (int)sizeof(s_http.response_body) - 1) {
        // 每次都从“当前已写入内容的后面”继续写，避免把前一轮读取到的正文覆盖掉。
        int once_read_len = esp_http_client_read(
            client,
            s_http.response_body + total_read_len,
            (sizeof(s_http.response_body) - 1) - total_read_len);
        if (once_read_len < 0) {
            // 读正文失败时立刻收尾，并把状态同步成 READ_FAIL。
            esp_http_client_close(client);
            esp_http_client_cleanup(client);
            s_http.request_in_progress = false;
            http_service_set_result(false, s_http.status_code, "READ_FAIL");
            return ESP_FAIL;
        }

        if (once_read_len == 0) {
            // 返回 0 表示当前已经没有更多正文可读，不是错误，正常结束循环。
            break;
        }

        // 累计已读取正文长度，下一轮继续从后面追加。
        total_read_len += once_read_len;
    }

    // 补上字符串结束符，后面日志打印和 cJSON 解析都依赖这一点。
    s_http.response_body[total_read_len] = '\0';
    ESP_LOGI(TAG, "http status=%d content_length=%d body_len=%d", s_http.status_code, content_length, total_read_len);

    if (total_read_len == 0) {
        // 这类情况说明请求本身有响应，但当前没有读到正文内容。
        // 先把状态码同步到显示层，方便继续排查是接口问题还是读取方式问题。
        http_service_set_result(false, s_http.status_code, http_service_status_to_string(s_http.status_code));
    } else {
        ESP_LOGI(TAG, "http body=%s", s_http.response_body);
        // 读到正文后再交给 JSON 摘要解析器，更新 LCD 和内部状态。
        http_service_parse_json_summary(s_http.response_body);
    }

    // 请求完成后统一收尾，避免句柄和连接泄漏。
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    s_http.request_in_progress = false;
    return ESP_OK;
}

/**
 * @brief HTTP 周期处理
 *
 * 当前版本先做成简单自动测试模式：
 * 设备联网成功后自动请求一次测试接口。
 */
void http_service_process(void)
{
    if (!s_http.inited || !APP_HTTP_AUTO_START) {
        return;
    }

    const app_runtime_config_t *cfg = config_service_get();
    const char *http_url = (cfg != NULL) ? cfg->http_test_url : APP_HTTP_TEST_URL;

    if (s_http.auto_request_done) {
        return;
    }

    // 自动请求的前提是已经拿到 IP，否则请求只会无意义失败。
    if (wifi_service_get_state() != WIFI_STATE_GOT_IP) {
        return;
    }

    if (http_url[0] == '\0') {
        http_service_set_result(false, 0, "URL_EMPTY");
        return;
    }

    if (http_service_request_get(http_url) == ESP_OK) {
        s_http.auto_request_done = true;
    }
    // 如果请求失败了，保持 auto_request_done 仍然为 false，
    // 这样后续周期还能继续自动尝试。
}

/**
 * @brief 判断 HTTP 服务是否已经初始化完成
 */
bool http_service_is_ready(void)
{
    return s_http.inited;
}

/**
 * @brief 获取最近一次 HTTP 请求是否成功
 */
bool http_service_is_success(void)
{
    return s_http.last_success;
}

/**
 * @brief 获取最近一次 HTTP 状态码
 */
int http_service_get_status_code(void)
{
    return s_http.status_code;
}

/**
 * @brief 获取最近一次 HTTP 结果摘要
 */
const char *http_service_get_message(void)
{
    return s_http.message;
}

/**
 * @brief 获取最近一次 HTTP 响应正文缓存
 *
 * 这个接口主要给 ota_service 这种“需要继续消费完整 JSON 正文”的模块使用。
 */
const char *http_service_get_response_body(void)
{
    return s_http.response_body;
}
