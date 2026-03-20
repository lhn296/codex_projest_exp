# NVS配置存储基础速记

## 1. NVS 是什么

`NVS` 是 `ESP-IDF` 提供的非易失性键值存储。

你可以把它先理解成：

```text
掉电不丢失的小型参数存储区
```

它很适合保存：

- Wi-Fi SSID
- Wi-Fi 密码
- OTA 接口地址
- HTTP 测试地址
- 开关类配置
- 上次运行状态

---

## 2. 为什么 v2.0.0 要用它

因为当前项目里很多关键参数还写死在：

- `app_config.h`

这种方式适合学习前期，但后面会遇到这些问题：

- 每次换 Wi‑Fi 都要重新编译
- 每次换服务器地址都要重新烧录
- 设备重启后无法记住运行时改过的配置

所以就需要：

```text
默认值在代码里
运行值在 NVS 里
```

---

## 3. NVS 的基本概念

### 3.1 命名空间

NVS 里通常会按“命名空间”分类保存参数。  
例如：

- `app_cfg`

这就像是一个配置分组。

### 3.2 key

每个参数都有一个 key，例如：

- `wifi_ssid`
- `wifi_pwd`
- `http_url`
- `ota_url`

### 3.3 value

value 就是实际保存的内容，例如：

- `"LV-HOME"`
- `"lv666666"`
- `"https://httpbin.org/json"`

---

## 4. 常见操作流程

### 4.1 初始化 NVS

通常在系统早期先做：

```c
esp_err_t ret = nvs_flash_init();
```

如果 NVS 空间损坏或版本不兼容，常见处理是：

```c
nvs_flash_erase();
nvs_flash_init();
```

### 4.2 打开命名空间

```c
nvs_open("app_cfg", NVS_READWRITE, &handle);
```

### 4.3 写入字符串

```c
nvs_set_str(handle, "wifi_ssid", "LV-HOME");
```

### 4.4 读取字符串

```c
nvs_get_str(handle, "wifi_ssid", buffer, &length);
```

### 4.5 提交保存

```c
nvs_commit(handle);
```

### 4.6 关闭句柄

```c
nvs_close(handle);
```

---

## 5. 为什么写完还要 commit

因为：

- `nvs_set_xxx()` 只是把改动写进当前操作上下文
- `nvs_commit()` 才是真正提交到 Flash

所以如果：

- 只 `set`
- 不 `commit`

那重启后可能就没有保存成功。

---

## 6. 推荐在项目里的使用方式

不建议每个服务都各自直接去读写 NVS。  
更推荐这样分层：

```text
config_service
-> 统一读写 NVS
-> 缓存当前配置结构体

wifi_service / http_service / ota_service
-> 只读取 config_service 当前配置
```

这样好处是：

- 结构更清楚
- 不会每层都耦合 NVS
- 后面替换配置来源也更容易

---

## 7. 默认值和 NVS 值的关系

推荐这样理解：

### 第一次启动

NVS 里还没有配置：

```text
使用 app_config.h 默认值
```

### 后续保存过配置

NVS 里已经有值：

```text
优先使用 NVS
```

也就是：

```text
代码默认值
-> 只是兜底
-> 不是唯一来源
```

---

## 8. v2.0.0 最适合先存哪些参数

建议先只存这些：

- `wifi_ssid`
- `wifi_password`
- `http_test_url`
- `ota_version_url`

这四个最有价值，也最能立刻体现配置化收益。

---

## 9. 常见错误点

### 9.1 忘记 commit

结果：

- 看起来写成功
- 重启后丢失

### 9.2 读取缓冲区太小

结果：

- 读不完整
- 或读取失败

### 9.3 没处理“key 不存在”

第一次启动时，很多 key 本来就不存在。  
这时应该回退到默认值，而不是直接报错退出。

### 9.4 所有服务都自己读 NVS

结果：

- 代码分散
- 后面难维护

---

## 10. 一句话记住

`NVS` 在这个项目里最适合扮演的角色就是：

```text
把原来写死在代码里的关键运行参数，
变成设备掉电后也能记住的配置存储层。
```
