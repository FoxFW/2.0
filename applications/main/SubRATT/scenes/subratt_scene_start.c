#include "../subratt_i.h"
#include "subratt_scene.h"

#define TAG "SubRattSceneStart"

void subratt_scene_start_callback(SubRattCustomEvent event, void* context) {
    furi_assert(context);

    SubRattState* instance = (SubRattState*)context;
    view_dispatcher_send_custom_event(instance->view_dispatcher, event);
}

void subratt_scene_start_on_enter(void* context) {
    furi_assert(context);
#ifdef FURI_DEBUG
    FURI_LOG_I(TAG, "subratt_scene_start_on_enter");
#endif
    SubRattState* instance = (SubRattState*)context;
    SubRattMainView* view = instance->view_main;

    instance->current_view = SubRattViewMain;
    subratt_main_view_set_callback(view, subratt_scene_start_callback, instance);

    instance->device->attack = instance->settings->last_index;

    subratt_main_view_set_index(
        view,
        instance->settings->last_index,
        instance->settings->repeat_values,
        false,
        instance->device->two_bytes,
        0);

    view_dispatcher_switch_to_view(instance->view_dispatcher, instance->current_view);
}

void subratt_scene_start_on_exit(void* context) {
    furi_assert(context);
}

bool subratt_scene_start_on_event(void* context, SceneManagerEvent event) {
    SubRattState* instance = (SubRattState*)context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
#ifdef FURI_DEBUG
        FURI_LOG_D(
            TAG,
            "Event: %ld, SubRattCustomEventTypeMenuSelected: %s, SubRattCustomEventTypeLoadFile: %s",
            event.event,
            event.event == SubRattCustomEventTypeMenuSelected ? "true" : "false",
            event.event == SubRattCustomEventTypeLoadFile ? "true" : "false");
#endif
        if(event.event == SubRattCustomEventTypeMenuSelected) {
            instance->settings->last_index = subratt_main_view_get_index(instance->view_main);
            subratt_settings_set_repeats(
                instance->settings, subratt_main_view_get_repeats(instance->view_main));
            uint8_t total_repeats = subratt_settings_get_current_repeats(instance->settings);

            if((subratt_device_attack_set(
                    instance->device, instance->settings->last_index, total_repeats) !=
                SubRattFileResultOk) ||
               (!subratt_worker_init_default_attack(
                   instance->worker,
                   instance->settings->last_index,
                   instance->device->current_step,
                   instance->device->protocol_info,
                   instance->device->extra_repeats))) {
                furi_crash("Invalid attack set!");
            }
            subratt_settings_save(instance->settings);

            scene_manager_next_scene(instance->scene_manager, SubRattSceneAttackMode);

            consumed = true;
        } else if(event.event == SubRattCustomEventTypeLoadFile) {

            scene_manager_next_scene(instance->scene_manager, SubRattSceneLoadFile);
            consumed = true;
        } else if(event.event == SubRattCustomEventTypeLoadSavedKeys) {
            scene_manager_next_scene(instance->scene_manager, SubRattSceneKeylogLoadSelect);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {

        instance->settings->last_index = subratt_main_view_get_index(instance->view_main);
        subratt_settings_set_repeats(
            instance->settings, subratt_main_view_get_repeats(instance->view_main));
        subratt_settings_save(instance->settings);

        scene_manager_stop(instance->scene_manager);
        view_dispatcher_stop(instance->view_dispatcher);
        consumed = true;
    }

    return consumed;
}
