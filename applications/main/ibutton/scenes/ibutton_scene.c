#include "ibutton_scene.h"

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_enter,
void (*const ibutton_on_enter_handlers[])(void*) = {
#include "ibutton_scene_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_event,
bool (*const ibutton_on_event_handlers[])(void* context, SceneManagerEvent event) = {
#include "ibutton_scene_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_exit,
void (*const ibutton_on_exit_handlers[])(void* context) = {
#include "ibutton_scene_config.h"
};
#undef ADD_SCENE

const SceneManagerHandlers ibutton_scene_handlers = {
    .on_enter_handlers = ibutton_on_enter_handlers,
    .on_event_handlers = ibutton_on_event_handlers,
    .on_exit_handlers = ibutton_on_exit_handlers,
    .scene_num = iButtonSceneNum,
};
