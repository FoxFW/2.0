#include "../subratt_i.h"
#include "subratt_scene.h"

#define TAG "SubRattSceneKeylogReplaySetup"

static void subratt_scene_keylog_replay_setup_callback(SubRattCustomEvent event, void* context) {
    furi_assert(context);
    SubRattState* instance = context;
    view_dispatcher_send_custom_event(instance->view_dispatcher, event);
}

static void
    subratt_scene_keylog_replay_setup_device_state_changed(void* context, SubRattWorkerState state) {
    furi_assert(context);
    SubRattState* instance = context;
    if(state == SubRattWorkerStateIDLE) {
        view_dispatcher_send_custom_event(instance->view_dispatcher, SubRattCustomEventTypeError);
    }
}

void subratt_scene_keylog_replay_setup_on_enter(void* context) {
    furi_assert(context);
    SubRattState* instance = context;
    SubRattAttackView* view = instance->view_attack;

    notification_message(instance->notifications, &sequence_reset_vibro);

    if(!subratt_worker_init_replay_attack(
           instance->worker,
           instance->device->replay_frequency,
           instance->device->replay_preset,
           instance->device->replay_file,
           instance->device->replay_bits,
           instance->device->replay_te,
           instance->device->replay_repeat,
           instance->device->replay_opencode,
           instance->device->replay_is_file_attack,
           instance->device->replay_load_index,
           instance->device->replay_file_key,
           instance->device->replay_two_bytes,
           instance->device->replay_total_keys,
           furi_string_get_cstr(instance->device->replay_file_path))) {
        dialog_message_show_storage_error(instance->dialogs, "Could not re-open\nkeys file!");
        scene_manager_search_and_switch_to_previous_scene(
            instance->scene_manager, SubRattSceneStart);
        return;
    }

    subratt_worker_set_callback(
        instance->worker, subratt_scene_keylog_replay_setup_device_state_changed, context);

    subratt_attack_view_init_values(
        view,
        SubRattAttackLoadSavedKeys,
        instance->device->replay_total_keys,
        0,
        false,
        instance->device->replay_repeat);

    instance->current_view = SubRattViewAttack;
    subratt_attack_view_set_callback(view, subratt_scene_keylog_replay_setup_callback, instance);
    view_dispatcher_switch_to_view(instance->view_dispatcher, instance->current_view);
}

void subratt_scene_keylog_replay_setup_on_exit(void* context) {
    furi_assert(context);
    SubRattState* instance = context;
    subratt_worker_stop(instance->worker);
}

bool subratt_scene_keylog_replay_setup_on_event(void* context, SceneManagerEvent event) {
    SubRattState* instance = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SubRattCustomEventTypeTransmitStarted) {
            scene_manager_next_scene(instance->scene_manager, SubRattSceneKeylogReplayAttack);
        } else if(event.event == SubRattCustomEventTypeBackPressed) {
            scene_manager_search_and_switch_to_previous_scene(
                instance->scene_manager, SubRattSceneStart);
        } else if(event.event == SubRattCustomEventTypeError) {
            notification_message(instance->notifications, &sequence_error);
        }
        consumed = true;
    }

    return consumed;
}
