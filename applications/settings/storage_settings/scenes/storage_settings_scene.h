#pragma once

#include <gui/scene_manager.h>

#define ADD_SCENE(prefix, name, id) StorageSettings##id,
typedef enum {
#include "storage_settings_scene_config.h"
    StorageSettingsSceneNum,
} StorageSettingsScene;
#undef ADD_SCENE

extern const SceneManagerHandlers storage_settings_scene_handlers;

#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_enter(void*);
#include "storage_settings_scene_config.h"
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) \
    bool prefix##_scene_##name##_on_event(void* context, SceneManagerEvent event);
#include "storage_settings_scene_config.h"
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_exit(void* context);
#include "storage_settings_scene_config.h"
#undef ADD_SCENE
