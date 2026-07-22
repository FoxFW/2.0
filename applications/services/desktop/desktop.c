#include "desktop_i.h"

#include <cli/cli_vcp.h>
#include <bt/bt_service/bt.h>
#include <furi_hal_serial_control.h>
#include <furi_hal.h>
#include <expansion/expansion.h>
#include <gui/gui_i.h>
#include <locale/locale.h>
#include <storage/storage.h>
#include <assets_icons.h>
#include <version.h>

#include "scenes/desktop_scene.h"
#include "scenes/desktop_scene_locked.h"
#include "helpers/pin_code.h"
#include "furi_hal_power.h"
#include <power/power_service/power.h>
#include <gui/modules/fox_theme.h>
#include <namechanger/namechanger.h>
#include <flipper_format/flipper_format.h>

#define TAG "Desktop"

static FuriHalSerialHandle* s_locked_gpio_usart  = NULL;
static FuriHalSerialHandle* s_locked_gpio_lpuart = NULL;
static FuriHalUsbInterface* s_locked_usb_config  = NULL; // saved USB config, restored on unlock
static bool s_animation_was_stalled = false;

#define WALLPAPER_PATH "/ext/wallpaper.xbm"
#define WALLPAPER_SIZE 1024

#define FOX_SETUP_FLAG_PATH      "/int/fox_setup.done"
#define FOX_SETUP_FLAG_EXT_PATH  "/ext/System/.fox_setup.done"  /* EXT mirror — both must be absent to bypass */
#define FOX_SETUP_AUTO_ARG   "auto"

/* Fox ESP32 companion apps (fox_esp32_commander etc.) write "1" or "0"
 * here whenever they confirm the ESP32's WiFi connect state changes -
 * see fox_esp32_commander's wifi_menu.c. Unrelated to the FOX_SETUP_*
 * flags above (different "Fox" subsystem, same project prefix). The
 * icon itself only ever reads this file (see
 * desktop_wifi_status_timer_callback() below) - it never touches the
 * UART directly, because only one thing can own the ESP32's serial
 * connection at a time and that's whichever Fox app the user
 * currently has open; the icon polling the UART itself on every tick
 * would fight whatever app is running.
 *
 * That used to mean this file could go stale forever with nobody
 * around to correct it - e.g. reflash the ESP32 (or unplug it) while
 * every Fox app is closed, and the icon would just keep showing
 * whatever it last said, since nothing was left to write "0". See
 * desktop_wifi_recheck_thread() further down for the fix: a slow
 * background probe that only touches the UART when nothing else is
 * using it, and rewrites this same file when it gets a real answer.
 * See desktop_wifi_icon_draw_callback()'s header comment for the rest
 * of the icon-drawing reasoning. */
#define FOX_ESP32_WIFI_STATUS_PATH EXT_PATH("apps_data/fox_esp32/wifi_status.txt")
#define FOX_ESP32_WIFI_POLL_MS 2000

static void desktop_auto_lock_arm(Desktop*);
static void desktop_auto_lock_inhibit(Desktop*);
static void desktop_start_auto_lock_timer(Desktop*);
static void desktop_apply_settings(Desktop*);
static void desktop_load_wallpaper(Desktop*);

static void fox_no_sd_draw_callback(Canvas* canvas, void* context);

static void fox_lockout_draw_callback(Canvas* canvas, void* context) {
    UNUSED(context);
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_icon(canvas, 2, 4, &I_fox_32x32);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 38, 14, "LOCKED!");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 38, 26, "Wrong PIN entered");
    canvas_draw_str(canvas, 38, 35, "too many times!");
    canvas_draw_str(canvas, 38, 45, "DFU > Repair >");
    canvas_draw_str(canvas, 38, 54, "Erase.");
}

static void fox_lockout_input_callback(InputEvent* event, void* context) {
    UNUSED(event);
    UNUSED(context);
}

static volatile bool s_sd_ejected_during_lockout = false;

static void fox_sd_eject_pubsub_callback(const void* message, void* context) {
    UNUSED(context);
    const StorageEvent* evt = message;
    if(evt->type == StorageEventTypeCardUnmount) {
        s_sd_ejected_during_lockout = true;
    } else if(evt->type == StorageEventTypeCardMount) {
        furi_hal_power_reset();
    }
}

static void fox_desktop_show_lockout_blocking(Desktop* desktop) {
    s_sd_ejected_during_lockout = false;
    FuriPubSubSubscription* sub = furi_pubsub_subscribe(
        storage_get_pubsub(desktop->storage), fox_sd_eject_pubsub_callback, NULL);

    ViewPort* lock_vp = view_port_alloc();
    view_port_draw_callback_set(lock_vp, fox_lockout_draw_callback, NULL);
    view_port_input_callback_set(lock_vp, fox_lockout_input_callback, NULL);
    gui_add_view_port(desktop->gui, lock_vp, GuiLayerFullscreen);

    if(furi_hal_usb_get_config() != NULL) {
        furi_hal_usb_set_config(NULL, NULL);
    }

    ViewPort* sd_overlay = NULL;

    while(true) {
        furi_delay_ms(1200);

        bool sd_gone = s_sd_ejected_during_lockout ||
                       (storage_sd_status(desktop->storage) != FSE_OK);

        if(sd_gone && sd_overlay == NULL) {
            sd_overlay = view_port_alloc();
            view_port_draw_callback_set(sd_overlay, fox_no_sd_draw_callback, NULL);
            view_port_input_callback_set(sd_overlay, fox_lockout_input_callback, NULL);
            gui_add_view_port(desktop->gui, sd_overlay, GuiLayerFullscreen);
        } else if(!sd_gone && sd_overlay != NULL) {
            furi_pubsub_unsubscribe(storage_get_pubsub(desktop->storage), sub);
            furi_hal_power_reset();
        }

        if(fox_recovery_check_and_reset()) {
            desktop_pin_code_reset();
            storage_common_remove(desktop->storage, FOX_LOCKOUT_FLAG_PATH);
            furi_pubsub_unsubscribe(storage_get_pubsub(desktop->storage), sub);
            furi_hal_power_reset();
        }
    }
}

/* SD-format blocking screen — shown when wipe_method=1 wiped the SD card.
 * No recovery loop: the device must be re-flashed via DFU. */
static void fox_format_draw_callback(Canvas* canvas, void* context) {
    UNUSED(context);
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_icon(canvas, 2, 0, &I_fox_32x32);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 38, 10, "SD Formatted!");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 38, 22, "PIN limit exceeded.");
    canvas_draw_str(canvas, 2,  38, "All data deleted.");
    canvas_draw_str(canvas, 2,  48, "DFU to recover.");
    canvas_draw_str(canvas, 2,  58, "See Help Files!");
}

static void fox_desktop_show_format_blocking(Desktop* desktop) {
    ViewPort* vp = view_port_alloc();
    view_port_draw_callback_set(vp, fox_format_draw_callback, NULL);
    view_port_input_callback_set(vp, fox_lockout_input_callback, NULL);
    gui_add_view_port(desktop->gui, vp, GuiLayerFullscreen);

    if(furi_hal_usb_get_config() != NULL) {
        furi_hal_usb_set_config(NULL, NULL);
    }

    while(true) {
        furi_delay_ms(10000);
    }
}

static void fox_corrupt_draw_callback(Canvas* canvas, void* context) {
    UNUSED(context);
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 8, AlignCenter, AlignTop, "Firmware Corrupt");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 26, AlignCenter, AlignTop, "Fox.data not found.");
    canvas_draw_str_aligned(canvas, 64, 36, AlignCenter, AlignTop, "Please re-install");
    canvas_draw_str_aligned(canvas, 64, 46, AlignCenter, AlignTop, "FoxFW firmware.");
}

static void fox_desktop_show_corrupt_blocking(Desktop* desktop) {
    ViewPort* vp = view_port_alloc();
    view_port_draw_callback_set(vp, fox_corrupt_draw_callback, NULL);
    view_port_input_callback_set(vp, fox_lockout_input_callback, NULL);
    gui_add_view_port(desktop->gui, vp, GuiLayerFullscreen);
    if(furi_hal_usb_get_config() != NULL) {
        furi_hal_usb_set_config(NULL, NULL);
    }
    while(true) {
        furi_delay_ms(60000);  // No escape — re-flash via DFU required
    }
}

static const uint8_t s_sample_wallpaper_xbm[WALLPAPER_SIZE] = {
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80,
    0xfd, 0xff, 0xff, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfe, 0xff, 0xff, 0xbf,
    0xfd, 0xff, 0xff, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfe, 0xff, 0xff, 0xbf,
    0xfd, 0x10, 0xfe, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x86, 0x83, 0x41, 0xb8,
    0xfd, 0x9e, 0xfc, 0x01, 0x00, 0x00, 0x80, 0x03, 0x1e, 0x00, 0x00, 0x00, 0x66, 0xbb, 0xf7, 0xbe,
    0xfd, 0x9e, 0xfd, 0x01, 0x00, 0x00, 0xc0, 0x06, 0x33, 0x00, 0x00, 0x00, 0x66, 0xbb, 0xf7, 0xbe,
    0xfd, 0x90, 0xfd, 0x01, 0x00, 0x00, 0x00, 0x06, 0x33, 0x00, 0x00, 0x00, 0x86, 0x83, 0xf7, 0xbe,
    0xfd, 0x93, 0xfc, 0x01, 0x00, 0x00, 0xc0, 0x03, 0x33, 0x00, 0x00, 0x00, 0x66, 0x83, 0xf7, 0xbe,
    0xfd, 0x10, 0xfe, 0x01, 0x00, 0x00, 0x40, 0x60, 0x33, 0x00, 0x00, 0x00, 0x66, 0xbb, 0xf7, 0xbe,
    0xfd, 0xff, 0xff, 0x01, 0x00, 0x00, 0xc0, 0x67, 0x1e, 0x00, 0x00, 0x00, 0x86, 0xbb, 0xf7, 0xbe,
    0xfd, 0xff, 0xff, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfe, 0xff, 0xff, 0xbf,
    0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xa0,
    0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xa0,
    0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xa0,
    0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xa0,
    0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xa0,
    0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xa0,
    0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xa0,
    0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xa0,
    0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xa0,
    0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xa0,
    0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xa0,
    0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xa0,
    0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xa0,
    0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xa0,
    0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xa0,
    0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xa0,
    0x05, 0x60, 0xcc, 0xc7, 0xdc, 0x07, 0x0c, 0x3c, 0xf8, 0xf0, 0x80, 0x19, 0xcf, 0x8f, 0x0f, 0xa0,
    0x05, 0x60, 0xcc, 0xc7, 0xdc, 0x07, 0x0c, 0x3c, 0xf8, 0xf0, 0x80, 0x19, 0xcf, 0x8f, 0x0f, 0xa0,
    0x05, 0x60, 0xec, 0xcf, 0xdc, 0x0f, 0x0c, 0x7e, 0xfc, 0xfc, 0x81, 0x19, 0xcf, 0x9f, 0x0f, 0xa0,
    0x05, 0x60, 0x6c, 0xce, 0xdc, 0x0c, 0x0c, 0xe6, 0x0c, 0x9c, 0x81, 0x19, 0xc3, 0x98, 0x01, 0xa0,
    0x05, 0x60, 0x6c, 0xce, 0xdc, 0x0c, 0x0c, 0xe6, 0x0c, 0x9c, 0x81, 0x19, 0xc3, 0x98, 0x01, 0xa0,
    0x05, 0xe0, 0x6f, 0xce, 0xdc, 0x07, 0x0c, 0xe6, 0x0c, 0x9c, 0x81, 0x1f, 0xcf, 0x8f, 0x0f, 0xa0,
    0x05, 0xc0, 0x67, 0xce, 0xdc, 0x0f, 0x0c, 0xe6, 0xcc, 0x9c, 0x81, 0x1f, 0xcf, 0x9f, 0x0f, 0xa0,
    0x05, 0xc0, 0x67, 0xce, 0xdc, 0x0f, 0x0c, 0xe6, 0xcc, 0x9c, 0x81, 0x1f, 0xcf, 0x9f, 0x0f, 0xa0,
    0x05, 0x80, 0x63, 0xce, 0xdc, 0x0c, 0x0c, 0xe6, 0xcc, 0x9c, 0x81, 0x19, 0xc3, 0x98, 0x01, 0xa0,
    0x05, 0x80, 0x63, 0xce, 0xdc, 0x0c, 0x0c, 0xe6, 0xcc, 0x9c, 0x81, 0x19, 0xc3, 0x98, 0x01, 0xa0,
    0x05, 0x80, 0xe3, 0xcf, 0xdf, 0x0c, 0x7c, 0x7e, 0xfc, 0xfc, 0x81, 0x19, 0xcf, 0x98, 0x0f, 0xa0,
    0x05, 0x80, 0xc3, 0x87, 0xc7, 0x0c, 0x7c, 0x3c, 0xf8, 0xf0, 0x80, 0x19, 0xcf, 0x98, 0x0f, 0xa0,
    0x05, 0x80, 0xc3, 0x87, 0xc7, 0x0c, 0x7c, 0x3c, 0xf8, 0xf0, 0x80, 0x19, 0xcf, 0x98, 0x0f, 0xa0,
    0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xa0,
    0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xa0,
    0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xa0,
    0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xa0,
    0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xa0,
    0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xa0,
    0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xa0,
    0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xa0,
    0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xa0,
    0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xa0,
    0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xa0,
    0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xa0,
    0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xa0,
    0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xa0,
    0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xa0,
    0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xa0,
    0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xa0,
    0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xa0,
    0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xa0,
    0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xa0,
    0xfd, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xbf,
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
};

static void desktop_ensure_wallpaper(Desktop* desktop) {
    if(storage_sd_status(desktop->storage) != FSE_OK) return;
    if(storage_file_exists(desktop->storage, WALLPAPER_PATH)) return;
    File* f = storage_file_alloc(desktop->storage);
    if(storage_file_open(f, WALLPAPER_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(f, s_sample_wallpaper_xbm, WALLPAPER_SIZE);
        storage_file_close(f);
        FURI_LOG_I("Desktop", "Default wallpaper created at %s", WALLPAPER_PATH);
    }
    storage_file_free(f);
}


// Boot-time SD card required blocking screen.
// Pubsub callback fires the moment storage mounts the card — restart is instant.
static volatile bool s_sd_mounted_event = false;

static void fox_sd_pubsub_callback(const void* message, void* context) {
    UNUSED(context);
    const StorageEvent* evt = message;
    if(evt->type == StorageEventTypeCardMount) {
        s_sd_mounted_event = true;
    }
}

static void fox_no_sd_draw_callback(Canvas* canvas, void* context) {
    UNUSED(context);
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_icon(canvas, 2, 6, &I_fox_32x32);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 38, 16, "SD Card Missing!");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 38, 28, "FoxFW requires an");
    canvas_draw_str(canvas, 38, 37, "SD Card to function.");
    canvas_draw_str(canvas, 38, 52, "Insert SD Card");
    canvas_draw_str(canvas, 38, 61, "to continue...");
}

static void fox_desktop_show_no_sd_blocking(Desktop* desktop) {
    s_sd_mounted_event = false;
    FuriPubSubSubscription* sub = furi_pubsub_subscribe(
        storage_get_pubsub(desktop->storage), fox_sd_pubsub_callback, NULL);

    ViewPort* vp = view_port_alloc();
    view_port_draw_callback_set(vp, fox_no_sd_draw_callback, NULL);
    view_port_input_callback_set(vp, fox_lockout_input_callback, NULL);
    gui_add_view_port(desktop->gui, vp, GuiLayerFullscreen);

    while(true) {
        furi_delay_ms(1200);
        if(s_sd_mounted_event || storage_sd_status(desktop->storage) == FSE_OK) {
            furi_pubsub_unsubscribe(storage_get_pubsub(desktop->storage), sub);
            furi_hal_power_reset();
        }
    }
}

#define FOX_SETUP_FAP_PATH EXT_PATH("apps/Fox/fox_setup.fap")

static FuriThread* s_fox_setup_launch_thread = NULL;

static int32_t desktop_fox_setup_launch_thread_fn(void* context) {
    Desktop* desktop = context;

    if(storage_file_exists(desktop->storage, FOX_SETUP_FLAG_PATH)) {
        return 0;
    }

    furi_delay_ms(200);

    if(!storage_file_exists(desktop->storage, FOX_SETUP_FAP_PATH)) {
        FURI_LOG_E("FoxSetup", "fox_setup.fap not found at %s — wizard cannot launch",
                   FOX_SETUP_FAP_PATH);
        /* If a slideshow was deferred waiting for fox_setup to exit, unblock
           it now — otherwise it stays stuck until the next app happens to run. */
        if(desktop->pending_slideshow) {
            view_dispatcher_send_custom_event(
                desktop->view_dispatcher, DesktopGlobalAfterAppFinished);
        }
        return 0;
    }

    FURI_LOG_I("FoxSetup", "Flag /int/fox_setup.done absent — launching %s", FOX_SETUP_FAP_PATH);

    FuriString* err = furi_string_alloc();
    LoaderStatus status = loader_start(desktop->loader, FOX_SETUP_FAP_PATH, FOX_SETUP_AUTO_ARG, err);
    if(status != LoaderStatusOk) {
        FURI_LOG_E("FoxSetup", "loader_start failed for fox_setup: %s (status=%d)",
                   furi_string_get_cstr(err), (int)status);
    }
    furi_string_free(err);
    return 0;
}

static void desktop_loader_callback(const void* message, void* context) {
    furi_assert(context);
    Desktop* desktop = context;
    const LoaderEvent* event = message;

    if(event->type == LoaderEventTypeApplicationBeforeLoad) {
        view_dispatcher_send_custom_event(desktop->view_dispatcher, DesktopGlobalBeforeAppStarted);
        furi_check(furi_semaphore_acquire(desktop->animation_semaphore, 3000) == FuriStatusOk);
    } else if(event->type == LoaderEventTypeNoMoreAppsInQueue) {
        view_dispatcher_send_custom_event(desktop->view_dispatcher, DesktopGlobalAfterAppFinished);
    }
}

static void desktop_storage_callback(const void* message, void* context) {
    furi_assert(context);
    Desktop* desktop = context;
    const StorageEvent* event = message;

    if(event->type == StorageEventTypeCardMount) {
        // If the "no SD" overlay is showing (mid-session ejection), request a safe
        // reboot via the view dispatcher thread rather than calling reset directly
        // from this storage callback context.
        if(desktop->no_sd_viewport != NULL) {
            view_dispatcher_send_custom_event(
                desktop->view_dispatcher, DesktopGlobalSdCardMounted);
        }
        view_dispatcher_send_custom_event(desktop->view_dispatcher, DesktopGlobalReloadSettings);
    } else if(event->type == StorageEventTypeCardUnmount) {
        // SD card was ejected mid-session — show blocking overlay immediately
        view_dispatcher_send_custom_event(desktop->view_dispatcher, DesktopGlobalSdCardRemoved);
    }
}

static void desktop_lock_icon_draw_callback(Canvas* canvas, void* context) {
    UNUSED(context);
    furi_assert(canvas);
    canvas_draw_icon(canvas, 0, 0, &I_Lock_7x8);
}

static void desktop_clock_update(Desktop* desktop) {
    furi_assert(desktop);

    DateTime curr_dt;
    furi_hal_rtc_get_datetime(&curr_dt);
    bool time_format_12 = locale_get_time_format() == LocaleTimeFormat12h;

    if(desktop->clock.hour != curr_dt.hour || desktop->clock.minute != curr_dt.minute ||
       desktop->clock.format_12 != time_format_12) {
        desktop->clock.format_12 = time_format_12;
        desktop->clock.hour = curr_dt.hour;
        desktop->clock.minute = curr_dt.minute;
        view_port_update(desktop->clock_viewport);
    }
}

static void desktop_clock_reconfigure(Desktop* desktop) {
    furi_assert(desktop);

    desktop_clock_update(desktop);

    if(desktop->settings.display_clock) {
        furi_timer_start(desktop->update_clock_timer, furi_ms_to_ticks(1000));
    } else {
        furi_timer_stop(desktop->update_clock_timer);
    }

    view_port_enabled_set(desktop->clock_viewport, desktop->settings.display_clock);
}

static void desktop_clock_draw_callback(Canvas* canvas, void* context) {
    furi_assert(context);
    furi_assert(canvas);

    Desktop* desktop = context;

    canvas_set_font(canvas, FontPrimary);

    uint8_t hour = desktop->clock.hour;
    if(desktop->clock.format_12) {
        if(hour > 12) hour -= 12;
        if(hour == 0) hour = 12;
    }

    char buffer[20];
    if(furi_hal_rtc_is_flag_set(FuriHalRtcFlagDebug)) {
        snprintf(buffer, sizeof(buffer), "D %02u:%02u", hour, desktop->clock.minute);
    } else {
        snprintf(buffer, sizeof(buffer), "%02u:%02u", hour, desktop->clock.minute);
    }

    canvas_draw_str_aligned(
        canvas, canvas_width(canvas) / 2, 8, AlignCenter, AlignBottom, buffer);
}

static void desktop_stealth_mode_icon_draw_callback(Canvas* canvas, void* context) {
    UNUSED(context);
    furi_assert(canvas);
    canvas_draw_icon(canvas, 0, 0, &I_Muted_8x8);
}

static void desktop_wifi_icon_draw_callback(Canvas* canvas, void* context) {
    Desktop* desktop = context;
    furi_assert(canvas);
    furi_assert(desktop);

    canvas_draw_icon(
        canvas, 2, 0, desktop->wifi_connected ? &I_WiFi_Connected_9x8 : &I_WiFi_Disconnected_9x8);
}

static void desktop_wifi_status_timer_callback(void* context) {
    Desktop* desktop = context;
    furi_assert(desktop);

    bool connected = false;
    File* f = storage_file_alloc(desktop->storage);
    if(storage_file_open(f, FOX_ESP32_WIFI_STATUS_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        char buf[1] = {0};
        if(storage_file_read(f, buf, 1) == 1) {
            connected = (buf[0] == '1');
        }
    }
    storage_file_close(f);
    storage_file_free(f);

    if(connected != desktop->wifi_connected) {
        desktop->wifi_connected = connected;
        view_port_update(desktop->wifi_icon_viewport);
    }

    gui_view_port_send_to_front(desktop->gui, desktop->wifi_icon_viewport);
}


#define FOX_ESP32_WIFI_RECHECK_MS        60000
#define FOX_ESP32_WIFI_PROBE_TIMEOUT_MS  500
#define FOX_ESP32_WIFI_PROBE_BAUD        115200

typedef struct {
    FuriStreamBuffer* stream;
} FoxWifiProbeCtx;

static void fox_wifi_probe_rx_callback(
    FuriHalSerialHandle* handle,
    FuriHalSerialRxEvent event,
    void* context) {
    FoxWifiProbeCtx* ctx = context;
    if(event == FuriHalSerialRxEventData) {
        uint8_t byte = furi_hal_serial_async_rx(handle);
        furi_stream_buffer_send(ctx->stream, &byte, 1, 0);
    }
}

/* Returns 1 for a confirmed "true" reply, 0 for a confirmed "false"
   reply, -1 if the UART was already owned by another app (port busy -
   a Fox app is running and managing wifi_status.txt itself; the caller
   must NOT overwrite the file this cycle), or -2 if we acquired the
   port ourselves but got no usable reply before the timeout (nothing
   wired to this pin pair, or whatever's out there didn't answer). The
   -1 vs -2 distinction matters to desktop_wifi_recheck_thread() below:
   -1 = skip the write, -2 = write "0" (genuinely nothing here). */
static int fox_wifi_probe_pins(FuriHalSerialId serial_id) {
    Expansion* expansion = furi_record_open(RECORD_EXPANSION);
    expansion_disable(expansion);

    FuriHalSerialHandle* handle = furi_hal_serial_control_acquire(serial_id);
    if(handle == NULL) {
        /* Already owned by a running Fox app - back off, don't contend. */
        expansion_enable(expansion);
        furi_record_close(RECORD_EXPANSION);
        return -1;
    }

    FuriHalBus bus = (serial_id == FuriHalSerialIdUsart) ? FuriHalBusUSART1 : FuriHalBusLPUART1;
    bool serial_owned = !furi_hal_bus_is_enabled(bus);
    if(serial_owned) {
        furi_hal_serial_init(handle, FOX_ESP32_WIFI_PROBE_BAUD);
    }
    furi_hal_serial_set_br(handle, FOX_ESP32_WIFI_PROBE_BAUD);

    FoxWifiProbeCtx ctx;
    ctx.stream = furi_stream_buffer_alloc(256, 1);
    furi_hal_serial_async_rx_start(handle, fox_wifi_probe_rx_callback, &ctx, false);

    static const char cmd[] = "[WIFI/STATUS]\r\n";
    furi_hal_serial_tx(handle, (const uint8_t*)cmd, strlen(cmd));

    char line[32];
    size_t line_len = 0;
    int result = -2; /* -2 = acquired port but no response yet */
    uint32_t deadline = furi_get_tick() + furi_ms_to_ticks(FOX_ESP32_WIFI_PROBE_TIMEOUT_MS);
    while(furi_get_tick() < deadline) {
        uint8_t byte;
        uint32_t remaining = deadline - furi_get_tick();
        if(furi_stream_buffer_receive(ctx.stream, &byte, 1, remaining) == 0) break;
        if(byte == '\n') {
            if(line_len > 0 && line[line_len - 1] == '\r') line_len--;
            line[line_len] = '\0';
            if(strcmp(line, "true") == 0) {
                result = 1;
                break;
            }
            if(strcmp(line, "false") == 0) {
                result = 0;
                break;
            }
            /* Some other line (banner text, an echoed prompt, whatever) -
               keep listening instead of giving up on the first miss. */
            line_len = 0;
        } else if(line_len < sizeof(line) - 1) {
            line[line_len++] = (char)byte;
        } else {
            line_len = 0; /* overlong line - drop it, resync on the next \n */
        }
    }

    furi_hal_serial_async_rx_stop(handle);
    if(serial_owned) {
        furi_hal_serial_deinit(handle);
    }
    furi_hal_serial_control_release(handle);
    expansion_enable(expansion);
    furi_record_close(RECORD_EXPANSION);
    furi_stream_buffer_free(ctx.stream);

    return result;
}

static void fox_wifi_status_write_raw(Storage* storage, bool connected) {
    storage_simply_mkdir(storage, "/ext/apps_data");
    storage_simply_mkdir(storage, "/ext/apps_data/fox_esp32");

    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, FOX_ESP32_WIFI_STATUS_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        const char* v = connected ? "1" : "0";
        storage_file_write(file, v, 1);
    }
    storage_file_close(file);
    storage_file_free(file);
}

static int32_t desktop_wifi_recheck_thread(void* context) {
    Desktop* desktop = context;

    while(true) {
        furi_delay_ms(FOX_ESP32_WIFI_RECHECK_MS);

        int result_usart = fox_wifi_probe_pins(FuriHalSerialIdUsart);
        int result = result_usart;
        if(result_usart < 0) {
            int result_lpuart = fox_wifi_probe_pins(FuriHalSerialIdLpuart);
            if(result_lpuart >= 0) {
                result = result_lpuart;
            } else if(result_usart == -1 || result_lpuart == -1) {
                result = -1;
            } else {
                result = -2;
            }
        }

        if(result != -1) {
            bool connected = (result == 1);
            fox_wifi_status_write_raw(desktop->storage, connected);
        }
    }

    return 0;
}

static void desktop_wallpaper_draw_callback(Canvas* canvas, void* model) {
    if(!model) return;
    Desktop* desktop = *(Desktop**)model;

    if(desktop->wallpaper_data && desktop->settings.wallpaper_enabled) {
        canvas_clear(canvas);
        canvas_draw_xbm(canvas, 0, 0, 128, 64, desktop->wallpaper_data);
    }
}

static bool desktop_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    Desktop* desktop = (Desktop*)context;

    if(event == DesktopGlobalBeforeAppStarted) {
        s_animation_was_stalled = animation_manager_is_animation_loaded(desktop->animation_manager);
        if(s_animation_was_stalled) {
            animation_manager_unload_and_stall_animation(desktop->animation_manager);
        }
        desktop_auto_lock_inhibit(desktop);
        desktop->app_running = true;
        furi_semaphore_release(desktop->animation_semaphore);

    } else if(event == DesktopGlobalAfterAppFinished) {
        /* Do NOT call animation_manager_load_and_continue_animation here.
         * Fox Theme does not install Dolphin animation files on the SD card,
         * so the animation loader returns NULL → null-pointer crash inside
         * the animation manager even when the stall flag guard is correct.
         * The animation manager stays in FreezedIdle which is invisible on
         * the Fox Theme home screen. Classic Theme users lose the Dolphin
         * animation between app sessions but this is an acceptable trade-off
         * for a stable exit path on all hardware. */
        s_animation_was_stalled = false;
        desktop_auto_lock_arm(desktop);
        desktop->app_running = false;

        if(desktop->pending_slideshow) {
            desktop->pending_slideshow = false;
            if(storage_file_exists(desktop->storage, SLIDESHOW_FS_PATH)) {
                scene_manager_next_scene(desktop->scene_manager, DesktopSceneSlideshow);
            }
        }

    } else if(event == DesktopGlobalAutoLock) {
        if(!desktop->app_running && !desktop->locked) {
            if((desktop->settings.usb_inhibit_auto_lock) && (furi_hal_usb_is_locked())) {
                return (0);
            }
            desktop_lock(desktop);
        }
    } else if(event == DesktopGlobalSaveSettings) {
        desktop_settings_save(&desktop->settings);
        desktop_apply_settings(desktop);

    } else if(event == DesktopGlobalReloadSettings) {
        desktop_settings_load(&desktop->settings);
        desktop_apply_settings(desktop);
    } else if(event == DesktopGlobalSdCardRemoved) {
        if(desktop->no_sd_viewport == NULL) {
            desktop->no_sd_viewport = view_port_alloc();
            view_port_draw_callback_set(desktop->no_sd_viewport, fox_no_sd_draw_callback, NULL);
            view_port_input_callback_set(desktop->no_sd_viewport, fox_lockout_input_callback, NULL);
            gui_add_view_port(desktop->gui, desktop->no_sd_viewport, GuiLayerFullscreen);
        }
    } else if(event == DesktopGlobalSdCardMounted) {
        fox_settings_sync_int_to_sd();
        fox_settings_sync_sd_to_int();
        if(desktop->no_sd_viewport != NULL) {
            furi_delay_ms(400);
            furi_hal_power_reset();
        }

    } else {
        return scene_manager_handle_custom_event(desktop->scene_manager, event);
    }

    return true;
}

static bool desktop_back_event_callback(void* context) {
    furi_assert(context);
    Desktop* desktop = (Desktop*)context;
    return scene_manager_handle_back_event(desktop->scene_manager);
}

static void desktop_tick_event_callback(void* context) {
    furi_assert(context);
    Desktop* app = context;
    scene_manager_handle_tick_event(app->scene_manager);

    // Run storage-intensive checks only every 2 s (tick fires every 500 ms).
    static uint32_t s_last_integrity_ms = 0;
    uint32_t now = furi_get_tick();
    if(now - s_last_integrity_ms < furi_ms_to_ticks(2000)) return;
    s_last_integrity_ms = now;

    {
        Storage* s = app->storage;
        bool sd_present = (storage_sd_status(s) == FSE_OK);

        bool int_ok = storage_file_exists(s, FOX_SETTINGS_INT_PATH);
        bool ext_ok = sd_present && storage_file_exists(s, FOX_SETTINGS_EXT_PATH);

        if(!int_ok && !ext_ok) {
            if(storage_file_exists(s, FOX_SETUP_FLAG_PATH)) {
                furi_hal_power_reset();  // boot will show Firmware Corrupt screen
            }
        } else if(!int_ok && ext_ok) {
            fox_settings_sync_sd_to_int();
        } else if(int_ok && sd_present && !ext_ok) {
            fox_settings_sync_int_to_sd();
        }

        if(sd_present) {
            bool flag_int = storage_file_exists(s, FOX_SETUP_FLAG_PATH);
            bool flag_ext = storage_file_exists(s, FOX_SETUP_FLAG_EXT_PATH);
            if(flag_int && !flag_ext) {
                storage_common_copy(s, FOX_SETUP_FLAG_PATH, FOX_SETUP_FLAG_EXT_PATH);
            } else if(!flag_int && flag_ext) {
                storage_common_copy(s, FOX_SETUP_FLAG_EXT_PATH, FOX_SETUP_FLAG_PATH);
            }

            const char* pin_pending = EXT_PATH("apps_data/fox_setup/fox_pend.tmp");
            if(storage_file_exists(s, pin_pending)) {
                File* pf = storage_file_alloc(s);
                if(storage_file_open(pf, pin_pending, FSAM_READ, FSOM_OPEN_EXISTING)) {
                    DesktopPinCode pin = {0};
                    storage_file_read(pf, &pin.length, sizeof(uint8_t));
                    if(pin.length > 0 && pin.length <= (uint8_t)(sizeof(pin.data) - 1)) {
                        storage_file_read(pf, pin.data, pin.length);
                        pin.data[pin.length] = '\0';
                    } else {
                        pin.length = 0;
                    }
                    storage_file_close(pf);
                    if(pin.length > 0) desktop_pin_code_set(&pin);
                }
                storage_file_free(pf);
                storage_common_remove(s, pin_pending);
            }
        }
    }
}

static void desktop_input_event_callback(const void* value, void* context) {
    furi_assert(value);
    furi_assert(context);
    const InputEvent* event = value;
    Desktop* desktop = context;
    if(event->type == InputTypePress) {
        desktop_start_auto_lock_timer(desktop);
    }
}

static void desktop_auto_lock_timer_callback(void* context) {
    furi_assert(context);
    Desktop* desktop = context;
    view_dispatcher_send_custom_event(desktop->view_dispatcher, DesktopGlobalAutoLock);
}

static void desktop_start_auto_lock_timer(Desktop* desktop) {
    furi_timer_start(
        desktop->auto_lock_timer, furi_ms_to_ticks(desktop->settings.auto_lock_delay_ms));
}

static void desktop_stop_auto_lock_timer(Desktop* desktop) {
    furi_timer_stop(desktop->auto_lock_timer);
}

static void desktop_auto_lock_arm(Desktop* desktop) {
    if(desktop->settings.auto_lock_delay_ms) {
        if(!desktop->input_events_subscription) {
            desktop->input_events_subscription = furi_pubsub_subscribe(
                desktop->input_events_pubsub, desktop_input_event_callback, desktop);
        }
        desktop_start_auto_lock_timer(desktop);
    }
}

static void desktop_auto_lock_inhibit(Desktop* desktop) {
    desktop_stop_auto_lock_timer(desktop);
    if(desktop->input_events_subscription) {
        furi_pubsub_unsubscribe(desktop->input_events_pubsub, desktop->input_events_subscription);
        desktop->input_events_subscription = NULL;
    }
}

static void desktop_clock_timer_callback(void* context) {
    furi_assert(context);
    Desktop* desktop = context;
    desktop_clock_update(desktop);
}

// Convert two ASCII hex characters to a byte value.
static uint8_t desktop_hex2byte(char hi, char lo) {
    uint8_t h = (hi >= 'a') ? (uint8_t)(hi - 'a' + 10) :
                (hi >= 'A') ? (uint8_t)(hi - 'A' + 10) : (uint8_t)(hi - '0');
    uint8_t l = (lo >= 'a') ? (uint8_t)(lo - 'a' + 10) :
                (lo >= 'A') ? (uint8_t)(lo - 'A' + 10) : (uint8_t)(lo - '0');
    return (h << 4) | l;
}

// Parses XBM text into a raw 1024-byte bitmap (scans for '{', pulls every 0xNN value).
static void desktop_load_wallpaper(Desktop* desktop) {
    furi_assert(desktop);

    if(desktop->wallpaper_data) {
        free(desktop->wallpaper_data);
        desktop->wallpaper_data = NULL;
    }

    if(!desktop->settings.wallpaper_enabled) return;

    File* file = storage_file_alloc(desktop->storage);
    if(!storage_file_open(file, WALLPAPER_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(file);
        return;
    }

    uint8_t* out = malloc(WALLPAPER_SIZE);
    size_t  count = 0;

    // State machine: 0=scan '{', 1=scan '0', 2=expect 'x', 3=first hex digit, 4=second hex digit
    uint8_t state = 0;
    char    hi_digit = 0;
    bool    finished = false;

    uint8_t  chunk[256];
    uint16_t n;

    while(!finished && count < WALLPAPER_SIZE &&
          (n = storage_file_read(file, chunk, sizeof(chunk))) > 0) {
        for(uint16_t i = 0; i < n && count < WALLPAPER_SIZE && !finished; i++) {
            char c = (char)chunk[i];
            switch(state) {
            case 0:
                if(c == '{') state = 1;
                break;
            case 1:
                if(c == '}') { finished = true; break; }
                if(c == '0') state = 2;
                break;
            case 2:
                state = (c == 'x' || c == 'X') ? 3 : 1;
                break;
            case 3:
                hi_digit = c;
                state = 4;
                break;
            case 4:
                out[count++] = desktop_hex2byte(hi_digit, c);
                state = 1;
                break;
            }
        }
    }

    storage_file_close(file);
    storage_file_free(file);

    if(count == WALLPAPER_SIZE) {
        desktop->wallpaper_data = out;
    } else {
        free(out);
        desktop->wallpaper_data = NULL;
    }
}

static void desktop_apply_settings(Desktop* desktop) {
    desktop->in_transition = true;

    desktop_clock_reconfigure(desktop);
    desktop_load_wallpaper(desktop);

    {
        Power* power = furi_record_open(RECORD_POWER);
        power_trigger_ui_update(power);
        furi_record_close(RECORD_POWER);
    }

    if(!desktop->app_running && !desktop->locked) {
        desktop_auto_lock_arm(desktop);
    }

    desktop->in_transition = false;
}

static void desktop_init_settings(Desktop* desktop) {
    furi_pubsub_subscribe(storage_get_pubsub(desktop->storage), desktop_storage_callback, desktop);

    if(storage_sd_status(desktop->storage) != FSE_OK) {
        FURI_LOG_D(TAG, "SD Card not ready, skipping settings");
        return;
    }

    desktop_settings_load(&desktop->settings);
    /* Sync Fox.cfg to match the loaded settings so the theme is correct
     * from first paint — also fixes stale Fox.cfg values after firmware update. */
    fox_theme_set(desktop->settings.menu_theme == MenuThemeFox);
    desktop_apply_settings(desktop);
}

static Desktop* desktop_alloc(void) {
    Desktop* desktop = malloc(sizeof(Desktop));

    desktop->wallpaper_data  = NULL;
    desktop->no_sd_viewport  = NULL;
    desktop->pending_slideshow = false;

    desktop->animation_semaphore = furi_semaphore_alloc(1, 0);
    desktop->animation_manager = animation_manager_alloc();
    desktop->gui = furi_record_open(RECORD_GUI);
    desktop->scene_thread = furi_thread_alloc();
    desktop->view_dispatcher = view_dispatcher_alloc();
    desktop->scene_manager = scene_manager_alloc(&desktop_scene_handlers, desktop);

    view_dispatcher_attach_to_gui(
        desktop->view_dispatcher, desktop->gui, ViewDispatcherTypeDesktop);
    view_dispatcher_set_tick_event_callback(
        desktop->view_dispatcher, desktop_tick_event_callback, 500);

    view_dispatcher_set_event_callback_context(desktop->view_dispatcher, desktop);
    view_dispatcher_set_custom_event_callback(
        desktop->view_dispatcher, desktop_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        desktop->view_dispatcher, desktop_back_event_callback);

    desktop->lock_menu = desktop_lock_menu_alloc();
    desktop->debug_view = desktop_debug_alloc();
    desktop->popup = popup_alloc();
    desktop->locked_view = desktop_view_locked_alloc();
    desktop->pin_input_view = desktop_view_pin_input_alloc();
    desktop->pin_timeout_view = desktop_view_pin_timeout_alloc();
    desktop->slideshow_view = desktop_view_slideshow_alloc();
    desktop->clock_lock_view = desktop_clock_lock_alloc();

    desktop->main_view_stack = view_stack_alloc();
    desktop->main_view = desktop_main_alloc();
    View* dolphin_view = animation_manager_get_animation_view(desktop->animation_manager);

    desktop->wallpaper_view = view_alloc();
    view_allocate_model(desktop->wallpaper_view, ViewModelTypeLocking, sizeof(Desktop*));
    with_view_model(
        desktop->wallpaper_view,
        Desktop** model,
        { *model = desktop; },
        false);
    view_set_draw_callback(desktop->wallpaper_view, desktop_wallpaper_draw_callback);

    view_stack_add_view(desktop->main_view_stack, desktop_main_get_view(desktop->main_view));
    view_stack_add_view(desktop->main_view_stack, dolphin_view);
    view_stack_add_view(desktop->main_view_stack, desktop->wallpaper_view);
    view_stack_add_view(
        desktop->main_view_stack, desktop_view_locked_get_view(desktop->locked_view));

    desktop->locked_view_stack = view_stack_alloc();
    view_stack_add_view(desktop->locked_view_stack, dolphin_view);
    view_stack_add_view(
        desktop->locked_view_stack, desktop_view_locked_get_view(desktop->locked_view));

    view_dispatcher_add_view(
        desktop->view_dispatcher,
        DesktopViewIdMain,
        view_stack_get_view(desktop->main_view_stack));
    view_dispatcher_add_view(
        desktop->view_dispatcher,
        DesktopViewIdLocked,
        view_stack_get_view(desktop->locked_view_stack));
    view_dispatcher_add_view(
        desktop->view_dispatcher,
        DesktopViewIdLockMenu,
        desktop_lock_menu_get_view(desktop->lock_menu));
    view_dispatcher_add_view(
        desktop->view_dispatcher, DesktopViewIdDebug, desktop_debug_get_view(desktop->debug_view));
    view_dispatcher_add_view(
        desktop->view_dispatcher, DesktopViewIdPopup, popup_get_view(desktop->popup));
    view_dispatcher_add_view(
        desktop->view_dispatcher,
        DesktopViewIdPinTimeout,
        desktop_view_pin_timeout_get_view(desktop->pin_timeout_view));
    view_dispatcher_add_view(
        desktop->view_dispatcher,
        DesktopViewIdPinInput,
        desktop_view_pin_input_get_view(desktop->pin_input_view));
    view_dispatcher_add_view(
        desktop->view_dispatcher,
        DesktopViewIdSlideshow,
        desktop_view_slideshow_get_view(desktop->slideshow_view));
    view_dispatcher_add_view(
        desktop->view_dispatcher,
        DesktopViewIdClockLock,
        desktop_clock_lock_get_view(desktop->clock_lock_view));

    desktop->lock_icon_viewport = view_port_alloc();
    view_port_set_width(desktop->lock_icon_viewport, icon_get_width(&I_Lock_7x8));
    view_port_draw_callback_set(
        desktop->lock_icon_viewport, desktop_lock_icon_draw_callback, desktop);
    view_port_enabled_set(desktop->lock_icon_viewport, false);
    gui_add_view_port(desktop->gui, desktop->lock_icon_viewport, GuiLayerStatusBarLeft);

    desktop->clock_viewport = view_port_alloc();
    view_port_set_width(desktop->clock_viewport, 50);
    view_port_draw_callback_set(desktop->clock_viewport, desktop_clock_draw_callback, desktop);
    view_port_enabled_set(desktop->clock_viewport, false);
    gui_add_view_port(desktop->gui, desktop->clock_viewport, GuiLayerStatusBarCenter);

    desktop->stealth_mode_icon_viewport = view_port_alloc();
    view_port_set_width(desktop->stealth_mode_icon_viewport, icon_get_width(&I_Muted_8x8));
    view_port_draw_callback_set(
        desktop->stealth_mode_icon_viewport, desktop_stealth_mode_icon_draw_callback, desktop);
    if(furi_hal_rtc_is_flag_set(FuriHalRtcFlagStealthMode)) {
        view_port_enabled_set(desktop->stealth_mode_icon_viewport, true);
    } else {
        view_port_enabled_set(desktop->stealth_mode_icon_viewport, false);
    }
    gui_add_view_port(desktop->gui, desktop->stealth_mode_icon_viewport, GuiLayerStatusBarLeft);

    desktop->wifi_icon_viewport = view_port_alloc();
    view_port_set_width(desktop->wifi_icon_viewport, icon_get_width(&I_WiFi_Connected_9x8) + 2);
    view_port_draw_callback_set(
        desktop->wifi_icon_viewport, desktop_wifi_icon_draw_callback, desktop);
    view_port_enabled_set(desktop->wifi_icon_viewport, true);
    gui_add_view_port(desktop->gui, desktop->wifi_icon_viewport, GuiLayerStatusBarRight);

    desktop->loader = furi_record_open(RECORD_LOADER);
    furi_pubsub_subscribe(loader_get_pubsub(desktop->loader), desktop_loader_callback, desktop);

    desktop->storage = furi_record_open(RECORD_STORAGE);
    desktop->notification = furi_record_open(RECORD_NOTIFICATION);
    desktop->input_events_pubsub = furi_record_open(RECORD_INPUT_EVENTS);

    desktop->auto_lock_timer =
        furi_timer_alloc(desktop_auto_lock_timer_callback, FuriTimerTypeOnce, desktop);

    desktop->status_pubsub = furi_pubsub_alloc();

    desktop->update_clock_timer =
        furi_timer_alloc(desktop_clock_timer_callback, FuriTimerTypePeriodic, desktop);

    desktop->update_wifi_timer =
        furi_timer_alloc(desktop_wifi_status_timer_callback, FuriTimerTypePeriodic, desktop);
    desktop_wifi_status_timer_callback(desktop); /* first read now, don't wait a full period */
    furi_timer_start(desktop->update_wifi_timer, furi_ms_to_ticks(FOX_ESP32_WIFI_POLL_MS));

    desktop->wifi_recheck_thread =
        furi_thread_alloc_ex("FoxWifiRecheck", 1024, desktop_wifi_recheck_thread, desktop);
    furi_thread_start(desktop->wifi_recheck_thread);

    desktop->app_running = loader_is_locked(desktop->loader);

    furi_record_create(RECORD_DESKTOP, desktop);

    return desktop;
}

void desktop_lock(Desktop* desktop) {
    furi_assert(!desktop->locked);

    furi_hal_rtc_set_flag(FuriHalRtcFlagLock);

    if(desktop->settings.lock_on_lock_enabled) {
        if(desktop->settings.lock_disconnect_ble) {
            Bt* bt = furi_record_open(RECORD_BT);
            bt_disconnect(bt);
            furi_record_close(RECORD_BT);
        }

        if(desktop->settings.lock_disconnect_gpio) {
            s_locked_gpio_usart  = furi_hal_serial_control_acquire(FuriHalSerialIdUsart);
            s_locked_gpio_lpuart = furi_hal_serial_control_acquire(FuriHalSerialIdLpuart);
        }

    }

    // USB disconnect on lock — force-kills qFlipper session if PIN is set or USB level requires it.
    {
        bool should_disconnect_usb =
            desktop_pin_code_is_set() ||
            (desktop->settings.lock_usb_level >= LockUsbLevelSessionBlock);

        if(should_disconnect_usb) {
            s_locked_usb_config = furi_hal_usb_get_config();
            furi_hal_usb_unlock(); // force-release VCP service lock → set_config works
            furi_hal_usb_set_config(NULL, NULL);
        }
    }

    desktop_auto_lock_inhibit(desktop);
    scene_manager_set_scene_state(
        desktop->scene_manager, DesktopSceneLocked, DesktopSceneLockedStateFirstEnter);
    scene_manager_next_scene(desktop->scene_manager, DesktopSceneLocked);

    DesktopStatus status = {.locked = true};
    furi_pubsub_publish(desktop->status_pubsub, &status);

    desktop->locked = true;
}

void desktop_unlock(Desktop* desktop) {
    furi_assert(desktop->locked);

    view_port_enabled_set(desktop->lock_icon_viewport, false);
    Gui* gui = furi_record_open(RECORD_GUI);
    gui_set_lockdown(gui, false);
    furi_record_close(RECORD_GUI);
    desktop_view_locked_unlock(desktop->locked_view);
    scene_manager_search_and_switch_to_previous_scene(desktop->scene_manager, DesktopSceneMain);
    desktop_auto_lock_arm(desktop);
    furi_hal_rtc_reset_flag(FuriHalRtcFlagLock);
    furi_hal_rtc_set_pin_fails(0);

    if(desktop->settings.lock_on_lock_enabled) {
        if(desktop->settings.lock_disconnect_gpio) {
            if(s_locked_gpio_usart) {
                furi_hal_serial_control_release(s_locked_gpio_usart);
                s_locked_gpio_usart = NULL;
            }
            if(s_locked_gpio_lpuart) {
                furi_hal_serial_control_release(s_locked_gpio_lpuart);
                s_locked_gpio_lpuart = NULL;
            }
        }

    }

    if(s_locked_usb_config) {
        furi_hal_usb_set_config(s_locked_usb_config, NULL);
        s_locked_usb_config = NULL;
    }

    DesktopStatus status = {.locked = false};
    furi_pubsub_publish(desktop->status_pubsub, &status);

    desktop->locked = false;
}

void desktop_set_stealth_mode_state(Desktop* desktop, bool enabled) {
    desktop->in_transition = true;

    if(enabled) {
        furi_hal_rtc_set_flag(FuriHalRtcFlagStealthMode);
    } else {
        furi_hal_rtc_reset_flag(FuriHalRtcFlagStealthMode);
    }

    view_port_enabled_set(desktop->stealth_mode_icon_viewport, enabled);

    desktop->in_transition = false;
}

bool desktop_api_is_locked(Desktop* instance) {
    furi_assert(instance);
    return furi_hal_rtc_is_flag_set(FuriHalRtcFlagLock);
}

void desktop_api_unlock(Desktop* instance) {
    furi_assert(instance);
    view_dispatcher_send_custom_event(instance->view_dispatcher, DesktopGlobalApiUnlock);
}

FuriPubSub* desktop_api_get_status_pubsub(Desktop* instance) {
    furi_assert(instance);
    return instance->status_pubsub;
}

void desktop_api_reload_settings(Desktop* instance) {
    furi_assert(instance);
    view_dispatcher_send_custom_event(instance->view_dispatcher, DesktopGlobalReloadSettings);
}

void desktop_api_get_settings(Desktop* instance, DesktopSettings* settings) {
    furi_assert(instance);
    furi_assert(settings);
    *settings = instance->settings;
}

void desktop_api_set_settings(Desktop* instance, const DesktopSettings* settings) {
    furi_assert(instance);
    furi_assert(settings);
    instance->settings = *settings;
    view_dispatcher_send_custom_event(instance->view_dispatcher, DesktopGlobalSaveSettings);
}

void desktop_api_set_pin(Desktop* instance, const DesktopPinCode* pin_code) {
    furi_assert(instance);
    furi_assert(pin_code);
    desktop_pin_code_set(pin_code);
}

void desktop_api_clear_pin(Desktop* instance) {
    furi_assert(instance);
    desktop_pin_code_reset();
}

int32_t desktop_srv(void* p) {
    UNUSED(p);

    if(furi_hal_rtc_get_boot_mode() != FuriHalRtcBootModeNormal) {
        FURI_LOG_W(TAG, "Skipping start in special boot mode");
        furi_thread_suspend(furi_thread_get_current_id());
        return 0;
    }

    Desktop* desktop = desktop_alloc();

    {
        furi_delay_ms(200);
        bool sd_ok = false;
        for(uint8_t i = 0; i < 8 && !sd_ok; i++) {
            sd_ok = (storage_sd_status(desktop->storage) == FSE_OK);
            if(!sd_ok) furi_delay_ms(100);
        }
        if(!sd_ok) {
            fox_desktop_show_no_sd_blocking(desktop);
            // Never returns — reboots when SD is inserted
        }
    }

    // Seed default wallpaper if user doesn't have one yet
    desktop_ensure_wallpaper(desktop);

    {
        bool format_flagged = storage_file_exists(desktop->storage, FOX_FORMAT_FLAG_PATH);
        bool lock_flagged   = storage_file_exists(desktop->storage, FOX_LOCKOUT_FLAG_PATH);
        if(format_flagged) {
            fox_desktop_show_format_blocking(desktop);
            /* never returns */
        }
        if(lock_flagged) {
            fox_desktop_show_lockout_blocking(desktop);
            /* never returns */
        }
    }

    desktop_init_settings(desktop);

    // Restore PIN from internal storage — survives soft resets unlike RAM variables
    desktop_pin_code_load_from_storage();

    if(!desktop_pin_code_is_set() && desktop->settings.auto_lock_delay_ms != 0) {
        desktop->settings.auto_lock_delay_ms = 0;
        desktop_settings_save(&desktop->settings);
    }

    if(fox_settings_import_override()) {
        FURI_LOG_I("Desktop", "Fox.Settings override applied");
        furi_hal_power_reset();
    }

    if(fox_recovery_check_and_reset()) {
        // Valid tech-support recovery token — clear PIN, lockout file, RTC bit, and fail count
        desktop_pin_code_reset();
        storage_common_remove(desktop->storage, FOX_LOCKOUT_FLAG_PATH);
        FoxEscrowData recovery_escrow;
        memset(&recovery_escrow, 0, sizeof(FoxEscrowData));
        if(fox_escrow_load_and_verify(&recovery_escrow)) {
            recovery_escrow.active_fail_count = 0;
            fox_escrow_save_state(&recovery_escrow);
        }
        furi_hal_power_reset();  // Clean reboot — device starts fresh
    }

    if(storage_file_exists(desktop->storage, SLIDESHOW_FS_PATH)) {
        storage_common_remove(desktop->storage, FOX_LOCKOUT_FLAG_PATH);
        storage_common_remove(desktop->storage, FOX_FORMAT_FLAG_PATH);
        desktop_pin_code_reset();
        storage_common_remove(desktop->storage, FOX_SETUP_FLAG_PATH);
        storage_common_remove(desktop->storage, FOX_SETUP_FLAG_EXT_PATH);  /* Also clear EXT mirror */
        storage_common_remove(desktop->storage,
                              EXT_PATH("apps_data/fox_setup/completed.flag"));
    }

    {
        bool setup_done = storage_file_exists(desktop->storage, FOX_SETUP_FLAG_PATH);
        bool int_ok = storage_file_exists(desktop->storage, FOX_SETTINGS_INT_PATH);
        bool ext_ok = storage_file_exists(desktop->storage, FOX_SETTINGS_EXT_PATH);

        if(!int_ok && !ext_ok && setup_done) {
            // Both copies deleted after initial setup — security breach
            fox_desktop_show_corrupt_blocking(desktop);
            // Never returns
        } else if(!int_ok && ext_ok) {
            // INT missing — restore from SD
            fox_settings_sync_sd_to_int();
        } else if(int_ok && !ext_ok) {
            // SD missing — restore from INT
            fox_settings_sync_int_to_sd();
        }
        // Both missing + no setup_done: genuine fresh install, proceed normally.
    }

    {
        bool flag_int = storage_file_exists(desktop->storage, FOX_SETUP_FLAG_PATH);
        bool flag_ext = storage_file_exists(desktop->storage, FOX_SETUP_FLAG_EXT_PATH);

        if(flag_int && !flag_ext) {
            /* INT flag exists but EXT mirror missing — create mirror */
            storage_common_copy(desktop->storage, FOX_SETUP_FLAG_PATH, FOX_SETUP_FLAG_EXT_PATH);
        } else if(!flag_int && flag_ext) {
            /* EXT mirror exists but INT flag missing — restore INT */
            storage_common_copy(desktop->storage, FOX_SETUP_FLAG_EXT_PATH, FOX_SETUP_FLAG_PATH);
        }
        /* Both missing: wizard will run. fox_setup_already_ran() also checks
         * desktop_pin_code_is_set() so the wizard cannot change an existing PIN. */
    }

    scene_manager_next_scene(desktop->scene_manager, DesktopSceneMain);

    bool wiper_screen_active = false;
    if(desktop_pin_code_is_set()) {
        FoxEscrowData hcheck;
        memset(&hcheck, 0, sizeof(FoxEscrowData));
        wiper_screen_active = fox_escrow_load_and_verify(&hcheck) &&
                          (hcheck.active_fail_count == 0xFF);
        desktop_lock(desktop);
    }

    if(!wiper_screen_active && storage_file_exists(desktop->storage, SLIDESHOW_FS_PATH)) {
        bool fox_setup_pending = !storage_file_exists(desktop->storage, FOX_SETUP_FLAG_PATH);
        if(fox_setup_pending) {
            desktop->pending_slideshow = true;
        } else {
            scene_manager_next_scene(desktop->scene_manager, DesktopSceneSlideshow);
        }
    }

    {
        s_fox_setup_launch_thread = furi_thread_alloc();
        furi_thread_set_name(s_fox_setup_launch_thread, "FoxSetupLaunch");
        furi_thread_set_stack_size(s_fox_setup_launch_thread, 1024);
        furi_thread_set_context(s_fox_setup_launch_thread, desktop);
        furi_thread_set_callback(s_fox_setup_launch_thread, desktop_fox_setup_launch_thread_fn);
        furi_thread_start(s_fox_setup_launch_thread);
    }

    if(!furi_hal_version_do_i_belong_here()) {
        scene_manager_next_scene(desktop->scene_manager, DesktopSceneHwMismatch);
    }

    if(furi_hal_rtc_get_fault_data()) {
        scene_manager_next_scene(desktop->scene_manager, DesktopSceneFault);
    }

    uint8_t keys_total, keys_valid;
    if(!furi_hal_crypto_enclave_verify(&keys_total, &keys_valid)) {
        FURI_LOG_E(
            TAG,
            "Secure Enclave verification failed: total %hhu, valid %hhu",
            keys_total,
            keys_valid);
        scene_manager_next_scene(desktop->scene_manager, DesktopSceneSecureEnclave);
    }

    if(desktop->app_running && animation_manager_is_animation_loaded(desktop->animation_manager)) {
        animation_manager_unload_and_stall_animation(desktop->animation_manager);
    }

    view_dispatcher_run(desktop->view_dispatcher);

    return 0;
}
