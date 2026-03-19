# 七牛云 version 接口教程

## 1. 文档定位

这份文档用于说明：

```text
不再使用腾讯云云函数返回版本 JSON，
而是直接把 version.json 放到七牛云，
让 ESP32 直接请求七牛云上的版本接口文件。
```

这条方案更轻量，适合后面把 OTA 云端链进一步简化成：

```text
ESP32
-> 七牛云 version.json
-> 七牛云 firmware.bin
```

---

## 2. 适用场景

这份方案适合：

- 已经能用七牛云公开地址访问 `.bin`
- 版本信息结构比较简单
- 当前只需要：
  - `version`
  - `url`
  - `message`
- 不急着上复杂后台逻辑

如果后面要做：

- 鉴权
- 多设备定制版本
- 动态灰度升级

那再考虑回到云函数或后端服务更合适。

---

## 3. 版本接口文件长什么样

建议直接在本地新建一个文件：

- `version.json`

内容模板如下：

```json
{
  "version": "v1.9.1",
  "url": "http://tc53j7zpx.hn-bkt.clouddn.com/codex_project_tep_v1.9.1.bin",
  "message": "new firmware available"
}
```

这三个字段的作用分别是：

- `version`
  - 云端目标版本号
- `url`
  - 真实固件下载地址
- `message`
  - 给 LCD 和日志显示的提示文本

---

## 4. 上传到七牛云

### 第一步

进入你当前已经在用的七牛云空间。

### 第二步

上传本地的：

- `version.json`

### 第三步

确认它和固件文件放在同一个公开可访问空间中。

例如当前如果你的固件地址是：

```text
http://tc53j7zpx.hn-bkt.clouddn.com/codex_project_tep_v1.9.1.bin
```

那么上传后的版本接口地址通常就是：

```text
http://tc53j7zpx.hn-bkt.clouddn.com/version.json
```

---

## 5. 浏览器验证

在让 ESP32 使用前，先在浏览器中访问：

- `http://tc53j7zpx.hn-bkt.clouddn.com/version.json`

理想结果应该直接看到：

```json
{
  "version": "v1.9.1",
  "url": "http://tc53j7zpx.hn-bkt.clouddn.com/codex_project_tep_v1.9.1.bin",
  "message": "new firmware available"
}
```

如果浏览器都打不开，ESP32 也一定访问不了。

---

## 6. ESP32 端怎么切换

只需要改：

- `components/system/app_config.h`

里的：

```c
#define APP_OTA_VERSION_URL "..."
```

把它从腾讯云版本接口地址改成：

```c
#define APP_OTA_VERSION_URL "http://tc53j7zpx.hn-bkt.clouddn.com/version.json"
```

然后重新：

```powershell
idf.py build
idf.py -p COM3 flash monitor
```

---

## 7. 这条方案的优点

- 结构更简单
- 不再依赖云函数
- 云端只保留两个静态文件：
  - `version.json`
  - `firmware.bin`
- 更适合长期做轻量 OTA 实验

---

## 8. 这条方案的限制

- 版本信息是静态文件，不适合复杂逻辑
- 如果要按设备、按批次返回不同版本，就不够灵活
- 仍然要注意七牛云当前网络环境是否稳定可访问

---

## 9. 推荐使用方式

建议你后面这样切换：

1. 先保留当前腾讯云方案作为已验证成功基线
2. 再单独上传 `version.json` 到七牛云
3. 浏览器确认七牛云 `version.json` 能正常访问
4. 把 `APP_OTA_VERSION_URL` 切到七牛云
5. 再做一轮 OTA 验证

这样就算七牛云方案暂时不通，也不会影响当前已经跑通的腾讯云方案。

---

## 10. 一句话记住

```text
七牛云 version 方案的本质，
就是把“版本接口”也静态化，
让 ESP32 直接读取七牛云上的 version.json，
再根据里面的 url 去下载同样托管在七牛云上的 firmware.bin。
```
