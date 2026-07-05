#pragma once

#include <stdint.h>

#define DISPLAY_BATTERY_BAR              0
#define DISPLAY_BATTERY_PERCENT          1
#define DISPLAY_BATTERY_INVERTED_PERCENT 2
#define DISPLAY_BATTERY_RETRO_3          3
#define DISPLAY_BATTERY_RETRO_5          4
#define DISPLAY_BATTERY_BAR_PERCENT      5

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    FavoriteAppLeftShort,
    FavoriteAppLeftLong,
    FavoriteAppRightShort,
    FavoriteAppRightLong,
    FavoriteAppOkLong,

    FavoriteAppNumber,
} FavoriteAppShortcut;

typedef struct {
    char name_or_path[128];
} FavoriteApp;

typedef enum {
    LockUsbLevelOff = 0,
    LockUsbLevelSessionBlock = 1,   // "CLI + RPC" — USB stays connected, sessions blocked
    LockUsbLevelFullDisconnect = 2, // "Full Disconnect" — physical USB teardown
} LockUsbLevel;

typedef enum {
    MenuThemeClassic = 0, // original 3-item scrolling list
    MenuThemeFox     = 1, // FoxFW 3×2 grid
} MenuTheme;

typedef struct {
    uint32_t auto_lock_delay_ms;
    uint8_t usb_inhibit_auto_lock;
    uint8_t displayBatteryPercentage;
    uint8_t display_clock;
    FavoriteApp favorite_apps[FavoriteAppNumber];
    uint8_t pin_max_attempts;
    uint8_t pin_exceed_action;
    uint8_t wallpaper_enabled;
    uint8_t lock_on_lock_enabled;
    uint8_t lock_disconnect_ble;
    uint8_t lock_disconnect_gpio;
    uint8_t lock_usb_level;
    uint8_t menu_theme; // MenuTheme enum
} DesktopSettings;

void desktop_settings_load(DesktopSettings* settings);
void desktop_settings_save(const DesktopSettings* settings);

#ifdef __cplusplus
}
#endif
