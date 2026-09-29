#include "../subratt_i.h"
#include "subratt_scene.h"

#define TAG "SubRattSceneSetupAttack"

static void subratt_scene_setup_attack_callback(SubRattCustomEvent event, void* context) {
    furi_assert(context);

    SubRattState* instance = (SubRattState*)context;
    view_dispatcher_send_custom_event(instance->view_dispatcher, event);
}

static void
    subratt_scene_setup_attack_device_state_changed(void* context, SubRattWorkerState state) {
    furi_assert(context);

    SubRattState* instance = (SubRattState*)context;

    if(state == SubRattWorkerStateIDLE) {

        view_dispatcher_send_custom_event(instance->view_dispatcher, SubRattCustomEventTypeError);
    }
}

void subratt_scene_setup_attack_on_enter(void* context) {
    furi_assert(context);
    SubRattState* instance = (SubRattState*)context;
    SubRattAttackView* view = instance->view_attack;

    notification_message(instance->notifications, &sequence_reset_vibro);

#ifdef FURI_DEBUG
    FURI_LOG_D(TAG, "Enter Attack: %s", subratt_protocol_name(instance->device->attack));
#endif

    subratt_worker_set_callback(
        instance->worker, subratt_scene_setup_attack_device_state_changed, context);
    if(subratt_worker_is_running(instance->worker)) {
        subratt_worker_stop(instance->worker);
        instance->device->current_step = subratt_worker_get_step(instance->worker);
    }

    subratt_attack_view_init_values(
        view,
        instance->device->attack,
        instance->device->max_value,
        instance->device->current_step,
        false,
        subratt_worker_get_repeats(instance->worker));

    instance->current_view = SubRattViewAttack;
    subratt_attack_view_set_callback(view, subratt_scene_setup_attack_callback, instance);
    view_dispatcher_switch_to_view(instance->view_dispatcher, instance->current_view);
}

void subratt_scene_setup_attack_on_exit(void* context) {
    furi_assert(context);
#ifdef FURI_DEBUG
    FURI_LOG_D(TAG, "subratt_scene_setup_attack_on_exit");
#endif
    SubRattState* instance = (SubRattState*)context;
    subratt_worker_stop(instance->worker);
    notification_message(instance->notifications, &sequence_blink_stop);
    notification_message(instance->notifications, &sequence_reset_vibro);
}

bool subratt_scene_setup_attack_on_event(void* context, SceneManagerEvent event) {
    SubRattState* instance = (SubRattState*)context;
    SubRattAttackView* view = instance->view_attack;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SubRattCustomEventTypeTransmitStarted) {
            scene_manager_next_scene(instance->scene_manager, SubRattSceneRunAttack);
        } else if(event.event == SubRattCustomEventTypeSaveFile) {
            subratt_attack_view_init_values(
                view,
                instance->device->attack,
                instance->device->max_value,
                instance->device->current_step,
                false,
                instance->device->extra_repeats);
            scene_manager_next_scene(instance->scene_manager, SubRattSceneSaveName);
        } else if(event.event == SubRattCustomEventTypeExtraSettings) {
            scene_manager_next_scene(instance->scene_manager, SubRattSceneSetupExtra);
        } else if(event.event == SubRattCustomEventTypeBackPressed) {
            subratt_attack_view_init_values(
                view,
                instance->device->attack,
                instance->device->max_value,
                instance->device->current_step,
                false,
                instance->device->extra_repeats);
            scene_manager_next_scene(instance->scene_manager, SubRattSceneStart);
        } else if(event.event == SubRattCustomEventTypeError) {
            notification_message(instance->notifications, &sequence_error);
        } else if(event.event == SubRattCustomEventTypeTransmitCustom) {

            if(subratt_worker_can_manual_transmit(instance->worker)) {

                notification_message(instance->notifications, &sequence_blink_green_100);
                subratt_worker_transmit_current_key(
                    instance->worker, instance->device->current_step);

                notification_message(instance->notifications, &sequence_blink_stop);
            }
        } else if(event.event == SubRattCustomEventTypeChangeStepUp) {

            uint64_t step = subratt_device_add_step(instance->device, 1);
            subratt_worker_set_step(instance->worker, step);
            subratt_attack_view_set_current_step(view, step);
        } else if(event.event == SubRattCustomEventTypeChangeStepUpMore) {

            uint64_t step = subratt_device_add_step(instance->device, 50);
            subratt_worker_set_step(instance->worker, step);
            subratt_attack_view_set_current_step(view, step);
        } else if(event.event == SubRattCustomEventTypeChangeStepDown) {

            uint64_t step = subratt_device_add_step(instance->device, -1);
            subratt_worker_set_step(instance->worker, step);
            subratt_attack_view_set_current_step(view, step);
        } else if(event.event == SubRattCustomEventTypeChangeStepDownMore) {

            uint64_t step = subratt_device_add_step(instance->device, -50);
            subratt_worker_set_step(instance->worker, step);
            subratt_attack_view_set_current_step(view, step);
        }

        consumed = true;
    } else if(event.type == SceneManagerEventTypeTick) {
        if(subratt_worker_is_running(instance->worker)) {
            instance->device->current_step = subratt_worker_get_step(instance->worker);
        }
        subratt_attack_view_set_current_step(view, instance->device->current_step);
        consumed = true;
    }

    return consumed;
}
