#include "../tpms_app_i.h"

/* Populated on_enter from tpms_vehicle_groups[] (helpers/tpms_vehicle_groups.c).
 * Static storage duration so the pointer handed to tpms_box_list_set_options()
 * stays valid for as long as this view might redraw - the same requirement
 * the Start scene's compile-time k_start_options array satisfies for free,
 * except here the box list's content is genuinely dynamic (5 vehicle
 * groups pulled from the shared data table rather than 3 hardcoded rows). */
static TPMSBoxListOption k_vehicle_options[TPMS_VEHICLE_GROUP_COUNT];

static void tpms_scene_vehicle_make_box_list_callback(void* context, uint32_t index) {
    TPMSApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void tpms_scene_vehicle_make_on_enter(void* context) {
    TPMSApp* app = context;

    for(uint8_t i = 0; i < TPMS_VEHICLE_GROUP_COUNT; i++) {
        k_vehicle_options[i].title = tpms_vehicle_groups[i].make_name;
        k_vehicle_options[i].subtitle = tpms_vehicle_groups[i].subtitle;
    }

    tpms_box_list_set_options(app->box_list, k_vehicle_options, TPMS_VEHICLE_GROUP_COUNT);
    tpms_box_list_set_selected(
        app->box_list,
        (uint8_t)scene_manager_get_scene_state(app->scene_manager, TPMSSceneVehicleMake));
    tpms_box_list_set_callback(app->box_list, tpms_scene_vehicle_make_box_list_callback, app);
    tpms_box_list_resume_scroll(app->box_list);

    view_dispatcher_switch_to_view(app->view_dispatcher, TPMSViewBoxList);
}

bool tpms_scene_vehicle_make_on_event(void* context, SceneManagerEvent event) {
    TPMSApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(app->scene_manager, TPMSSceneVehicleMake, event.event);
        if(event.event < TPMS_VEHICLE_GROUP_COUNT) {
            app->active_vehicle_group = (int8_t)event.event;
            app->vehicle_step_index = 0;
            scene_manager_next_scene(app->scene_manager, TPMSSceneVehicleSteps);
            consumed = true;
        }
    }
    // Back isn't handled here - SceneManager's default behaviour (this
    // scene's on_event returning false for a Back event) pops back to
    // whichever scene pushed this one, i.e. Start. Same as
    // tpms_scene_receiver_config.c / tpms_scene_receiver_info.c already do.

    return consumed;
}

void tpms_scene_vehicle_make_on_exit(void* context) {
    TPMSApp* app = context;
    tpms_box_list_pause_scroll(app->box_list);
}
