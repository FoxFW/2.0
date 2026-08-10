#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* VGM Options (ported from Momentum Firmware): lets the RPC screen stream
 * (used by qFlipper / companion screen-mirroring tools) render in colors
 * other than plain black/white. Shared globally, same pattern as
 * gpio_remap_settings and cli_settings. */

typedef enum {
    ScreenColorModeDefault = 0, /* Plain black/white, stock behavior */
    ScreenColorModeCustom, /* Fixed RGB value below */
    ScreenColorModeRainbow, /* Receiving client animates its own rainbow */
    ScreenColorModeRgbBacklight, /* Mirrors this device's live RGB backlight color */
} ScreenColorMode;

typedef struct {
    uint8_t fg_mode; /* ScreenColorMode */
    uint8_t fg_r;
    uint8_t fg_g;
    uint8_t fg_b;
    uint8_t bg_mode; /* ScreenColorMode */
    uint8_t bg_r;
    uint8_t bg_g;
    uint8_t bg_b;
} ScreenColorSettings;

void screen_color_settings_load(ScreenColorSettings* settings);
void screen_color_settings_save(const ScreenColorSettings* settings);

/* Packs (mode, r, g, b) into the wire format used by the ScreenFrame proto's
 * fg_color/bg_color fields, matching Momentum Firmware's ScreenFrameColor
 * union layout (mode in the low byte, then r, g, b). */
uint32_t screen_color_pack(uint8_t mode, uint8_t r, uint8_t g, uint8_t b);

void screen_color_on_system_start(void);

#ifdef __cplusplus
}
#endif
