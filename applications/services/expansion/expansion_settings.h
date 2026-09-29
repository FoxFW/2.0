#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {

    uint8_t uart_index;
} ExpansionSettings;

void expansion_settings_load(ExpansionSettings* settings);

void expansion_settings_save(const ExpansionSettings* settings);

#ifdef __cplusplus
}
#endif
