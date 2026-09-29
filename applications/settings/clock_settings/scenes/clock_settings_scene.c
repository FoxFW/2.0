#include "../clock_settings.h"

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_enter,
void (*const clock_settings_scene_on_enter_handlers[])(void*) = {
#include "clock_settings_scene_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_event,
bool (*const clock_settings_scene_on_event_handlers[])(void* context, SceneManagerEvent event) = {
#include "clock_settings_scene_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_exit,
void (*const clock_settings_scene_on_exit_handlers[])(void* context) = {
#include "clock_settings_scene_config.h"
};
#undef ADD_SCENE

const SceneManagerHandlers clock_settings_scene_handlers = {
    .on_enter_handlers = clock_settings_scene_on_enter_handlers,
    .on_event_handlers = clock_settings_scene_on_event_handlers,
    .on_exit_handlers = clock_settings_scene_on_exit_handlers,
    .scene_num = ClockSettingsSceneNum,
};
