#include "desktop_scene.h"

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_enter,
void (*const desktop_on_enter_handlers[])(void*) = {
#include "desktop_scene_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_event,
bool (*const desktop_on_event_handlers[])(void* context, SceneManagerEvent event) = {
#include "desktop_scene_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_exit,
void (*const desktop_on_exit_handlers[])(void* context) = {
#include "desktop_scene_config.h"
};
#undef ADD_SCENE

const SceneManagerHandlers desktop_scene_handlers = {
    .on_enter_handlers = desktop_on_enter_handlers,
    .on_event_handlers = desktop_on_event_handlers,
    .on_exit_handlers = desktop_on_exit_handlers,
    .scene_num = DesktopSceneNum,
};
