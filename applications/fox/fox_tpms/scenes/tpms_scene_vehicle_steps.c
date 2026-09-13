#include "../tpms_app_i.h"

/* Widget-based instruction walker for the guided "Select Model" flow. Shows
 * one tpms_vehicle_groups[].steps[] entry at a time with a single Center
 * button that either advances to the next step or, on the last step, fires
 * the LF wake trigger and jumps straight to the Receiver (already tuned to
 * that vehicle group's first RF candidate - see tpms_scene_receiver.c). */

typedef enum {
    TPMSVehicleStepsEventNext,
} TPMSVehicleStepsEvent;

static bool tpms_scene_vehicle_steps_group_valid(TPMSApp* app) {
    return app->active_vehicle_group >= 0 && app->active_vehicle_group < TPMS_VEHICLE_GROUP_COUNT;
}

static void
    tpms_scene_vehicle_steps_widget_callback(GuiButtonType result, InputType type, void* context) {
    TPMSApp* app = context;
    if(type != InputTypeShort) return;
    if(result == GuiButtonTypeCenter) {
        view_dispatcher_send_custom_event(app->view_dispatcher, TPMSVehicleStepsEventNext);
    }
}

// Renders whichever step app->vehicle_step_index currently points at. Called
// both from on_enter and again each time Next/Back moves to a different step
// within this same scene instance (no scene re-push needed for that).
static void tpms_scene_vehicle_steps_show(TPMSApp* app) {
    widget_reset(app->widget);

    if(!tpms_scene_vehicle_steps_group_valid(app)) {
        // Defensive only - Vehicle Make always sets a valid index before
        // pushing this scene. Bail out to Receiver with no group active
        // rather than reading tpms_vehicle_groups[] out of bounds.
        app->active_vehicle_group = -1;
        scene_manager_next_scene(app->scene_manager, TPMSSceneReceiver);
        return;
    }

    const TPMSVehicleGroup* group = &tpms_vehicle_groups[app->active_vehicle_group];
    uint8_t step_count = group->step_count ? group->step_count : 1;
    if(app->vehicle_step_index >= step_count) {
        app->vehicle_step_index = step_count - 1;
    }
    const TPMSVehicleStep* step = &group->steps[app->vehicle_step_index];
    bool is_last_step = (app->vehicle_step_index + 1) >= step_count;

    // Heading gets its own fixed top band; the body goes in a bounded,
    // scrollable box (widget_add_text_scroll_element(), not a plain
    // multiline string) so it can never run into the button row below no
    // matter how long a future group's instruction text gets - a plain
    // string element draws unbounded and was overlapping "Next"/"Start
    // Reading" on real hardware for the current 4-line generic step text.
    widget_add_string_multiline_element(
        app->widget, 64, 6, AlignCenter, AlignTop, FontPrimary, step->heading);
    widget_add_text_scroll_element(app->widget, 0, 18, 128, 34, step->body);
    widget_add_button_element(
        app->widget,
        GuiButtonTypeCenter,
        is_last_step ? "Start Reading" : "Next",
        tpms_scene_vehicle_steps_widget_callback,
        app);

    view_dispatcher_switch_to_view(app->view_dispatcher, TPMSViewWidget);
}

void tpms_scene_vehicle_steps_on_enter(void* context) {
    TPMSApp* app = context;
    tpms_scene_vehicle_steps_show(app);
}

bool tpms_scene_vehicle_steps_on_event(void* context, SceneManagerEvent event) {
    TPMSApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == TPMSVehicleStepsEventNext) {
            const TPMSVehicleGroup* group = tpms_scene_vehicle_steps_group_valid(app) ?
                                                 &tpms_vehicle_groups[app->active_vehicle_group] :
                                                 NULL;
            bool is_last_step = !group || ((app->vehicle_step_index + 1) >= group->step_count);
            if(is_last_step) {
                // Same LF wake helper the manual Relearn scene uses - see
                // tpms_relearn_lf_start()'s comment in tpms_app_i.h.
                tpms_relearn_lf_start(app);
                scene_manager_next_scene(app->scene_manager, TPMSSceneReceiver);
            } else {
                app->vehicle_step_index++;
                tpms_scene_vehicle_steps_show(app);
            }
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        if(app->vehicle_step_index > 0) {
            app->vehicle_step_index--;
            tpms_scene_vehicle_steps_show(app);
            consumed = true;
        }
        // else: not consumed - falls through to SceneManager's default
        // previous-scene navigation, back to Vehicle Make.
    }

    return consumed;
}

void tpms_scene_vehicle_steps_on_exit(void* context) {
    TPMSApp* app = context;
    widget_reset(app->widget);
}
