#include "desktop_settings_view_pin_setup_howto.h"
#include <gui/canvas.h>
#include <assets_icons.h>
#include <furi.h>

struct DesktopSettingsViewPinSetupHowto {
    View* view;
    DesktopSettingsViewPinSetupHowtoDoneCallback done_callback;
    void* context;
};

static void desktop_settings_view_pin_setup_howto_draw_callback(Canvas* canvas, void* context) {
    furi_assert(context);
    
    canvas_clear(canvas);
    
    // FBT automatically links this from the images/fox_64x64.png file
    canvas_draw_icon(canvas, 0, 0, &I_fox_64x64);

    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 68, 12, "PIN Setup");
    
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 68, 26, "Create a");
    canvas_draw_str(canvas, 68, 36, "secure code");
    canvas_draw_str(canvas, 68, 46, "using the");
    canvas_draw_str(canvas, 68, 56, "keypad.");
}

static bool desktop_settings_view_pin_setup_howto_input_callback(InputEvent* event, void* context) {
    furi_assert(context);
    struct DesktopSettingsViewPinSetupHowto* instance = context;
    
    /* When OK or Right is short-pressed, cleanly execute our bound scene event callback */
    if(event->type == InputTypeShort) {
        if(event->key == InputKeyOk || event->key == InputKeyRight) {
            if(instance->done_callback) {
                instance->done_callback(instance->context);
                return true;
            }
        }
    }
    return false;
}

DesktopSettingsViewPinSetupHowto* desktop_settings_view_pin_setup_howto_alloc(void) {
    DesktopSettingsViewPinSetupHowto* instance = malloc(sizeof(DesktopSettingsViewPinSetupHowto));
    instance->view = view_alloc();
    instance->done_callback = NULL;
    instance->context = NULL;
    
    view_set_context(instance->view, instance);
    view_set_draw_callback(instance->view, desktop_settings_view_pin_setup_howto_draw_callback);
    view_set_input_callback(instance->view, desktop_settings_view_pin_setup_howto_input_callback);
    
    return instance;
}

void desktop_settings_view_pin_setup_howto_free(DesktopSettingsViewPinSetupHowto* instance) {
    furi_assert(instance);
    view_free(instance->view);
    free(instance);
}

View* desktop_settings_view_pin_setup_howto_get_view(DesktopSettingsViewPinSetupHowto* instance) {
    furi_assert(instance);
    return instance->view;
}

void desktop_settings_view_pin_setup_howto_set_callback(
    DesktopSettingsViewPinSetupHowto* instance,
    DesktopSettingsViewPinSetupHowtoDoneCallback callback,
    void* context) {
    furi_assert(instance);
    instance->done_callback = callback;
    instance->context = context;
}