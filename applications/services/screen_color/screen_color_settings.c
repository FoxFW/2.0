#include "screen_color_settings.h"

#include <furi.h>
#include <saved_struct.h>
#include <storage/storage.h>

#define SCREEN_COLOR_SETTINGS_FILE_NAME ".screen_color.settings"
#define SCREEN_COLOR_SETTINGS_PATH      INT_PATH(SCREEN_COLOR_SETTINGS_FILE_NAME)
#define SCREEN_COLOR_SETTINGS_VER       (1)
#define SCREEN_COLOR_SETTINGS_MAGIC     (0x1B)

void screen_color_settings_load(ScreenColorSettings* settings) {
    furi_assert(settings);

    bool success = saved_struct_load(
        SCREEN_COLOR_SETTINGS_PATH,
        settings,
        sizeof(ScreenColorSettings),
        SCREEN_COLOR_SETTINGS_MAGIC,
        SCREEN_COLOR_SETTINGS_VER);

    if(!success) {
        settings->fg_mode = ScreenColorModeDefault;
        settings->fg_r = settings->fg_g = settings->fg_b = 0;
        settings->bg_mode = ScreenColorModeDefault;
        settings->bg_r = settings->bg_g = settings->bg_b = 0;
        screen_color_settings_save(settings);
    }
}

void screen_color_settings_save(const ScreenColorSettings* settings) {
    furi_assert(settings);

    saved_struct_save(
        SCREEN_COLOR_SETTINGS_PATH,
        settings,
        sizeof(ScreenColorSettings),
        SCREEN_COLOR_SETTINGS_MAGIC,
        SCREEN_COLOR_SETTINGS_VER);
}

uint32_t screen_color_pack(uint8_t mode, uint8_t r, uint8_t g, uint8_t b) {
    return (uint32_t)mode | ((uint32_t)r << 8) | ((uint32_t)g << 16) | ((uint32_t)b << 24);
}

void screen_color_on_system_start(void) {
}
