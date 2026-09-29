#include "storage_settings_scene.h"

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_enter,
void (*const storage_settings_on_enter_handlers[])(void*) = {
#include "storage_settings_scene_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_event,
bool (*const storage_settings_on_event_handlers[])(void* context, SceneManagerEvent event) = {
#include "storage_settings_scene_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_exit,
void (*const storage_settings_on_exit_handlers[])(void* context) = {
#include "storage_settings_scene_config.h"
};
#undef ADD_SCENE

const SceneManagerHandlers storage_settings_scene_handlers = {
    .on_enter_handlers = storage_settings_on_enter_handlers,
    .on_event_handlers = storage_settings_on_event_handlers,
    .on_exit_handlers = storage_settings_on_exit_handlers,
    .scene_num = StorageSettingsSceneNum,
};
