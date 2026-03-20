#include "config_service.h"

#include <stdio.h>
#include <string.h>

#include "app_config.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "CFG_SVC";
static const char *APP_CFG_NAMESPACE = "app_cfg";

typedef struct {
    bool inited;                  // 配置服务是否已初始化。
    bool loaded;                  // 当前配置是否已从默认值或 NVS 加载完成。
    app_runtime_config_t runtime; // 当前生效的运行时配置。
    config_value_source_t wifi_source; // 当前 Wi-Fi 参数来源。
    config_value_source_t http_source; // 当前 HTTP URL 来源。
    config_value_source_t ota_source;  // 当前 OTA URL 来源。
} config_service_ctx_t;

static config_service_ctx_t s_config = {
    .inited = false,
    .loaded = false,
    .runtime = {{0}},
    .wifi_source = CONFIG_VALUE_SOURCE_DEFAULT,
    .http_source = CONFIG_VALUE_SOURCE_DEFAULT,
    .ota_source = CONFIG_VALUE_SOURCE_DEFAULT,
};

static void config_service_copy_string(char *dst, size_t dst_size, const char *src)
{
    if (dst == NULL || dst_size == 0) {
        return;
    }

    (void)snprintf(dst, dst_size, "%s", src != NULL ? src : "");
}

static bool config_service_is_ipv4_host(const char *host)
{
    if (host == NULL || host[0] == '\0') {
        return false;
    }

    for (size_t i = 0; host[i] != '\0'; ++i) {
        if (!((host[i] >= '0' && host[i] <= '9') || host[i] == '.')) {
            return false;
        }
    }

    return strchr(host, '.') != NULL;
}

static bool config_service_url_is_basic_valid(const char *url)
{
    if (url == NULL || url[0] == '\0') {
        return false;
    }

    const char *host = NULL;
    if (strncmp(url, "http://", 7) == 0) {
        host = url + 7;
    } else if (strncmp(url, "https://", 8) == 0) {
        host = url + 8;
    } else {
        return false;
    }

    if (host[0] == '\0' || host[0] == '/' || host[0] == ':') {
        return false;
    }

    size_t host_len = strcspn(host, "/:");
    if (host_len == 0 || host_len >= 80) {
        return false;
    }

    char host_buf[80];
    memcpy(host_buf, host, host_len);
    host_buf[host_len] = '\0';

    if (strcmp(host_buf, "localhost") == 0) {
        return true;
    }

    if (strchr(host_buf, '.') != NULL) {
        return true;
    }

    return config_service_is_ipv4_host(host_buf);
}

static void config_service_load_default(app_runtime_config_t *cfg)
{
    if (cfg == NULL) {
        return;
    }

    config_service_copy_string(cfg->wifi_ssid, sizeof(cfg->wifi_ssid), APP_WIFI_STA_SSID);
    config_service_copy_string(cfg->wifi_password, sizeof(cfg->wifi_password), APP_WIFI_STA_PASSWORD);
    config_service_copy_string(cfg->http_test_url, sizeof(cfg->http_test_url), APP_HTTP_TEST_URL);
    config_service_copy_string(cfg->ota_version_url, sizeof(cfg->ota_version_url), APP_OTA_VERSION_URL);
}

static void config_service_mark_default_sources(void)
{
    s_config.wifi_source = CONFIG_VALUE_SOURCE_DEFAULT;
    s_config.http_source = CONFIG_VALUE_SOURCE_DEFAULT;
    s_config.ota_source = CONFIG_VALUE_SOURCE_DEFAULT;
}

static esp_err_t config_service_init_nvs(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_OK || ret == ESP_ERR_INVALID_STATE) {
        return ESP_OK;
    }

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {// NVS 分区无空闲页或版本不匹配时需要擦除后重新初始化。
        ESP_LOGW(TAG, "nvs needs erase, ret=0x%x", ret);
        ret = nvs_flash_erase();// 擦除后再次初始化 NVS。
        if (ret != ESP_OK) {
            return ret;
        }

        ret = nvs_flash_init();
        if (ret == ESP_OK || ret == ESP_ERR_INVALID_STATE) {
            return ESP_OK;
        }
    }

    return ret;
}

static void config_service_try_load_str(nvs_handle_t handle, const char *key, char *dst, size_t dst_size)
{
    size_t required_size = dst_size;
    if (nvs_get_str(handle, key, dst, &required_size) != ESP_OK) {
        // 第一次启动时 key 不存在很正常，这里直接保留默认值。
    }
}

esp_err_t config_service_init(void)
{
    if (s_config.inited) {
        return ESP_OK;
    }

    esp_err_t ret = config_service_init_nvs();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "config_service_init_nvs failed, ret=0x%x", ret);
        return ret;
    }

    config_service_load_default(&s_config.runtime); //  先加载默认配置，后续 load 时会覆盖为 NVS 中的值（如果存在）。
    config_service_mark_default_sources();
    s_config.inited = true;
    ESP_LOGI(TAG, "config service ready");
    return ESP_OK;
}

esp_err_t config_service_load(void)
{
    if (!s_config.inited) {
        return ESP_ERR_INVALID_STATE;
    }

    config_service_load_default(&s_config.runtime);// 先加载默认配置，后续覆盖为 NVS 中的值（如果存在）。
    config_service_mark_default_sources();

    nvs_handle_t handle = 0;
    esp_err_t ret = nvs_open(APP_CFG_NAMESPACE, NVS_READWRITE, &handle);// 尝试打开配置命名空间，如果不存在会返回 ESP_ERR_NVS_NOT_FOUND，此时直接使用默认配置继续。
    if (ret != ESP_OK) {
        // 当前没有配置命名空间时，直接继续使用默认值。
        s_config.loaded = true;
        ESP_LOGI(TAG, "config loaded from defaults");
        return ESP_OK;
    }

    // 从 NVS 中加载配置项，失败时保留默认值继续。
    {
        size_t len = sizeof(s_config.runtime.wifi_ssid);
        if (nvs_get_str(handle, "wifi_ssid", s_config.runtime.wifi_ssid, &len) == ESP_OK) {
            s_config.wifi_source = CONFIG_VALUE_SOURCE_NVS;
        }
    }
    {
        size_t len = sizeof(s_config.runtime.wifi_password);
        if (nvs_get_str(handle, "wifi_pwd", s_config.runtime.wifi_password, &len) == ESP_OK) {
            s_config.wifi_source = CONFIG_VALUE_SOURCE_NVS;
        }
    }
    {
        size_t len = sizeof(s_config.runtime.http_test_url);
        if (nvs_get_str(handle, "http_url", s_config.runtime.http_test_url, &len) == ESP_OK) {
            s_config.http_source = CONFIG_VALUE_SOURCE_NVS;
        }
    }
    {
        size_t len = sizeof(s_config.runtime.ota_version_url);
        if (nvs_get_str(handle, "ota_url", s_config.runtime.ota_version_url, &len) == ESP_OK) {
            s_config.ota_source = CONFIG_VALUE_SOURCE_NVS;
        }
    }

    nvs_close(handle);// 关闭 NVS 句柄。

    if (!config_service_url_is_basic_valid(s_config.runtime.http_test_url)) {
        ESP_LOGW(TAG, "invalid http_url in NVS, fallback to default");
        config_service_copy_string(s_config.runtime.http_test_url, sizeof(s_config.runtime.http_test_url), APP_HTTP_TEST_URL);
        s_config.http_source = CONFIG_VALUE_SOURCE_DEFAULT;
    }

    if (!config_service_url_is_basic_valid(s_config.runtime.ota_version_url)) {
        ESP_LOGW(TAG, "invalid ota_url in NVS, fallback to default");
        config_service_copy_string(s_config.runtime.ota_version_url, sizeof(s_config.runtime.ota_version_url), APP_OTA_VERSION_URL);
        s_config.ota_source = CONFIG_VALUE_SOURCE_DEFAULT;
    }

    s_config.loaded = true;

    ESP_LOGI(TAG, "config loaded, ssid=\"%s\" http_url=%s ota_url=%s",
             s_config.runtime.wifi_ssid[0] != '\0' ? s_config.runtime.wifi_ssid : "<empty>",
             s_config.runtime.http_test_url,
             s_config.runtime.ota_version_url);
    return ESP_OK;
}

esp_err_t config_service_save(void)
{
    if (!s_config.inited) {
        return ESP_ERR_INVALID_STATE;
    }

    nvs_handle_t handle = 0;
    esp_err_t ret = nvs_open(APP_CFG_NAMESPACE, NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = nvs_set_str(handle, "wifi_ssid", s_config.runtime.wifi_ssid);
    if (ret == ESP_OK) {
        ret = nvs_set_str(handle, "wifi_pwd", s_config.runtime.wifi_password);
    }
    if (ret == ESP_OK) {
        ret = nvs_set_str(handle, "http_url", s_config.runtime.http_test_url);
    }
    if (ret == ESP_OK) {
        ret = nvs_set_str(handle, "ota_url", s_config.runtime.ota_version_url);
    }
    if (ret == ESP_OK) {
        ret = nvs_commit(handle);// 提交更改到 NVS，确保数据写入闪存。
    }

    nvs_close(handle);// 关闭 NVS 句柄。

    if (ret == ESP_OK) {
        if (s_config.wifi_source == CONFIG_VALUE_SOURCE_RUNTIME) {
            s_config.wifi_source = CONFIG_VALUE_SOURCE_NVS;
        }
        if (s_config.http_source == CONFIG_VALUE_SOURCE_RUNTIME) {
            s_config.http_source = CONFIG_VALUE_SOURCE_NVS;
        }
        if (s_config.ota_source == CONFIG_VALUE_SOURCE_RUNTIME) {
            s_config.ota_source = CONFIG_VALUE_SOURCE_NVS;
        }
        ESP_LOGI(TAG, "config saved");
    }

    return ret;
}

esp_err_t config_service_reset_to_default(void)
{
    if (!s_config.inited) {
        return ESP_ERR_INVALID_STATE;
    }

    config_service_load_default(&s_config.runtime);
    return config_service_save();
}

const app_runtime_config_t *config_service_get(void)
{
    return &s_config.runtime;
}

config_value_source_t config_service_get_wifi_source(void)
{
    return s_config.wifi_source;
}

config_value_source_t config_service_get_http_source(void)
{
    return s_config.http_source;
}

config_value_source_t config_service_get_ota_source(void)
{
    return s_config.ota_source;
}

const char *config_service_source_to_string(config_value_source_t source)
{
    switch (source) {
        case CONFIG_VALUE_SOURCE_DEFAULT:
            return "DEFAULT";
        case CONFIG_VALUE_SOURCE_NVS:
            return "NVS";
        case CONFIG_VALUE_SOURCE_RUNTIME:
            return "RUNTIME";
        default:
            return "UNKNOWN";
    }
}

const char *config_service_get_source_summary(void)
{
    bool has_default = false;
    bool has_nvs = false;
    bool has_runtime = false;
    config_value_source_t sources[] = {
        s_config.wifi_source,
        s_config.http_source,
        s_config.ota_source,
    };

    for (size_t i = 0; i < sizeof(sources) / sizeof(sources[0]); ++i) {
        if (sources[i] == CONFIG_VALUE_SOURCE_DEFAULT) {
            has_default = true;
        } else if (sources[i] == CONFIG_VALUE_SOURCE_NVS) {
            has_nvs = true;
        } else if (sources[i] == CONFIG_VALUE_SOURCE_RUNTIME) {
            has_runtime = true;
        }
    }

    if (has_runtime && !has_default && !has_nvs) {
        return "RUNTIME";
    }
    if (has_default && !has_nvs && !has_runtime) {
        return "DEFAULT";
    }
    if (has_nvs && !has_default && !has_runtime) {
        return "NVS";
    }
    return "MIXED";
}

esp_err_t config_service_set_wifi(const char *ssid, const char *password)
{
    if (!s_config.inited) {
        return ESP_ERR_INVALID_STATE;
    }

    if (ssid != NULL) {
        config_service_copy_string(s_config.runtime.wifi_ssid, sizeof(s_config.runtime.wifi_ssid), ssid);
        s_config.wifi_source = CONFIG_VALUE_SOURCE_RUNTIME;
    }
    if (password != NULL) {
        config_service_copy_string(s_config.runtime.wifi_password, sizeof(s_config.runtime.wifi_password), password);
        s_config.wifi_source = CONFIG_VALUE_SOURCE_RUNTIME;
    }

    return ESP_OK;
}

esp_err_t config_service_set_urls(const char *http_url, const char *ota_url)
{
    if (!s_config.inited) {
        return ESP_ERR_INVALID_STATE;
    }

    if (http_url != NULL) {
        if (!config_service_url_is_basic_valid(http_url)) {
            return ESP_ERR_INVALID_ARG;
        }
        config_service_copy_string(s_config.runtime.http_test_url, sizeof(s_config.runtime.http_test_url), http_url);
        s_config.http_source = CONFIG_VALUE_SOURCE_RUNTIME;
    }
    if (ota_url != NULL) {
        if (!config_service_url_is_basic_valid(ota_url)) {
            return ESP_ERR_INVALID_ARG;
        }
        config_service_copy_string(s_config.runtime.ota_version_url, sizeof(s_config.runtime.ota_version_url), ota_url);
        s_config.ota_source = CONFIG_VALUE_SOURCE_RUNTIME;
    }

    return ESP_OK;
}

bool config_service_is_ready(void)
{
    return s_config.inited && s_config.loaded;
}

esp_err_t config_service_self_test(void)
{
    if (!config_service_is_ready()) {
        return ESP_ERR_INVALID_STATE;
    }

    app_runtime_config_t backup = s_config.runtime;
    esp_err_t ret = ESP_OK;

    ESP_LOGI(TAG, "config self-test start");

    // 自检时只测试 URL 保存与重载，避免动到 Wi-Fi 参数影响当前联网流程。
    ret = config_service_set_urls("http://selftest.local/http", "http://selftest.local/ota");
    if (ret != ESP_OK) {
        return ret;
    }

    ret = config_service_save();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "config self-test save failed, ret=0x%x", ret);
        goto restore;
    }

    ret = config_service_load();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "config self-test reload failed, ret=0x%x", ret);
        goto restore;
    }

    if (strcmp(s_config.runtime.http_test_url, "http://selftest.local/http") != 0 ||
        strcmp(s_config.runtime.ota_version_url, "http://selftest.local/ota") != 0) {
        ESP_LOGE(TAG, "config self-test compare failed");
        ret = ESP_FAIL;
        goto restore;
    }

    ESP_LOGI(TAG, "config self-test passed");

    // 再额外验证一次“恢复默认值”链路，确认 reset_to_default 能真正写回默认配置。
    ret = config_service_reset_to_default();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "config self-test reset_to_default failed, ret=0x%x", ret);
        goto restore;
    }

    ret = config_service_load();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "config self-test default reload failed, ret=0x%x", ret);
        goto restore;
    }

    if (strcmp(s_config.runtime.wifi_ssid, APP_WIFI_STA_SSID) != 0 ||
        strcmp(s_config.runtime.http_test_url, APP_HTTP_TEST_URL) != 0 ||
        strcmp(s_config.runtime.ota_version_url, APP_OTA_VERSION_URL) != 0) {
        ESP_LOGE(TAG, "config self-test default compare failed");
        ret = ESP_FAIL;
        goto restore;
    }

    ESP_LOGI(TAG, "config self-test reset_to_default passed");

restore:
    // 无论自检是否成功，都恢复原始配置，避免影响后续业务。
    s_config.runtime = backup;

    {
        esp_err_t restore_ret = config_service_save();
        if (restore_ret != ESP_OK) {
            ESP_LOGE(TAG, "config self-test restore save failed, ret=0x%x", restore_ret);
            if (ret == ESP_OK) {
                ret = restore_ret;
            }
        }

        restore_ret = config_service_load();
        if (restore_ret != ESP_OK) {
            ESP_LOGE(TAG, "config self-test restore load failed, ret=0x%x", restore_ret);
            if (ret == ESP_OK) {
                ret = restore_ret;
            }
        }
    }

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "config self-test restore done");
    }

    return ret;
}
