#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ScreenColorModeDefault = 0,
    ScreenColorModeCustom,
    ScreenColorModeRainbow,
    ScreenColorModeRgbBacklight,
} ScreenColorMode;

typedef struct {
    uint8_t fg_mode;
    uint8_t fg_r;
    uint8_t fg_g;
    uint8_t fg_b;
    uint8_t bg_mode;
    uint8_t bg_r;
    uint8_t bg_g;
    uint8_t bg_b;
} ScreenColorSettings;

void screen_color_settings_load(ScreenColorSettings* settings);
void screen_color_settings_save(const ScreenColorSettings* settings);

uint32_t screen_color_pack(uint8_t mode, uint8_t r, uint8_t g, uint8_t b);

void screen_color_on_system_start(void);

#ifdef __cplusplus
}
#endif
