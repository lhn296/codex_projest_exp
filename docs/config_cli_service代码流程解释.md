# config_cli_service代码流程解释

## 1. 这个文件是做什么的

[config_cli_service.c](g:\ESP32_WORK\ESP32_PRO\ESP32_projest_template_codex\Codex_Project_tep\components\services\config_cli_service.c)
的作用是：

- 让我们可以在 `idf.py monitor` 里直接输入配置命令
- 把输入的一整行命令解析成：
  - `cfg show`
  - `cfg set http ...`
  - `cfg save`
  - `cfg reboot`
- 再调用 [config_service.c](g:\ESP32_WORK\ESP32_PRO\ESP32_projest_template_codex\Codex_Project_tep\components\services\config_service.c) 去真正修改、保存和加载配置

也就是说，它本身不是配置存储层，而是：

```text
monitor 输入
-> config_cli_service 接收字符
-> 组装成完整命令
-> 解析命令
-> 调用 config_service
```

## 2. 整体执行流程

### 启动阶段

在 [app_main_task.c](g:\ESP32_WORK\ESP32_PRO\ESP32_projest_template_codex\Codex_Project_tep\components\app\app_main_task.c) 里会先调用：

```c
config_cli_service_init();
```

这一步主要做三件事：

1. 确保 `UART0` 驱动已经安装
2. 把 `UART0` 挂到 `VFS`
3. 把标准输入改成非阻塞模式

这样后面就可以用：

```c
read(STDIN_FILENO, &ch, 1);
```

来直接读取 monitor 里输入的字符。

这里的 `VFS` 是：

- `Virtual File System`
- 可以理解成“虚拟文件系统接口层”

在当前这个文件里，它的作用不是拿来操作磁盘文件，而是：

- 把 `UART0` 挂接到标准输入输出接口
- 这样后面就能直接使用：
  - `read(STDIN_FILENO, ...)`
  - `printf(...)`
  - `putchar(...)`

也就是说，这里形成的是：

```text
monitor 输入
-> STDIN / STDOUT
-> VFS
-> UART0
```

如果没有这一步，当前这套 CLI 就不能这么自然地使用标准输入输出方式去读 monitor 字符。

---

### 主循环阶段

在 [app_main_task.c](g:\ESP32_WORK\ESP32_PRO\ESP32_projest_template_codex\Codex_Project_tep\components\app\app_main_task.c) 的主循环里会周期调用：

```c
config_cli_service_process();
```

这就是 CLI 的“逐字符接收入口”。

它的逻辑是：

1. 每次尝试读 1 个字符
2. 如果是普通字符，就追加到 `line_buf`
3. 如果是回车换行，就把这整行交给命令解析函数
4. 如果是退格，就删掉缓冲区尾部字符

## 3. 关键数据结构

文件里有一个状态结构体：

```c
typedef struct {
    bool inited;
    char line_buf[256];
    size_t line_len;
} config_cli_service_ctx_t;
```

这三个成员分别表示：

- `inited`
  - CLI 是否已经初始化完成
- `line_buf`
  - 当前正在输入的一整行命令缓存
- `line_len`
  - 当前已经输入了多少字符

你可以把它理解成：

```text
用户一边输入
-> 字符一边追加到 line_buf
-> 按回车时再整体解析
```

## 4. 命令提示符在哪里打印

提示符在这个函数里：

```c
static void config_cli_service_print_prompt(void)
{
    printf("\r\ncfg> ");
    fflush(stdout);
}
```

这就是 monitor 里看到的：

```text
cfg>
```

它会在两种时机打印：

1. CLI 初始化完成后
2. 一条命令执行完后

## 5. 字符回显是哪一段

你前面最关心的“为什么现在输入能看见字符”，核心代码就是这段：

```c
if (s_cli.line_len < sizeof(s_cli.line_buf) - 1) {
    s_cli.line_buf[s_cli.line_len++] = (char)ch;
    s_cli.line_buf[s_cli.line_len] = '\0';
    putchar(ch);
    fflush(stdout);
}
```

这里做了两件事：

1. 把输入字符追加到 `line_buf`
2. 用 `putchar(ch)` 把字符立刻打印回 monitor

所以这就是“字符回显”的实现位置。

## 6. 退格回显是哪一段

退格处理在这里：

```c
if ((ch == '\b' || ch == 0x7f) && s_cli.line_len > 0) {
    s_cli.line_len--;
    s_cli.line_buf[s_cli.line_len] = '\0';
    printf("\b \b");
    fflush(stdout);
}
```

这里的意思是：

- 把缓存尾部字符删掉
- 再把 monitor 上最后一个字符视觉上擦掉

这就是为什么退格键现在也能正常工作。

## 7. 回车后为什么会执行命令

当读到换行符时，会走这段：

```c
if (ch == '\n') {
    s_cli.line_buf[s_cli.line_len] = '\0';
    if (s_cli.line_len > 0) {
        config_cli_service_execute_line(s_cli.line_buf);
    }
    s_cli.line_len = 0;
    s_cli.line_buf[0] = '\0';
    config_cli_service_print_prompt();
}
```

流程就是：

1. 给 `line_buf` 补上字符串结束符
2. 调用 `config_cli_service_execute_line()`
3. 清空输入缓存
4. 再打印新的 `cfg>` 提示符

## 8. 命令解析在哪

真正解析命令的是：

```c
static void config_cli_service_execute_line(char *line)
```

它会先用 `strtok(line, " ")` 按空格拆命令，再识别：

- `cfg help`
- `cfg show`
- `cfg save`
- `cfg load`
- `cfg reset`
- `cfg reboot`
- `cfg set wifi <ssid> <password>`
- `cfg set http <url>`
- `cfg set ota <url>`

## 9. 命令最终是怎么改配置的

比如这些命令：

```text
cfg set http https://httpbin.org/json
cfg save
cfg reboot
```

背后会调用：

```c
config_service_set_urls(...)
config_service_save()
esp_restart()
```

也就是说：

- `set` 只改当前运行时配置
- `save` 才真正写入 `NVS`
- `reboot` 后会重新从 `NVS` 读出

## 10. 为什么之前改坏地址后会一直刷 HTTP 错误

你前面碰到过这种情况：

```text
cfg set http http://selftest/manual
cfg save
cfg reboot
```

然后设备启动后，串口和屏幕一直刷：

```text
HTTP_SVC: esp_http_client_open failed
couldn't get hostname for :selftest
```

原因不是 CLI 坏了，而是：

1. `cfg set http ...` 把坏地址写进了运行时配置
2. `cfg save` 又把这个坏地址保存进了 `NVS`
3. 重启后 `config_service_load()` 把它重新读出来
4. `http_service_process()` 又真的拿这个地址去发请求
5. `selftest` 不是可解析的真实域名，于是就不停报错

后来之所以不再刷屏，是因为在
[config_service.c](g:\ESP32_WORK\ESP32_PRO\ESP32_projest_template_codex\Codex_Project_tep\components\services\config_service.c)
里补了两层保护：

- 保存前基础校验
- 加载后兜底回退默认值

也就是现在的行为变成：

```text
如果用户输入明显不合理的 URL
-> config_service_set_urls() 直接拒绝

如果 NVS 里已经有明显不合理的 URL
-> config_service_load() 自动回退到 app_config.h 默认值
```

所以现在坏 URL 不会再把 monitor 刷满。

## 11. 为什么这里要用非阻塞读取

初始化里有这段：

```c
int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
if (flags >= 0) {
    (void)fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
}
```

目的是：

- 即使当前没有输入字符
- `config_cli_service_process()` 也能立刻返回
- 不会把主循环卡住

这对当前项目很重要，因为主循环里还要周期跑：

- `wifi_service_process()`
- `http_service_process()`
- `ota_service_process()`
- `display_service_process()`

## 12. 为什么要排除 EAGAIN / EWOULDBLOCK

文件里最后有这段判断：

```c
if (read_len < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
    ESP_LOGW(TAG, "stdin read failed, errno=%d", errno);
}
```

它的作用是：

- 如果 `read()` 返回负数
- 先判断这是不是“真正异常”
- 而不是“当前没有输入数据”

这里要分清两层：

### 第一层：`read()` 的返回值

```c
ssize_t read_len = read(STDIN_FILENO, &ch, 1);
```

`read_len` 可能是：

- `> 0`
  - 成功读到了字节
- `== 0`
  - 当前流结束或被关闭
- `< 0`
  - 这次读取失败，具体原因要继续看 `errno`

### 第二层：`errno` 的值

`EAGAIN` 和 `EWOULDBLOCK` 不是 `read()` 的返回值，  
它们是：

- `errno` 的取值

也就是说，这里真正的关系是：

```text
read() 返回 -1
-> 再看 errno 是什么
-> 如果是 EAGAIN / EWOULDBLOCK
   就说明“当前暂时没数据”
```

因为我们前面已经把标准输入设成了：

- `O_NONBLOCK`

也就是：

- 有输入就读
- 没输入就立刻返回
- 不等待

所以当用户当前没有敲任何字符时：

```c
read(STDIN_FILENO, &ch, 1);
```

返回负数其实很正常。  
这时常见的 `errno` 就是：

- `EAGAIN`
- `EWOULDBLOCK`

这两个在这里都表示：

```text
当前暂时没有数据可读
不是程序真的出错了
```

所以这两种情况不应该打印 warning。  
只有真的发生了别的读取异常时，才打印：

```c
ESP_LOGW(TAG, "stdin read failed, errno=%d", errno);
```

如果不排除这两个错误码，主循环会在“用户没输入命令”的大多数时间里不停刷日志。

## 13. 当前这份文件在项目里的定位

它更像一个：

- 轻量命令入口
- 调试入口
- 配置入口

而不是完整的 shell 系统。

当前它的优点是：

- 实现简单
- 易调试
- 适合学习
- 和 `config_service` 解耦清晰

## 14. 一句话总结

[config_cli_service.c](g:\ESP32_WORK\ESP32_PRO\ESP32_projest_template_codex\Codex_Project_tep\components\services\config_cli_service.c)
做的事情就是：

```text
从 monitor 读字符
-> 拼成一整行命令
-> 解析 cfg 命令
-> 调用 config_service 改配置
-> 支持提示符、字符回显、退格回显
```
