#include "gpio_scene.h"

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_enter,
void (*const gpio_scene_on_enter_handlers[])(void*) = {
#include "gpio_scene_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_event,
bool (*const gpio_scene_on_event_handlers[])(void* context, SceneManagerEvent event) = {
#include "gpio_scene_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_exit,
void (*const gpio_scene_on_exit_handlers[])(void* context) = {
#include "gpio_scene_config.h"
};
#undef ADD_SCENE

const SceneManagerHandlers gpio_scene_handlers = {
    .on_enter_handlers = gpio_scene_on_enter_handlers,
    .on_event_handlers = gpio_scene_on_event_handlers,
    .on_exit_handlers = gpio_scene_on_exit_handlers,
    .scene_num = GpioSceneNum,
};
