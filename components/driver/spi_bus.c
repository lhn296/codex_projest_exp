#include "spi_bus.h"

#include <string.h>

#include "esp_log.h"

static const char *TAG = "SPI_BUS";
static bool s_spi_bus_inited = false;
static spi_host_device_t s_spi_host = SPI2_HOST;

/**
 * @brief 初始化 SPI 总线模板
 *
 * 这一层只关心“SPI 总线怎么建立”，不关心具体接的是 LCD 还是其他外设。
 */
esp_err_t spi_bus_init(const spi_bus_config_ex_t *config)
{
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_spi_bus_inited) {
        return ESP_OK;
    }

    // 把项目层的简化配置翻译成 ESP-IDF 原生 SPI bus 配置结构。
    spi_bus_config_t buscfg = {
        .mosi_io_num = config->mosi_io_num,
        .miso_io_num = config->miso_io_num,
        .sclk_io_num = config->sclk_io_num,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = config->max_transfer_sz,
    };

    // 总线只初始化一次，后面不同 SPI 设备共用这条 bus。
    esp_err_t ret = spi_bus_initialize(config->host, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "spi_bus_initialize failed, ret=0x%x", ret);
        return ret;
    }

    s_spi_host = config->host;
    s_spi_bus_inited = true;
    ESP_LOGI(TAG, "SPI bus ready, host=%d mosi=%d miso=%d sclk=%d max_transfer=%d",
             config->host,
             config->mosi_io_num,
             config->miso_io_num,
             config->sclk_io_num,
             config->max_transfer_sz);
    return ESP_OK;
}

/**
 * @brief 判断 SPI 总线是否已可用
 */
bool spi_bus_is_ready(void)
{
    return s_spi_bus_inited;
}

/**
 * @brief 在已经初始化好的 SPI bus 上注册一个设备
 *
 * 这一步相当于给某个具体外设建立一个 handle，后面收发数据都围绕这个 handle 进行。
 */
esp_err_t spi_bus_register_device(const spi_bus_device_config_ex_t *config, spi_device_handle_t *out_handle)
{
    if (!s_spi_bus_inited || config == NULL || out_handle == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    // 设备配置和 bus 配置分离，方便一条总线挂多个设备。
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = config->clock_speed_hz,
        .mode = config->mode,
        .spics_io_num = config->cs_io_num,
        .queue_size = config->queue_size,
        .command_bits = config->command_bits,
        .address_bits = config->address_bits,
    };

    // 这里调用的是 ESP-IDF 原生的 spi_bus_add_device()，不是我们自己的包装层。
    esp_err_t ret = spi_bus_add_device(s_spi_host, &devcfg, out_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "spi_bus_add_device failed, ret=0x%x", ret);
        return ret;
    }

    ESP_LOGI(TAG, "SPI device added, cs=%d clock=%d",
             config->cs_io_num,
             config->clock_speed_hz);
    return ESP_OK;
}

/**
 * @brief 发送一段原始 SPI 数据
 *
 * 这一层不区分“命令”还是“数据”，只负责把字节送出去。
 */
esp_err_t spi_bus_transmit(spi_device_handle_t handle, const void *tx_buffer, size_t tx_bytes)
{
    if (handle == NULL || (tx_buffer == NULL && tx_bytes > 0)) {
        return ESP_ERR_INVALID_ARG;
    }

    // 事务结构每次按“发多少字节”临时组装，保持接口简单直观。
    spi_transaction_t trans = {0};
    trans.length = tx_bytes * 8;
    trans.tx_buffer = tx_buffer;
    return spi_device_transmit(handle, &trans);
}

/**
 * @brief 发送一个 SPI 命令字节
 */
esp_err_t spi_bus_transmit_cmd(spi_device_handle_t handle, uint8_t cmd)
{
    return spi_bus_transmit(handle, &cmd, sizeof(cmd));
}

/**
 * @brief 发送一段 SPI 数据区
 */
esp_err_t spi_bus_transmit_data(spi_device_handle_t handle, const void *data, size_t data_bytes)
{
    return spi_bus_transmit(handle, data, data_bytes);
}
