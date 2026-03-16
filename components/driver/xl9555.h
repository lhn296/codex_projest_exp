#ifndef XL9555_H
#define XL9555_H

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* XL9555 层只关心芯片本身的端口、位和寄存器逻辑，不关心板子怎么接。 */
typedef enum {
    XL9555_PORT_0 = 0,
    XL9555_PORT_1,
    XL9555_PORT_MAX
} xl9555_port_t;

typedef enum {
    XL9555_PIN_IO0_0 = 0,
    XL9555_PIN_IO0_1,
    XL9555_PIN_IO0_2,
    XL9555_PIN_IO0_3,
    XL9555_PIN_IO0_4,
    XL9555_PIN_IO0_5,
    XL9555_PIN_IO0_6,
    XL9555_PIN_IO0_7,
    XL9555_PIN_IO1_0,
    XL9555_PIN_IO1_1,
    XL9555_PIN_IO1_2,
    XL9555_PIN_IO1_3,
    XL9555_PIN_IO1_4,
    XL9555_PIN_IO1_5,
    XL9555_PIN_IO1_6,
    XL9555_PIN_IO1_7,
    XL9555_PIN_MAX
} xl9555_pin_t;

typedef enum {
    XL9555_PIN_MODE_OUTPUT = 0,
    XL9555_PIN_MODE_INPUT,
} xl9555_pin_mode_t;

/* init 会完成设备探测、句柄建立、缓存同步和极性初始化。 */
esp_err_t xl9555_init(uint16_t dev_addr);
bool xl9555_is_ready(void);
esp_err_t xl9555_set_pin_mode(xl9555_pin_t pin, xl9555_pin_mode_t mode);
esp_err_t xl9555_set_port_direction(xl9555_port_t port, uint8_t dir_mask);
esp_err_t xl9555_read_pin(xl9555_pin_t pin, bool *level);
esp_err_t xl9555_write_pin(xl9555_pin_t pin, bool level);
esp_err_t xl9555_read_port(xl9555_port_t port, uint8_t *value);
esp_err_t xl9555_write_port(xl9555_port_t port, uint8_t value);
esp_err_t xl9555_set_bits(xl9555_port_t port, uint8_t mask);
esp_err_t xl9555_clear_bits(xl9555_port_t port, uint8_t mask);
esp_err_t xl9555_toggle_pin(xl9555_pin_t pin);

#ifdef __cplusplus
}
#endif

#endif
