# ESP32 Button to Multi LED Template

基于 `ESP-IDF 5.5` 的 ESP32-S3 模板工程，用来演示并复用“按键控制多路 LED 模式切换”的基础架构。

## 项目概览

- 工程名：`codex_project_tep`
- 显示名称：`ESP32 Button to Multi LED Template`
- 当前版本：`v1.0.0`
- 目标芯片：`ESP32-S3`

当前行为：

- 主循环周期扫描按键并驱动 LED 服务
- 每个按键对应一个 LED
- 按下按键后，该 LED 在以下模式间循环切换：
  `OFF -> ON -> BLINK_SLOW -> BLINK_FAST -> OFF`

默认 LED 模式：

- `SYS`：`LED_MODE_BLINK_SLOW`
- `NET`：`LED_MODE_BLINK_FAST`
- `ERR`：`LED_MODE_OFF`

## 工程结构

- `main/`：程序入口，只负责打印启动信息并启动应用任务
- `components/app/`：应用编排层，负责初始化服务和主循环调度
- `components/services/`：业务服务层，负责 LED 模式控制和按键事件处理
- `components/bsp/`：板级支持层，负责 GPIO 与硬件读写
- `components/system/`：系统配置与公共类型定义
- `docs/`：补充说明文档和发布笔记

## 硬件连接

### LED

| 名称 | GPIO | 点亮有效电平 | 默认上电状态 | 默认模式 |
| --- | --- | --- | --- | --- |
| 系统状态灯 `SYS` | `GPIO1` | 低电平有效 | 点亮 | 慢闪 |
| 网络状态灯 `NET` | `GPIO19` | 高电平有效 | 点亮 | 快闪 |
| 错误状态灯 `ERR` | `GPIO36` | 高电平有效 | 点亮 | 关闭 |

### 按键

| 名称 | GPIO | 输入配置 | 按下判定 |
| --- | --- | --- | --- |
| `BTN_SYS` | `GPIO0` | 内部上拉 | 低电平按下 |
| `BTN_NET` | `GPIO7` | 内部上拉 | 低电平按下 |
| `BTN_ERR` | `GPIO16` | 内部上拉 | 低电平按下 |

说明：

- `GPIO0` 常用于启动相关功能，接线前请结合开发板原理图确认不会影响下载或启动。
- 如果硬件连接变化，优先修改 `components/system/app_config.h` 中的 GPIO 和默认模式配置。

## 关键配置入口

以下模板级参数已经集中在 `components/system/app_config.h`：

- 项目名、显示名、版本号、目标芯片
- 任务名、任务栈大小、任务优先级、主循环周期
- 三路 LED 的 GPIO、有效电平、默认上电状态、默认模式
- 三个按键的 GPIO、按下有效电平、按钮数量
- LED 快闪和慢闪周期

## 开发与验证

进入工程目录后，使用以下命令：

```powershell
idf.py build
idf.py -p COM3 flash
idf.py -p COM3 monitor
```

串口日志中应看到：

- 统一的项目显示名称与版本号
- `app_main_task` 创建成功
- LED 服务与按键服务初始化成功
- 默认 LED 模式已应用

上板验证建议：

1. 上电后观察 `SYS` 慢闪、`NET` 快闪、`ERR` 熄灭。
2. 依次按下三个按键，确认各自对应 LED 在四种模式间循环。
3. 观察串口日志，确认模块标签和项目版本信息一致。

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
