#ifndef CONFIG_SERVICE_H
#define CONFIG_SERVICE_H

#include <stdbool.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char wifi_ssid[33];         // 当前运行时使用的 Wi-Fi SSID。
    char wifi_password[65];     // 当前运行时使用的 Wi-Fi 密码。
    char http_test_url[160];    // 当前 HTTP 测试接口地址。
    char ota_version_url[160];  // 当前 OTA 版本接口地址。
} app_runtime_config_t;

typedef enum {
    CONFIG_VALUE_SOURCE_DEFAULT = 0, // 当前值来自默认配置。
    CONFIG_VALUE_SOURCE_NVS,         // 当前值来自 NVS 保存值。
    CONFIG_VALUE_SOURCE_RUNTIME,     // 当前值已在运行时被修改，但还未重新加载。
} config_value_source_t;

// 初始化配置服务，准备 NVS 和运行时配置缓存。
esp_err_t config_service_init(void);

// 加载当前配置，优先使用 NVS 中的保存值，没有时回退默认值。
esp_err_t config_service_load(void);

// 把当前运行时配置保存到 NVS。
esp_err_t config_service_save(void);

// 恢复默认配置，并立即保存到 NVS。
esp_err_t config_service_reset_to_default(void);

// 获取当前运行时配置，只读使用。
const app_runtime_config_t *config_service_get(void);

// 获取 Wi-Fi 配置当前来源。
config_value_source_t config_service_get_wifi_source(void);

// 获取 HTTP URL 当前来源。
config_value_source_t config_service_get_http_source(void);

// 获取 OTA URL 当前来源。
config_value_source_t config_service_get_ota_source(void);

// 把来源枚举转成简短文本。
const char *config_service_source_to_string(config_value_source_t source);

// 获取整体配置来源摘要：DEFAULT / NVS / RUNTIME / MIXED。
const char *config_service_get_source_summary(void);

// 更新 Wi-Fi 配置到运行时缓存，保存需额外调用 config_service_save。
esp_err_t config_service_set_wifi(const char *ssid, const char *password);

// 更新 URL 配置到运行时缓存，保存需额外调用 config_service_save。
esp_err_t config_service_set_urls(const char *http_url, const char *ota_url);

// 判断配置服务是否已完成初始化和加载。
bool config_service_is_ready(void);

// 执行一次配置服务自检：保存、重载、比对并恢复原值。
esp_err_t config_service_self_test(void);

#ifdef __cplusplus
}
#endif

#endif
