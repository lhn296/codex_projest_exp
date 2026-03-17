#include "app_main_task.h"

#include "app_config.h"
#include "app_event_task.h"
#include "beep_service.h"
#include "button_service.h"
#include "display_service.h"
#include "led_service.h"
#include "wifi_service.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

static const char *TAG = "APP_MAIN";
static QueueHandle_t s_button_event_queue = NULL;

/**
 * @brief 打印当前版本的硬件映射
 *
 * 这个函数的作用是把本版最关键的 GPIO / XL9555 / SPI 映射一次性打印出来，
 * 方便上板时快速核对接线和配置。
 */
static void app_main_task_log_gpio_mapping(void)
{
    ESP_LOGI(TAG, "External LED mapping:");
    ESP_LOGI(TAG, "  SYS -> GPIO%d active_level=%d default_mode=%d",
             APP_SYS_LED_GPIO, APP_SYS_LED_ACTIVE_LEVEL, APP_LED_SYS_DEFAULT_MODE);
    ESP_LOGI(TAG, "  NET -> GPIO%d active_level=%d default_mode=%d",
             APP_NET_LED_GPIO, APP_NET_LED_ACTIVE_LEVEL, APP_LED_NET_DEFAULT_MODE);
    ESP_LOGI(TAG, "  ERR -> GPIO%d active_level=%d default_mode=%d",
             APP_ERR_LED_GPIO, APP_ERR_LED_ACTIVE_LEVEL, APP_LED_ERR_DEFAULT_MODE);

    ESP_LOGI(TAG, "I2C / XL9555 mapping:");
    ESP_LOGI(TAG, "  I2C0 -> SDA GPIO%d SCL GPIO%d addr=0x%02X",
             APP_I2C_MASTER_SDA_GPIO, APP_I2C_MASTER_SCL_GPIO, APP_XL9555_I2C_ADDR);
    ESP_LOGI(TAG, "  XL9555 INT -> GPIO%d pullup=board/external intr=negedge",
             APP_XL9555_INT_GPIO);
    ESP_LOGI(TAG, "  KEY0 -> IO1_7 KEY1 -> IO1_6 KEY2 -> IO1_5 KEY3 -> IO1_4 active_level=%d",
             APP_XL9555_KEY_ACTIVE_LEVEL);
    ESP_LOGI(TAG, "  BEEP -> IO0_3 active_level=%d LCD_RST -> IO1_2 LCD_PWR -> IO1_3",
             APP_XL9555_BEEP_ACTIVE_LEVEL);

    ESP_LOGI(TAG, "LCD / SPI mapping:");
    ESP_LOGI(TAG, "  ST7789V %dx%d host=%d MOSI=%d DC=%d SCLK=%d CS=%d",
             APP_LCD_WIDTH,
             APP_LCD_HEIGHT,
             APP_LCD_SPI_HOST,
             APP_LCD_SPI_MOSI_GPIO,
             APP_LCD_SPI_DC_GPIO,
             APP_LCD_SPI_SCLK_GPIO,
             APP_LCD_SPI_CS_GPIO);

    ESP_LOGI(TAG, "Wi-Fi config:");
    ESP_LOGI(TAG, "  SSID=\"%s\" max_retry=%d timeout_ms=%d",
             APP_WIFI_STA_SSID[0] != '\0' ? APP_WIFI_STA_SSID : "<empty>",
             APP_WIFI_MAX_RETRY,
             APP_WIFI_CONNECT_TIMEOUT_MS);
}

/**
 * @brief 把配置层定义的默认输出状态同步到运行时
 *
 * 这里不只是给 LED 套默认模式，还会把这些状态同步到显示服务缓存里，
 * 这样上电后 LCD 首页能立刻显示正确的默认状态。
 */
static void app_main_task_apply_default_outputs(void)
{
    // 先应用三路 LED 的默认模式。
    (void)led_service_set_mode(LED_ID_SYS, APP_LED_SYS_DEFAULT_MODE);
    (void)led_service_set_mode(LED_ID_NET, APP_LED_NET_DEFAULT_MODE);
    (void)led_service_set_mode(LED_ID_ERR, APP_LED_ERR_DEFAULT_MODE);

    // 再把版本、阶段和当前输出状态同步到 LCD 首页。
    (void)display_service_show_version(APP_PROJECT_VERSION);
    (void)display_service_show_stage(APP_PROJECT_STAGE_NAME);
    (void)display_service_show_led_status(LED_ID_SYS, APP_LED_SYS_DEFAULT_MODE);
    (void)display_service_show_led_status(LED_ID_NET, APP_LED_NET_DEFAULT_MODE);
    (void)display_service_show_led_status(LED_ID_ERR, APP_LED_ERR_DEFAULT_MODE);
    (void)display_service_show_beep_status(beep_service_is_enabled(), beep_service_is_test_mode_enabled());
    (void)display_service_show_wifi_status(wifi_service_get_state());
    (void)display_service_show_wifi_ip(wifi_service_get_ip_string());
    (void)display_service_refresh_home();
}

/**
 * @brief 应用主任务
 *
 * 当前项目的主任务负责三件事：
 * 1. 建立事件队列
 * 2. 初始化输出与输入服务
 * 3. 在 while(1) 中周期推进各个 service
 */
static void app_main_task(void *param)
{
    (void)param;

    // 统一事件队列是输入层和业务层的桥梁，后面的 button_service 和 app_event_task 都依赖它。
    s_button_event_queue = xQueueCreate(APP_EVENT_QUEUE_LENGTH, sizeof(app_event_msg_t));
    if (s_button_event_queue == NULL) {
        ESP_LOGE(TAG, "xQueueCreate failed, queue_len=%d", APP_EVENT_QUEUE_LENGTH);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "button event queue ready, queue_len=%d", APP_EVENT_QUEUE_LENGTH);

    // 初始化顺序上先准备输出，再准备输入，这样按键一触发就能立刻看到反馈。
    esp_err_t ret = led_service_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "led_service_init failed, ret=0x%x", ret);
        vTaskDelete(NULL);
        return;
    }

    ret = beep_service_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "beep_service_init failed, ret=0x%x", ret);
        vTaskDelete(NULL);
        return;
    }

    // 显示服务现在也是输出链的一部分，初始化成功后上电就能看到首页。
    ret = display_service_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "display_service_init failed, ret=0x%x", ret);
        vTaskDelete(NULL);
        return;
    }

    // Wi-Fi 服务这版开始成为正式系统模块，后面的 HTTP / OTA / AI 都会建立在它之上。
    ret = wifi_service_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "wifi_service_init failed, ret=0x%x", ret);
        vTaskDelete(NULL);
        return;
    }

    // 启动统一事件任务，后面按键状态机识别到的事件会都发到这里处理。
    ret = app_event_task_start(s_button_event_queue);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "app_event_task_start failed, ret=0x%x", ret);
        vTaskDelete(NULL);
        return;
    }

    // 最后初始化按键服务，让它拿着现成的队列句柄去发布事件。
    ret = button_service_init(s_button_event_queue);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "button_service_init failed, ret=0x%x", ret);
        vTaskDelete(NULL);
        return;
    }

    app_main_task_apply_default_outputs();
    app_main_task_log_gpio_mapping();

    // 所有显示和日志准备就绪后，再正式开始联网，这样状态变化能马上反馈到屏幕和串口。
    ret = wifi_service_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "wifi_service_start failed, ret=0x%x", ret);
    }

    ESP_LOGI(TAG, "%s started", APP_PROJECT_DISPLAY_NAME);
    ESP_LOGI(TAG, "Default LED modes: SYS=%d NET=%d ERR=%d",
             APP_LED_SYS_DEFAULT_MODE,
             APP_LED_NET_DEFAULT_MODE,
             APP_LED_ERR_DEFAULT_MODE);
    ESP_LOGI(TAG, "Event flow: XL9555 -> button_service -> unified_event_queue -> app_event_task -> led_service + beep_service + display_service");
    ESP_LOGI(TAG, "Network flow: wifi_service -> display_service + log -> future http/ota/ai");

    while (1) {
        // 主循环里不直接写业务逻辑，而是只负责推进各个服务层的周期处理。
        led_service_process();
        beep_service_process();
        button_service_process();
        display_service_process();
        wifi_service_process();
        vTaskDelay(pdMS_TO_TICKS(APP_LED_SERVICE_TASK_PERIOD_MS));
    }
}

/**
 * @brief 创建应用主任务
 */
void app_main_task_start(void)
{
    BaseType_t ret = xTaskCreate(
        app_main_task,
        APP_MAIN_TASK_NAME,
        APP_MAIN_TASK_STACK_SIZE,
        NULL,
        APP_MAIN_TASK_PRIORITY,
        NULL
    );

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create app_main_task");
    } else {
        ESP_LOGI(TAG, "app_main_task created successfully");
    }
}
