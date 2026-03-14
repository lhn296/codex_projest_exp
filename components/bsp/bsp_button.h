#ifndef BSP_BUTTON_H
#define BSP_BUTTON_H

#include <stdbool.h>
#include "esp_err.h"
#include "app_types.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t bsp_button_init(button_id_t btn_id);
bool bsp_button_read(button_id_t btn_id);
bool bsp_button_consume_press_irq(button_id_t btn_id);

#ifdef __cplusplus
}
#endif

#endif
