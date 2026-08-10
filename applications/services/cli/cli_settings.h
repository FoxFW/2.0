#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t shell_color_index;
} CliSettings;

void cli_settings_load(CliSettings* settings);
void cli_settings_save(const CliSettings* settings);

#ifdef __cplusplus
}
#endif
