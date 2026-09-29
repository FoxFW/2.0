#pragma once

#include <gui/scene_manager.h>

#define ADD_SCENE(prefix, name, id) SubRattScene##id,
typedef enum {
#include "subratt_scene_config.h"
    SubRattSceneNum,
} SubRattScene;
#undef ADD_SCENE

extern const SceneManagerHandlers subratt_scene_handlers;

#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_enter(void*);
#include "subratt_scene_config.h"
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) \
    bool prefix##_scene_##name##_on_event(void* context, SceneManagerEvent event);
#include "subratt_scene_config.h"
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_exit(void* context);
#include "subratt_scene_config.h"
#undef ADD_SCENE
