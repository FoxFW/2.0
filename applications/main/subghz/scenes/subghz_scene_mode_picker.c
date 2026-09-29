#include "../subghz_i.h"
#include "../views/subghz_view_mode_picker.h"
#include <loader/loader.h>

void subghz_blank_transition_draw_cb(Canvas* canvas, void* ctx);

#define SUBGHZ_TPMS_FAP_PATH EXT_PATH("apps/Fox/fox_tpms.fap")
#define SUBGHZ_RF_JAMMER_FAP_PATH EXT_PATH("apps/Fox/fox_rf_jammer.fap")
#define SUBGHZ_RANDOMATTACK_FAP_PATH EXT_PATH("apps/Sub-GHz/subghz_randomattack.fap")

static void subghz_scene_mode_picker_callback(void* context, uint32_t index) {
    SubGhz* subghz = context;
    view_dispatcher_send_custom_event(subghz->view_dispatcher, index);
}

void subghz_scene_mode_picker_on_enter(void* context) {
    SubGhz* subghz = context;

    if(subghz->startup_holder) {
        view_holder_set_view(subghz->startup_holder, NULL);
        view_holder_free(subghz->startup_holder);
        subghz->startup_holder = NULL;
    }
    if(subghz->startup_loading) {
        loading_free(subghz->startup_loading);
        subghz->startup_loading = NULL;
    }
    if(subghz->state_notifications == SubGhzNotificationStateStarting) {
        subghz->state_notifications = SubGhzNotificationStateIDLE;
    }

    uint32_t last_selected =
        scene_manager_get_scene_state(subghz->scene_manager, SubGhzSceneModePicker);
    subghz_mode_picker_set_selected(subghz->mode_picker, (uint8_t)last_selected);

    subghz_mode_picker_set_callback(
        subghz->mode_picker, subghz_scene_mode_picker_callback, subghz);

    view_dispatcher_switch_to_view(subghz->view_dispatcher, SubGhzViewIdModePicker);
}

static void subghz_scene_mode_picker_launch_jammer_and_exit(SubGhz* subghz) {

    if(!subghz->blank_transition_viewport) {
        subghz->blank_transition_viewport = view_port_alloc();
        view_port_draw_callback_set(
            subghz->blank_transition_viewport, subghz_blank_transition_draw_cb, NULL);
        gui_add_view_port(subghz->gui, subghz->blank_transition_viewport, GuiLayerFullscreen);
        view_port_update(subghz->blank_transition_viewport);
    }

    Loader* loader = furi_record_open(RECORD_LOADER);
    loader_enqueue_launch(
        loader, SUBGHZ_RF_JAMMER_FAP_PATH, "menu:jammer", LoaderDeferredLaunchFlagNone);
    furi_record_close(RECORD_LOADER);

    scene_manager_stop(subghz->scene_manager);
    view_dispatcher_stop(subghz->view_dispatcher);
}

static void subghz_scene_mode_picker_launch_bruteforcer_and_exit(SubGhz* subghz) {

    if(!subghz->blank_transition_viewport) {
        subghz->blank_transition_viewport = view_port_alloc();
        view_port_draw_callback_set(
            subghz->blank_transition_viewport, subghz_blank_transition_draw_cb, NULL);
        gui_add_view_port(subghz->gui, subghz->blank_transition_viewport, GuiLayerFullscreen);
        view_port_update(subghz->blank_transition_viewport);
    }

    Loader* loader = furi_record_open(RECORD_LOADER);
    loader_enqueue_launch(
        loader,
        SUBGHZ_RANDOMATTACK_FAP_PATH,
        "core:menu:bruteforcer",
        LoaderDeferredLaunchFlagNone);
    furi_record_close(RECORD_LOADER);

    scene_manager_stop(subghz->scene_manager);
    view_dispatcher_stop(subghz->view_dispatcher);
}

static void subghz_scene_mode_picker_launch_tpms_and_exit(SubGhz* subghz) {

    if(!subghz->blank_transition_viewport) {
        subghz->blank_transition_viewport = view_port_alloc();
        view_port_draw_callback_set(
            subghz->blank_transition_viewport, subghz_blank_transition_draw_cb, NULL);
        gui_add_view_port(subghz->gui, subghz->blank_transition_viewport, GuiLayerFullscreen);
        view_port_update(subghz->blank_transition_viewport);
    }

    Loader* loader = furi_record_open(RECORD_LOADER);
    loader_enqueue_launch(
        loader, SUBGHZ_TPMS_FAP_PATH, "core:menu:tpms", LoaderDeferredLaunchFlagNone);
    furi_record_close(RECORD_LOADER);

    scene_manager_stop(subghz->scene_manager);
    view_dispatcher_stop(subghz->view_dispatcher);
}

bool subghz_scene_mode_picker_on_event(void* context, SceneManagerEvent event) {
    SubGhz* subghz = context;
    if(event.type == SceneManagerEventTypeBack) {

        scene_manager_stop(subghz->scene_manager);
        view_dispatcher_stop(subghz->view_dispatcher);
        return true;
    } else if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SUBGHZ_MODE_PICKER_AUTOMOTIVE) {
            scene_manager_set_scene_state(
                subghz->scene_manager, SubGhzSceneModePicker, event.event);
            scene_manager_next_scene(subghz->scene_manager, SubGhzSceneStart);
            return true;
        } else if(event.event == SUBGHZ_MODE_PICKER_GARAGE) {
            scene_manager_set_scene_state(
                subghz->scene_manager, SubGhzSceneModePicker, event.event);
            scene_manager_next_scene(subghz->scene_manager, SubGhzSceneGarageMenu);
            return true;
        } else if(event.event == SUBGHZ_MODE_PICKER_JAMMER) {
            scene_manager_set_scene_state(
                subghz->scene_manager, SubGhzSceneModePicker, event.event);
            subghz_scene_mode_picker_launch_jammer_and_exit(subghz);
            return true;
        } else if(event.event == SUBGHZ_MODE_PICKER_BRUTEFORCER) {
            scene_manager_set_scene_state(
                subghz->scene_manager, SubGhzSceneModePicker, event.event);
            subghz_scene_mode_picker_launch_bruteforcer_and_exit(subghz);
            return true;
        } else if(event.event == SUBGHZ_MODE_PICKER_TPMS) {
            scene_manager_set_scene_state(
                subghz->scene_manager, SubGhzSceneModePicker, event.event);
            subghz_scene_mode_picker_launch_tpms_and_exit(subghz);
            return true;
        } else if(event.event == SUBGHZ_MODE_PICKER_RADIO_SETTINGS) {

            scene_manager_set_scene_state(
                subghz->scene_manager, SubGhzSceneModePicker, event.event);
            scene_manager_next_scene(subghz->scene_manager, SubGhzSceneExtModuleSettings);
            return true;
        }
    }
    return false;
}

void subghz_scene_mode_picker_on_exit(void* context) {
    UNUSED(context);
}
