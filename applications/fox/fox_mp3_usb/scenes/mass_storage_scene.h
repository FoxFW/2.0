#pragma once

#include <gui/scene_manager.h>

#define ADD_SCENE(prefix, name, id) MassStorageScene##id,
typedef enum {
#include "mass_storage_scene_config.h"
    MassStorageSceneNum,
} MassStorageScene;
#undef ADD_SCENE

extern const SceneManagerHandlers mass_storage_scene_handlers;

#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_enter(void*);
#include "mass_storage_scene_config.h"
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) \
    bool prefix##_scene_##name##_on_event(void* context, SceneManagerEvent event);
#include "mass_storage_scene_config.h"
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_exit(void* context);
#include "mass_storage_scene_config.h"
#undef ADD_SCENE
