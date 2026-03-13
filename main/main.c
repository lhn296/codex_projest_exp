#include "app_main_task.h"
#include "app_config.h"
#include "esp_log.h"

/**
 * @brief 应用入口函数
 *
 * 说明：
 * 1. 这里只做系统启动日志输出
 * 2. 启动应用主任务
 * 3. 不在这里堆业务逻辑，避免 main.c 失控膨胀
 */
void app_main(void)
{
    ESP_LOGI("MAIN", "========================================");
    ESP_LOGI("MAIN", "%s", APP_PROJECT_DISPLAY_NAME);
    ESP_LOGI("MAIN", "Project: %s | Version: %s", APP_PROJECT_NAME, APP_PROJECT_VERSION);
    ESP_LOGI("MAIN", "Target: %s", APP_PROJECT_TARGET);
    ESP_LOGI("MAIN", "========================================");

    app_main_task_start();
}
