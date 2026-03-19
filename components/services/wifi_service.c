#include "wifi_service.h"

#include <stdio.h>
#include <string.h>

#include "app_config.h"
#include "display_service.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "lwip/inet.h"
#include "nvs_flash.h"

static const char *TAG = "WIFI_SVC";

#define WIFI_CONNECTED_BIT   BIT0 // 已经成功连上路由器，但此时不一定已经拿到 IP。
#define WIFI_GOT_IP_BIT      BIT1 // 已经通过 DHCP 拿到 IP，说明网络真正可用。
#define WIFI_FAIL_BIT        BIT2 // 重试次数耗尽后置位，表示本轮联网失败。

typedef struct {
    bool inited;                    // Wi-Fi 服务是否已完成一次性初始化。
    bool started;                   // 是否已经执行过 esp_wifi_start()。
    bool netif_ready;               // STA 网络接口和默认事件循环是否已准备完成。
    wifi_state_t state;             // 当前对外展示的 Wi-Fi 高层状态。
    esp_netif_t *sta_netif;         // esp_netif 创建的默认 STA 网络接口句柄。
    EventGroupHandle_t event_group; // 用于同步连接、获取 IP、失败结果的事件组。
    uint32_t retry_count;           // 断线后当前已经执行的重连次数。
    char ip_string[20];             // 缓存的 IPv4 字符串，供日志和屏幕显示复用。
    int8_t rssi;                    // 当前连接 AP 的 RSSI，单位 dBm。
    uint8_t channel;                // 当前连接 AP 的信道编号。
    int32_t last_disconnect_reason; // 最近一次断线原因码，便于后续排查。
    int64_t last_ap_info_update_ms; // 上次刷新 AP 信息的时间戳，避免每轮都读 Wi-Fi 驱动。
} wifi_service_ctx_t;

// Wi-Fi 服务的全局上下文，统一保存初始化状态、联网状态和显示需要的缓存数据。
static wifi_service_ctx_t s_wifi = {
    .inited = false,
    .started = false,
    .netif_ready = false,
    .state = WIFI_STATE_IDLE,
    .sta_netif = NULL,
    .event_group = NULL,
    .retry_count = 0,
    .ip_string = "0.0.0.0",
    .rssi = -127,
    .channel = 0,
    .last_disconnect_reason = 0,
    .last_ap_info_update_ms = 0,
};

/**
 * @brief 把 Wi-Fi 状态枚举转成便于日志和 LCD 显示的文本
 *
 * 这一层做字符串转换的目的，是让业务层始终围绕统一状态枚举工作，
 * 真正打印或显示时再转成文本，避免上层到处写重复的 switch 判断。
 */
static const char *wifi_service_state_to_string(wifi_state_t state)
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
 * @brief 把当前网络状态同步到显示服务
 *
 * Wi-Fi 服务本身不直接画屏，它只负责把“当前状态”同步给显示服务的缓存。
 * 真正的局部刷新会在 display_service_process() 里统一执行。
 */
static void wifi_service_sync_display(void)
{
    (void)display_service_show_wifi_status(s_wifi.state);
    (void)display_service_show_wifi_ip(s_wifi.ip_string);
    (void)display_service_show_wifi_signal(s_wifi.rssi, s_wifi.channel);
}

/**
 * @brief 刷新当前连接 AP 的信息
 *
 * 当前版本主要缓存：
 * 1. RSSI：用于判断信号强弱
 * 2. channel：用于快速定位是否存在信道环境问题
 *
 * 后面如果还要显示 BSSID、加密方式、带宽等信息，也可以继续从
 * wifi_ap_record_t 里补出来。
 */
static void wifi_service_refresh_ap_info(void)
{
    wifi_ap_record_t ap_info = {0};
    esp_err_t ret = esp_wifi_sta_get_ap_info(&ap_info);
    if (ret != ESP_OK) {
        return;
    }

    s_wifi.rssi = ap_info.rssi;
    s_wifi.channel = ap_info.primary;
    wifi_service_sync_display();
}

/**
 * @brief 更新内部网络状态
 *
 * 这层统一做三件事：
 * 1. 更新内部状态缓存
 * 2. 打印状态变化日志
 * 3. 把状态同步给 LCD 显示服务
 */
static void wifi_service_set_state(wifi_state_t state)
{
    if (s_wifi.state == state) {
        return;
    }

    s_wifi.state = state;
    ESP_LOGI(TAG, "wifi state -> %s", wifi_service_state_to_string(state));
    wifi_service_sync_display();
}

/**
 * @brief 统一处理 Wi-Fi 和 IP 事件
 *
 * 这是整个 Wi-Fi 模板里最核心的一层：
 * 1. ESP-IDF 底层产生事件
 * 2. 事件循环回调到这里
 * 3. 这里再把底层事件翻译成项目里统一使用的高层状态
 *
 * 所以 Wi-Fi 的状态推进不是靠主循环轮询，而是靠事件驱动。
 */
static void wifi_service_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    (void)arg;

    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                // STA 启动后立即开始尝试连接路由器。
                s_wifi.retry_count = 0; // 每次启动都把重试计数清零，断线重连时再递增。
                // 先把高层状态切到 CONNECTING，实际是否连上要等后续事件继续推进。
                wifi_service_set_state(WIFI_STATE_CONNECTING);
                ESP_LOGI(TAG, "wifi sta started, begin connect");
                // 真正向 Wi-Fi 驱动发起连接动作，后续结果仍然通过事件异步返回。
                (void)esp_wifi_connect();
                break;

            case WIFI_EVENT_STA_CONNECTED:
                // 这里表示已经和路由器建立链路，但通常还要等 DHCP 分配 IP。
                // CONNECTED 表示“已经连上路由器”，但此时不一定已经有可用 IP。
                xEventGroupSetBits(s_wifi.event_group, WIFI_CONNECTED_BIT);
                wifi_service_set_state(WIFI_STATE_CONNECTED);
                ESP_LOGI(TAG, "wifi connected to AP");
                break;

            case WIFI_EVENT_STA_DISCONNECTED:
            {
                const wifi_event_sta_disconnected_t *event =
                    (const wifi_event_sta_disconnected_t *)event_data; // 断线事件会带原因码，后面排查超时、认证失败、AP 侧问题时很重要。

                // 断线后先清掉“已连接 / 已拿到 IP”标志，再决定是否自动重连。
                xEventGroupClearBits(s_wifi.event_group, WIFI_CONNECTED_BIT | WIFI_GOT_IP_BIT);
                snprintf(s_wifi.ip_string, sizeof(s_wifi.ip_string), "0.0.0.0");
                s_wifi.rssi = -127;
                s_wifi.channel = 0;
                s_wifi.last_disconnect_reason = event ? event->reason : 0;
                wifi_service_set_state(WIFI_STATE_DISCONNECTED);

                if (s_wifi.retry_count < APP_WIFI_MAX_RETRY) {
                    s_wifi.retry_count++;
                    ESP_LOGW(TAG, "wifi disconnected, reason=%d retry=%lu/%d",
                             event ? event->reason : -1,
                             (unsigned long)s_wifi.retry_count,
                             APP_WIFI_MAX_RETRY);
                    // 重连前先把高层状态重新切回 CONNECTING，便于日志和屏幕同步表现。
                    wifi_service_set_state(WIFI_STATE_CONNECTING);
                    // 重新向驱动发起连接，真正结果仍然要靠后续事件返回。
                    (void)esp_wifi_connect();
                } else {
                    ESP_LOGE(TAG, "wifi retry exhausted, last_reason=%d",
                             event ? event->reason : -1);
                    // 超过最大重试次数后，置位 FAIL_BIT，并把高层状态切到 ERROR。
                    xEventGroupSetBits(s_wifi.event_group, WIFI_FAIL_BIT);
                    wifi_service_set_state(WIFI_STATE_ERROR);
                }
                break;
            }

            default:
                break;
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        // GOT_IP 才表示后面 HTTP / OTA / AI 这些能力真正有了网络前提。
        const ip_event_got_ip_t *event = (const ip_event_got_ip_t *)event_data; // GOT_IP 事件里会带本次 DHCP 分配到的 IP 信息。
        // GOT_IP 表示网络真正可用，这时后续 HTTP / OTA / AI 才能安全发起。
        xEventGroupSetBits(s_wifi.event_group, WIFI_GOT_IP_BIT);
        s_wifi.retry_count = 0;
        // 把 IP 地址转成字符串缓存起来，供日志和屏幕显示复用。
        (void)esp_ip4addr_ntoa(&event->ip_info.ip, s_wifi.ip_string, sizeof(s_wifi.ip_string));
        wifi_service_refresh_ap_info();
        wifi_service_set_state(WIFI_STATE_GOT_IP);
        ESP_LOGI(TAG, "wifi got ip=%s rssi=%d channel=%u", s_wifi.ip_string, s_wifi.rssi, s_wifi.channel);
    }
}

/**
 * @brief 初始化 NVS
 *
 * Wi-Fi 栈通常依赖 NVS 保存内部校准或配置数据，所以联网前要先保证 NVS 可用。
 */
static esp_err_t wifi_service_init_nvs(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "nvs needs erase, ret=0x%x", ret);
        ret = nvs_flash_erase();
        if (ret != ESP_OK) {
            return ret;
        }
        ret = nvs_flash_init();
    }

    return ret;
}

/**
 * @brief 初始化 Wi-Fi 服务
 *
 * 当前版本把 Wi-Fi 做成独立 service，后面 HTTP / OTA / AI 都会复用它。
 */
esp_err_t wifi_service_init(void)
{
    if (s_wifi.inited) {
        return ESP_OK;
    }
    // 先确保 NVS 可用，Wi-Fi 驱动本身就依赖它保存校准和配置类数据。
    esp_err_t ret = wifi_service_init_nvs();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "wifi_service_init_nvs failed, ret=0x%x", ret);
        return ret;
    }

    if (s_wifi.event_group == NULL) {
        // 创建事件组，用于同步“已连接 / 已拿 IP / 联网失败”等结果。
        s_wifi.event_group = xEventGroupCreate();
        if (s_wifi.event_group == NULL) {
            return ESP_ERR_NO_MEM;
        }
    }

    // 先初始化网络接口和默认事件循环，Wi-Fi 事件后面都要挂在它们上面。
    ret = esp_netif_init();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "esp_netif_init failed, ret=0x%x", ret);
        return ret;
    }
    // 这里可能返回 ESP_ERR_INVALID_STATE，表示默认事件循环已经存在，
    // 通常是别的 service 提前创建了，这里不把它当成错误处理。
    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "esp_event_loop_create_default failed, ret=0x%x", ret);
        return ret;
    }

    // Wi-Fi 驱动需要一个默认的 STA 网络接口，如果还没有就创建一个。
    if (!s_wifi.netif_ready) {
        s_wifi.sta_netif = esp_netif_create_default_wifi_sta();
        s_wifi.netif_ready = true;
    }

    // 初始化 Wi-Fi 驱动，后面才可以 set_mode / set_config / start。
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT(); // 先用官方默认配置，后面如果要调底层缓存数量可再扩展。
    ret = esp_wifi_init(&cfg);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "esp_wifi_init failed, ret=0x%x", ret);
        return ret;
    }

    // 注册 Wi-Fi 和 IP 事件回调，让后面联网状态完全通过事件驱动推进。
    ret = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_service_event_handler, NULL);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_service_event_handler, NULL);
    if (ret != ESP_OK) {
        return ret;
    }
    // 当前模板先固定为 STA 模式，后面如果要做配网或 AP+STA，再从这里扩展。
    ret = esp_wifi_set_mode(WIFI_MODE_STA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_set_mode failed, ret=0x%x", ret);
        return ret;
    }
    // 到这里 Wi-Fi 服务的初始化就算完成了，后续调用 wifi_service_start() 才会真正开始联网。
    wifi_service_set_state(WIFI_STATE_IDLE);
    s_wifi.inited = true;
    ESP_LOGI(TAG, "wifi service ready");
    return ESP_OK;
}

/**
 * @brief 启动 Wi-Fi STA 联网
 *
 * 这一层只负责把配置写入 Wi-Fi 驱动并启动联网，
 * 真正的连接结果由事件回调异步告诉我们。
 */
esp_err_t wifi_service_start(void)
{
    if (!s_wifi.inited) {
        return ESP_ERR_INVALID_STATE;
    }
    // SSID 不能为空，后面如果做 NVS 配网，这里也可以改成“从 NVS 读取并校验”。
    if (APP_WIFI_STA_SSID[0] == '\0') {
        ESP_LOGE(TAG, "wifi ssid is empty, please update APP_WIFI_STA_SSID");
        wifi_service_set_state(WIFI_STATE_ERROR); // 直接把状态置成 ERROR，后续如果有设置界面可以在里面提示用户去设置 Wi-Fi。
        return ESP_ERR_INVALID_ARG;
    }

    // wifi_config_t 是 ESP-IDF 提供的 Wi-Fi 配置结构，这里先只使用 STA 模式下最常用的字段。
    wifi_config_t wifi_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK, // 要求接入点至少满足 WPA2，避免连接到过低安全级别网络。
            .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,         // 为后续兼容 WPA3/WPA2 混合网络预留。
        },
    };

    // 这里把 app_config.h 里的 SSID 和密码拷进 Wi-Fi 配置结构，后面很适合替换成 NVS 读取。
    (void)snprintf((char *)wifi_config.sta.ssid, sizeof(wifi_config.sta.ssid), "%s", APP_WIFI_STA_SSID);
    (void)snprintf((char *)wifi_config.sta.password, sizeof(wifi_config.sta.password), "%s", APP_WIFI_STA_PASSWORD);
    
    // 把 SSID / 密码写入驱动，真正连接结果仍由事件系统异步返回。
    esp_err_t ret = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_set_config failed, ret=0x%x", ret);
        wifi_service_set_state(WIFI_STATE_ERROR);
        return ret;
    }

    // 启动 Wi-Fi 驱动，后续会先收到 WIFI_EVENT_STA_START，再真正发起连接。
    ret = esp_wifi_start();
    if (ret == ESP_OK || ret == ESP_ERR_WIFI_CONN) {
        // 当前先关闭 Wi-Fi 省电，优先排查 beacon timeout 导致的链路不稳定问题。
        esp_err_t ps_ret = esp_wifi_set_ps(WIFI_PS_NONE);
        if (ps_ret != ESP_OK) {
            ESP_LOGW(TAG, "esp_wifi_set_ps(WIFI_PS_NONE) failed, ret=0x%x", ps_ret);
        }

        s_wifi.started = true;
        // 启动成功后先切到 CONNECTING，等事件系统继续推进到 CONNECTED / GOT_IP。
        wifi_service_set_state(WIFI_STATE_CONNECTING);
        ESP_LOGI(TAG, "wifi start requested, ssid=\"%s\"", APP_WIFI_STA_SSID);
        return ESP_OK;
    }
    
    ESP_LOGE(TAG, "esp_wifi_start failed, ret=0x%x", ret);
    wifi_service_set_state(WIFI_STATE_ERROR);
    return ret;
}

/**
 * @brief Wi-Fi 周期处理
 *
 * 当前 Wi-Fi 主链主要靠事件驱动推进，这里只做轻量的附加工作：
 * 定期刷新一次 AP 信息，把 RSSI / channel 同步给显示层。
 */
void wifi_service_process(void)
{
    if (!s_wifi.inited || s_wifi.state != WIFI_STATE_GOT_IP) {
        return;
    }

    int64_t now_ms = esp_timer_get_time() / 1000;
    if ((now_ms - s_wifi.last_ap_info_update_ms) < 1000) {
        return;
    }

    s_wifi.last_ap_info_update_ms = now_ms;
    wifi_service_refresh_ap_info();
}

/**
 * @brief 获取当前 Wi-Fi 状态
 */
wifi_state_t wifi_service_get_state(void)
{
    return s_wifi.state;
}

/**
 * @brief 获取当前 IP 地址字符串
 */
const char *wifi_service_get_ip_string(void)
{
    return s_wifi.ip_string;
}

int8_t wifi_service_get_rssi(void)
{
    return s_wifi.rssi;
}

uint8_t wifi_service_get_channel(void)
{
    return s_wifi.channel;
}

int32_t wifi_service_get_last_disconnect_reason(void)
{
    return s_wifi.last_disconnect_reason;
}

/**
 * @brief 判断 Wi-Fi 服务是否已初始化完成
 */
bool wifi_service_is_ready(void)
{
    return s_wifi.inited;
}
