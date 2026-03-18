#include "http_service.h"

#include <stdio.h>
#include <string.h>

#include "app_config.h"
#include "cJSON.h"
#include "display_service.h"
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
 */
static void http_service_sync_display(void)
{
    (void)display_service_show_http_result(s_http.last_success, s_http.status_code, s_http.message);
}

/**
 * @brief 统一更新 HTTP 结果缓存
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
 * 1. 优先取 slideshow.title
 * 2. 如果没有，再取顶层 title
 * 3. 都没有就只保留 OK
 */
static void http_service_parse_json_summary(const char *json_text)
{
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

    const cJSON *title = cJSON_GetObjectItemCaseSensitive(root, "title");
    const cJSON *slideshow = cJSON_GetObjectItemCaseSensitive(root, "slideshow");
    const cJSON *slide_title = NULL;

    if (cJSON_IsObject(slideshow)) {
        slide_title = cJSON_GetObjectItemCaseSensitive(slideshow, "title");
    }

    if (cJSON_IsString(slide_title) && slide_title->valuestring != NULL) {
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
 */
esp_err_t http_service_init(void)
{
    if (s_http.inited) {
        return ESP_OK;
    }

    http_service_set_result(false, 0, "IDLE");
    s_http.inited = true;
    ESP_LOGI(TAG, "http service ready, url=%s timeout_ms=%d", APP_HTTP_TEST_URL, APP_HTTP_TIMEOUT_MS);
    return ESP_OK;
}

/**
 * @brief 发起一次 HTTP GET 请求
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
    s_http.last_request_time_ms = esp_timer_get_time() / 1000;// 获取当前时间戳，单位 ms
    http_service_set_result(false, 0, "REQUESTING");// 更新状态为请求中，供 LCD 显示。

    // 配置 HTTP 客户端，当前只支持最基本的 URL 和超时设置，后续可以根据需要继续扩展。
    esp_http_client_config_t config = {
        .url = url,
        .timeout_ms = APP_HTTP_TIMEOUT_MS,
        .method = HTTP_METHOD_GET,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);// 初始化 HTTP 客户端
    if (client == NULL) {
        s_http.request_in_progress = false;
        http_service_set_result(false, 0, "CLIENT_INIT_FAIL");
        return ESP_FAIL;
    }

    // 这里改成显式 open + read 的方式，
    // 目的是确保响应正文由我们自己主动读取，而不是只拿到状态码。
    esp_err_t ret = esp_http_client_open(client, 0);// 打开 HTTP 连接，GET 请求不需要发送正文长度。
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_http_client_open failed, ret=0x%x", ret);
        esp_http_client_cleanup(client);
        s_http.request_in_progress = false;
        http_service_set_result(false, 0, "HTTP_FAIL");
        return ret;
    }

    // 先读取响应头，拿到状态码和内容长度，再决定怎么读正文。
    int content_length = esp_http_client_fetch_headers(client);// 获取 HTTP 响应头部，返回内容长度，如果有的话。
    s_http.status_code = esp_http_client_get_status_code(client); // 获取 HTTP 响应状态码
    if (content_length < 0) {
        // 分块传输或长度未知时，仍然允许继续读到本地缓冲区上限。
        content_length = (int)sizeof(s_http.response_body) - 1;
    }

    // 采用循环读取，避免只拿到部分正文导致 JSON 解析失败。
    int total_read_len = 0;
    while (total_read_len < (int)sizeof(s_http.response_body) - 1) {
        int once_read_len = esp_http_client_read(
            client,
            s_http.response_body + total_read_len,
            (sizeof(s_http.response_body) - 1) - total_read_len);
        if (once_read_len < 0) {
            esp_http_client_close(client);// 关闭当前 HTTP 会话
            esp_http_client_cleanup(client);// 清理 HTTP 客户端资源
            s_http.request_in_progress = false;
            http_service_set_result(false, s_http.status_code, "READ_FAIL");// 更新状态为读取失败，供 LCD 显示。
            return ESP_FAIL;
        }

        if (once_read_len == 0) {
            break;
        }

        total_read_len += once_read_len;
    }

    s_http.response_body[total_read_len] = '\0'; // 确保响应正文以 null 结尾，方便后续处理和显示。
    ESP_LOGI(TAG, "http status=%d content_length=%d body_len=%d", s_http.status_code, content_length, total_read_len);

    if (total_read_len == 0) {
        // 这类情况说明请求本身有响应，但当前没有读到正文内容。
        // 先把状态码同步到显示层，方便继续排查是接口问题还是读取方式问题。
        http_service_set_result(false, s_http.status_code, http_service_status_to_string(s_http.status_code));
    } else {
        ESP_LOGI(TAG, "http body=%s", s_http.response_body);
        http_service_parse_json_summary(s_http.response_body);// 从响应正文中解析出摘要信息，更新显示状态。
    }

    esp_http_client_close(client);// 关闭当前 HTTP 会话
    esp_http_client_cleanup(client);// 清理 HTTP 客户端资源
    s_http.request_in_progress = false;// 请求完成，更新状态为非进行中。
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
    }// 如果已经自动请求过了，就不再重复请求了。

    if (s_http.auto_request_done) {
        return;
    }// 自动请求的前提是必须联网成功拿到 IP，避免无谓的请求失败和状态更新。

    if (wifi_service_get_state() != WIFI_STATE_GOT_IP) {//  如果还没有联网成功，就先不自动请求了，等下次周期再检查。
        return;
    }//

    if (http_service_request_get(APP_HTTP_TEST_URL) == ESP_OK) {
        s_http.auto_request_done = true;
    }// 如果请求失败了，保持 auto_request_done 还是 false，这样下个周期还会继续尝试，直到成功为止。
}

bool http_service_is_ready(void)
{
    return s_http.inited;
}

bool http_service_is_success(void)
{
    return s_http.last_success;
}

int http_service_get_status_code(void)
{
    return s_http.status_code;
}

const char *http_service_get_message(void)
{
    return s_http.message;
}
