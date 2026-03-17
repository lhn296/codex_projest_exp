#ifndef APP_TYPES_H
#define APP_TYPES_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief LED 工作模式
 */
typedef enum {
    LED_MODE_OFF = 0,         /*!< 常灭 */
    LED_MODE_ON,              /*!< 常亮 */
    LED_MODE_BLINK_SLOW,      /*!< 慢闪 */
    LED_MODE_BLINK_FAST,      /*!< 快闪 */
    LED_MODE_MAX
} led_mode_t;

/**
 * @brief LED 编号定义
 */
typedef enum {
    LED_ID_SYS = 0,           /*!< 系统状态灯 */
    LED_ID_NET,               /*!< 网络状态灯 */
    LED_ID_ERR,               /*!< 错误状态灯 */
    LED_ID_MAX
} led_id_t;

/**
 * @brief 按钮编号定义
 */
typedef enum {
    BTN_SYS = 0,              /*!< 系统功能对应按键 */
    BTN_NET,                  /*!< 网络功能对应按键 */
    BTN_ERR,                  /*!< 错误/提示功能对应按键 */
    BTN_FUNC,                 /*!< 功能扩展按键 */
    BTN_MAX
} button_id_t;

/**
 * @brief 按钮事件类型定义
 */
typedef enum {
    BUTTON_EVENT_NONE = 0,    /*!< 无事件 */
    BUTTON_EVENT_SHORT,       /*!< 单击事件 */
    BUTTON_EVENT_LONG,        /*!< 长按事件 */
    BUTTON_EVENT_DOUBLE       /*!< 双击事件 */
} button_event_t;

/**
 * @brief Wi-Fi 联网状态定义
 */
typedef enum {
    WIFI_STATE_IDLE = 0,      /*!< 空闲，尚未开始联网 */
    WIFI_STATE_CONNECTING,    /*!< 正在连接路由器 */
    WIFI_STATE_CONNECTED,     /*!< 已连上路由器，但还没拿到 IP */
    WIFI_STATE_GOT_IP,        /*!< 已拿到 IP，可继续访问网络服务 */
    WIFI_STATE_DISCONNECTED,  /*!< 已断开或连接失败 */
    WIFI_STATE_ERROR,         /*!< 初始化或运行过程中出现错误 */
} wifi_state_t;

/**
 * @brief 统一事件来源
 */
typedef enum {
    APP_EVENT_SOURCE_BUTTON = 0, /*!< 按键输入产生的事件 */
    APP_EVENT_SOURCE_SYSTEM,     /*!< 系统状态或内部流程产生的事件 */
    APP_EVENT_SOURCE_MAX
} app_event_source_t;

/**
 * @brief 统一事件类型
 */
typedef enum {
    APP_EVENT_TYPE_BUTTON = 0,   /*!< 按键事件类型 */
    APP_EVENT_TYPE_STATUS,       /*!< 状态同步或系统状态类型 */
    APP_EVENT_TYPE_MAX
} app_event_type_t;

/**
 * @brief 统一事件消息
 *
 * v1.2.1 开始，队列里传递的是统一事件格式，
 * 这样后续不止按键，其他模块事件也可以接入同一条消息链路。
 */
typedef struct {
    app_event_source_t source;   // 事件来源，用来区分是按键还是系统内部事件。
    app_event_type_t type;       // 事件类型，决定后面按什么方式解释参数。
    int32_t param1;              // 主参数，当前版本主要用于传递 button_id。
    int32_t param2;              // 次参数，当前版本主要用于传递 button_event。
    int64_t timestamp_ms;        // 事件发生时间戳，单位毫秒，便于日志和时序分析。
} app_event_msg_t;


#ifdef __cplusplus
}
#endif

#endif // APP_TYPES_H
