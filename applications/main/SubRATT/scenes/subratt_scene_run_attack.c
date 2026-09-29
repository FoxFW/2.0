#include "../subratt_i.h"
#include "subratt_scene.h"

#define TAG "SubRattSceneRunAttack"

static void subratt_scene_run_attack_callback(SubRattCustomEvent event, void* context) {
    furi_assert(context);

    SubRattState* instance = (SubRattState*)context;
    view_dispatcher_send_custom_event(instance->view_dispatcher, event);
}

static void
    subratt_scene_run_attack_device_state_changed(void* context, SubRattWorkerState state) {
    furi_assert(context);

    SubRattState* instance = (SubRattState*)context;

    if(state == SubRattWorkerStateIDLE) {

        view_dispatcher_send_custom_event(instance->view_dispatcher, SubRattCustomEventTypeError);
    } else if(state == SubRattWorkerStateFinished) {
        view_dispatcher_send_custom_event(
            instance->view_dispatcher, SubRattCustomEventTypeTransmitFinished);
    }
}
void subratt_scene_run_attack_on_exit(void* context) {
    furi_assert(context);
    SubRattState* instance = (SubRattState*)context;

    notification_message(instance->notifications, &sequence_blink_stop);
    subratt_worker_stop(instance->worker);
}

void subratt_scene_run_attack_on_enter(void* context) {
    furi_assert(context);
    SubRattState* instance = (SubRattState*)context;
    SubRattAttackView* view = instance->view_attack;

    instance->current_view = SubRattViewAttack;
    subratt_attack_view_set_callback(view, subratt_scene_run_attack_callback, instance);
    view_dispatcher_switch_to_view(instance->view_dispatcher, instance->current_view);

    subratt_worker_set_callback(
        instance->worker, subratt_scene_run_attack_device_state_changed, instance);

    if(!subratt_worker_is_running(instance->worker)) {
        subratt_worker_set_step(instance->worker, instance->device->current_step);
        if(!subratt_worker_start(instance->worker)) {
            view_dispatcher_send_custom_event(
                instance->view_dispatcher, SubRattCustomEventTypeError);
        } else {
            notification_message(instance->notifications, &sequence_single_vibro);
            notification_message(instance->notifications, &sequence_blink_start_yellow);
        }
    }
}

bool subratt_scene_run_attack_on_event(void* context, SceneManagerEvent event) {
    SubRattState* instance = (SubRattState*)context;
    SubRattAttackView* view = instance->view_attack;

    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        uint64_t step = subratt_worker_get_step(instance->worker);
        instance->device->current_step = step;
        subratt_attack_view_set_current_step(view, step);

        bool have_logged_keys = subratt_worker_get_keylog_count(instance->worker) > 0;

        if(event.event == SubRattCustomEventTypeTransmitFinished) {
            notification_message(instance->notifications, &sequence_display_backlight_on);
            notification_message(instance->notifications, &sequence_double_vibro);

            if(have_logged_keys) {
                scene_manager_next_scene(instance->scene_manager, SubRattSceneKeylogStopConfirm);
            } else {
                scene_manager_next_scene(instance->scene_manager, SubRattSceneSetupAttack);
            }
        } else if(
            event.event == SubRattCustomEventTypeTransmitNotStarted ||
            event.event == SubRattCustomEventTypeBackPressed) {
            if(subratt_worker_is_running(instance->worker)) {

                notification_message(instance->notifications, &sequence_single_vibro);
            }

            if(have_logged_keys) {
                scene_manager_next_scene(instance->scene_manager, SubRattSceneKeylogStopConfirm);
            } else {
                scene_manager_search_and_switch_to_previous_scene(
                    instance->scene_manager, SubRattSceneSetupAttack);
            }
        } else if(event.event == SubRattCustomEventTypeError) {
            notification_message(instance->notifications, &sequence_error);

            if(have_logged_keys) {
                scene_manager_next_scene(instance->scene_manager, SubRattSceneKeylogStopConfirm);
            } else {
                scene_manager_search_and_switch_to_previous_scene(
                    instance->scene_manager, SubRattSceneSetupAttack);
            }
        } else if(event.event == SubRattCustomEventTypeUpdateView) {

        }
        consumed = true;
    } else if(event.type == SceneManagerEventTypeTick) {
        uint64_t step = subratt_worker_get_step(instance->worker);
        instance->device->current_step = step;
        subratt_attack_view_set_current_step(view, step);

        consumed = true;
    }

    return consumed;
}
