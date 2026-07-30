#include "gdr_scene.h"

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_enter,
void (*const gdr_scene_on_enter_handlers[])(void*) = {
#include "gdr_scene_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_event,
bool (*const gdr_scene_on_event_handlers[])(void* context, SceneManagerEvent event) = {
#include "gdr_scene_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_exit,
void (*const gdr_scene_on_exit_handlers[])(void* context) = {
#include "gdr_scene_config.h"
};
#undef ADD_SCENE

const SceneManagerHandlers gdr_scene_handlers = {
    .on_enter_handlers = gdr_scene_on_enter_handlers,
    .on_event_handlers = gdr_scene_on_event_handlers,
    .on_exit_handlers = gdr_scene_on_exit_handlers,
    .scene_num = GDRSceneNum,
};
