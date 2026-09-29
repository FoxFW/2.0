#pragma once

#include <gui/scene_manager.h>

#define ADD_SCENE(prefix, name, id) iButtonScene##id,
typedef enum {
#include "ibutton_scene_config.h"
    iButtonSceneNum,
} iButtonScene;
#undef ADD_SCENE

extern const SceneManagerHandlers ibutton_scene_handlers;

#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_enter(void*);
#include "ibutton_scene_config.h"
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) \
    bool prefix##_scene_##name##_on_event(void* context, SceneManagerEvent event);
#include "ibutton_scene_config.h"
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_exit(void* context);
#include "ibutton_scene_config.h"
#undef ADD_SCENE
