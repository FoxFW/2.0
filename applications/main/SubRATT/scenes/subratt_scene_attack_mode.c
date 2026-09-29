#include "../subratt_i.h"
#include "subratt_scene.h"

#define TAG "SubRattSceneAttackMode"

enum SubRattAttackModeIndex {
    SubRattAttackModeIndexBruteForce,
    SubRattAttackModeIndexRandomize,
};

static void subratt_scene_attack_mode_enter_callback(void* context, uint32_t index) {
    furi_assert(context);
    SubRattState* instance = (SubRattState*)context;

    bool random_mode = (index == SubRattAttackModeIndexRandomize);
    subratt_worker_set_random_mode(instance->worker, random_mode);

#ifdef FURI_DEBUG
    FURI_LOG_D(TAG, "Attack mode chosen: %s", random_mode ? "Randomize" : "Brute Force");
#endif

    scene_manager_next_scene(instance->scene_manager, SubRattSceneSetupAttack);
}

void subratt_scene_attack_mode_on_enter(void* context) {
    furi_assert(context);
    SubRattState* instance = (SubRattState*)context;

    const SubRattProtocol* protocol = instance->device->protocol_info;

    if(protocol == NULL || protocol->bits > SUBRATT_DUAL_MODE_MAX_BITS) {

        subratt_worker_set_random_mode(instance->worker, true);
        scene_manager_next_scene(instance->scene_manager, SubRattSceneSetupAttack);
        return;
    }

    SubRattAttackModeView* view = instance->view_attack_mode;
    subratt_attack_mode_view_set_title(view, subratt_protocol_title(instance->device->attack));
    subratt_attack_mode_view_set_callback(
        view, subratt_scene_attack_mode_enter_callback, instance);

    view_dispatcher_switch_to_view(instance->view_dispatcher, SubRattViewAttackMode);
}

void subratt_scene_attack_mode_on_exit(void* context) {
    UNUSED(context);
}

bool subratt_scene_attack_mode_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}
