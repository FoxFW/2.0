#include "../tpms_app_i.h"

/* Manual LF 125kHz wake/activation trigger screen - reached from the Start
 * scene's "Trigger" row (renamed from "Relearn / Activate" - this only ever
 * fires a one-shot LF pulse, it doesn't "relearn" anything, so the button
 * below has said "Trigger" from the start; the heading/row label just
 * hadn't caught up until now). Previously this scene was a VariableItemList
 * of two settings ("Relearn 125kHz" ON/OFF, "Type": Common) that wrote to
 * app->relearn/app->relearn_type, fields read nowhere else in the app -
 * selecting them did nothing. Replaced with a real action screen: one
 * button that actually fires the LF pulse via tpms_relearn_lf_start()
 * (tpms_app_i.c), the same helper the guided vehicle-steps flow's last page
 * uses. */

typedef enum {
    TPMSSceneRelearnEventTrigger,
} TPMSSceneRelearnEvent;

static void
    tpms_scene_relearn_widget_callback(GuiButtonType result, InputType type, void* context) {
    TPMSApp* app = context;
    if(type != InputTypeShort) return;
    if(result == GuiButtonTypeCenter) {
        view_dispatcher_send_custom_event(app->view_dispatcher, TPMSSceneRelearnEventTrigger);
    }
}

void tpms_scene_relearn_config_on_enter(void* context) {
    TPMSApp* app = context;

    widget_reset(app->widget);
    // Same bounded/scrollable body as the guided flow's Vehicle Steps
    // screen (see that file's comment) - the previous plain multiline
    // string ran under the "Trigger" button on real hardware.
    widget_add_string_multiline_element(
        app->widget, 64, 4, AlignCenter, AlignTop, FontPrimary, "Trigger TPMS");
    // Explicit \n-separated lines (exact wording/line breaks per the user's
    // spec, to stop mid-word wrapping) rather than one auto-wrapping
    // paragraph - so, per widget_element_text_scroll.c's own behavior
    // (alignment resets to left on every explicit new line), \ec has to be
    // repeated on each line to stay centered, same as tpms_vehicle_groups.c's
    // k_generic_steps does for its own explicit-line body text (see that
    // file's longer comment on this).
    widget_add_text_scroll_element(
        app->widget,
        0,
        18,
        128,
        34,
        "\ecHold the Flippers back flat\n"
        "\ecagainst the base of the valve\n"
        "\ecstem & Press TRIGGER to send\n"
        "\eca 125kHz wake/activation pulse.");
    widget_add_button_element(
        app->widget, GuiButtonTypeCenter, "Trigger", tpms_scene_relearn_widget_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, TPMSViewWidget);
}

bool tpms_scene_relearn_config_on_event(void* context, SceneManagerEvent event) {
    TPMSApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == TPMSSceneRelearnEventTrigger) {
            tpms_relearn_lf_start(app);
            notification_message(app->notifications, &sequence_blink_green_10);
            consumed = true;
        }
    }

    return consumed;
}

void tpms_scene_relearn_config_on_exit(void* context) {
    TPMSApp* app = context;
    if(app->lf_relearn_active) {
        tpms_relearn_lf_stop(app);
    }
    widget_reset(app->widget);
}
