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
    LockUsbLevelSessionBlock = 1,
    LockUsbLevelFullDisconnect = 2,
} LockUsbLevel;

typedef enum {
    MenuThemeClassic  = 0,
    MenuThemeFox      = 1,
    MenuThemeCarousel = 2,
    MenuThemeSlider   = 3,
    MenuThemeTiny     = 4,
} MenuTheme;

#define FOX_ALARM_MAX_COUNT 8

#define FOX_ALARM_DAY_SUN (1 << 0)
#define FOX_ALARM_DAY_MON (1 << 1)
#define FOX_ALARM_DAY_TUE (1 << 2)
#define FOX_ALARM_DAY_WED (1 << 3)
#define FOX_ALARM_DAY_THU (1 << 4)
#define FOX_ALARM_DAY_FRI (1 << 5)
#define FOX_ALARM_DAY_SAT (1 << 6)

typedef struct {
    uint8_t hour;
    uint8_t minute;
    uint8_t days_mask;
    uint8_t active;
    uint8_t recurring;

} FoxAlarm;

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
    uint8_t menu_theme;
    uint8_t wifi_icon_hidden;
    char wallpaper_filename[64];
    uint8_t allow_poweroff_locked;
    uint8_t lock_show_time;
    uint8_t lock_show_seconds;
    uint8_t lock_show_date;
    uint8_t lock_show_statusbar;
    uint8_t lock_unlock_prompt;
    uint8_t statusbar_show_icons;
    uint8_t clock_midnight_zero;

    FoxAlarm alarms[FOX_ALARM_MAX_COUNT];
    uint8_t alarm_count;
    uint8_t alarm_keep_backlight_all_night;

    uint8_t alarm_beep_enabled;
    uint8_t alarm_vibrate_enabled;
} DesktopSettings;

void desktop_settings_load(DesktopSettings* settings);
void desktop_settings_save(const DesktopSettings* settings);

#ifdef __cplusplus
}
#endif
