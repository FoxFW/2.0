#include "../predator_i.h"
#include "predator_scene.h"

// Generate scene on_enter handlers array
#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_enter,
void (*const predator_on_enter_handlers[])(void*) = {
#include "predator_scene_config.h"
};
#undef ADD_SCENE

// Generate scene on_event handlers array
#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_event,
bool (*const predator_on_event_handlers[])(void* context, SceneManagerEvent event) = {
#include "predator_scene_config.h"
};
#undef ADD_SCENE

// Generate scene on_exit handlers array
#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_exit,
void (*const predator_on_exit_handlers[])(void* context) = {
#include "predator_scene_config.h"
};
#undef ADD_SCENE

// Initialize scene handlers configuration
const SceneManagerHandlers predator_scene_handlers = {
    .on_enter_handlers = predator_on_enter_handlers,
    .on_event_handlers = predator_on_event_handlers,
    .on_exit_handlers = predator_on_exit_handlers,
    .scene_num = sizeof(predator_on_enter_handlers) / sizeof(predator_on_enter_handlers[0]),
};
