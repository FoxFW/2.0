// helpers/gdr_settings.h
#pragma once

#include <stdint.h>
#include <stdbool.h>

#define GDR_SETTINGS_FILE APP_DATA_PATH("settings.txt")
#define GDR_SETTINGS_DIR  APP_DATA_PATH()
#define GDR_PRESET_NAME_MAX 64U

typedef struct {
    uint32_t frequency;
    uint8_t preset_index;
    uint8_t tx_power;
    bool auto_save;
    bool hopping_enabled;
    bool emulate_feature_enabled;
    uint32_t dual_freq_a;
    uint32_t dual_freq_b;
    uint8_t dual_preset_a;
    uint8_t dual_preset_b;
    char dual_preset_name_a[GDR_PRESET_NAME_MAX];
    char dual_preset_name_b[GDR_PRESET_NAME_MAX];
    uint32_t shield_freq;
    uint8_t shield_preset_index;
    uint8_t shield_tx_offset_index;
    uint8_t shield_tx_power;
} GDRSettings;

void gdr_settings_load(GDRSettings* settings);
void gdr_settings_save(GDRSettings* settings);
void gdr_settings_set_defaults(GDRSettings* settings);
