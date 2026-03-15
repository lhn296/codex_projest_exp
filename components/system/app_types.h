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
    BTN_SYS = 0,
    BTN_NET,
    BTN_ERR,
    BTN_MAX
} button_id_t;

/**
 * @brief 按钮事件类型定义
 */
typedef enum {
    BUTTON_EVENT_NONE = 0,
    BUTTON_EVENT_SHORT,
    BUTTON_EVENT_LONG,
    BUTTON_EVENT_DOUBLE
} button_event_t;

/**
 * @brief 统一事件来源
 */
typedef enum {
    APP_EVENT_SOURCE_BUTTON = 0,
    APP_EVENT_SOURCE_SYSTEM,
    APP_EVENT_SOURCE_MAX
} app_event_source_t;

/**
 * @brief 统一事件类型
 */
typedef enum {
    APP_EVENT_TYPE_BUTTON = 0,
    APP_EVENT_TYPE_STATUS,
    APP_EVENT_TYPE_MAX
} app_event_type_t;

/**
 * @brief 统一事件消息
 *
 * v1.2.1 开始，队列里传递的是统一事件格式，
 * 这样后续不止按键，其他模块事件也可以接入同一条消息链路。
 */
typedef struct {
    app_event_source_t source;// 事件来源
    app_event_type_t type;// 事件类型
    int32_t param1;// 主参数，当前版本用于 button_id
    int32_t param2;// 次参数，当前版本用于 button_event
    int64_t timestamp_ms;// 事件发生的时间戳，单位毫秒
} app_event_msg_t;


#ifdef __cplusplus
}
#endif

#endif // APP_TYPES_H
