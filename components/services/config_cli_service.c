#include "config_cli_service.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "config_service.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_vfs_dev.h"

static const char *TAG = "CFG_CLI";

typedef struct {
    bool inited;            // 串口配置命令服务是否已初始化。
    char line_buf[256];     // 当前正在接收的一行命令缓存。
    size_t line_len;        // 当前缓存里已写入的命令长度。
} config_cli_service_ctx_t;

static config_cli_service_ctx_t s_cli = {
    .inited = false,
    .line_buf = {0},
    .line_len = 0,
};

/**
 * @brief 打印串口配置命令帮助
 *
 * 当前 CLI 故意保持最小化，只提供：
 * - show / save / load / reset / reboot
 * - set wifi / set http / set ota
 */
static void config_cli_service_log_help(void)
{
    ESP_LOGI(TAG, "cfg help");
    ESP_LOGI(TAG, "cfg show");
    ESP_LOGI(TAG, "cfg set wifi <ssid> <password>");
    ESP_LOGI(TAG, "cfg set http <url>");
    ESP_LOGI(TAG, "cfg set ota <url>");
    ESP_LOGI(TAG, "cfg save");
    ESP_LOGI(TAG, "cfg load");
    ESP_LOGI(TAG, "cfg reset");
    ESP_LOGI(TAG, "cfg reboot");
}

/**
 * @brief 打印 monitor 里的命令提示符
 *
 * 这就是你在串口监视器里看到的：
 * `cfg> `
 *
 * 每次：
 * - CLI 初始化完成后
 * - 一条命令执行结束后
 * 都会再打印一次提示符，方便继续输入下一条命令。
 */
static void config_cli_service_print_prompt(void)
{
    printf("\r\ncfg> ");
    fflush(stdout);
}

/**
 * @brief 打印当前运行时配置
 *
 * 这里展示的是 config_service 当前内存里的配置值，
 * 不是直接去读 app_config.h 宏。
 */
static void config_cli_service_log_current_config(void)
{
    const app_runtime_config_t *cfg = config_service_get();
    if (cfg == NULL) {
        ESP_LOGW(TAG, "config is null");
        return;
    }

    ESP_LOGI(TAG, "current config:");
    ESP_LOGI(TAG, "  wifi_ssid=%s", cfg->wifi_ssid);
    ESP_LOGI(TAG, "  wifi_password=%s", cfg->wifi_password);
    ESP_LOGI(TAG, "  http_test_url=%s", cfg->http_test_url);
    ESP_LOGI(TAG, "  ota_version_url=%s", cfg->ota_version_url);
}

/**
 * @brief 解析并执行一整行 cfg 命令
 *
 * 整个流程是：
 * 1. 按空格把一整行命令拆成 argv
 * 2. 先检查是不是以 `cfg` 开头
 * 3. 再分发到具体子命令
 *
 * 这里不负责“逐字符读取”，它只负责处理已经接收完整的一行命令。
 */
static void config_cli_service_execute_line(char *line)
{
    char *argv[6] = {0};
    int argc = 0;
    char *token = strtok(line, " ");
    while (token != NULL && argc < (int)(sizeof(argv) / sizeof(argv[0]))) {
        argv[argc++] = token;
        token = strtok(NULL, " ");
    }

    if (argc == 0) {
        return;
    }

    if (strcmp(argv[0], "cfg") != 0) {
        ESP_LOGW(TAG, "unknown command: %s", argv[0]);
        config_cli_service_log_help();
        return;
    }

    if (argc == 1 || strcmp(argv[1], "help") == 0) {
        config_cli_service_log_help();
        return;
    }

    if (strcmp(argv[1], "show") == 0) {
        config_cli_service_log_current_config();
        return;
    }

    if (strcmp(argv[1], "save") == 0) {
        esp_err_t ret = config_service_save();
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "cfg save ok");
        } else {
            ESP_LOGE(TAG, "cfg save failed, ret=0x%x", ret);
        }
        return;
    }

    if (strcmp(argv[1], "load") == 0) {
        esp_err_t ret = config_service_load();
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "cfg load ok");
            config_cli_service_log_current_config();
        } else {
            ESP_LOGE(TAG, "cfg load failed, ret=0x%x", ret);
        }
        return;
    }

    if (strcmp(argv[1], "reset") == 0) {
        esp_err_t ret = config_service_reset_to_default();
        if (ret == ESP_OK) {
            ret = config_service_load();
        }
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "cfg reset ok");
            config_cli_service_log_current_config();
        } else {
            ESP_LOGE(TAG, "cfg reset failed, ret=0x%x", ret);
        }
        return;
    }

    if (strcmp(argv[1], "reboot") == 0) {
        ESP_LOGI(TAG, "cfg reboot requested");
        esp_restart();
        return;
    }

    if (strcmp(argv[1], "set") == 0) {
        if (argc >= 5 && strcmp(argv[2], "wifi") == 0) {
            esp_err_t ret = config_service_set_wifi(argv[3], argv[4]);
            if (ret == ESP_OK) {
                ESP_LOGI(TAG, "cfg set wifi ok, save/reboot to apply");
            } else {
                ESP_LOGE(TAG, "cfg set wifi failed, ret=0x%x", ret);
            }
            return;
        }

        if (argc >= 4 && strcmp(argv[2], "http") == 0) {
            esp_err_t ret = config_service_set_urls(argv[3], NULL);
            if (ret == ESP_OK) {
                ESP_LOGI(TAG, "cfg set http ok, save/reboot to apply");
            } else {
                ESP_LOGE(TAG, "cfg set http failed, ret=0x%x", ret);
            }
            return;
        }

        if (argc >= 4 && strcmp(argv[2], "ota") == 0) {
            esp_err_t ret = config_service_set_urls(NULL, argv[3]);
            if (ret == ESP_OK) {
                ESP_LOGI(TAG, "cfg set ota ok, save/reboot to apply");
            } else {
                ESP_LOGE(TAG, "cfg set ota failed, ret=0x%x", ret);
            }
            return;
        }
    }

    ESP_LOGW(TAG, "invalid cfg command");
    config_cli_service_log_help();
}

/**
 * @brief 初始化串口配置命令服务
 *
 * 这一步主要做三件事：
 * 1. 确保 UART0 驱动已安装
 * 2. 把 UART0 挂到 VFS，这样后面可以直接 read(STDIN_FILENO, ...)
 * 3. 把标准输入改成非阻塞，避免主循环卡住
 */
esp_err_t config_cli_service_init(void)
{
    if (s_cli.inited) {
        return ESP_OK;
    }

    if (!uart_is_driver_installed(UART_NUM_0)) {
        esp_err_t ret = uart_driver_install(UART_NUM_0, 1024, 0, 0, NULL, 0);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "uart_driver_install failed, ret=0x%x", ret);
            return ret;
        }
    }

    // 把 UART0 接到 VFS/标准输入输出，后面就能通过 read(STDIN_FILENO, ...)
    // 直接读取 monitor 发过来的字符，而不用自己再手写一层 UART 轮询封装。
    esp_vfs_dev_uart_use_driver(UART_NUM_0);

    // 把标准输入设成非阻塞模式，主循环里即使当前没有用户输入，
    // config_cli_service_process() 也会立刻返回，不影响其他 service 周期运行。
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    if (flags >= 0) {
        (void)fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
    }

    s_cli.line_len = 0;
    s_cli.line_buf[0] = '\0';
    s_cli.inited = true;

    ESP_LOGI(TAG, "config cli ready, type 'cfg help' in monitor");
    config_cli_service_print_prompt();
    return ESP_OK;
}

/**
 * @brief 串口配置命令周期处理入口
 *
 * 这段是整个 CLI 最关键的“逐字符接收状态机”：
 * 1. 从 monitor 标准输入一次读 1 个字符
 * 2. 普通字符就追加到 line_buf
 * 3. 遇到回车换行，就把 line_buf 当成完整命令交给 execute_line
 * 4. 遇到退格，就同步删掉缓存尾部字符
 *
 * 这样设计的好处是：
 * - 主循环里非阻塞
 * - 逻辑简单
 * - 很适合这种轻量级配置入口
 */
void config_cli_service_process(void)
{
    if (!s_cli.inited) {
        return;
    }

    char ch = 0;
    ssize_t read_len = read(STDIN_FILENO, &ch, 1);
    while (read_len > 0) {
        if (ch == '\r') {
            read_len = read(STDIN_FILENO, &ch, 1);
            continue;
        }

        if (ch == '\n') {
            // 用户按下回车后，先把光标移动到新行，
            // 再把当前缓存的一整行命令交给 execute_line 解析。
            printf("\r\n");
            fflush(stdout);
            s_cli.line_buf[s_cli.line_len] = '\0';
            if (s_cli.line_len > 0) {
                ESP_LOGI(TAG, "cmd> %s", s_cli.line_buf);
                config_cli_service_execute_line(s_cli.line_buf);
            }
            s_cli.line_len = 0;
            s_cli.line_buf[0] = '\0';
            config_cli_service_print_prompt();
            read_len = read(STDIN_FILENO, &ch, 1);
            continue;
        }

        if ((ch == '\b' || ch == 0x7f) && s_cli.line_len > 0) {
            s_cli.line_len--;
            s_cli.line_buf[s_cli.line_len] = '\0';
            // 这一句是“退格回显”，用于把 monitor 上最后一个字符视觉上擦掉。
            printf("\b \b");
            fflush(stdout);
            read_len = read(STDIN_FILENO, &ch, 1);
            continue;
        }

        if (s_cli.line_len < sizeof(s_cli.line_buf) - 1) {
            s_cli.line_buf[s_cli.line_len++] = (char)ch;
            s_cli.line_buf[s_cli.line_len] = '\0';
            // 这一句就是“字符回显”的核心代码。
            // 用户在 monitor 里敲下的每个普通字符，都会立刻显示出来，
            // 所以现在输入时你能直接看到自己正在输入什么。
            putchar(ch);
            fflush(stdout);
        }

        read_len = read(STDIN_FILENO, &ch, 1);
    }

    // 非阻塞读取时，没有输入数据通常会返回 EAGAIN / EWOULDBLOCK，
    // 这两种都属于正常情况，不打印警告，避免刷屏。
    if (read_len < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
        ESP_LOGW(TAG, "stdin read failed, errno=%d", errno);
    }
}

/**
 * @brief 判断 CLI 是否已经可用
 */
bool config_cli_service_is_ready(void)
{
    return s_cli.inited;
}
