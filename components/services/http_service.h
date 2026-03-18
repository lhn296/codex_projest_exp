#ifndef HTTP_SERVICE_H
#define HTTP_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// 初始化 HTTP 服务，准备结果缓存和默认状态。
esp_err_t http_service_init(void);

// 发起一次 HTTP GET 请求，并解析响应中的基础 JSON 字段。
esp_err_t http_service_request_get(const char *url);

// HTTP 周期处理入口，当前用于自动请求和后续扩展。
void http_service_process(void);

// 判断 HTTP 服务是否已完成初始化。
bool http_service_is_ready(void);

// 获取最近一次 HTTP 是否成功。
bool http_service_is_success(void);

// 获取最近一次 HTTP 响应状态码。
int http_service_get_status_code(void);

// 获取最近一次 HTTP 结果摘要文本。
const char *http_service_get_message(void);

#ifdef __cplusplus
}
#endif

#endif
