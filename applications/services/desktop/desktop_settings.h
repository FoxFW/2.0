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

#define FOX_ALARM_MAX_COUNT 8

/* Bitmask values for FoxAlarm.days_mask - which days a recurring alarm
 * repeats on. Unused when recurring == 0 (one-time alarms just fire at
 * their next matching hour:minute, whatever day that lands on). */
#define FOX_ALARM_DAY_SUN (1 << 0)
#define FOX_ALARM_DAY_MON (1 << 1)
#define FOX_ALARM_DAY_TUE (1 << 2)
#define FOX_ALARM_DAY_WED (1 << 3)
#define FOX_ALARM_DAY_THU (1 << 4)
#define FOX_ALARM_DAY_FRI (1 << 5)
#define FOX_ALARM_DAY_SAT (1 << 6)

typedef struct {
    uint8_t hour;      /* 0-23 */
    uint8_t minute;    /* 0-59 */
    uint8_t days_mask; /* FOX_ALARM_DAY_* bitmask, only meaningful when recurring */
    uint8_t active;    /* 1 = enabled */
    uint8_t recurring; /* 1 = repeats weekly on days_mask; 0 = fires once at the
                         * next matching time then auto-deactivates */
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
    uint8_t menu_theme;       /* MenuTheme enum */
    uint8_t wifi_icon_hidden; /* 0 = show (default), 1 = hide */
    char wallpaper_filename[64]; /* selected file in /ext/wallpapers, e.g. "Default.xbm" */
    uint8_t allow_poweroff_locked; /* 0 = OFF (default), 1 = long-press Back on lock screen powers off */
    uint8_t lock_show_time;        /* Big clock on the lock screen itself */
    uint8_t lock_show_seconds;     /* Append :SS to the lock screen clock */
    uint8_t lock_show_date;        /* Show date on the lock screen */
    uint8_t lock_show_statusbar;   /* 0 = hide clock/wifi/stealth status icons while locked */
    uint8_t lock_unlock_prompt;    /* 0 = hide the "Back x3 to unlock" / "Unlocked" hint text */
    uint8_t statusbar_show_icons;      /* 0 = hide all status bar icons */
    uint8_t clock_midnight_zero;       /* 12h format at midnight: 0 = show "12" (default), 1 = show "0" */

    /* Fox Alarm Clock (Fox Settings). Fires in the background regardless of
     * which app is running - see desktop.c's alarm_check_timer. */
    FoxAlarm alarms[FOX_ALARM_MAX_COUNT];
    uint8_t alarm_count;                    /* alarms[0..alarm_count-1] are in use */
    uint8_t alarm_keep_backlight_all_night; /* 1 = backlight stays on while Fox Clock is
                                              * showing instead of timing out normally */
    uint8_t alarm_beep_enabled;
    uint8_t alarm_vibrate_enabled;
} DesktopSettings;

void desktop_settings_load(DesktopSettings* settings);
void desktop_settings_save(const DesktopSettings* settings);

#ifdef __cplusplus
}
#endif
