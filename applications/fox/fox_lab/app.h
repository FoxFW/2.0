#pragma once

#include <furi.h>
#include <furi_hal_serial_types.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>

#include "esp_at.h"
#include "esp_at_router.h"
#include "foxr_companion.h"

typedef enum {
    FoxLabViewMessage,
    FoxLabViewLauncher,
    FoxLabViewConnectSettings,
    FoxLabViewFlpr,
    FoxLabViewRestartConfirm,
} FoxLabView;

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;

    EspAt* esp_at;

    EspAtRouter* router;

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

    bool launcher_focus_left;

    View* flpr_view;

    View* connect_settings_view;
    uint8_t connect_settings_selected;

    volatile bool device_name_restart_pending;
    View* restart_confirm_view;

    bool restart_confirm_focus_left;
} App;

size_t app_pin_option_count(void);
const char* app_pin_option_label(size_t index);

bool app_probe_uart_selected(App* app);
void app_retry_detection(App* app);

void app_show_launcher(App* app);
void app_show_flpr(App* app);

bool app_wait_for_reply_prefix(App* app, const char* prefix, uint32_t timeout_ms, EspAtMsg* out);
