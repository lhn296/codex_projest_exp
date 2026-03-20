# ESP32 Config Template

基于 `ESP-IDF 5.5.3` 的 ESP32-S3 学习工程，用来完成 `I2C + XL9555 + SPI LCD + Wi-Fi + HTTP + JSON + OTA + NVS 配置化` 的板载交互、显示、联网、云端版本检查、真实 OTA 升级与运行时配置管理模板练习。

## 项目概览

- 工程名：`codex_project_tep`
- 显示名称：`ESP32 Config Template`
- 当前版本：`v2.0.0`
- 目标芯片：`ESP32-S3`
- 当前阶段：`v2.0.0 Device Config Foundation`

当前行为：

- 使用 `I2C0` 驱动 `XL9555`
- 使用 `XL9555` 读取 `KEY0 ~ KEY3` 板载按键输入
- 使用 `XL9555 INT -> ESP32 GPIO39` 触发按键处理
- 使用 `SPI` 驱动 `ST7789V` LCD，并显示首页状态
- 主循环统一推进 `LED / BEEP / BTN / DISPLAY / WIFI / HTTP / OTA / CONFIG_CLI`
- 已具备真实 `Wi-Fi -> HTTP -> JSON -> OTA` 升级主链
- 已切到 `Two OTA Large` 分区方案
- 新增 `config_service`，用于统一维护运行时配置
- 新增 `config_cli_service`，允许在 `idf.py monitor` 中通过 `cfg ...` 命令修改配置
- 关键联网参数已经改成从运行时配置读取，而不是直接写死在业务服务里
- 已增加坏 URL 基础校验与 NVS 异常值回退，避免错误地址刷屏

默认 LED 模式：

- `SYS`：`LED_MODE_BLINK_SLOW`
- `NET`：`LED_MODE_BLINK_FAST`
- `ERR`：`LED_MODE_OFF`

## 工程结构

- `main/`：程序入口，只负责打印启动信息并启动应用任务
- `components/app/`：应用编排层，负责初始化服务和主循环调度
- `components/services/`：业务服务层，负责 LED、蜂鸣器、显示、Wi-Fi、HTTP、OTA、配置与串口配置入口
- `components/bsp/`：板级支持层，负责 GPIO、I2C、XL9555 与硬件读写
- `components/driver/`：通用驱动层，负责 `i2c_bus`、`spi_bus`、`xl9555`、`lcd_st7789v`
- `components/system/`：系统配置与公共类型定义
- `docs/`：补充说明文档、模板文档、发布笔记与调试记录

## 硬件连接

本工程当前主要基于 `DNESP32S3` 开发板和板载 `XL9555` 输入链路。

### LED

| 名称 | GPIO | 点亮有效电平 | 默认上电状态 | 默认模式 |
| --- | --- | --- | --- | --- |
| 系统状态灯 `SYS` | `GPIO16` | 低电平有效 | 点亮 | 慢闪 |
| 网络状态灯 `NET` | `GPIO19` | 高电平有效 | 点亮 | 快闪 |
| 错误状态灯 `ERR` | `GPIO36` | 高电平有效 | 点亮 | 关闭 |

### XL9555 / I2C

| 项目 | 配置 |
| --- | --- |
| `I2C 地址` | `0x20` |
| `SDA` | `GPIO41` |
| `SCL` | `GPIO42` |
| `INT` | `GPIO39` |
| `INT 上拉` | 板级/外部上拉 |

### LCD / SPI

| 项目 | 配置 |
| --- | --- |
| LCD 驱动 | `ST7789V` |
| 分辨率 | `320 x 240` |
| `RST` | `XL9555 IO1_2` |
| `PWR` | `XL9555 IO1_3` |
| `MOSI` | `GPIO11` |
| `DC/WR` | `GPIO13` |
| `SCLK` | `GPIO12` |
| `CS` | `GPIO21` |

### 板载按键

| 名称 | XL9555 引脚 | 有效电平 | 当前映射 |
| --- | --- | --- | --- |
| `KEY0` | `IO1_7` | 低电平有效 | `BTN_SYS` |
| `KEY1` | `IO1_6` | 低电平有效 | `BTN_NET` |
| `KEY2` | `IO1_5` | 低电平有效 | `BTN_ERR` |
| `KEY3` | `IO1_4` | 低电平有效 | `BTN_FUNC` |

### 其他板载控制口

| 资源 | XL9555 引脚 |
| --- | --- |
| `BEEP` | `IO0_3` |
| `LCD_CTRL0` | `IO1_3` |
| `LCD_CTRL1` | `IO1_2` |

## 当前配置体系

本版开始把项目里的关键联网参数拆成两层：

### 1. `app_config.h`

负责：

- 提供默认值
- 提供编译期常量

### 2. `config_service`

负责：

- 运行时配置缓存
- 从 NVS 加载保存值
- 保存当前配置
- 恢复默认值

当前纳入运行时配置的字段：

- `wifi_ssid`
- `wifi_password`
- `http_test_url`
- `ota_version_url`

## 串口配置入口

本版新增 `config_cli_service`，可以在 `idf.py monitor` 中直接输入：

```text
cfg help
cfg show
cfg set wifi <ssid> <password>
cfg set http <url>
cfg set ota <url>
cfg save
cfg load
cfg reset
cfg reboot
```

当前行为规则：

- `cfg set ...`
  - 只修改当前运行时配置
- `cfg save`
  - 才真正写入 NVS
- `cfg reboot`
  - 重启后重新从 NVS 加载

## 当前说明

- 当前默认保持 `APP_OTA_AUTO_UPGRADE = 0`
- 当前设备配置化已经完成第一轮基础验证
- 当前 OTA 版本比较已改成按 `vX.Y.Z` 数字比较
- 当前串口配置入口已经可用，并已验证 `show / set / save / reboot`

## 开发与验证

进入工程目录后，使用以下命令：

```powershell
idf.py build
idf.py -p COM3 flash
idf.py -p COM3 monitor
```

串口日志中应看到：

- 统一的项目显示名称、版本号与学习阶段
- `config_service` 初始化成功
- `config self-test passed`
- `config self-test reset_to_default passed`
- `config self-test restore done`
- `config cli ready, type 'cfg help' in monitor`
- `wifi/http/ota` 从运行时配置读取参数

建议验证：

1. 输入 `cfg show`，确认当前配置正确显示
2. 输入 `cfg set http https://httpbin.org/json`
3. 输入 `cfg save`
4. 输入 `cfg reboot`
5. 重启后再次 `cfg show`，确认配置仍然保留
6. 如果输入明显不合理的 URL，确认系统会拒绝或自动回退默认值，不再刷屏

## 发布与维护

- 版本变更统一记录在 `CHANGELOG.md`
- 发布前先执行 `git status`，避免把日志文件或临时文件一起提交
- 推荐只暂存本次需要提交的文件，不使用 `git add .`

常用发布命令：

```powershell
git status
git add CHANGELOG.md README.md components/system/app_config.h
git commit -m "chore: release vX.Y.Z"
git tag -a vX.Y.Z -m "Release vX.Y.Z"
git push origin main
git push origin vX.Y.Z
```
