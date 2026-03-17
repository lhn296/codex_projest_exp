#include "xl9555.h"

#include "i2c_bus.h"
#include "app_config.h"
#include "esp_log.h"

static const char *TAG = "XL9555";

#define XL9555_REG_INPUT_PORT0      0x00
#define XL9555_REG_INPUT_PORT1      0x01
#define XL9555_REG_OUTPUT_PORT0     0x02
#define XL9555_REG_OUTPUT_PORT1     0x03
#define XL9555_REG_POLARITY_PORT0   0x04
#define XL9555_REG_POLARITY_PORT1   0x05
#define XL9555_REG_CONFIG_PORT0     0x06
#define XL9555_REG_CONFIG_PORT1     0x07

// 这个上下文把“设备句柄 + 输出缓存 + 配置缓存”集中保存在一起，
// 这样后续做单 pin 读写时就不需要每次都先把整口状态重新读一遍。
typedef struct {
    uint16_t dev_addr;                         // 当前 XL9555 的 I2C 从设备地址。
    i2c_master_dev_handle_t dev_handle;        // ESP-IDF I2C 主机为该设备创建的句柄。
    uint8_t output_cache[XL9555_PORT_MAX];     // Port0/Port1 输出寄存器软件缓存。
    uint8_t config_cache[XL9555_PORT_MAX];     // Port0/Port1 方向配置寄存器软件缓存。
    bool inited;                               // 驱动层是否已经完成初始化。
} xl9555_ctx_t;

static xl9555_ctx_t s_xl9555 = {
    .dev_addr = 0,
    .dev_handle = NULL,
    .output_cache = {0},
    .config_cache = {0xFF, 0xFF},
    .inited = false,
};

static inline bool xl9555_is_valid_port(xl9555_port_t port)
{
    return (port >= XL9555_PORT_0) && (port < XL9555_PORT_MAX);
}

static inline bool xl9555_is_valid_pin(xl9555_pin_t pin)
{
    return (pin >= XL9555_PIN_IO0_0) && (pin < XL9555_PIN_MAX);
}

/* XL9555 的 pin 定义是从 0 开始连续的，IO0_0=0, IO0_1=1, ..., IO1_7=15。 */
static inline xl9555_port_t xl9555_get_port(xl9555_pin_t pin)
{
    return (pin < XL9555_PIN_IO1_0) ? XL9555_PORT_0 : XL9555_PORT_1;
}
// 通过 pin 定位到 port 后，还需要计算出这个 pin 在 port 内的 bit 位，才能正确读写寄存器。
static inline uint8_t xl9555_get_bit(xl9555_pin_t pin)
{
    return (uint8_t)(pin % 8); //获得 pin 在所属 port 内的 bit 位，范围是 0-7
}

// 通过 pin 定位到 port 和 bit 后，就可以构造出访问寄存器时需要的掩码了。
static inline uint8_t xl9555_get_mask(xl9555_pin_t pin)
{
    return (uint8_t)(1U << xl9555_get_bit(pin));
}

// 获得 pin 所属 port 的输入寄存器地址，方便后续读写操作。
static inline uint8_t xl9555_get_input_reg(xl9555_port_t port)
{
    return (port == XL9555_PORT_0) ? XL9555_REG_INPUT_PORT0 : XL9555_REG_INPUT_PORT1;
}

// 获得 pin 所属 port 的输出寄存器地址，方便后续读写操作。
static inline uint8_t xl9555_get_output_reg(xl9555_port_t port)
{
    return (port == XL9555_PORT_0) ? XL9555_REG_OUTPUT_PORT0 : XL9555_REG_OUTPUT_PORT1;
}

// 获得 pin 所属 port 的配置寄存器地址，方便后续读写操作。
static inline uint8_t xl9555_get_config_reg(xl9555_port_t port)
{
    return (port == XL9555_PORT_0) ? XL9555_REG_CONFIG_PORT0 : XL9555_REG_CONFIG_PORT1;
}

esp_err_t xl9555_init(uint16_t dev_addr)
{
    // 先保证总线存在，再去碰具体芯片。
    esp_err_t ret = i2c_bus_init();
    if (ret != ESP_OK) {
        return ret;
    }

    if (s_xl9555.inited && s_xl9555.dev_addr == dev_addr) {
        return ESP_OK;
    }

    ret = i2c_bus_probe(dev_addr);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "probe failed, addr=0x%02X ret=0x%x", dev_addr, ret);
        return ret;
    }

    ret = i2c_bus_add_device(dev_addr, &s_xl9555.dev_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "add device failed, addr=0x%02X ret=0x%x", dev_addr, ret);
        return ret;
    }

    // 初始化时先把芯片当前寄存器值读回来，作为软件缓存基线。
    // 这样后面 write_pin/set_bits/clear_bits 才知道“整口当前是什么状态”。
    ret = i2c_bus_read_reg(s_xl9555.dev_handle, XL9555_REG_OUTPUT_PORT0, &s_xl9555.output_cache[XL9555_PORT_0]);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = i2c_bus_read_reg(s_xl9555.dev_handle, XL9555_REG_OUTPUT_PORT1, &s_xl9555.output_cache[XL9555_PORT_1]);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = i2c_bus_read_reg(s_xl9555.dev_handle, XL9555_REG_CONFIG_PORT0, &s_xl9555.config_cache[XL9555_PORT_0]);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = i2c_bus_read_reg(s_xl9555.dev_handle, XL9555_REG_CONFIG_PORT1, &s_xl9555.config_cache[XL9555_PORT_1]);
    if (ret != ESP_OK) {
        return ret;
    }

    // 这版默认不做输入极性翻转，直接保持“芯片原始电平”语义。
    uint8_t normal_polarity = 0x00;
    ret = i2c_bus_write_reg(s_xl9555.dev_handle, XL9555_REG_POLARITY_PORT0, normal_polarity);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = i2c_bus_write_reg(s_xl9555.dev_handle, XL9555_REG_POLARITY_PORT1, normal_polarity);
    if (ret != ESP_OK) {
        return ret;
    }

    s_xl9555.dev_addr = dev_addr;
    s_xl9555.inited = true; 

    ESP_LOGI(TAG, "XL9555 ready, addr=0x%02X cfg0=0x%02X cfg1=0x%02X out0=0x%02X out1=0x%02X",
             dev_addr,
             s_xl9555.config_cache[XL9555_PORT_0],
             s_xl9555.config_cache[XL9555_PORT_1],
             s_xl9555.output_cache[XL9555_PORT_0],
             s_xl9555.output_cache[XL9555_PORT_1]);
    return ESP_OK;
}

// 这个函数对外暴露一个接口，让上层能检查驱动是否准备好，避免在驱动未就绪时就调用读写函数导致错误。
bool xl9555_is_ready(void)
{
    return s_xl9555.inited;
}

/* 设置端口方向 */
esp_err_t xl9555_set_port_direction(xl9555_port_t port, uint8_t dir_mask)
{
    if (!s_xl9555.inited || !xl9555_is_valid_port(port)) {
        return ESP_ERR_INVALID_STATE;
    }

    // XL9555 的方向寄存器是按“整口 8 位”配置的，
    // 所以哪怕只改一个 pin，本质上也是改 port 的方向掩码。
    esp_err_t ret = i2c_bus_write_reg(s_xl9555.dev_handle, xl9555_get_config_reg(port), dir_mask);
    if (ret == ESP_OK) {
        s_xl9555.config_cache[port] = dir_mask;
    }
    return ret;
}

/* 设置引脚模式 */
esp_err_t xl9555_set_pin_mode(xl9555_pin_t pin, xl9555_pin_mode_t mode)
{
    if (!s_xl9555.inited || !xl9555_is_valid_pin(pin)) {
        return ESP_ERR_INVALID_STATE;
    }

    // 单 pin 模式设置，实际做法还是：
    // 1. 先找到它所在的 port
    // 2. 修改对应 bit
    // 3. 再把新的整口方向写回去
    xl9555_port_t port = xl9555_get_port(pin);
    uint8_t mask = xl9555_get_mask(pin);
    uint8_t dir_mask = s_xl9555.config_cache[port];

    if (mode == XL9555_PIN_MODE_INPUT) {
        dir_mask |= mask;
    } else {
        dir_mask &= (uint8_t)~mask;
    }

    return xl9555_set_port_direction(port, dir_mask);
}

/* 读写函数的实现都基于“先读整口状态，再按位修改”的思路，方便理解和维护。 */
esp_err_t xl9555_read_port(xl9555_port_t port, uint8_t *value)
{
    if (!s_xl9555.inited || !xl9555_is_valid_port(port) || value == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // 输入寄存器读取的是芯片当前实时输入状态，适合按键、INT 状态等场景。
    return i2c_bus_read_reg(s_xl9555.dev_handle, xl9555_get_input_reg(port), value);
}

esp_err_t xl9555_write_port(xl9555_port_t port, uint8_t value)
{
    if (!s_xl9555.inited || !xl9555_is_valid_port(port)) {
        return ESP_ERR_INVALID_ARG;
    }

    // 输出寄存器写入后同步刷新缓存，后续单 pin 修改就能基于最新软件镜像继续操作。
    esp_err_t ret = i2c_bus_write_reg(s_xl9555.dev_handle, xl9555_get_output_reg(port), value);
    if (ret == ESP_OK) {
        s_xl9555.output_cache[port] = value;
    }
    return ret;
}

esp_err_t xl9555_read_pin(xl9555_pin_t pin, bool *level)
{
    if (level == NULL || !xl9555_is_valid_pin(pin)) {
        return ESP_ERR_INVALID_ARG;
    }

    // 读单 pin 的本质仍然是“先读整口，再按位提取”。
    uint8_t port_value = 0;
    esp_err_t ret = xl9555_read_port(xl9555_get_port(pin), &port_value);
    if (ret != ESP_OK) {
        return ret;
    }

    *level = (port_value & xl9555_get_mask(pin)) != 0; // 根据掩码提取对应 bit 的电平状态，非 0 就是高电平，0 就是低电平。
    return ESP_OK;
}

esp_err_t xl9555_write_pin(xl9555_pin_t pin, bool level)
{
    if (!xl9555_is_valid_pin(pin)) {
        return ESP_ERR_INVALID_ARG;
    }

    // 写单 pin 时不能直接只写 1 位，因为芯片寄存器是按整口写入的。
    // 所以这里先基于缓存改 bit，再整体写回。
    xl9555_port_t port = xl9555_get_port(pin);
    uint8_t value = s_xl9555.output_cache[port];
    uint8_t mask = xl9555_get_mask(pin);

    if (level) {
        value |= mask;
    } else {
        value &= (uint8_t)~mask;
    }

    return xl9555_write_port(port, value);
}

esp_err_t xl9555_set_bits(xl9555_port_t port, uint8_t mask)
{
    if (!xl9555_is_valid_port(port)) {
        return ESP_ERR_INVALID_ARG;
    }

    // 适合一次性把同一口上的多个输出位置 1。
    return xl9555_write_port(port, (uint8_t)(s_xl9555.output_cache[port] | mask));
}

esp_err_t xl9555_clear_bits(xl9555_port_t port, uint8_t mask)
{
    if (!xl9555_is_valid_port(port)) {
        return ESP_ERR_INVALID_ARG;
    }

    // 适合一次性把同一口上的多个输出清 0。
    return xl9555_write_port(port, (uint8_t)(s_xl9555.output_cache[port] & (uint8_t)~mask));
}

esp_err_t xl9555_toggle_pin(xl9555_pin_t pin)
{
    // toggle 走“先读后写”的最直观路径，方便教学理解；
    // 如果后面追求更高效率，也可以直接基于 output_cache 翻转。
    bool level = false;
    esp_err_t ret = xl9555_read_pin(pin, &level);
    if (ret != ESP_OK) {
        return ret;
    }

    return xl9555_write_pin(pin, !level);
}
