#pragma once

#include <furi.h>
#include <furi_hal_serial_types.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>

#include "esp_at.h"
#include "esp_at_router.h"
#include "foxr_companion.h"
#include "fox_splash.h"

typedef enum {
    FoxLabViewSplash,
    FoxLabViewMessage,
    FoxLabViewLauncher,
    FoxLabViewConnectSettings,
    FoxLabViewFlpr,
    FoxLabViewRestartConfirm,
} FoxLabView;

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;

    FoxSplash* splash;

    EspAt* esp_at;
    /* The sole esp_at_receive() consumer for the whole app session (see
     * esp_at_router.h) - allocated right alongside esp_at in
     * app_probe_uart(), freed alongside it too. Every call site that used
     * to read esp_at directly now reads through this instead. */
    EspAtRouter* router;
    /* Registered as the router's FLPR handler - answers every
     * "[FLPR/...]" companion command the browser sends via the ESP32. See
     * foxr_companion.h and the Fox Remote Protocol design doc. */
    FoxrCompanion* companion;
    size_t pin_option_index;
    size_t baud_option_index;

    bool esp32_detected;
    FoxLabView current_view;

    bool message_view_detecting;
    bool message_view_not_detected_focus_left;
    bool message_view_serial_busy;
    bool message_view_serial_retrying;
    bool message_view_serial_retry_failed;
    bool message_view_not_supported;
    uint8_t serial_busy_countdown;
    FuriTimer* serial_busy_timer;
    FuriTimer* serial_retry_timer;
    View* message_view;

    bool lab_active;
    bool lab_busy;
    View* launcher_view;
    /* Which of the Launcher's two footer buttons ("< Start/Stop" /
     * "Terminal >") is currently focused - Left/Right only move this,
     * OK activates whichever side it's on, same focus-then-confirm model
     * message_view.c's two-button screens already use. Reset to true
     * (Start/Stop focused) in app_show_launcher() so OK's default action
     * matches muscle memory from before this screen had two buttons. */
    bool launcher_focus_left;

    /* "FoxLAB Active" - shown once [LAB/START] succeeds; the companion
     * (above) is what actually answers FLPR commands regardless of which
     * view is showing, this is just the visible "keep this open" screen
     * with a live command log. See flpr_view.h. */
    View* flpr_view;

    View* connect_settings_view;
    uint8_t connect_settings_selected;

    /* Set by the FLPR companion (see foxr_companion.h's restart_pending_
     * flag param) when a Device Name change has been saved but not yet
     * applied - namechanger_srv only picks it up at boot, and this app
     * deliberately no longer reboots the instant it's saved (that would
     * also power-cycle the attached ESP32 and drop the FoxLAB connection
     * being used to make the change - see foxr_handle_settings_system_
     * set()). navigation_callback() (main.c) checks this when the app is
     * about to close and detours to restart_confirm_view instead, letting
     * the user choose to restart now or leave it for next boot. volatile:
     * written from the companion's own background thread (see esp_at_
     * router.h), read from the main UI thread. */
    volatile bool device_name_restart_pending;
    View* restart_confirm_view;
    /* Which of restart_confirm_view's two buttons ("< Later" / "Restart >")
     * is focused - same focus-then-OK model as launcher_focus_left above. */
    bool restart_confirm_focus_left;
} App;

size_t app_pin_option_count(void);
const char* app_pin_option_label(size_t index);

bool app_probe_uart_selected(App* app);
void app_retry_detection(App* app);

void app_show_launcher(App* app);
void app_show_flpr(App* app);

/* Waits up to timeout_ms for a UART line starting with `prefix`,
 * discarding anything else that arrives first (see main.c for why this
 * matters for LAB/START specifically). Returns true and fills *out with
 * the matched line, or false on timeout. Reads through the router (see
 * EspAtRouter above), not esp_at directly - esp_at_receive() is a
 * single-consumer queue and the router is now the one consumer. */
bool app_wait_for_reply_prefix(App* app, const char* prefix, uint32_t timeout_ms, EspAtMsg* out);
