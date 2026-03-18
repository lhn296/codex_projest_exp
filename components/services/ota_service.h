#ifndef OTA_SERVICE_H
#define OTA_SERVICE_H

#include <stdbool.h>

#include "app_types.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// 初始化 OTA 服务，准备状态缓存和默认显示内容。
esp_err_t ota_service_init(void);

// OTA 周期处理入口，当前版本用于版本检查和状态推进。
void ota_service_process(void);

// 判断 OTA 服务是否已完成初始化。
bool ota_service_is_ready(void);

// 获取当前 OTA 状态。
ota_state_t ota_service_get_state(void);

// 获取当前 OTA 状态说明文本。
const char *ota_service_get_message(void);

// 判断当前是否检测到可升级版本。
bool ota_service_has_update(void);

#ifdef __cplusplus
}
#endif

#endif
