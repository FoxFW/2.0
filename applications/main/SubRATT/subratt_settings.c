#include "subratt_settings.h"
#include "subratt_i.h"

#define TAG "SubRattSettings"

#define SUBRATT_SETTINGS_FILE_TYPE "Sub-GHz RandomAttack Settings File"
#define SUBRATT_SETTINGS_FILE_VERSION 2
#define SUBRATT_SETTINGS_PATH APP_DATA_PATH("subratt.settings")

#define SUBRATT_FIELD_LAST_INDEX "LastIndex"
#define SUBRATT_FIELD_REPEAT_VALUES "RepeatValue"

SubRattSettings* subratt_settings_alloc(void) {

    SubRattSettings* instance = calloc(1, sizeof(SubRattSettings));
    return instance;
}

void subratt_settings_free(SubRattSettings* instance) {
    furi_assert(instance);
    free(instance);
}

void subratt_settings_load(SubRattSettings* instance) {
    furi_assert(instance);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* fff_data_file = flipper_format_file_alloc(storage);

    uint32_t temp_last_index = 0;
    uint8_t temp_repeat_values[SubRattAttackTotalCount] = {0};
    bool was_read_last_index = false;
    bool was_read_repeat_values = false;

    if(FSE_OK == storage_sd_status(storage) && SUBRATT_SETTINGS_PATH &&
       flipper_format_file_open_existing(fff_data_file, SUBRATT_SETTINGS_PATH)) {
        was_read_last_index = flipper_format_read_uint32(
            fff_data_file, SUBRATT_FIELD_LAST_INDEX, (uint32_t*)&temp_last_index, 1);
        was_read_repeat_values = flipper_format_read_hex(
            fff_data_file,
            SUBRATT_FIELD_REPEAT_VALUES,
            temp_repeat_values,
            SubRattAttackTotalCount);
    } else {
        FURI_LOG_E(TAG, "Error open file %s", SUBRATT_SETTINGS_PATH);
    }

    if(was_read_last_index && temp_last_index < SubRattAttackTotalCount) {
        instance->last_index = temp_last_index;
    } else {
        FURI_LOG_W(TAG, "Last used index not found or can't be used!");
        instance->last_index = (uint32_t)SubRattAttackCAME12bit433;
    }
    if(was_read_repeat_values) {
        for(size_t i = 0; i < SubRattAttackTotalCount; i++) {
            uint8_t protocol_count = subratt_protocol_repeats_count(i);
            uint8_t max_protocol_count = protocol_count * 3;
            if(temp_repeat_values[i] < protocol_count) {
                instance->repeat_values[i] = protocol_count;
            } else if(temp_repeat_values[i] > max_protocol_count) {
                instance->repeat_values[i] = max_protocol_count;
            } else {
                instance->repeat_values[i] = temp_repeat_values[i];
            }
        }
    } else {
        FURI_LOG_W(TAG, "Last used repeat values can't be used!");
        for(size_t i = 0; i < SubRattAttackTotalCount; i++) {
            instance->repeat_values[i] = subratt_protocol_repeats_count(i);
        }
    }

    flipper_format_file_close(fff_data_file);
    flipper_format_free(fff_data_file);
    furi_record_close(RECORD_STORAGE);
}

bool subratt_settings_save(SubRattSettings* instance) {
    furi_assert(instance);

    bool saved = false;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* file = flipper_format_file_alloc(storage);

    do {
        if(FSE_OK != storage_sd_status(storage)) {
            break;
        }

        if(!flipper_format_file_open_always(file, SUBRATT_SETTINGS_PATH)) {
            break;
        }

        if(!flipper_format_write_header_cstr(
               file, SUBRATT_SETTINGS_FILE_TYPE, SUBRATT_SETTINGS_FILE_VERSION)) {
            break;
        }
        if(!flipper_format_insert_or_update_uint32(
               file, SUBRATT_FIELD_LAST_INDEX, &instance->last_index, 1)) {
            break;
        }

        if(!flipper_format_insert_or_update_hex(
               file,
               SUBRATT_FIELD_REPEAT_VALUES,
               instance->repeat_values,
               SubRattAttackTotalCount)) {
            break;
        }
        saved = true;
        break;
    } while(true);

    if(!saved) {
        FURI_LOG_E(TAG, "Error save file %s", SUBRATT_SETTINGS_PATH);
    }

    flipper_format_file_close(file);
    flipper_format_free(file);
    furi_record_close(RECORD_STORAGE);

    return saved;
}

void subratt_settings_set_value(SubRattSettings* instance, SubRattAttacks index, uint8_t value) {
    furi_assert(instance);

    instance->repeat_values[index] = value;
}
uint8_t subratt_settings_get_value(SubRattSettings* instance, SubRattAttacks index) {
    furi_assert(instance);

    return instance->repeat_values[index];
}

void subratt_settings_set_repeats(SubRattSettings* instance, const uint8_t* repeated_values) {
    furi_assert(instance);

    for(size_t i = 0; i < SubRattAttackTotalCount; i++) {
        instance->repeat_values[i] = repeated_values[i];
    }
}

uint8_t subratt_settings_get_current_repeats(SubRattSettings* instance) {
    furi_assert(instance);

    return instance->repeat_values[instance->last_index];
}
