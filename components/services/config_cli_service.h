#ifndef CONFIG_CLI_SERVICE_H
#define CONFIG_CLI_SERVICE_H

#include <stdbool.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// 初始化串口配置命令服务。
esp_err_t config_cli_service_init(void);

// 串口配置命令周期处理入口。
void config_cli_service_process(void);

// 判断串口配置命令服务是否已初始化。
bool config_cli_service_is_ready(void);

#ifdef __cplusplus
}
#endif

#endif
