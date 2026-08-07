#include "message_view.h"
#include "connect_settings.h"
#include "fox_file_downloader_icons.h"

#include <gui/icon.h>
#include <loader/loader.h>
#include <storage/storage.h>

static App* s_message_view_app = NULL;

#define MESSAGE_BOTTOM_BAR_H 16

static void message_draw_two_buttons(
    Canvas* canvas, bool focus_left, const char* left_label, const char* right_label) {
    int32_t bar_y = 64 - MESSAGE_BOTTOM_BAR_H;
    int32_t btn_gap = 4;
    int32_t btn_w = (128 - btn_gap * 3) / 2;
    int32_t left_x = btn_gap;
    int32_t right_x = btn_gap * 2 + btn_w;

    canvas_set_color(canvas, ColorBlack);
    if(focus_left) {
        canvas_draw_rbox(canvas, left_x, bar_y, btn_w, MESSAGE_BOTTOM_BAR_H, 3);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str_aligned(
            canvas, left_x + btn_w / 2, bar_y + MESSAGE_BOTTOM_BAR_H / 2, AlignCenter, AlignCenter, left_label);
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_rframe(canvas, right_x, bar_y, btn_w, MESSAGE_BOTTOM_BAR_H, 3);
        canvas_draw_str_aligned(
            canvas, right_x + btn_w / 2, bar_y + MESSAGE_BOTTOM_BAR_H / 2, AlignCenter, AlignCenter, right_label);
    } else {
        canvas_draw_rframe(canvas, left_x, bar_y, btn_w, MESSAGE_BOTTOM_BAR_H, 3);
        canvas_draw_str_aligned(
            canvas, left_x + btn_w / 2, bar_y + MESSAGE_BOTTOM_BAR_H / 2, AlignCenter, AlignCenter, left_label);
        canvas_draw_rbox(canvas, right_x, bar_y, btn_w, MESSAGE_BOTTOM_BAR_H, 3);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str_aligned(
            canvas, right_x + btn_w / 2, bar_y + MESSAGE_BOTTOM_BAR_H / 2, AlignCenter, AlignCenter, right_label);
        canvas_set_color(canvas, ColorBlack);
    }
}

static void message_draw_one_button(Canvas* canvas, const char* label) {
    int32_t bar_y = 64 - MESSAGE_BOTTOM_BAR_H;

    canvas_set_font(canvas, FontSecondary);
    const Icon* icon = &I_ButtonCenter_7x7;
    int32_t icon_w = icon_get_width(icon);
    int32_t icon_h = icon_get_height(icon);
    int32_t icon_gap = 3;
    int32_t pad_x = 10;
    int32_t content_w = icon_w + icon_gap + (int32_t)canvas_string_width(canvas, label);
    int32_t btn_w = content_w + pad_x * 2;
    int32_t x = (128 - btn_w) / 2;

    canvas_set_color(canvas, ColorBlack);
    canvas_draw_rbox(canvas, x, bar_y, btn_w, MESSAGE_BOTTOM_BAR_H, 3);
    canvas_set_color(canvas, ColorWhite);

    int32_t gx = x + (btn_w - content_w) / 2;
    int32_t gy_icon = bar_y + (MESSAGE_BOTTOM_BAR_H - icon_h) / 2;
    canvas_draw_icon(canvas, gx, gy_icon, icon);
    canvas_draw_str_aligned(
        canvas, gx + icon_w + icon_gap, bar_y + MESSAGE_BOTTOM_BAR_H / 2, AlignLeft, AlignCenter, label);

    canvas_set_color(canvas, ColorBlack);
}

static void message_draw_cb(Canvas* canvas, void* model) {
    UNUSED(model);
    App* app = s_message_view_app;
    if(app == NULL) return;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontSecondary);

    if(app->message_view_detecting) {
        canvas_draw_str_aligned(canvas, 64, 26, AlignCenter, AlignCenter, "Detecting ESP32...");
        canvas_draw_str_aligned(canvas, 64, 38, AlignCenter, AlignCenter, "Checking UART pins");
        return;
    }

    if(app->message_view_serial_busy) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 14, AlignCenter, AlignCenter, "Serial Busy.");
        canvas_set_font(canvas, FontSecondary);
        if(app->message_view_serial_retrying) {
            canvas_draw_str_aligned(canvas, 64, 36, AlignCenter, AlignCenter, "Retrying...");
        } else {
            char buf[4];
            snprintf(buf, sizeof(buf), "%u", app->serial_busy_countdown);
            canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, buf);
            canvas_draw_str_aligned(canvas, 64, 48, AlignCenter, AlignCenter, "Retrying in...");
        }
        return;
    }

    if(app->message_view_serial_retry_failed) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 14, AlignCenter, AlignCenter, "Serial Busy.");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, "Retry Failed.");

        message_draw_two_buttons(canvas, app->message_view_not_detected_focus_left, "Back", "Retry");
        return;
    }

    if(app->message_view_portal_running) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignCenter, "Fox Portal is running");
        canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, "on ESP32");

        message_draw_one_button(canvas, "Portal Settings");
        return;
    }

    canvas_draw_str_aligned(canvas, 64, 14, AlignCenter, AlignCenter, "Fox ESP32 Firmware");
    canvas_draw_str_aligned(canvas, 64, 25, AlignCenter, AlignCenter, "required on ESP32");
    canvas_draw_str_aligned(canvas, 64, 36, AlignCenter, AlignCenter, "connected via GPIO.");

    message_draw_two_buttons(
        canvas, app->message_view_not_detected_focus_left, "Settings", "Retry");
}

static bool message_input_cb(InputEvent* event, void* context) {
    App* app = context;
    if(event->type != InputTypeShort) return false;

    if(app->message_view_detecting) return false;

    if(app->message_view_serial_busy) return false;

    if(app->message_view_portal_running) {
        switch(event->key) {
        case InputKeyOk: {
            Loader* loader = furi_record_open(RECORD_LOADER);
            loader_enqueue_launch(
                loader,
                EXT_PATH("apps/Fox/ESP32/fox_portal.fap"),
                "SKIPSPLASH",
                LoaderDeferredLaunchFlagGui);
            furi_record_close(RECORD_LOADER);
            view_dispatcher_stop(app->view_dispatcher);
            return true;
        }
        case InputKeyBack:
            app->message_view_portal_running = false;
            app_switch_to_menu(app, MenuContextMain);
            return true;
        default:
            return false;
        }
    }

    if(app->message_view_serial_retry_failed) {
        switch(event->key) {
        case InputKeyLeft:
            if(!app->message_view_not_detected_focus_left) {
                app->message_view_not_detected_focus_left = true;
                with_view_model(app->message_view, uint8_t * _m, { UNUSED(_m); }, true);
            }
            return true;
        case InputKeyRight:
            if(app->message_view_not_detected_focus_left) {
                app->message_view_not_detected_focus_left = false;
                with_view_model(app->message_view, uint8_t * _m, { UNUSED(_m); }, true);
            }
            return true;
        case InputKeyOk:
            if(app->message_view_not_detected_focus_left) {
                view_dispatcher_stop(app->view_dispatcher);
                return true;
            }
            app->message_view_serial_retry_failed = false;
            app->message_view_serial_busy = true;
            app->message_view_serial_retrying = true;
            with_view_model(app->message_view, uint8_t * _m, { UNUSED(_m); }, true);
            furi_timer_start(app->serial_retry_timer, 500);
            return true;
        default:
            return false;
        }
    }

    switch(event->key) {
    case InputKeyLeft:
        if(!app->message_view_not_detected_focus_left) {
            app->message_view_not_detected_focus_left = true;
            with_view_model(app->message_view, uint8_t * _m, { UNUSED(_m); }, true);
        }
        return true;
    case InputKeyRight:
        if(app->message_view_not_detected_focus_left) {
            app->message_view_not_detected_focus_left = false;
            with_view_model(app->message_view, uint8_t * _m, { UNUSED(_m); }, true);
        }
        return true;
    case InputKeyOk:
        if(app->message_view_not_detected_focus_left) {
            connect_settings_view_reset(app);
            app->current_view = FoxDownloaderViewConnectSettings;
            view_dispatcher_switch_to_view(app->view_dispatcher, FoxDownloaderViewConnectSettings);
        } else {
            app_retry_detection(app);
        }
        return true;
    case InputKeyUp:
    case InputKeyDown:
        return true;
    case InputKeyBack:
    default:
        return false;
    }
}

View* message_view_alloc(App* app) {
    s_message_view_app = app;
    View* view = view_alloc();
    view_set_draw_callback(view, message_draw_cb);
    view_set_input_callback(view, message_input_cb);
    view_set_context(view, app);
    view_allocate_model(view, ViewModelTypeLocking, sizeof(uint8_t));
    return view;
}

void message_view_free(View* view) {
    s_message_view_app = NULL;
    view_free(view);
}

void message_view_show_detecting(App* app) {
    app->message_view_detecting = true;
    app->message_view_portal_running = false;
    app->current_view = FoxDownloaderViewMessage;
    view_dispatcher_switch_to_view(app->view_dispatcher, FoxDownloaderViewMessage);
}

void message_view_show_not_detected(App* app) {
    app->message_view_detecting           = false;
    app->message_view_not_detected_focus_left = false;
    app->message_view_serial_busy         = false;
    app->message_view_serial_retrying     = false;
    app->message_view_serial_retry_failed = false;
    app->message_view_portal_running      = false;
    app->current_view = FoxDownloaderViewMessage;
    view_dispatcher_switch_to_view(app->view_dispatcher, FoxDownloaderViewMessage);
}

void message_view_show_serial_busy(App* app) {
    app->message_view_detecting           = false;
    app->message_view_serial_retry_failed = false;
    app->message_view_serial_retrying     = false;
    app->message_view_serial_busy         = true;
    app->message_view_portal_running      = false;
    app->current_view = FoxDownloaderViewMessage;
    view_dispatcher_switch_to_view(app->view_dispatcher, FoxDownloaderViewMessage);
}

void message_view_show_serial_retry_failed(App* app) {
    app->message_view_detecting           = false;
    app->message_view_serial_busy         = false;
    app->message_view_serial_retrying     = false;
    app->message_view_not_detected_focus_left = false;
    app->message_view_serial_retry_failed = true;
    app->message_view_portal_running      = false;
    app->current_view = FoxDownloaderViewMessage;
    view_dispatcher_switch_to_view(app->view_dispatcher, FoxDownloaderViewMessage);
}

void message_view_show_portal_running(App* app) {
    app->message_view_detecting           = false;
    app->message_view_serial_busy         = false;
    app->message_view_serial_retrying     = false;
    app->message_view_serial_retry_failed = false;
    app->message_view_not_detected_focus_left = false;
    app->message_view_portal_running      = true;
    app->current_view = FoxDownloaderViewMessage;
    view_dispatcher_switch_to_view(app->view_dispatcher, FoxDownloaderViewMessage);
}
