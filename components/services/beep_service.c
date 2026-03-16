#include "beep_service.h"

#include "app_config.h"
#include "bsp_xl9555.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "BEEP_SERVICE";

typedef struct {
    bool inited;
    bool enabled;
    bool test_mode;
    bool output_on;
    bool sequence_active;
    uint8_t remaining_pulses;
    uint32_t on_ms;
    uint32_t off_ms;
    int64_t last_change_time_ms;
    int64_t last_test_trigger_time_ms;
} beep_service_ctx_t;

static beep_service_ctx_t s_beep = {
    .inited = false,
    .enabled = true,
    .test_mode = false,
    .output_on = false,
    .sequence_active = false,
    .remaining_pulses = 0,
    .on_ms = 0,
    .off_ms = 0,
    .last_change_time_ms = 0,
    .last_test_trigger_time_ms = 0,
};

static int64_t beep_service_get_time_ms(void)
{
    return esp_timer_get_time() / 1000;
}

static const char *beep_service_pattern_to_string(beep_pattern_t pattern)
{
    switch (pattern) {
        case BEEP_PATTERN_SHORT:
            return "SHORT";
        case BEEP_PATTERN_DOUBLE:
            return "DOUBLE";
        case BEEP_PATTERN_LONG:
            return "LONG";
        case BEEP_PATTERN_NONE:
        default:
            return "NONE";
    }
}

/* 设置蜂鸣器输出状态，并更新上下文记录当前状态。 */
static esp_err_t beep_service_output_set(bool on)
{
    esp_err_t ret = bsp_xl9555_beep_set(on);
    if (ret == ESP_OK) {
        s_beep.output_on = on;
    }
    return ret;
}

//声音模式设置函数，内部会把不同模式的节奏参数统一转换成一个时序对象，后续 process() 只负责按时间推进，不在事件任务里阻塞等待。
static esp_err_t beep_service_start_pattern(beep_pattern_t pattern)
{
    // 这里把“响几次、每次响多久、间隔多久”统一折叠成一个时序对象，
    // 后面 process() 只负责按时间推进，不在事件任务里阻塞等待。
    switch (pattern) {
        case BEEP_PATTERN_SHORT:
            s_beep.remaining_pulses = 1; // 响一次
            s_beep.on_ms = APP_BEEP_SHORT_ON_MS;
            s_beep.off_ms = APP_BEEP_SHORT_OFF_MS;
            break;

        case BEEP_PATTERN_DOUBLE:
            s_beep.remaining_pulses = 2;// 响两次
            s_beep.on_ms = APP_BEEP_SHORT_ON_MS;
            s_beep.off_ms = APP_BEEP_SHORT_OFF_MS;
            break;

        case BEEP_PATTERN_LONG:
            s_beep.remaining_pulses = 1;// 响一次，但时间更长
            s_beep.on_ms = APP_BEEP_LONG_ON_MS;
            s_beep.off_ms = APP_BEEP_SHORT_OFF_MS;
            break;

        case BEEP_PATTERN_NONE:
        default:
            return ESP_OK;
    }

    s_beep.sequence_active = true; // 启动时序，后续由 process() 推进状态机
    s_beep.last_change_time_ms = beep_service_get_time_ms(); // 立即生效，记录起始时间
    return beep_service_output_set(true);
}

esp_err_t beep_service_init(void)
{
    if (s_beep.inited) {
        return ESP_OK;
    }

    esp_err_t ret = bsp_xl9555_beep_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "bsp_xl9555_beep_init failed, ret=0x%x", ret);
        return ret;
    }

    ret = beep_service_output_set(false);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "beep output set failed, ret=0x%x", ret);
        return ret;
    }

    s_beep.inited = true;
    s_beep.last_change_time_ms = beep_service_get_time_ms();
    s_beep.last_test_trigger_time_ms = s_beep.last_change_time_ms;
    ESP_LOGI(TAG, "beep service ready, active_level=%d", APP_XL9555_BEEP_ACTIVE_LEVEL);
    return ESP_OK;
}

esp_err_t beep_service_set_enabled(bool enabled)
{
    if (!s_beep.inited) {
        return ESP_ERR_INVALID_STATE;
    }

    s_beep.enabled = enabled;
    ESP_LOGI(TAG, "beep enabled=%d", enabled);
    return ESP_OK;
}

bool beep_service_is_enabled(void)
{
    return s_beep.enabled;
}

esp_err_t beep_service_set_test_mode(bool enabled)
{
    if (!s_beep.inited) {
        return ESP_ERR_INVALID_STATE;
    }

    s_beep.test_mode = enabled;
    s_beep.last_test_trigger_time_ms = beep_service_get_time_ms();
    ESP_LOGI(TAG, "beep test_mode=%d", enabled);
    return ESP_OK;
}

bool beep_service_is_test_mode_enabled(void)
{
    return s_beep.test_mode;
}

esp_err_t beep_service_play(beep_pattern_t pattern)
{
    if (!s_beep.inited) {
        return ESP_ERR_INVALID_STATE;
    }

    // 普通播放遵守“蜂鸣提示是否启用”这个开关，适合业务键提示音。
    if (!s_beep.enabled) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "play pattern=%s", beep_service_pattern_to_string(pattern));
    return beep_service_start_pattern(pattern);
}

esp_err_t beep_service_play_force(beep_pattern_t pattern)
{
    if (!s_beep.inited) {
        return ESP_ERR_INVALID_STATE;
    }

    // 强制播放用于功能键确认音，即使当前业务提示被关闭，也允许这一次提示发声。
    ESP_LOGI(TAG, "force play pattern=%s", beep_service_pattern_to_string(pattern));
    return beep_service_start_pattern(pattern); // 设置时候发出的生效
}

void beep_service_process(void)
{
    if (!s_beep.inited) {
        return;
    }

    int64_t now_ms = beep_service_get_time_ms();

    // 测试模式下，空闲时每隔一段时间自动发一个短提示音，方便确认蜂鸣器链路是否正常。
    if (s_beep.test_mode &&
        !s_beep.sequence_active &&
        (now_ms - s_beep.last_test_trigger_time_ms) >= APP_BEEP_TEST_INTERVAL_MS) {
        s_beep.last_test_trigger_time_ms = now_ms;
        (void)beep_service_play_force(BEEP_PATTERN_SHORT);
    }

    if (!s_beep.sequence_active) {
        return;
    }

    if (s_beep.output_on) {
        if ((now_ms - s_beep.last_change_time_ms) < s_beep.on_ms) {
            return;
        }

        (void)beep_service_output_set(false);
        s_beep.last_change_time_ms = now_ms;
        return;
    }

    if ((now_ms - s_beep.last_change_time_ms) < s_beep.off_ms) {
        return;
    }

    if (s_beep.remaining_pulses > 0) {
        s_beep.remaining_pulses--;
    }

    if (s_beep.remaining_pulses == 0) {
        s_beep.sequence_active = false;
        return;
    }

    (void)beep_service_output_set(true);
    s_beep.last_change_time_ms = now_ms;
}
