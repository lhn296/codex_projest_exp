# ESP32 Wi-Fi Network Template

基于 `ESP-IDF 5.5.3` 的 ESP32-S3 学习工程，用来完成 `I2C + XL9555 + SPI LCD + Wi-Fi` 的板载交互、显示与联网模板练习，并继续复用现有统一事件架构。

## 项目概览

- 工程名：`codex_project_tep`
- 显示名称：`ESP32 Wi-Fi Network Template`
- 当前版本：`v1.5.0`
- 目标芯片：`ESP32-S3`
- 当前阶段：`v1.5.0 Wi-Fi Network Foundation`

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
- 主循环负责按键状态机和 LED 周期服务
- `KEY0 ~ KEY2` 映射到三路 LED 业务
- `KEY3` 作为板载功能键
- 新增 `beep_service`，支持基础蜂鸣反馈和测试模式
- 新增 LCD 首页显示版本、阶段、LED 状态、蜂鸣器状态和最近按键事件
- 首页显示服务已升级为分区布局，支持局部区域刷新
- 新增 `wifi_service`，用于 Wi-Fi 初始化、联网状态管理和 IP 获取
- 新增 LCD 首页显示 `Wi-Fi` 状态和 `IP` 信息
- 支持按键消抖、单击、长按、双击三种手势
- 按键服务不再直接修改 LED，而是通过队列发送事件
- 增加统一事件日志和基础发送/接收统计
- 新增可复用的 `i2c_bus` 通用访问层
- 新增可复用的 `xl9555` 驱动层
- 新增 `bsp_xl9555` 板级适配层，预留蜂鸣器与 LCD 控制接口
- 单击后，该 LED 在以下模式间循环切换：
  `OFF -> ON -> BLINK_SLOW -> BLINK_FAST -> OFF`
- 长按后，对应 LED 无论当前状态如何都直接关闭
- 双击后，对应 LED 无论当前状态如何都直接进入快速闪烁

默认 LED 模式：

- `SYS`：`LED_MODE_BLINK_SLOW`
- `NET`：`LED_MODE_BLINK_FAST`
- `ERR`：`LED_MODE_OFF`

## 工程结构

- `main/`：程序入口，只负责打印启动信息并启动应用任务
- `components/app/`：应用编排层，负责初始化服务和主循环调度
- `components/services/`：业务服务层，负责 LED、蜂鸣器、显示和 Wi-Fi 状态管理
- `components/bsp/`：板级支持层，负责 GPIO、I2C、XL9555 与硬件读写
- `components/system/`：系统配置与公共类型定义
- `docs/`：补充说明文档和发布笔记

## 硬件连接

本版本开始切到 `DNESP32S3` 开发板的板载 `XL9555` 输入链路。

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
- 如果板级接法变化，优先修改 `components/system/app_config.h` 中的 `I2C / XL9555` 配置。
- 如果 LCD 模块初始化参数需要微调，优先查看 `components/driver/lcd_st7789v.c` 和 `components/bsp/bsp_lcd.c`。
- 如果外部 LED 使用方式不同，优先修改 `components/system/app_config.h` 中的 LED GPIO 和有效电平配置。

按键时序参数同样集中在 `components/system/app_config.h`：

- 消抖时间：`APP_BUTTON_DEBOUNCE_MS`
- 长按判定时间：`APP_BUTTON_LONG_PRESS_MS`
- 双击判定窗口：`APP_BUTTON_DOUBLE_CLICK_MS`
- 蜂鸣器短响时间：`APP_BEEP_SHORT_ON_MS`
- 蜂鸣器长响时间：`APP_BEEP_LONG_ON_MS`
- `SPI` 总线、LCD 分辨率、LCD 控制引脚配置
- `Wi-Fi` 的 SSID、密码、重试次数和连接超时配置

## 关键配置入口

以下模板级参数已经集中在 `components/system/app_config.h`：

- 项目名、显示名、版本号、目标芯片
- 任务名、任务栈大小、任务优先级、主循环周期
- 三路 LED 的 GPIO、有效电平、默认上电状态、默认模式
- `I2C` 总线配置、`XL9555` 地址、板级引脚映射
- LED 快闪和慢闪周期
- `Wi-Fi STA` 配置与联网重试参数

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
- LED 服务、蜂鸣器服务、显示服务、Wi-Fi 服务与按键服务初始化成功
- 当前 `I2C / XL9555` 映射打印完成
- 当前 `Wi-Fi` 配置和联网状态打印完成
- 按键服务打印消抖、长按、双击配置时间
- 事件任务日志打印统一事件接收、处理和 LED 模式变化
- 按键初始化日志打印 `source=XL9555`
- 默认 LED 模式已应用

上板验证建议：

1. 上电后观察 `SYS` 慢闪、`NET` 快闪、`ERR` 熄灭。
2. 单击 `KEY0 ~ KEY2`，确认对应 LED 在四种模式间循环，并伴随蜂鸣提示。
3. 长按 `KEY0 ~ KEY2` 约 `0.8s`，确认对应 LED 无论当前状态如何都直接关闭。
4. 快速双击 `KEY0 ~ KEY2`，确认对应 LED 无论当前状态如何都直接进入快闪。
5. 单击 `KEY3`，确认蜂鸣器提示使能状态切换。
6. 双击 `KEY3`，确认蜂鸣器测试模式切换。
7. 观察 LCD 首页，确认版本号、阶段名、LED 状态、蜂鸣器状态、Wi-Fi 状态、IP 信息和最近事件会刷新。
8. 连续操作不同按键时，观察页面局部区域刷新是否正常，不再每次整页重绘。
9. 如果 `SSID` 和密码配置正确，观察串口日志和 LCD，确认能看到 `CONNECTING -> CONNECTED -> GOT_IP` 的状态变化。
10. 观察串口日志，确认能看到按键名称、手势类型、LED / 蜂鸣器 / 显示 / Wi-Fi 业务处理过程。

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
