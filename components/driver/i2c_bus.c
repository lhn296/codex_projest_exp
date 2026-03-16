#include "i2c_bus.h"

#include "app_config.h"
#include "esp_log.h"

static const char *TAG = "I2C_BUS";
static i2c_master_bus_handle_t s_bus_handle = NULL;

esp_err_t i2c_bus_init(void)
{
    if (s_bus_handle != NULL) {
        return ESP_OK;
    }

    i2c_master_bus_config_t bus_config = {
        .i2c_port = APP_I2C_MASTER_PORT,
        .sda_io_num = APP_I2C_MASTER_SDA_GPIO,
        .scl_io_num = APP_I2C_MASTER_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .intr_priority = 0,
        .trans_queue_depth = 0,
        .flags.enable_internal_pullup = 1,
    };

    esp_err_t ret = i2c_new_master_bus(&bus_config, &s_bus_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2c_new_master_bus failed, port=%d ret=0x%x",
                 APP_I2C_MASTER_PORT, ret);
        return ret;
    }

    ESP_LOGI(TAG, "I2C bus ready, port=%d sda=%d scl=%d speed=%lu",
             APP_I2C_MASTER_PORT,
             APP_I2C_MASTER_SDA_GPIO,
             APP_I2C_MASTER_SCL_GPIO,
             (unsigned long)APP_I2C_MASTER_FREQ_HZ);
    return ESP_OK;
}

bool i2c_bus_is_ready(void)
{
    return s_bus_handle != NULL;
}


esp_err_t i2c_bus_probe(uint16_t dev_addr)
{
    if (s_bus_handle == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    return i2c_master_probe(s_bus_handle, dev_addr, APP_I2C_XFER_TIMEOUT_MS);
}

esp_err_t i2c_bus_add_device(uint16_t dev_addr, i2c_master_dev_handle_t *out_dev_handle)
{
    if (s_bus_handle == NULL || out_dev_handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = dev_addr,
        .scl_speed_hz = APP_I2C_MASTER_FREQ_HZ,
        .scl_wait_us = 0,
        .flags.disable_ack_check = 0,
    };

    return i2c_master_bus_add_device(s_bus_handle, &dev_config, out_dev_handle);
}

esp_err_t i2c_bus_read_reg(i2c_master_dev_handle_t dev_handle, uint8_t reg_addr, uint8_t *value)
{
    return i2c_bus_read_regs(dev_handle, reg_addr, value, 1);
}

esp_err_t i2c_bus_write_reg(i2c_master_dev_handle_t dev_handle, uint8_t reg_addr, uint8_t value)
{
    return i2c_bus_write_regs(dev_handle, reg_addr, &value, 1);
}

esp_err_t i2c_bus_read_regs(i2c_master_dev_handle_t dev_handle, uint8_t reg_addr, uint8_t *buffer, size_t len)
{
    if (dev_handle == NULL || buffer == NULL || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    return i2c_master_transmit_receive(dev_handle,
                                       &reg_addr,
                                       1,
                                       buffer,
                                       len,
                                       APP_I2C_XFER_TIMEOUT_MS);
}

esp_err_t i2c_bus_write_regs(i2c_master_dev_handle_t dev_handle, uint8_t reg_addr, const uint8_t *buffer, size_t len)
{
    if (dev_handle == NULL || buffer == NULL || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t tx_buffer[1 + 16];
    if (len > 16) {
        return ESP_ERR_INVALID_SIZE;
    }

    tx_buffer[0] = reg_addr;
    for (size_t i = 0; i < len; i++) {
        tx_buffer[i + 1] = buffer[i];
    }

    return i2c_master_transmit(dev_handle,
                               tx_buffer,
                               len + 1,
                               APP_I2C_XFER_TIMEOUT_MS);
}
