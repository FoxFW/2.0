#include "desktop_settings_scene.h"

// Generate enter handlers array
#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_enter,
void (*const desktop_settings_scene_on_enter_handlers[])(void*) = {
#include "desktop_settings_scene_config.h"
};
#undef ADD_SCENE

// Generate event handlers array
#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_event,
bool (*const desktop_settings_scene_on_event_handlers[])(void*, SceneManagerEvent) = {
#include "desktop_settings_scene_config.h"
};
#undef ADD_SCENE

// Generate exit handlers array
#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_exit,
void (*const desktop_settings_scene_on_exit_handlers[])(void*) = {
#include "desktop_settings_scene_config.h"
};
#undef ADD_SCENE

// Bind arrays into the master configuration layout
const SceneManagerHandlers desktop_settings_scene_handlers = {
    .on_enter_handlers = desktop_settings_scene_on_enter_handlers,
    .on_event_handlers = desktop_settings_scene_on_event_handlers,
    .on_exit_handlers = desktop_settings_scene_on_exit_handlers,
    .scene_num = DesktopSettingsAppSceneNum,
};