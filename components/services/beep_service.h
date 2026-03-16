#ifndef BEEP_SERVICE_H
#define BEEP_SERVICE_H

#include <stdbool.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BEEP_PATTERN_NONE = 0,
    BEEP_PATTERN_SHORT,
    BEEP_PATTERN_DOUBLE,
    BEEP_PATTERN_LONG,
} beep_pattern_t;

esp_err_t beep_service_init(void);
esp_err_t beep_service_set_enabled(bool enabled);
bool beep_service_is_enabled(void);
esp_err_t beep_service_set_test_mode(bool enabled);
bool beep_service_is_test_mode_enabled(void);
esp_err_t beep_service_play(beep_pattern_t pattern);
esp_err_t beep_service_play_force(beep_pattern_t pattern);
void beep_service_process(void);

#ifdef __cplusplus
}
#endif

#endif
