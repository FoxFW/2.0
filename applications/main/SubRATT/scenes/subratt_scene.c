#include "subratt_scene.h"

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_enter,
void (*const subratt_on_enter_handlers[])(void*) = {
#include "subratt_scene_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_event,
bool (*const subratt_on_event_handlers[])(void* context, SceneManagerEvent event) = {
#include "subratt_scene_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_exit,
void (*const subratt_on_exit_handlers[])(void* context) = {
#include "subratt_scene_config.h"
};
#undef ADD_SCENE

const SceneManagerHandlers subratt_scene_handlers = {
    .on_enter_handlers = subratt_on_enter_handlers,
    .on_event_handlers = subratt_on_event_handlers,
    .on_exit_handlers = subratt_on_exit_handlers,
    .scene_num = SubRattSceneNum,
};
