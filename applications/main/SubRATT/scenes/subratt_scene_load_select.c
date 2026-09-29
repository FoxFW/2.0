#include "../subratt_i.h"
#include "subratt_scene.h"

#define TAG "SubRattSceneStart"

void subratt_scene_load_select_callback(SubRattCustomEvent event, void* context) {
    furi_assert(context);

    SubRattState* instance = (SubRattState*)context;
    view_dispatcher_send_custom_event(instance->view_dispatcher, event);
}

void subratt_scene_load_select_on_enter(void* context) {
    furi_assert(context);
#ifdef FURI_DEBUG
    FURI_LOG_I(TAG, "subratt_scene_load_select_on_enter");
#endif
    SubRattState* instance = (SubRattState*)context;
    SubRattMainView* view = instance->view_main;

    instance->current_view = SubRattViewMain;
    subratt_main_view_set_callback(view, subratt_scene_load_select_callback, instance);
    subratt_main_view_set_index(
        view,
        7,
        instance->settings->repeat_values,
        true,
        instance->device->two_bytes,
        instance->device->key_from_file);

    view_dispatcher_switch_to_view(instance->view_dispatcher, instance->current_view);
}

void subratt_scene_load_select_on_exit(void* context) {
    UNUSED(context);
#ifdef FURI_DEBUG
    FURI_LOG_I(TAG, "subratt_scene_load_select_on_exit");
#endif
}

bool subratt_scene_load_select_on_event(void* context, SceneManagerEvent event) {
    SubRattState* instance = (SubRattState*)context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SubRattCustomEventTypeIndexSelected) {

            instance->device->current_step = 0;
            instance->device->bit_index = subratt_main_view_get_index(instance->view_main);
            instance->device->two_bytes = subratt_main_view_get_two_bytes(instance->view_main);

            instance->settings->last_index = instance->device->attack;
            subratt_settings_set_repeats(
                instance->settings, subratt_main_view_get_repeats(instance->view_main));
            uint8_t total_repeats = subratt_settings_get_current_repeats(instance->settings);

            instance->device->max_value = subratt_protocol_calc_max_value(
                instance->device->attack,
                instance->device->bit_index,
                instance->device->two_bytes);

            if(!subratt_worker_init_file_attack(
                   instance->worker,
                   instance->device->current_step,
                   instance->device->bit_index,
                   instance->device->key_from_file,
                   instance->device->file_protocol_info,
                   total_repeats,
                   instance->device->two_bytes)) {
                furi_crash("Invalid attack set!");
            }
            subratt_settings_save(instance->settings);
            scene_manager_next_scene(instance->scene_manager, SubRattSceneSetupAttack);

            consumed = true;
        }

    } else if(event.type == SceneManagerEventTypeBack) {
        if(!scene_manager_search_and_switch_to_previous_scene(
               instance->scene_manager, SubRattSceneStart)) {
            scene_manager_next_scene(instance->scene_manager, SubRattSceneStart);
        }
        consumed = true;
    }

    return consumed;
}
