#ifndef I2C_BUS_H
#define I2C_BUS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "driver/i2c_master.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* I2C 总线层只关心“如何访问设备”，不关心具体是哪颗芯片。 */
esp_err_t i2c_bus_init(void);
bool i2c_bus_is_ready(void);
/* 探测某个地址是否有设备响应，适合做上电自检。 */
esp_err_t i2c_bus_probe(uint16_t dev_addr);
/* 为某个设备地址创建驱动句柄，后续收发都通过这个 handle 完成。 */
esp_err_t i2c_bus_add_device(uint16_t dev_addr, i2c_master_dev_handle_t *out_dev_handle);
esp_err_t i2c_bus_read_reg(i2c_master_dev_handle_t dev_handle, uint8_t reg_addr, uint8_t *value);
esp_err_t i2c_bus_write_reg(i2c_master_dev_handle_t dev_handle, uint8_t reg_addr, uint8_t value);
esp_err_t i2c_bus_read_regs(i2c_master_dev_handle_t dev_handle, uint8_t reg_addr, uint8_t *buffer, size_t len);
esp_err_t i2c_bus_write_regs(i2c_master_dev_handle_t dev_handle, uint8_t reg_addr, const uint8_t *buffer, size_t len);

#ifdef __cplusplus
}
#endif

#endif
