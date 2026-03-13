# Bug 调试日记

## 基本信息

- 工程名称：`Project_2_1_0_BUTTON`
- 平台：`ESP32-S3`
- 框架：`ESP-IDF v5.5.3`
- 工具链：`xtensa-esp-elf gcc 14.2.0_20251107`
- 调试日期：`2026-03-12`

## 问题现象

工程执行 `idf.py build` 时编译失败，最初日志中出现两类异常：

1. `bootloader` 配置阶段失败
2. `esp_lcd` 相关源码编译时触发编译器内部错误

后续在工程改名后，又暴露出组件 `CMakeLists.txt` 写法错误。

## 初始报错记录

### 1. 工程路径问题引发的 bootloader 配置失败

旧工程目录名包含空格：

`Project 2_1_1 BUTTON`

日志中出现：

```text
FileNotFoundError: [Errno 2] No such file or directory:
'.../Project 2_1_1 BUTTON/build/bootloader/component_requires.temp.cmake'
```

结论：

- Windows 下旧工程路径带空格
- `build` 目录中缓存了旧路径
- 导致 `bootloader` 子工程配置阶段读取临时文件失败

### 2. `esp_lcd` 编译器内部崩溃

日志位置：

- `after_cmake_fix.log`
- `build/log/idf_py_stdout_output_17356`

典型错误：

```text
FAILED: esp-idf/esp_lcd/CMakeFiles/__idf_esp_lcd.dir/rgb/esp_lcd_panel_rgb.c.obj
...
G:/ESP32_WORK/v5.5.3/esp-idf/components/esp_lcd/rgb/esp_lcd_panel_rgb.c:700:1:
internal compiler error: Segmentation fault
```

结论：

- 失败文件不在业务代码中
- 出错文件位于 ESP-IDF 自带组件 `esp_lcd`
- 触发点是工具链编译 `esp_lcd_panel_rgb.c` 时崩溃

## 第二阶段排查

工程改名为：

`Project_2_1_0_BUTTON`

重新编译后，路径问题不再是主因，新的首要错误变为组件 CMake 定义错误。

## 发现的工程内错误

### 1. `components/bsp/CMakeLists.txt`

原始问题：

```cmake
idf_component_register(
    SRCS "bsp_led.c",
         "bsp_button.c"
    INCLUDE_DIRS "."
    REQUIRES
    esp_driver_gpio
    log
    system
)
```

问题点：

- `"bsp_led.c",` 后面多了逗号
- CMake 会把带逗号内容当成异常参数

### 2. `components/services/CMakeLists.txt`

原始问题：

```cmake
idf_component_register(
    SRCS "led_service.c",
         "button_service.c"
    INCLUDE_DIRS "."
    REQUIRES
    bsp
    log
    system
    esp_timer
)
```

问题点：

- `"led_service.c",` 后面多了逗号

### 3. `components/system/CMakeLists.txt`

原始问题：

```cmake
idf_component_register(
    SRCS ""
    INCLUDE_DIRS "."
)
```

问题点：

- 空组件不应写 `SRCS ""`
- 只保留 `INCLUDE_DIRS "."` 即可

## 对应修复

### 修复后的 `components/bsp/CMakeLists.txt`

```cmake
idf_component_register(
    SRCS "bsp_led.c"
         "bsp_button.c"
    INCLUDE_DIRS "."
    REQUIRES
        esp_driver_gpio
        log
        system
)
```

### 修复后的 `components/services/CMakeLists.txt`

```cmake
idf_component_register(
    SRCS "led_service.c"
         "button_service.c"
    INCLUDE_DIRS "."
    REQUIRES
        bsp
        log
        system
        esp_timer
)
```

### 修复后的 `components/system/CMakeLists.txt`

```cmake
idf_component_register(
    INCLUDE_DIRS "."
)
```

## `esp_lcd` 排查结果

经过对 `build/project_description.json` 的反查，确认：

- 业务组件 `main`
- 业务组件 `app`
- 业务组件 `services`
- 业务组件 `bsp`
- 业务组件 `system`

均没有显式依赖 `esp_lcd`。

也就是说：

- `esp_lcd` 不是被某条业务依赖链拉进来的
- 它是因为工程没有限制组件集合，导致 ESP-IDF 默认构建了大量内置组件

## `esp_lcd` 的实际位置

`esp_lcd` 位于 ESP-IDF 自带组件目录：

`G:\ESP32_WORK\v5.5.3\esp-idf\components\esp_lcd`

实际崩溃文件：

`G:\ESP32_WORK\v5.5.3\esp-idf\components\esp_lcd\rgb\esp_lcd_panel_rgb.c`

## 最终解决方案

在工程顶层 `CMakeLists.txt` 中限制构建组件集合，只保留本工程实际需要的组件：

```cmake
set(COMPONENTS main app bsp services system)
```

修复后顶层文件关键内容如下：

```cmake
cmake_minimum_required(VERSION 3.16)

if(NOT DEFINED ENV{IDF_PATH})
    message(FATAL_ERROR "IDF_PATH is not set. Configure ESP-IDF and reopen the project from an ESP-IDF environment.")
endif()

set(COMPONENTS main app bsp services system)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(Project_2_1_0_button)
```

## 修复后的结果

执行：

```powershell
idf.py fullclean
idf.py build
```

结果：

- 编译通过
- `esp_lcd_panel_rgb.c` 不再参与构建
- 最终生成了可烧录文件

产物包括：

- `build/Project_2_1_0_button.bin`
- `build/Project_2_1_0_button.elf`
- `build/bootloader/bootloader.bin`
- `build/partition_table/partition-table.bin`

## 根因总结

本次问题一共包含三层：

1. 旧工程路径带空格，且残留旧 `build` 缓存，导致 `bootloader` 配置异常
2. 业务组件 `CMakeLists.txt` 写法错误，导致组件源文件解析失败
3. 工程未限制组件集合，默认把 `esp_lcd` 一并编译，进而触发工具链在 `esp_lcd_panel_rgb.c` 上崩溃

## 后续建议

1. 工程目录名避免空格
2. 改名或移动工程后先执行 `idf.py fullclean`
3. 顶层 `CMakeLists.txt` 明确设置 `COMPONENTS`
4. 小工程尽量只编译实际需要的组件，减少构建时间和工具链风险

## 当前状态

- 问题已定位
- 修复已完成
- 工程已成功编译
