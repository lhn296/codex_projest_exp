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
    spi_host_device_t host;
    int mosi_io_num;
    int miso_io_num;
    int sclk_io_num;
    int max_transfer_sz;
} spi_bus_config_ex_t;

typedef struct {
    int cs_io_num;
    int clock_speed_hz;
    int mode;
    int queue_size;
    int command_bits;
    int address_bits;
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
