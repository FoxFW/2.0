#include "app.h"
#include "fox_lab_icons.h"
#include "message_view.h"
#include "connect_settings.h"
#include "launcher_view.h"
#include "flpr_view.h"
#include "restart_confirm_view.h"
#include "gpio_remap_compat.h"

#include <string.h>
#include <stdio.h>

typedef struct {
    FuriHalSerialId serial_id;
    const char* label;
} PinOption;

static const PinOption pin_options[] = {
    {FuriHalSerialIdUsart, "13/14 (USART)"},
    {FuriHalSerialIdLpuart, "15/16 (LPUART)"},
};
#define PIN_OPTION_COUNT (sizeof(pin_options) / sizeof(pin_options[0]))

static const uint32_t baud_options[] = {115200};
#define BAUD_OPTION_DEFAULT_INDEX 0

#define FOX_LAB_EVENT_SERIAL_BUSY_TICK 1
#define FOX_LAB_EVENT_SERIAL_DO_RETRY  2

typedef enum {
    ProbeResultOk,
    ProbeResultSerialBusy,
    ProbeResultNotFound,
} ProbeResult;

static bool wait_for_line(App* app, const char* expected, uint32_t timeout_ms) {
    EspAtMsg msg;
    uint32_t deadline = furi_get_tick() + timeout_ms;

    while(furi_get_tick() < deadline) {
        uint32_t remaining = deadline - furi_get_tick();

        if(!esp_at_router_wait_line(app->router, &msg, remaining)) break;
        if(strcmp(msg.line, expected) == 0) return true;
    }
    return false;
}

static bool wait_for_line_prefix(App* app, const char* prefix, uint32_t timeout_ms, EspAtMsg* out) {
    size_t prefix_len = strlen(prefix);
    uint32_t deadline = furi_get_tick() + timeout_ms;

    while(furi_get_tick() < deadline) {
        uint32_t remaining = deadline - furi_get_tick();
        if(!esp_at_router_wait_line(app->router, out, remaining)) break;
        if(strncmp(out->line, prefix, prefix_len) == 0) return true;
    }
    return false;
}

bool app_wait_for_reply_prefix(App* app, const char* prefix, uint32_t timeout_ms, EspAtMsg* out) {
    return wait_for_line_prefix(app, prefix, timeout_ms, out);
}

void app_show_launcher(App* app) {

    app->launcher_focus_left = true;
    app->current_view = FoxLabViewLauncher;
    view_dispatcher_switch_to_view(app->view_dispatcher, FoxLabViewLauncher);
}

void app_show_flpr(App* app) {
    app->current_view = FoxLabViewFlpr;
    view_dispatcher_switch_to_view(app->view_dispatcher, FoxLabViewFlpr);
}

static void query_lab_status_and_show(App* app) {
    app->lab_active = false;
    app->lab_busy = false;

    esp_at_send(app->esp_at, "[LAB/STATUS]");
    EspAtMsg msg;

    if(wait_for_line_prefix(app, "[LAB/STATUS/", 1500, &msg)) {
        if(strncmp(msg.line, "[LAB/STATUS/SUCCESS]", 21) == 0) {
            app->lab_active = strstr(msg.line, "\"active\":1") != NULL;
        } else if(strncmp(msg.line, "[LAB/STATUS/ERROR]NOLAB", 23) == 0) {

            message_view_show_not_supported(app);
            return;
        }
    }

    app_show_launcher(app);
}

static ProbeResult app_probe_uart(App* app, size_t pin_index, size_t baud_index) {
    app->esp_at = esp_at_alloc(pin_options[pin_index].serial_id, baud_options[baud_index]);
    if(app->esp_at == NULL) return ProbeResultSerialBusy;

    app->companion =
        foxr_companion_alloc(app->esp_at, app->gui, &app->device_name_restart_pending);
    app->router = esp_at_router_alloc(app->esp_at, foxr_companion_dispatch, app->companion);

    esp_at_send(app->esp_at, "info");
    bool ok = wait_for_line(app, "Fox ESP32 Firmware", 1500);

    if(!ok) {
        esp_at_router_free(app->router);
        app->router = NULL;
        foxr_companion_free(app->companion);
        app->companion = NULL;
        esp_at_free(app->esp_at);
        app->esp_at = NULL;
        return ProbeResultNotFound;
    }

    app->pin_option_index = pin_index;
    app->baud_option_index = baud_index;

    GpioRemapSettings gpio_remap = {.esp32_uart_channel = (uint8_t)pin_index};
    gpio_remap_settings_save(&gpio_remap);

    return ProbeResultOk;
}

static void action_check_esp32(App* app) {
    message_view_show_detecting(app);

    bool any_busy = false;
    for(size_t i = 0; i < PIN_OPTION_COUNT; i++) {
        ProbeResult r = app_probe_uart(app, i, BAUD_OPTION_DEFAULT_INDEX);
        if(r == ProbeResultOk) {
            app->esp32_detected = true;
            query_lab_status_and_show(app);
            return;
        }
        if(r == ProbeResultSerialBusy) any_busy = true;
    }

    if(any_busy) {
        app->serial_busy_countdown = 3;
        message_view_show_serial_busy(app);
        furi_timer_start(app->serial_busy_timer, 1000);
    } else {
        message_view_show_not_detected(app);
    }
}

void app_retry_detection(App* app) {
    action_check_esp32(app);
}

size_t app_pin_option_count(void) {
    return PIN_OPTION_COUNT;
}

const char* app_pin_option_label(size_t index) {
    if(index >= PIN_OPTION_COUNT) index = 0;
    return pin_options[index].label;
}

bool app_probe_uart_selected(App* app) {
    message_view_show_detecting(app);

    ProbeResult r = app_probe_uart(app, app->pin_option_index, app->baud_option_index);
    if(r == ProbeResultOk) {
        app->esp32_detected = true;
        query_lab_status_and_show(app);
        return true;
    }

    if(r == ProbeResultSerialBusy) {
        app->serial_busy_countdown = 3;
        message_view_show_serial_busy(app);
        furi_timer_start(app->serial_busy_timer, 1000);
        return false;
    }

    message_view_show_not_detected(app);
    return false;
}

static void serial_busy_timer_cb(void* context) {
    App* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, FOX_LAB_EVENT_SERIAL_BUSY_TICK);
}

static void serial_retry_timer_cb(void* context) {
    App* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, FOX_LAB_EVENT_SERIAL_DO_RETRY);
}

static bool custom_event_callback(void* context, uint32_t event) {
    App* app = context;
    if(event == FOX_LAB_EVENT_SERIAL_BUSY_TICK) {
        if(app->serial_busy_countdown > 0) {
            app->serial_busy_countdown--;
            if(app->serial_busy_countdown == 0) {
                furi_timer_stop(app->serial_busy_timer);
                app->message_view_serial_retrying = true;
            }
            with_view_model(app->message_view, uint8_t * _m, { UNUSED(_m); }, true);
            if(app->serial_busy_countdown == 0) {
                furi_timer_start(app->serial_retry_timer, 500);
            }
        }
        return true;
    }
    if(event == FOX_LAB_EVENT_SERIAL_DO_RETRY) {
        bool found = false;
        for(size_t i = 0; i < PIN_OPTION_COUNT; i++) {
            ProbeResult r = app_probe_uart(app, i, BAUD_OPTION_DEFAULT_INDEX);
            if(r == ProbeResultOk) {
                app->esp32_detected = true;
                app->message_view_serial_busy     = false;
                app->message_view_serial_retrying = false;
                query_lab_status_and_show(app);
                found = true;
                break;
            }
        }
        if(!found) message_view_show_serial_retry_failed(app);
        return true;
    }
    return false;
}

static bool navigation_callback(void* context) {
    App* app = context;

    if(app->current_view == FoxLabViewConnectSettings) {
        app->current_view = FoxLabViewMessage;
        view_dispatcher_switch_to_view(app->view_dispatcher, FoxLabViewMessage);
        return true;
    }

    if(app->current_view == FoxLabViewFlpr) {

        app_show_launcher(app);
        return true;
    }

    if(app->device_name_restart_pending) {
        app->restart_confirm_focus_left = true;
        app->current_view = FoxLabViewRestartConfirm;
        view_dispatcher_switch_to_view(app->view_dispatcher, FoxLabViewRestartConfirm);
        return true;
    }

    view_dispatcher_stop(app->view_dispatcher);
    return true;
}

static App* app_alloc(void) {
    App* app = malloc(sizeof(App));
    memset(app, 0, sizeof(App));

    app->baud_option_index = BAUD_OPTION_DEFAULT_INDEX;

    GpioRemapSettings gpio_remap;
    gpio_remap_settings_load(&gpio_remap);
    if(gpio_remap.esp32_uart_channel < app_pin_option_count()) {
        app->pin_option_index = gpio_remap.esp32_uart_channel;
    }

    app->gui = furi_record_open(RECORD_GUI);
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, navigation_callback);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, custom_event_callback);

    app->message_view = message_view_alloc(app);
    app->launcher_view = launcher_view_alloc(app);
    app->connect_settings_view = connect_settings_view_alloc(app);
    app->flpr_view = flpr_view_alloc(app);
    app->restart_confirm_view = restart_confirm_view_alloc(app);

    view_dispatcher_add_view(app->view_dispatcher, FoxLabViewMessage, app->message_view);
    view_dispatcher_add_view(app->view_dispatcher, FoxLabViewLauncher, app->launcher_view);
    view_dispatcher_add_view(
        app->view_dispatcher, FoxLabViewConnectSettings, app->connect_settings_view);
    view_dispatcher_add_view(app->view_dispatcher, FoxLabViewFlpr, app->flpr_view);
    view_dispatcher_add_view(
        app->view_dispatcher, FoxLabViewRestartConfirm, app->restart_confirm_view);

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    app->serial_busy_timer  = furi_timer_alloc(serial_busy_timer_cb,  FuriTimerTypePeriodic, app);
    app->serial_retry_timer = furi_timer_alloc(serial_retry_timer_cb, FuriTimerTypeOnce,     app);

    action_check_esp32(app);

    return app;
}

static void app_free(App* app) {
    furi_timer_stop(app->serial_busy_timer);
    furi_timer_free(app->serial_busy_timer);
    furi_timer_stop(app->serial_retry_timer);
    furi_timer_free(app->serial_retry_timer);

    if(app->router != NULL) esp_at_router_free(app->router);
    if(app->companion != NULL) foxr_companion_free(app->companion);
    if(app->esp_at != NULL) esp_at_free(app->esp_at);

    view_dispatcher_remove_view(app->view_dispatcher, FoxLabViewMessage);
    view_dispatcher_remove_view(app->view_dispatcher, FoxLabViewLauncher);
    view_dispatcher_remove_view(app->view_dispatcher, FoxLabViewConnectSettings);
    view_dispatcher_remove_view(app->view_dispatcher, FoxLabViewFlpr);
    view_dispatcher_remove_view(app->view_dispatcher, FoxLabViewRestartConfirm);

    message_view_free(app->message_view);
    launcher_view_free(app->launcher_view);
    connect_settings_view_free(app->connect_settings_view);
    flpr_view_free(app->flpr_view);
    restart_confirm_view_free(app->restart_confirm_view);
    view_dispatcher_free(app->view_dispatcher);
    furi_record_close(RECORD_GUI);

    free(app);
}

int32_t fox_lab_main(void* p) {
    UNUSED(p);
    App* app = app_alloc();
    view_dispatcher_run(app->view_dispatcher);
    app_free(app);
    return 0;
}
