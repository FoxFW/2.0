#include "message_view.h"
#include "connect_settings.h"
#include "fox_chat_icons.h"


static App* s_message_view_app = NULL;

#define MESSAGE_BOTTOM_BAR_H 16

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

    if(app->message_view_wifi_not_connected) {
        canvas_draw_str(canvas, 2, 10, "WiFi not connected.");
        canvas_draw_str(canvas, 2, 22, "Use Fox Commander");
        canvas_draw_str(canvas, 2, 34, "to connect WiFi.");

        int32_t bar_y = 64 - MESSAGE_BOTTOM_BAR_H;
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_box(canvas, 0, bar_y, 128, MESSAGE_BOTTOM_BAR_H);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_icon(canvas, 34, bar_y + 4, &I_ButtonCenter_7x7);
        canvas_draw_str_aligned(
            canvas, 44, bar_y + MESSAGE_BOTTOM_BAR_H / 2,
            AlignLeft, AlignCenter, "Commander");
        canvas_set_color(canvas, ColorBlack);
        return;
    }

    canvas_draw_str(canvas, 2, 10, "Fox ESP32 Firmware");
    canvas_draw_str(canvas, 2, 20, "required on ESP32");
    canvas_draw_str(canvas, 2, 30, "connected via GPIO.");

    int32_t bar_y = 64 - MESSAGE_BOTTOM_BAR_H;
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, bar_y, 128, MESSAGE_BOTTOM_BAR_H);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_str_aligned(
        canvas, 4, bar_y + MESSAGE_BOTTOM_BAR_H / 2, AlignLeft, AlignCenter, "< Settings");
    canvas_draw_str_aligned(
        canvas, 124, bar_y + MESSAGE_BOTTOM_BAR_H / 2, AlignRight, AlignCenter, "Retry >");
    canvas_set_color(canvas, ColorBlack);
}

static bool message_input_cb(InputEvent* event, void* context) {
    App* app = context;
    if(event->type != InputTypeShort) return false;

    if(app->message_view_detecting) return false;

    if(app->message_view_wifi_not_connected) {
        switch(event->key) {
        case InputKeyRight:
        case InputKeyOk:
            app_launch_commander(app);
            return true;
        case InputKeyUp:
        case InputKeyDown:
        case InputKeyLeft:
            return true; /* consumed, intentional no-op */
        case InputKeyBack:
        default:
            return false;
        }
    }

    switch(event->key) {
    case InputKeyLeft:
        connect_settings_view_reset(app);
        app->current_view = FoxCommanderViewConnectSettings;
        view_dispatcher_switch_to_view(app->view_dispatcher, FoxCommanderViewConnectSettings);
        return true;
    case InputKeyRight:
    case InputKeyOk:
        app_retry_detection(app);
        return true;
    case InputKeyUp:
    case InputKeyDown:
        return true; /* consumed, intentional no-op */
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
    app->current_view = FoxCommanderViewMessage;
    view_dispatcher_switch_to_view(app->view_dispatcher, FoxCommanderViewMessage);
}

void message_view_show_not_detected(App* app) {
    app->message_view_detecting = false;
    app->message_view_wifi_not_connected = false;
    app->current_view = FoxCommanderViewMessage;
    view_dispatcher_switch_to_view(app->view_dispatcher, FoxCommanderViewMessage);
}

void message_view_show_wifi_not_connected(App* app) {
    app->message_view_detecting = false;
    app->message_view_wifi_not_connected = true;
    app->current_view = FoxCommanderViewMessage;
    view_dispatcher_switch_to_view(app->view_dispatcher, FoxCommanderViewMessage);
}
