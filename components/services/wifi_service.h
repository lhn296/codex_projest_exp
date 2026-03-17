#ifndef WIFI_SERVICE_H
#define WIFI_SERVICE_H

#include <stdbool.h>

#include "app_types.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// 初始化 Wi-Fi 服务，完成 NVS、netif、事件循环和 Wi-Fi 驱动准备。
esp_err_t wifi_service_init(void);

// 按当前配置启动 STA 联网流程，真正结果通过事件回调异步返回。
esp_err_t wifi_service_start(void);

// Wi-Fi 周期处理入口，当前版本预留给后续超时、心跳等逻辑扩展。
void wifi_service_process(void);

// 获取当前 Wi-Fi 高层状态，供业务层和显示层查询。
wifi_state_t wifi_service_get_state(void);

// 获取当前缓存的 IP 字符串，未联网时通常返回 0.0.0.0。
const char *wifi_service_get_ip_string(void);

// 获取当前缓存的 RSSI，未连接时返回较小默认值。
int8_t wifi_service_get_rssi(void);

// 获取当前连接 AP 的信道，未连接时返回 0。
uint8_t wifi_service_get_channel(void);

// 获取最近一次断线原因码，未断线时返回 0。
int32_t wifi_service_get_last_disconnect_reason(void);

// 判断 Wi-Fi 服务是否已经完成初始化。
bool wifi_service_is_ready(void);

#ifdef __cplusplus
}
#endif

#endif
