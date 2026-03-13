#ifndef APP_TYPES_H
#define APP_TYPES_H

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


#ifdef __cplusplus
}
#endif

#endif // APP_TYPES_H