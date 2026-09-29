#pragma once

#include <furi_hal.h>
#include <stdint.h>
#include <stdbool.h>
#include <storage/storage.h>
#include "subratt_protocols.h"

typedef struct {
    uint8_t repeat_values[SubRattAttackTotalCount];
    uint32_t last_index;
} SubRattSettings;

SubRattSettings* subratt_settings_alloc(void);

void subratt_settings_free(SubRattSettings* instance);

void subratt_settings_load(SubRattSettings* instance);

bool subratt_settings_save(SubRattSettings* instance);

void subratt_settings_set_value(SubRattSettings* instance, SubRattAttacks index, uint8_t value);

uint8_t subratt_settings_get_value(SubRattSettings* instance, SubRattAttacks index);

void subratt_settings_set_repeats(SubRattSettings* instance, const uint8_t* repeated_values);

uint8_t subratt_settings_get_current_repeats(SubRattSettings* instance);
