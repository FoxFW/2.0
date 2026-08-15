#include "message_view.h"
#include "fox_ai_chat_icons.h"
#include <gui/icon_i.h>

static App* s_message_view_app = NULL;

static void message_draw_single_button(Canvas* canvas, const char* label) {
    const Icon* icon = &I_ButtonCenter_7x7;
    int32_t icon_gap = 3;
    int32_t pad = 10;
    int32_t btn_h = 14;
    int32_t btn_y = 64 - btn_h - 4;
    int32_t group_w = icon->width + icon_gap + (int32_t)canvas_string_width(canvas, label);
    int32_t btn_w = group_w + pad * 2;
    int32_t btn_x = (128 - btn_w) / 2;

    canvas_set_color(canvas, ColorBlack);
    canvas_draw_rbox(canvas, btn_x, btn_y, btn_w, btn_h, 3);
    canvas_set_color(canvas, ColorWhite);
    int32_t gx = btn_x + pad;
    canvas_draw_icon(canvas, gx, btn_y + (btn_h - icon->height) / 2, icon);
    canvas_draw_str_aligned(
        canvas, gx + icon->width + icon_gap, btn_y + btn_h / 2, AlignLeft, AlignCenter, label);
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
        canvas_draw_str_aligned(canvas, 64, 28, AlignCenter, AlignCenter, "Retry Failed.");
        message_draw_single_button(canvas, "Retry");
        return;
    }

    if(app->message_view_wifi_not_connected) {
        canvas_draw_str_aligned(canvas, 64, 14, AlignCenter, AlignCenter, "WiFi not connected.");
        canvas_draw_str_aligned(canvas, 64, 25, AlignCenter, AlignCenter, "Use Fox Commander");
        canvas_draw_str_aligned(canvas, 64, 36, AlignCenter, AlignCenter, "to connect WiFi.");
        message_draw_single_button(canvas, "Commander");
        return;
    }

    canvas_draw_str_aligned(canvas, 64, 14, AlignCenter, AlignCenter, "Fox ESP32 Firmware");
    canvas_draw_str_aligned(canvas, 64, 25, AlignCenter, AlignCenter, "required on ESP32");
    canvas_draw_str_aligned(canvas, 64, 36, AlignCenter, AlignCenter, "connected via GPIO.");
    message_draw_single_button(canvas, "Retry");
}

static bool message_input_cb(InputEvent* event, void* context) {
    App* app = context;
    if(event->type != InputTypeShort) return false;

    if(app->message_view_detecting) return false;
    if(app->message_view_serial_busy) return false;

    if(app->message_view_serial_retry_failed) {
        if(event->key == InputKeyOk) {
            app->message_view_serial_retry_failed = false;
            app->message_view_serial_busy = true;
            app->message_view_serial_retrying = true;
            with_view_model(app->message_view, uint8_t * _m, { UNUSED(_m); }, true);
            furi_timer_start(app->serial_retry_timer, 500);
            return true;
        }
        return false;
    }

    if(app->message_view_wifi_not_connected) {
        if(event->key == InputKeyOk) {
            app_launch_commander(app);
            return true;
        }
        return false;
    }

    if(event->key == InputKeyOk) {
        app_retry_detection(app);
        return true;
    }
    return false;
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
    app->current_view = AiChatViewMessage;
    view_dispatcher_switch_to_view(app->view_dispatcher, AiChatViewMessage);
}

void message_view_show_not_detected(App* app) {
    app->message_view_detecting = false;
    app->message_view_wifi_not_connected = false;
    app->message_view_serial_busy = false;
    app->message_view_serial_retrying = false;
    app->message_view_serial_retry_failed = false;
    app->current_view = AiChatViewMessage;
    view_dispatcher_switch_to_view(app->view_dispatcher, AiChatViewMessage);
}

void message_view_show_wifi_not_connected(App* app) {
    app->message_view_detecting = false;
    app->message_view_serial_busy = false;
    app->message_view_serial_retrying = false;
    app->message_view_serial_retry_failed = false;
    app->message_view_wifi_not_connected = true;
    app->current_view = AiChatViewMessage;
    view_dispatcher_switch_to_view(app->view_dispatcher, AiChatViewMessage);
}

void message_view_show_serial_busy(App* app) {
    app->message_view_detecting = false;
    app->message_view_wifi_not_connected = false;
    app->message_view_serial_retry_failed = false;
    app->message_view_serial_retrying = false;
    app->message_view_serial_busy = true;
    app->current_view = AiChatViewMessage;
    view_dispatcher_switch_to_view(app->view_dispatcher, AiChatViewMessage);
}

void message_view_show_serial_retry_failed(App* app) {
    app->message_view_detecting = false;
    app->message_view_wifi_not_connected = false;
    app->message_view_serial_busy = false;
    app->message_view_serial_retrying = false;
    app->message_view_serial_retry_failed = true;
    app->current_view = AiChatViewMessage;
    view_dispatcher_switch_to_view(app->view_dispatcher, AiChatViewMessage);
}
