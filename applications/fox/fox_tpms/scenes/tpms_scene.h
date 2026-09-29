#pragma once

#include <gui/scene_manager.h>

#define ADD_SCENE(prefix, name, id) TPMSScene##id,
typedef enum {
#include "tpms_scene_config.h"
    TPMSSceneNum,
} TPMSScene;
#undef ADD_SCENE

extern const SceneManagerHandlers tpms_scene_handlers;

#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_enter(void*);
#include "tpms_scene_config.h"
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) \
    bool prefix##_scene_##name##_on_event(void* context, SceneManagerEvent event);
#include "tpms_scene_config.h"
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_exit(void* context);
#include "tpms_scene_config.h"
#undef ADD_SCENE
