#include "power_settings.h"
#include "power_settings_filename.h"

#include <saved_struct.h>
#include <storage/storage.h>

#define TAG "PowerSettings"

#define POWER_SETTINGS_VER_1 (1)
#define POWER_SETTINGS_VER   (2)

#define POWER_SETTINGS_PATH     INT_PATH(POWER_SETTINGS_FILE_NAME)
#define POWER_SETTINGS_MAGIC_V1 (0x19)
#define POWER_SETTINGS_MAGIC    (0x21)

typedef struct {
    uint32_t auto_poweroff_delay_ms;
} PowerSettingsPrevious;

void power_settings_load(PowerSettings* settings) {
    furi_assert(settings);

    bool success = false;

    do {
        uint8_t version;
        if(!saved_struct_get_metadata(POWER_SETTINGS_PATH, NULL, &version, NULL)) break;

        if(version == POWER_SETTINGS_VER) {
            success = saved_struct_load(
                POWER_SETTINGS_PATH,
                settings,
                sizeof(PowerSettings),
                POWER_SETTINGS_MAGIC,
                POWER_SETTINGS_VER);

        } else if(version == POWER_SETTINGS_VER_1) {
            PowerSettingsPrevious* settings_previous = malloc(sizeof(PowerSettingsPrevious));

            success = saved_struct_load(
                POWER_SETTINGS_PATH,
                settings_previous,
                sizeof(PowerSettingsPrevious),
                POWER_SETTINGS_MAGIC_V1,
                POWER_SETTINGS_VER_1);

            if(success) {
                settings->auto_poweroff_delay_ms = settings_previous->auto_poweroff_delay_ms;
                settings->charge_supress_percent = 0;
            }

            free(settings_previous);
        }

    } while(false);

    if(!success) {
        FURI_LOG_W(TAG, "Failed to load file, using defaults");
        memset(settings, 0, sizeof(PowerSettings));
        power_settings_save(settings);
    }
}

void power_settings_save(const PowerSettings* settings) {
    furi_assert(settings);

    const bool success = saved_struct_save(
        POWER_SETTINGS_PATH,
        settings,
        sizeof(PowerSettings),
        POWER_SETTINGS_MAGIC,
        POWER_SETTINGS_VER);

    if(!success) {
        FURI_LOG_E(TAG, "Failed to save file");
    }
}
