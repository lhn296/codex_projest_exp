# OTA服务模板文档

## 1. 文档定位

这份文档把当前项目里的 `ota_service` 整理成一份可复用模板，目标是：

- 后面换项目时可以直接照搬 OTA 主结构
- 能快速理解“版本检查”和“真实升级执行”之间的关系
- 直接复用到“真实 bin 下载 + OTA 分区写入 + 重启切换”的项目里

---

## 2. 模块职责

`ota_service` 这一层的职责是：

- 管理 OTA 高层状态
- 执行版本检查
- 判断是否存在新版本
- 缓存目标版本号和固件地址
- 把 OTA 状态同步给 LCD 和日志

它不负责：

- Wi-Fi 联网
- HTTP 底层请求
- LCD 具体绘制

也就是说，它只解决：

```text
设备怎么判断有没有升级、升级状态是什么
```

---

## 3. 推荐分层关系

建议在工程里保持这种关系：

```text
wifi_service
-> http_service
-> ota_service
-> display_service / log
```

这样 `ota_service` 就能保持清晰：

- 下层依赖 Wi‑Fi 和 HTTP
- 上层只关心有没有更新、当前 OTA 到哪一步

---

## 4. 当前模板的核心能力

当前这份 `ota_service` 模板已经具备：

- OTA 状态缓存
- 云端版本检查
- 当前版本与目标版本比较
- 目标版本号缓存
- 固件 URL 缓存
- LCD 状态联动
- 日志联动
- 自动检查一次的周期处理入口
- 真实固件 bin 下载
- OTA 分区写入
- 设置启动分区
- 重启切换固件

---

## 5. 关键接口

当前最核心的对外接口是：

- `ota_service_init()`
- `ota_service_process()`
- `ota_service_is_ready()`
- `ota_service_get_state()`
- `ota_service_get_message()`
- `ota_service_has_update()`

推荐理解方式：

- `init`
  - 准备 OTA 状态缓存
- `process`
  - 周期检查是否具备执行 OTA 检查的条件
- `get_xxx`
  - 提供给显示层和业务层查询当前 OTA 状态

---

## 5.1 推荐头文件模板

```c
#ifndef OTA_SERVICE_H
#define OTA_SERVICE_H

#include <stdbool.h>

#include "app_types.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// 初始化 OTA 服务，准备状态缓存和默认显示内容。
esp_err_t ota_service_init(void);

// OTA 周期处理入口，当前版本用于版本检查和状态推进。
void ota_service_process(void);

// 判断 OTA 服务是否已完成初始化。
bool ota_service_is_ready(void);

// 获取当前 OTA 状态。
ota_state_t ota_service_get_state(void);

// 获取当前 OTA 状态说明文本。
const char *ota_service_get_message(void);

// 判断当前是否检测到可升级版本。
bool ota_service_has_update(void);

#ifdef __cplusplus
}
#endif

#endif
```

---

## 6. 推荐状态结构体模板

建议保留一份上下文结构体，集中缓存 OTA 相关状态：

```c
typedef struct {
    bool inited;                      // OTA 服务是否已完成初始化。
    bool checked_once;                // 当前上电后是否已经做过一次自动检查。
    bool has_update;                  // 当前是否检测到可升级版本。
    ota_state_t state;                // 当前 OTA 高层状态。
    int64_t last_check_time_ms;       // 最近一次执行版本检查的时间戳。
    char target_version[24];          // 最近一次检测到的目标版本号。
    char firmware_url[160];           // 最近一次检测到的固件下载地址。
    char message[64];                 // 当前状态说明文本，供 LCD 和日志复用。
} ota_service_ctx_t;
```

这个结构体的作用是：

- 把 OTA 检查结果集中起来
- 后面接真实 OTA 下载链时可以继续沿用

---

## 7. OTA 状态模型

当前模板建议至少保留这些状态：

- `IDLE`
- `CHECK`
- `READY`
- `DOWNLOADING`
- `VERIFY`
- `SUCCESS`
- `FAIL`

推荐理解方式：

- `IDLE`
  - 默认空闲状态
- `CHECK`
  - 正在做版本检查
- `READY`
  - 发现可升级版本
- `DOWNLOADING`
  - 正在下载固件
- `VERIFY`
  - 固件写入完成后，正在做 OTA 结束校验和切换前处理
- `SUCCESS`
  - 升级流程成功
- `FAIL`
  - 失败

---

## 8. 推荐初始化代码模板

```c
esp_err_t ota_service_init(void)
{
    if (s_ota.inited) {
        // 已初始化过时直接返回，避免重复覆盖状态缓存。
        return ESP_OK;
    }

    // 默认先把目标版本设成当前版本，避免未检查前显示脏值。
    snprintf(s_ota.target_version, sizeof(s_ota.target_version), "%s", APP_PROJECT_VERSION);
    snprintf(s_ota.firmware_url, sizeof(s_ota.firmware_url), "%s", "");
    ota_service_set_state(OTA_STATE_IDLE, "IDLE");
    s_ota.inited = true;
    return ESP_OK;
}
```

这段初始化代码的关键点是：

- OTA 服务本身不直接初始化 Wi‑Fi 或 HTTP
- 它只负责准备自己的状态缓存

---

## 9. 推荐版本比较模板

当前学习阶段，推荐先用最简单的比较规则：

```c
static bool ota_service_compare_version(const char *current_version, const char *target_version)
{
    if (current_version == NULL || target_version == NULL) {
        return false;
    }

    // 当前先用“字符串不相等就认为有更新”的简单策略。
    return strcmp(current_version, target_version) != 0;
}
```

这样做的优点是：

- 先把 OTA 主链跑通
- 不会太早陷进语义版本比较细节里

后面如果需要，可以再扩展成：

- `v1.7.0 < v1.8.0`
- 补丁版本比较
- 预发布版本比较

---

## 10. 推荐版本检查模板

```c
static void ota_service_check_once(void)
{
    // 先记录检查时间，并把状态切到 CHECK。
    s_ota.last_check_time_ms = esp_timer_get_time() / 1000;
    ota_service_set_state(OTA_STATE_CHECK, "CHECK");

    // 通过云端版本接口拿到真实 JSON，再解析 version / url / message。
    esp_err_t ret = http_service_request(APP_OTA_VERSION_URL);
    if (ret != ESP_OK) {
        ota_service_set_state(OTA_STATE_FAIL, "HTTP_FAIL");
        return;
    }

    if (!ota_service_parse_version_json(http_service_get_response_body())) {
        ota_service_set_state(OTA_STATE_FAIL, "JSON_FAIL");
        return;
    }

    s_ota.has_update = ota_service_compare_version(APP_PROJECT_VERSION, s_ota.target_version);
    if (!s_ota.has_update) {
        // 没有新版本时，直接显示 NO_UPDATE。
        ota_service_set_state(OTA_STATE_IDLE, "NO_UPDATE");
        return;
    }

    // 检测到新版本时，进入 READY，消息里优先显示云端 message。
    ota_service_set_state(OTA_STATE_READY, s_ota.message);

    if (APP_OTA_AUTO_UPGRADE) {
        ota_service_download_and_apply();
    }
}
```

---

## 11. 推荐周期处理模板

```c
void ota_service_process(void)
{
    if (!s_ota.inited || !APP_OTA_AUTO_CHECK) {
        return;
    }

    if (s_ota.checked_once) {
        return;
    }

    // OTA 检查至少要求网络已经可用。
    if (wifi_service_get_state() != WIFI_STATE_GOT_IP) {
        return;
    }

    // 当前版本要求 HTTP 基础服务已经准备好，保持依赖关系清晰。
    if (!http_service_is_ready()) {
        return;
    }

    // 到这里说明联网和 HTTP 基础都准备好了，可以执行一次 OTA 检查。
    ota_service_check_once();
    s_ota.checked_once = true;
}
```

当前这样设计的好处是：

- 主依赖关系很清楚
- 后面接入真实服务器时，结构不用推翻重写

---

## 12. 推荐真实 OTA 下载模板

```c
static esp_err_t ota_service_download_and_apply(void)
{
    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
    esp_ota_handle_t ota_handle = 0;
    esp_http_client_handle_t client = NULL;
    esp_err_t ret = ESP_FAIL;
    int total_bytes = 0;
    char buffer[APP_OTA_WRITE_BUFFER_SIZE];

    if (update_partition == NULL) {
        ota_service_set_state(OTA_STATE_FAIL, "NO_PARTITION");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "target ota partition: label=%s address=0x%08lx size=%ld",
             update_partition->label,
             (unsigned long)update_partition->address,
             (long)update_partition->size);

    ota_service_set_state(OTA_STATE_DOWNLOADING, "DOWNLOADING");

    client = http_service_open_stream(s_ota.firmware_url);
    if (client == NULL) {
        ota_service_set_state(OTA_STATE_FAIL, "HTTP_FAIL");
        return ESP_FAIL;
    }

    ret = esp_ota_begin(update_partition, OTA_WITH_SEQUENTIAL_WRITES, &ota_handle);
    if (ret != ESP_OK) {
        http_service_close_stream(client);
        ota_service_set_state(OTA_STATE_FAIL, "OTA_BEGIN_FAIL");
        return ret;
    }

    while (true) {
        int read_len = esp_http_client_read(client, buffer, sizeof(buffer));
        if (read_len < 0) {
            ret = ESP_FAIL;
            break;
        }

        if (read_len == 0) {
            ret = ESP_OK;
            break;
        }

        ret = esp_ota_write(ota_handle, buffer, read_len);
        if (ret != ESP_OK) {
            break;
        }

        total_bytes += read_len;
    }

    http_service_close_stream(client);

    if (ret != ESP_OK) {
        esp_ota_abort(ota_handle);
        ota_service_set_state(OTA_STATE_FAIL, "WRITE_FAIL");
        return ret;
    }

    ota_service_set_state(OTA_STATE_VERIFY, "VERIFY");

    ret = esp_ota_end(ota_handle);
    if (ret != ESP_OK) {
        ota_service_set_state(OTA_STATE_FAIL, "OTA_END_FAIL");
        return ret;
    }

    ret = esp_ota_set_boot_partition(update_partition);
    if (ret != ESP_OK) {
        ota_service_set_state(OTA_STATE_FAIL, "BOOT_SET_FAIL");
        return ret;
    }

    ota_service_set_state(OTA_STATE_SUCCESS, "REBOOTING");
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();
    return ESP_OK;
}
```

这段模板体现的就是当前项目已经跑通的真实 OTA 主链：

```text
云端版本接口
-> 获取 firmware url
-> 打开固件流
-> esp_ota_begin
-> read + write 循环
-> esp_ota_end
-> esp_ota_set_boot_partition
-> esp_restart
```

---

## 13. 真实 OTA 下载链后续扩展方向

当前模板先只做“检查骨架”，后面真实 OTA 下载链通常可以按这条路线扩展：

```text
READY
-> HTTP 下载固件 bin
-> 写入 OTA 分区
-> 设置启动分区
-> 重启
-> 新固件启动
```

后面扩展时，建议沿当前状态机继续细化，而不是重新再起一套完全不同的结构。比如：

- 下载进度百分比
- 当前下载字节数
- 回滚状态
- 升级来源标识

---

## 14. 显示层联动建议

建议 `ota_service` 至少同步这些信息到 LCD：

- OTA 当前状态
- 当前消息

推荐显示格式：

```text
OTA : READY
MSG : new firmware available
```

升级过程中也建议至少能看到：

```text
OTA : DOWNLOADING
MSG : DOWNLOADING
```

升级完成前：

```text
OTA : SUCCESS
MSG : REBOOTING
```

---

## 15. 成功路径日志建议

为了后面排查方便，真实 OTA 模板里建议保留这些日志：

- 自动升级开始日志
- 目标 OTA 分区日志
- 固件 URL 打开成功日志
- 响应头和 content_length 日志
- `esp_ota_begin()` 成功日志
- 下载进度日志
- `esp_ota_end()` 成功日志
- `esp_ota_set_boot_partition()` 成功日志
- 重启前日志

这样成功路径和失败路径都能看清，不会只在失败时有信息。

---

## 16. 推荐搭配的文档

这份模板建议搭配这些文档一起看：

- `OTA基础速记.md`
- `ESP32 OTA分区基础速记.md`
- `Wi-Fi服务模板文档.md`
- `HTTP服务模板文档.md`
- `v1.9.0_项目的事件和函数关系流程表.md`

---

## 17. 一句话记住

把 `ota_service` 记成一句话就够了：

```text
它负责把“设备有没有新版本、当前 OTA 到哪一步、怎么把新固件真正写进 OTA 分区并切过去”
整理成一层可复用状态服务。
```
