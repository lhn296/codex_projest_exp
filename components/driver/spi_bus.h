#ifndef SPI_BUS_H
#define SPI_BUS_H

#include <stdbool.h>
#include <stdint.h>

#include "driver/spi_master.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    spi_host_device_t host;  // 使用哪一路 SPI Host，例如 SPI2_HOST。
    int mosi_io_num;         // SPI MOSI 引脚编号。
    int miso_io_num;         // SPI MISO 引脚编号，不需要可设为 -1。
    int sclk_io_num;         // SPI SCLK 时钟引脚编号。
    int max_transfer_sz;     // 单次最大传输字节数，影响 DMA 和大块刷屏能力。
} spi_bus_config_ex_t;

typedef struct {
    int cs_io_num;           // 设备片选引脚编号。
    int clock_speed_hz;      // 当前 SPI 设备使用的时钟频率。
    int mode;                // SPI 模式，常见为 0~3。
    int queue_size;          // SPI 事务队列深度。
    int command_bits;        // 命令字段位宽，当前项目多用于 8bit 命令。
    int address_bits;        // 地址字段位宽，不需要时可设为 0。
} spi_bus_device_config_ex_t;

esp_err_t spi_bus_init(const spi_bus_config_ex_t *config);
bool spi_bus_is_ready(void);
esp_err_t spi_bus_register_device(const spi_bus_device_config_ex_t *config, spi_device_handle_t *out_handle);
esp_err_t spi_bus_transmit(spi_device_handle_t handle, const void *tx_buffer, size_t tx_bytes);
esp_err_t spi_bus_transmit_cmd(spi_device_handle_t handle, uint8_t cmd);
esp_err_t spi_bus_transmit_data(spi_device_handle_t handle, const void *data, size_t data_bytes);

#ifdef __cplusplus
}
#endif

#endif
