// helpers/gdr_settings.c
#include "gdr_settings.h"
#include <storage/storage.h>
#include <flipper_format/flipper_format.h>
#include <furi.h>
#include <stdio.h>
#include "../defines.h"
#include "../protocols/protocols_common.h"

#define TAG "GDRSettings"

#define SETTINGS_FILE_HEADER  "GDR Settings"
#define SETTINGS_FILE_VERSION 1

void gdr_settings_set_defaults(GDRSettings* settings) {
    settings->frequency = 433920000;
    settings->preset_index = 0;
    settings->tx_power = 0;
    settings->auto_save = false;
    settings->hopping_enabled = false;
    settings->emulate_feature_enabled = false;
    settings->dual_freq_a = 433920000;
    settings->dual_freq_b = 433920000;
    settings->dual_preset_a = 0xFF;
    settings->dual_preset_b = 0xFF;
    settings->dual_preset_name_a[0] = '\0';
    settings->dual_preset_name_b[0] = '\0';
    settings->shield_freq = 433920000;
    settings->shield_preset_index = 0;
    settings->shield_tx_offset_index = 3;
    settings->shield_tx_power = 0;
}

void gdr_settings_load(GDRSettings* settings) {
    // Set defaults first
    gdr_settings_set_defaults(settings);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* ff = flipper_format_file_alloc(storage);

    do {
        if(!flipper_format_file_open_existing(ff, GDR_SETTINGS_FILE)) {
            FURI_LOG_I(TAG, "Settings file not found, using defaults");
            break;
        }

        FuriString* header = furi_string_alloc();
        uint32_t version = 0;

        if(!flipper_format_read_header(ff, header, &version)) {
            FURI_LOG_W(TAG, "Failed to read settings header");
            furi_string_free(header);
            break;
        }

        if(version != SETTINGS_FILE_VERSION) {
            FURI_LOG_W(TAG, "Unsupported settings version %lu", (unsigned long)version);
            furi_string_free(header);
            break;
        }

        if(furi_string_cmp_str(header, SETTINGS_FILE_HEADER) != 0) {
            FURI_LOG_W(TAG, "Invalid settings file header");
            furi_string_free(header);
            break;
        }

        furi_string_free(header);

        // Read frequency
        if(!flipper_format_read_uint32(ff, FF_FREQUENCY, &settings->frequency, 1)) {
            FURI_LOG_W(TAG, "Failed to read frequency, using default");
            settings->frequency = 433920000;
        }

        // Read preset index
        uint32_t preset_temp = 0;
        if(!flipper_format_read_uint32(ff, "PresetIndex", &preset_temp, 1)) {
            FURI_LOG_W(TAG, "Failed to read preset index, using default");
            preset_temp = 0;
        }
        settings->preset_index = (uint8_t)preset_temp;

        // Read auto-save
        uint32_t auto_save_temp = 0;
        if(!flipper_format_read_uint32(ff, "AutoSave", &auto_save_temp, 1)) {
            FURI_LOG_W(TAG, "Failed to read auto-save, using default");
            auto_save_temp = 0;
        }
        settings->auto_save = (auto_save_temp == 1);

        // Read tx-power
        uint32_t tx_power_temp = 0;
        if(!flipper_format_read_uint32(ff, "TXPower", &tx_power_temp, 1)) {
            FURI_LOG_W(TAG, "Failed to read TXPower, using default");
            tx_power_temp = 0;
        }
        settings->tx_power = (uint8_t)tx_power_temp;

        // Read hopping
        uint32_t hopping_temp = 0;
        if(!flipper_format_read_uint32(ff, "Hopping", &hopping_temp, 1)) {
            FURI_LOG_W(TAG, "Failed to read hopping, using default");
            hopping_temp = 0;
        }
        settings->hopping_enabled = (hopping_temp == 1);

        uint32_t emulate_temp = 0;
        if(!flipper_format_read_uint32(ff, "EmulateFeature", &emulate_temp, 1)) {
            FURI_LOG_I(TAG, "EmulateFeature key missing, defaulting to disabled");
            emulate_temp = 0;
        }
        settings->emulate_feature_enabled = (emulate_temp == 1);

        flipper_format_rewind(ff);
        flipper_format_read_uint32(ff, "DualFreqA", &settings->dual_freq_a, 1);
        flipper_format_rewind(ff);
        flipper_format_read_uint32(ff, "DualFreqB", &settings->dual_freq_b, 1);
        uint32_t dual_preset_temp = 0;
        flipper_format_rewind(ff);
        if(flipper_format_read_uint32(ff, "DualPresetA", &dual_preset_temp, 1)) {
            settings->dual_preset_a = (uint8_t)dual_preset_temp;
        }
        dual_preset_temp = 0;
        flipper_format_rewind(ff);
        if(flipper_format_read_uint32(ff, "DualPresetB", &dual_preset_temp, 1)) {
            settings->dual_preset_b = (uint8_t)dual_preset_temp;
        }
        FuriString* preset_name = furi_string_alloc();
        flipper_format_rewind(ff);
        if(flipper_format_read_string(ff, "DualPresetNameA", preset_name)) {
            snprintf(
                settings->dual_preset_name_a,
                sizeof(settings->dual_preset_name_a),
                "%s",
                furi_string_get_cstr(preset_name));
        }
        flipper_format_rewind(ff);
        if(flipper_format_read_string(ff, "DualPresetNameB", preset_name)) {
            snprintf(
                settings->dual_preset_name_b,
                sizeof(settings->dual_preset_name_b),
                "%s",
                furi_string_get_cstr(preset_name));
        }
        furi_string_free(preset_name);

        flipper_format_rewind(ff);
        flipper_format_read_uint32(ff, "ShieldFreq", &settings->shield_freq, 1);
        dual_preset_temp = 0;
        flipper_format_rewind(ff);
        if(flipper_format_read_uint32(ff, "ShieldPreset", &dual_preset_temp, 1)) {
            settings->shield_preset_index = (uint8_t)dual_preset_temp;
        }
        dual_preset_temp = 0;
        flipper_format_rewind(ff);
        if(flipper_format_read_uint32(ff, "ShieldTxOffset", &dual_preset_temp, 1)) {
            settings->shield_tx_offset_index = (uint8_t)dual_preset_temp;
        }
        dual_preset_temp = 0;
        flipper_format_rewind(ff);
        if(flipper_format_read_uint32(ff, "ShieldTxPower", &dual_preset_temp, 1)) {
            settings->shield_tx_power = (uint8_t)dual_preset_temp;
        }
        FURI_LOG_I(
            TAG,
            "Settings loaded: freq=%lu, preset=%u, auto_save=%d, hopping=%d, emulate=%d",
            settings->frequency,
            settings->preset_index,
            settings->auto_save,
            settings->hopping_enabled,
            settings->emulate_feature_enabled);

    } while(false);

    flipper_format_free(ff);
    furi_record_close(RECORD_STORAGE);
}

void gdr_settings_save(GDRSettings* settings) {
    Storage* storage = furi_record_open(RECORD_STORAGE);

    // Ensure directory exists
    storage_simply_mkdir(storage, GDR_SETTINGS_DIR);

    FlipperFormat* ff = flipper_format_file_alloc(storage);

    do {
        if(!flipper_format_file_open_always(ff, GDR_SETTINGS_FILE)) {
            FURI_LOG_E(TAG, "Failed to open settings file for writing");
            break;
        }

        if(!flipper_format_write_header_cstr(ff, SETTINGS_FILE_HEADER, SETTINGS_FILE_VERSION)) {
            FURI_LOG_E(TAG, "Failed to write settings header");
            break;
        }

        if(!flipper_format_write_uint32(ff, FF_FREQUENCY, &settings->frequency, 1)) {
            FURI_LOG_E(TAG, "Failed to write frequency");
            break;
        }

        uint32_t preset_temp = settings->preset_index;
        if(!flipper_format_write_uint32(ff, "PresetIndex", &preset_temp, 1)) {
            FURI_LOG_E(TAG, "Failed to write preset index");
            break;
        }

        uint32_t auto_save_temp = settings->auto_save ? 1 : 0;
        if(!flipper_format_write_uint32(ff, "AutoSave", &auto_save_temp, 1)) {
            FURI_LOG_E(TAG, "Failed to write auto-save");
            break;
        }

        uint32_t tx_power_temp = settings->tx_power;
        if(!flipper_format_write_uint32(ff, "TXPower", &tx_power_temp, 1)) {
            FURI_LOG_E(TAG, "Failed to write TX Power");
            break;
        }

        uint32_t hopping_temp = settings->hopping_enabled ? 1 : 0;
        if(!flipper_format_write_uint32(ff, "Hopping", &hopping_temp, 1)) {
            FURI_LOG_E(TAG, "Failed to write hopping");
            break;
        }

        uint32_t emulate_temp = settings->emulate_feature_enabled ? 1 : 0;
        if(!flipper_format_write_uint32(ff, "EmulateFeature", &emulate_temp, 1)) {
            FURI_LOG_E(TAG, "Failed to write emulate feature flag");
            break;
        }

        flipper_format_write_uint32(ff, "DualFreqA", &settings->dual_freq_a, 1);
        flipper_format_write_uint32(ff, "DualFreqB", &settings->dual_freq_b, 1);
        uint32_t dual_preset_a_temp = settings->dual_preset_a;
        flipper_format_write_uint32(ff, "DualPresetA", &dual_preset_a_temp, 1);
        uint32_t dual_preset_b_temp = settings->dual_preset_b;
        flipper_format_write_uint32(ff, "DualPresetB", &dual_preset_b_temp, 1);
        flipper_format_write_string_cstr(
            ff, "DualPresetNameA", settings->dual_preset_name_a);
        flipper_format_write_string_cstr(
            ff, "DualPresetNameB", settings->dual_preset_name_b);
        flipper_format_write_uint32(ff, "ShieldFreq", &settings->shield_freq, 1);
        uint32_t shield_preset_temp = settings->shield_preset_index;
        flipper_format_write_uint32(ff, "ShieldPreset", &shield_preset_temp, 1);
        uint32_t shield_offset_temp = settings->shield_tx_offset_index;
        flipper_format_write_uint32(ff, "ShieldTxOffset", &shield_offset_temp, 1);
        uint32_t shield_power_temp = settings->shield_tx_power;
        flipper_format_write_uint32(ff, "ShieldTxPower", &shield_power_temp, 1);
        FURI_LOG_I(
            TAG,
            "Settings saved: freq=%lu, preset=%u, auto_save=%d, hopping=%d, emulate=%d",
            settings->frequency,
            settings->preset_index,
            settings->auto_save,
            settings->hopping_enabled,
            settings->emulate_feature_enabled);

    } while(false);

    flipper_format_free(ff);
    furi_record_close(RECORD_STORAGE);
}
