#include "lfrfid_scene.h"

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_enter,
void (*const lfrfid_on_enter_handlers[])(void*) = {
#include "lfrfid_scene_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_event,
bool (*const lfrfid_on_event_handlers[])(void* context, SceneManagerEvent event) = {
#include "lfrfid_scene_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_exit,
void (*const lfrfid_on_exit_handlers[])(void* context) = {
#include "lfrfid_scene_config.h"
};
#undef ADD_SCENE

const SceneManagerHandlers lfrfid_scene_handlers = {
    .on_enter_handlers = lfrfid_on_enter_handlers,
    .on_event_handlers = lfrfid_on_event_handlers,
    .on_exit_handlers = lfrfid_on_exit_handlers,
    .scene_num = LfRfidSceneNum,
};
