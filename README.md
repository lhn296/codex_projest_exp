# ESP32 Real OTA Template

基于 `ESP-IDF 5.5.3` 的 ESP32-S3 学习工程，用来完成 `I2C + XL9555 + SPI LCD + Wi-Fi + HTTP + JSON + OTA` 的板载交互、显示、联网、云端版本检查与真实 OTA 升级模板练习，并继续复用现有统一事件架构。

## 项目概览

- 工程名：`codex_project_tep`
- 显示名称：`ESP32 Real OTA Template`
- 当前版本：`v1.9.0`
- 目标芯片：`ESP32-S3`
- 当前阶段：`v1.9.0 Real OTA Upgrade`

当前行为：

- 使用 `I2C0` 驱动 `XL9555`
- 使用 `XL9555` 读取 `KEY0 ~ KEY3` 板载按键输入
- 使用 `XL9555 INT -> ESP32 GPIO39` 触发按键处理
- 新增 `SPI` 通用总线模板
- 新增 `lcd_st7789v` 显示驱动模板
- 新增 `bsp_lcd` 板级显示适配层
- 新增 `display_service` 显示服务层
- 引入 `FreeRTOS Queue` 传递按键事件
- 新增 `app_event_task` 处理业务事件
- 统一队列消息结构为通用事件格式
- 主循环负责按键状态机、LED、蜂鸣器、显示、Wi-Fi、HTTP 与 OTA 服务推进
- `KEY0 ~ KEY2` 映射到三路 LED 业务
- `KEY3` 作为板载功能键
- 新增 `beep_service`，支持基础蜂鸣反馈和测试模式
- 新增 LCD 首页显示版本、阶段、LED 状态、蜂鸣器状态、Wi-Fi 状态、IP 信息、HTTP 状态和 OTA 状态
- 首页显示服务已升级为分区布局，支持局部区域刷新
- 新增 `wifi_service`，用于 Wi-Fi 初始化、联网状态管理和 IP 获取
- 新增 `http_service`，用于基础 `HTTP GET` 请求、完整响应体缓存和 JSON 解析
- 新增 `ota_service` 云端版本检查链路，支持从真实版本接口获取 `version / url / message`
- 新增 HTTPS 证书包挂接，支持访问通用 HTTPS JSON 接口
- 当前已切到 `Two OTA Large` 分区方案，为真实 OTA 预留双 OTA 分区空间
- 新增真实 OTA 下载与写分区主链，支持 `esp_ota_begin -> esp_ota_write -> esp_ota_end -> esp_ota_set_boot_partition`
- OTA 状态新增 `VERIFY`
- 支持按键消抖、单击、长按、双击三种手势
- 新增可复用的 `i2c_bus`、`xl9555`、`spi_bus`、`lcd_st7789v`、`wifi_service`、`http_service`、`ota_service` 模板

默认 LED 模式：

- `SYS`：`LED_MODE_BLINK_SLOW`
- `NET`：`LED_MODE_BLINK_FAST`
- `ERR`：`LED_MODE_OFF`

## 工程结构

- `main/`：程序入口，只负责打印启动信息并启动应用任务
- `components/app/`：应用编排层，负责初始化服务和主循环调度
- `components/services/`：业务服务层，负责 LED、蜂鸣器、显示、Wi-Fi、HTTP 与 OTA 状态管理
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

说明：

- 板载按键通过 `XL9555` 读取，按下为低电平有效。
- `KEY3` 作为功能键，用于控制蜂鸣器提示使能和测试模式。
- 当前版本默认把 `GPIO13` 按 LCD 的 `DC/WR` 控制脚使用，而不是读回 `MISO`。
- 如果板级接法变化，优先修改 `components/system/app_config.h` 中的 `I2C / XL9555 / LCD / OTA` 配置。

## 关键配置入口

以下模板级参数已经集中在 `components/system/app_config.h`：

- 项目名、显示名、版本号、目标芯片
- 任务名、任务栈大小、任务优先级、主循环周期
- 三路 LED 的 GPIO、有效电平、默认上电状态、默认模式
- `I2C` 总线配置、`XL9555` 地址、板级引脚映射
- `SPI` 总线、LCD 分辨率、LCD 控制引脚配置
- `Wi-Fi STA` 配置与联网重试参数
- `HTTP` 的测试 URL、请求超时和自动请求配置
- `OTA` 的版本接口地址、自动检查开关、自动升级开关、写入缓冲大小

## 当前说明

- 当前默认配置仍保持 `APP_OTA_AUTO_UPGRADE = 0`，也就是上电后默认只做云端版本检查，不会直接开始真实升级。
- 当前版本已经具备真实 OTA 下载和写分区主链，但只有在云端 `url` 指向真实可访问的 `.bin` 固件地址时，真实升级才有意义。
- 真正上板验证完整 OTA 升级前，建议先准备一份可公网访问的真实 `.bin` 固件地址。

## 开发与验证

进入工程目录后，使用以下命令：

```powershell
idf.py build
idf.py -p COM3 flash
idf.py -p COM3 monitor
```

串口日志中应看到：

- 统一的项目显示名称、版本号与学习阶段
- `app_main_task` 创建成功
- LED、蜂鸣器、显示、Wi-Fi、HTTP、OTA 与按键服务初始化成功
- 当前 `I2C / XL9555`、LCD / SPI、Wi-Fi、HTTP、OTA 配置打印完成
- 当前 `HTTP` 请求结果和 JSON 解析摘要打印完成
- 当前 `OTA` 云端版本检查结果打印完成
- 当开启真实升级时，还会看到下载、写入、切换启动分区和重启流程日志

上板验证建议：

1. 上电后观察 `SYS` 慢闪、`NET` 快闪、`ERR` 熄灭。
2. 单击 `KEY0 ~ KEY2`，确认对应 LED 在四种模式间循环，并伴随蜂鸣提示。
3. 长按 `KEY0 ~ KEY2` 约 `0.8s`，确认对应 LED 无论当前状态如何都直接关闭。
4. 快速双击 `KEY0 ~ KEY2`，确认对应 LED 无论当前状态如何都直接进入快闪。
5. 单击 `KEY3`，确认蜂鸣器提示使能状态切换。
6. 双击 `KEY3`，确认蜂鸣器测试模式切换。
7. 观察 LCD 首页，确认版本号、阶段名、LED 状态、蜂鸣器状态、Wi-Fi 状态、HTTP 状态和 OTA 状态会刷新。
8. 如果 `SSID` 和密码配置正确，观察串口日志和 LCD，确认能看到 `CONNECTING -> CONNECTED -> GOT_IP` 的状态变化。
9. 观察设备是否自动发起一次 HTTP 请求，并在 LCD 上显示 `HTTP / CODE / MSG`。
10. 观察 OTA 区域，确认云端版本检查后能看到 `CHECK / READY / NO_UPDATE / FAIL`。
11. 如果后续开启 `APP_OTA_AUTO_UPGRADE = 1` 且云端 `url` 指向真实可下载固件，观察串口日志是否进入 `DOWNLOADING -> VERIFY -> SUCCESS -> REBOOTING`。

## 发布与维护

- 版本变更统一记录在 `CHANGELOG.md`
- 发布前先执行 `git status`，避免把日志文件或临时文件一起提交
- 推荐只暂存本次需要提交的文件，不使用 `git add .`

常用发布命令：

```powershell
git status
git add CHANGELOG.md README.md components/system/app_config.h
git commit -m "chore: align template metadata and docs"
git tag -a vX.Y.Z -m "Release vX.Y.Z"
git push origin main
git push origin vX.Y.Z
```
