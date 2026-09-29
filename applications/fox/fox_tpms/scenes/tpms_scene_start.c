#include "../tpms_app_i.h"

typedef enum {
    TPMSStartIndexSelectModel,
    TPMSStartIndexManualScan,
    TPMSStartIndexRelearn,
    TPMSStartIndexCount,
} TPMSStartIndex;

static const TPMSBoxListOption k_start_options[TPMSStartIndexCount] = {
    [TPMSStartIndexSelectModel] = {"Select Model", "Ford, Toyota, etc"},
    [TPMSStartIndexManualScan] = {"Manual Scan", "Unknown vehicle / all freqs"},
    [TPMSStartIndexRelearn] = {"Trigger", "Manual 125kHz activation"},
};

static void tpms_scene_start_box_list_callback(void* context, uint32_t index) {
    TPMSApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void tpms_scene_start_on_enter(void* context) {
    TPMSApp* app = context;

    if(app->startup_holder) {
        view_holder_set_view(app->startup_holder, NULL);
        view_holder_free(app->startup_holder);
        app->startup_holder = NULL;
    }
    if(app->startup_loading) {
        loading_free(app->startup_loading);
        app->startup_loading = NULL;
    }

    tpms_box_list_set_options(app->box_list, k_start_options, TPMSStartIndexCount);
    tpms_box_list_set_selected(
        app->box_list, (uint8_t)scene_manager_get_scene_state(app->scene_manager, TPMSSceneStart));
    tpms_box_list_set_callback(app->box_list, tpms_scene_start_box_list_callback, app);
    tpms_box_list_resume_scroll(app->box_list);

    view_dispatcher_switch_to_view(app->view_dispatcher, TPMSViewBoxList);
}

bool tpms_scene_start_on_event(void* context, SceneManagerEvent event) {
    TPMSApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeBack) {

        scene_manager_stop(app->scene_manager);
        view_dispatcher_stop(app->view_dispatcher);
        consumed = true;
    } else if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(app->scene_manager, TPMSSceneStart, event.event);
        if(event.event == TPMSStartIndexSelectModel) {
            scene_manager_next_scene(app->scene_manager, TPMSSceneVehicleMake);
            consumed = true;
        } else if(event.event == TPMSStartIndexManualScan) {

            app->active_vehicle_group = -1;
            scene_manager_next_scene(app->scene_manager, TPMSSceneReceiver);
            consumed = true;
        } else if(event.event == TPMSStartIndexRelearn) {
            scene_manager_next_scene(app->scene_manager, TPMSSceneRelearn);
            consumed = true;
        }
    }

    return consumed;
}

void tpms_scene_start_on_exit(void* context) {
    TPMSApp* app = context;
    tpms_box_list_pause_scroll(app->box_list);
}
