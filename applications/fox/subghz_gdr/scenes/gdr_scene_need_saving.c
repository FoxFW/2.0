// scenes/gdr_scene_need_saving.c
#include "../gdr_app_i.h"
#include "gdr_icons.h"

#define TAG "GDRNeedSaving"

static void
    gdr_scene_need_saving_callback(GuiButtonType result, InputType type, void* context) {
    furi_assert(context);
    GDRApp* app = context;

    if((result == GuiButtonTypeRight) && (type == InputTypeShort)) {
        view_dispatcher_send_custom_event(app->view_dispatcher, GDRCustomEventSceneStay);
    } else if((result == GuiButtonTypeLeft) && (type == InputTypeShort)) {
        view_dispatcher_send_custom_event(app->view_dispatcher, GDRCustomEventSceneExit);
    }
}

void gdr_scene_need_saving_on_enter(void* context) {
    furi_check(context);
    GDRApp* app = context;

    if(!gdr_ensure_widget(app)) {
        notification_message(app->notifications, &sequence_error);
        scene_manager_previous_scene(app->scene_manager);
        return;
    }

    widget_add_icon_element(app->widget, 0, 12, &I_WarningDolphin_45x42);
    widget_add_string_multiline_element(
        app->widget, 86, 2, AlignCenter, AlignTop, FontPrimary, "Exit to\nMain Menu?");
    widget_add_string_multiline_element(
        app->widget,
        86,
        26,
        AlignCenter,
        AlignTop,
        FontSecondary,
        "All unsaved data\nwill be lost!");

    widget_add_button_element(
        app->widget, GuiButtonTypeRight, "Stay", gdr_scene_need_saving_callback, app);
    widget_add_button_element(
        app->widget, GuiButtonTypeLeft, "Exit", gdr_scene_need_saving_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, GDRViewWidget);
}

bool gdr_scene_need_saving_on_event(void* context, SceneManagerEvent event) {
    furi_check(context);
    GDRApp* app = context;

    if(event.type == SceneManagerEventTypeBack) {
        // Hardware back button = same as "Stay"
        scene_manager_previous_scene(app->scene_manager);
        return true;
    } else if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == GDRCustomEventSceneStay) {
            scene_manager_previous_scene(app->scene_manager);
            return true;
        } else if(event.event == GDRCustomEventSceneExit) {
            bool history_owner_handled = false;
#ifdef ENABLE_DUAL_RX_SCENE
            if(app->unsaved_history_owner == GDRCaptureOwnerDualReceiver) {
                if(app->dual_history && app->dual_history_mutex) {
                    furi_mutex_acquire(app->dual_history_mutex, FuriWaitForever);
                    gdr_history_reset(app->dual_history);
                    furi_mutex_release(app->dual_history_mutex);
                }
                if(app->dual_receiver) {
                    gdr_view_dual_receiver_reset_menu(app->dual_receiver);
                }
                if(app->selected_capture.owner == GDRCaptureOwnerDualReceiver) {
                    gdr_selected_capture_clear(app);
                }
                history_owner_handled = true;
            }
#endif
#ifdef ENABLE_SHIELD_RX_SCENE
            if(app->unsaved_history_owner == GDRCaptureOwnerShieldReceiver) {
                if(app->shield_history && app->shield_history_mutex) {
                    furi_mutex_acquire(app->shield_history_mutex, FuriWaitForever);
                    gdr_history_reset(app->shield_history);
                    app->shield_auto_save_failed = false;
                    furi_mutex_release(app->shield_history_mutex);
                }
                if(app->gdr_receiver) {
                    gdr_view_receiver_reset_menu(app->gdr_receiver);
                }
                if(app->selected_capture.owner == GDRCaptureOwnerShieldReceiver) {
                    gdr_selected_capture_clear(app);
                }
                history_owner_handled = true;
            }
#endif
            if(!history_owner_handled) {
                gdr_release_shared_radio_state(app);
            }
            scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, GDRSceneStart);
            return true;
        }
    }
    return false;
}

void gdr_scene_need_saving_on_exit(void* context) {
    furi_check(context);
    GDRApp* app = context;
    widget_reset(app->widget);
    app->unsaved_history_owner = GDRCaptureOwnerNone;
}
