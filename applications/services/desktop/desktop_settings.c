#include "desktop_settings.h"
#include "desktop_settings_filename.h"
 
#include <saved_struct.h>
#include <storage/storage.h>
 
#define TAG "DesktopSettings"
 
#define DESKTOP_SETTINGS_VER_17 (17)
#define DESKTOP_SETTINGS_VER_18 (18)
#define DESKTOP_SETTINGS_VER_19 (19)
#define DESKTOP_SETTINGS_VER_20 (20)
#define DESKTOP_SETTINGS_VER_21 (21)
#define DESKTOP_SETTINGS_VER    (22)
 
#define DESKTOP_SETTINGS_PATH  INT_PATH(DESKTOP_SETTINGS_FILE_NAME)
#define DESKTOP_SETTINGS_MAGIC (0x17)
 
typedef struct {
    uint32_t auto_lock_delay_ms;
    uint8_t usb_inhibit_auto_lock;
    uint8_t displayBatteryPercentage;
    uint8_t dummy_mode;
    uint8_t display_clock;
    FavoriteApp favorite_apps[FavoriteAppNumber];
    FavoriteApp dummy_apps[9];
} DesktopSettingsV17;
 
typedef struct {
    uint32_t auto_lock_delay_ms;
    uint8_t usb_inhibit_auto_lock;
    uint8_t displayBatteryPercentage;
    uint8_t display_clock;
    FavoriteApp favorite_apps[FavoriteAppNumber];
} DesktopSettingsV18;
 
typedef struct {
    uint32_t auto_lock_delay_ms;
    uint8_t usb_inhibit_auto_lock;
    uint8_t displayBatteryPercentage;
    uint8_t display_clock;
    FavoriteApp favorite_apps[FavoriteAppNumber];
    uint8_t pin_max_attempts;
    uint8_t pin_exceed_action;
} DesktopSettingsV19;
 
typedef struct {
    uint32_t auto_lock_delay_ms;
    uint8_t usb_inhibit_auto_lock;
    uint8_t displayBatteryPercentage;
    uint8_t display_clock;
    FavoriteApp favorite_apps[FavoriteAppNumber];
    uint8_t pin_max_attempts;
    uint8_t pin_exceed_action;
    uint8_t wallpaper_enabled;
    uint8_t lock_ble_usb_disconnect;
} DesktopSettingsV20;

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
} DesktopSettingsV21;

void desktop_settings_load(DesktopSettings* settings) {
    furi_assert(settings);
 
    bool success = false;
 
    do {
        uint8_t version;
        if(!saved_struct_get_metadata(DESKTOP_SETTINGS_PATH, NULL, &version, NULL)) break;
 
        if(version == DESKTOP_SETTINGS_VER) {
            success = saved_struct_load(
                DESKTOP_SETTINGS_PATH,
                settings,
                sizeof(DesktopSettings),
                DESKTOP_SETTINGS_MAGIC,
                DESKTOP_SETTINGS_VER);

        } else if(version == DESKTOP_SETTINGS_VER_21) {
            DesktopSettingsV21* s = malloc(sizeof(DesktopSettingsV21));

            success = saved_struct_load(
                DESKTOP_SETTINGS_PATH,
                s,
                sizeof(DesktopSettingsV21),
                DESKTOP_SETTINGS_MAGIC,
                DESKTOP_SETTINGS_VER_21);

            if(success) {
                settings->auto_lock_delay_ms      = s->auto_lock_delay_ms;
                settings->usb_inhibit_auto_lock   = s->usb_inhibit_auto_lock;
                settings->displayBatteryPercentage = s->displayBatteryPercentage;
                settings->display_clock           = s->display_clock;
                memcpy(settings->favorite_apps, s->favorite_apps, sizeof(settings->favorite_apps));
                settings->pin_max_attempts        = s->pin_max_attempts;
                settings->pin_exceed_action       = s->pin_exceed_action;
                settings->wallpaper_enabled       = s->wallpaper_enabled;
                settings->lock_on_lock_enabled    = s->lock_on_lock_enabled;
                settings->lock_disconnect_ble     = s->lock_disconnect_ble;
                settings->lock_disconnect_gpio    = s->lock_disconnect_gpio;
                settings->lock_usb_level          = s->lock_usb_level;
                settings->menu_theme              = MenuThemeFox; // new users get Fox theme by default
            }

            free(s);

        } else if(version == DESKTOP_SETTINGS_VER_20) {
            DesktopSettingsV20* s = malloc(sizeof(DesktopSettingsV20));

            success = saved_struct_load(
                DESKTOP_SETTINGS_PATH,
                s,
                sizeof(DesktopSettingsV20),
                DESKTOP_SETTINGS_MAGIC,
                DESKTOP_SETTINGS_VER_20);

            if(success) {
                settings->auto_lock_delay_ms        = s->auto_lock_delay_ms;
                settings->usb_inhibit_auto_lock      = s->usb_inhibit_auto_lock;
                settings->displayBatteryPercentage   = s->displayBatteryPercentage;
                settings->display_clock              = s->display_clock;
                memcpy(settings->favorite_apps, s->favorite_apps, sizeof(settings->favorite_apps));
                settings->pin_max_attempts           = s->pin_max_attempts;
                settings->pin_exceed_action          = s->pin_exceed_action;
                settings->wallpaper_enabled          = s->wallpaper_enabled;
                // Migrate: if the old single toggle was ON, preserve as Full Disconnect + BLE
                if(s->lock_ble_usb_disconnect) {
                    settings->lock_on_lock_enabled  = 1;
                    settings->lock_disconnect_ble   = 1;
                    settings->lock_disconnect_gpio  = 0;
                    settings->lock_usb_level        = LockUsbLevelFullDisconnect;
                    settings->menu_theme            = MenuThemeFox;
                } else {
                    settings->lock_on_lock_enabled  = 0;
                    settings->lock_disconnect_ble   = 0;
                    settings->lock_disconnect_gpio  = 0;
                    settings->lock_usb_level        = LockUsbLevelOff;
                }
            }

            free(s);

        } else if(version == DESKTOP_SETTINGS_VER_19) {
            DesktopSettingsV19* s = malloc(sizeof(DesktopSettingsV19));
 
            success = saved_struct_load(
                DESKTOP_SETTINGS_PATH,
                s,
                sizeof(DesktopSettingsV19),
                DESKTOP_SETTINGS_MAGIC,
                DESKTOP_SETTINGS_VER_19);
 
            if(success) {
                settings->auto_lock_delay_ms   = s->auto_lock_delay_ms;
                settings->usb_inhibit_auto_lock = s->usb_inhibit_auto_lock;
                settings->displayBatteryPercentage = s->displayBatteryPercentage;
                settings->display_clock        = s->display_clock;
                memcpy(settings->favorite_apps, s->favorite_apps, sizeof(settings->favorite_apps));
                settings->pin_max_attempts     = s->pin_max_attempts;
                settings->pin_exceed_action    = s->pin_exceed_action;
                settings->wallpaper_enabled       = 0;
                settings->lock_on_lock_enabled    = 0;
                settings->lock_disconnect_ble     = 0;
                settings->lock_disconnect_gpio    = 0;
                settings->lock_usb_level          = LockUsbLevelOff;
                settings->menu_theme              = MenuThemeFox;
            }
 
            free(s);
 
        } else if(version == DESKTOP_SETTINGS_VER_18) {
            DesktopSettingsV18* s = malloc(sizeof(DesktopSettingsV18));
 
            success = saved_struct_load(
                DESKTOP_SETTINGS_PATH,
                s,
                sizeof(DesktopSettingsV18),
                DESKTOP_SETTINGS_MAGIC,
                DESKTOP_SETTINGS_VER_18);
 
            if(success) {
                settings->auto_lock_delay_ms   = s->auto_lock_delay_ms;
                settings->usb_inhibit_auto_lock = s->usb_inhibit_auto_lock;
                settings->displayBatteryPercentage = s->displayBatteryPercentage;
                settings->display_clock        = s->display_clock;
                memcpy(settings->favorite_apps, s->favorite_apps, sizeof(settings->favorite_apps));
                settings->pin_max_attempts     = 0;
                settings->pin_exceed_action    = 0;
                settings->wallpaper_enabled       = 0;
                settings->lock_on_lock_enabled    = 0;
                settings->lock_disconnect_ble     = 0;
                settings->lock_disconnect_gpio    = 0;
                settings->lock_usb_level          = LockUsbLevelOff;
                settings->menu_theme              = MenuThemeFox;
            }
 
            free(s);
 
        } else if(version == DESKTOP_SETTINGS_VER_17) {
            DesktopSettingsV17* s = malloc(sizeof(DesktopSettingsV17));
 
            success = saved_struct_load(
                DESKTOP_SETTINGS_PATH,
                s,
                sizeof(DesktopSettingsV17),
                DESKTOP_SETTINGS_MAGIC,
                DESKTOP_SETTINGS_VER_17);
 
            if(success) {
                settings->auto_lock_delay_ms   = s->auto_lock_delay_ms;
                settings->usb_inhibit_auto_lock = s->usb_inhibit_auto_lock;
                settings->displayBatteryPercentage = s->displayBatteryPercentage;
                settings->display_clock        = s->display_clock;
                memcpy(settings->favorite_apps, s->favorite_apps, sizeof(settings->favorite_apps));
                settings->pin_max_attempts     = 0;
                settings->pin_exceed_action    = 0;
                settings->wallpaper_enabled       = 0;
                settings->lock_on_lock_enabled    = 0;
                settings->lock_disconnect_ble     = 0;
                settings->lock_disconnect_gpio    = 0;
                settings->lock_usb_level          = LockUsbLevelOff;
                settings->menu_theme              = MenuThemeFox;
            }
 
            free(s);
        }
 
    } while(false);
 
    if(!success) {
        FURI_LOG_W(TAG, "Failed to load file, using defaults");
        memset(settings, 0, sizeof(DesktopSettings));
        settings->menu_theme = MenuThemeFox;

        /* Default all arrow-key shortcuts to FFV so users immediately get
         * the file viewer instead of the apps menu or the old SubGHz fallback. */
        const char ffv_path[] = "/ext/apps/Settings/ffb.fap";
        strlcpy(settings->favorite_apps[FavoriteAppLeftShort].name_or_path,
                ffv_path, sizeof(settings->favorite_apps[0].name_or_path));
        strlcpy(settings->favorite_apps[FavoriteAppLeftLong].name_or_path,
                ffv_path, sizeof(settings->favorite_apps[0].name_or_path));
        strlcpy(settings->favorite_apps[FavoriteAppRightShort].name_or_path,
                ffv_path, sizeof(settings->favorite_apps[0].name_or_path));
        strlcpy(settings->favorite_apps[FavoriteAppRightLong].name_or_path,
                ffv_path, sizeof(settings->favorite_apps[0].name_or_path));
        strlcpy(settings->favorite_apps[FavoriteAppOkLong].name_or_path,
                ffv_path, sizeof(settings->favorite_apps[0].name_or_path));

        desktop_settings_save(settings);
    }
}
 
void desktop_settings_save(const DesktopSettings* settings) {
    furi_assert(settings);
 
    const bool success = saved_struct_save(
        DESKTOP_SETTINGS_PATH,
        settings,
        sizeof(DesktopSettings),
        DESKTOP_SETTINGS_MAGIC,
        DESKTOP_SETTINGS_VER);
 
    if(!success) {
        FURI_LOG_E(TAG, "Failed to save file");
    }
}