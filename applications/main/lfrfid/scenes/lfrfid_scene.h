#pragma once

#include <gui/scene_manager.h>

#define ADD_SCENE(prefix, name, id) LfRfidScene##id,
typedef enum {
#include "lfrfid_scene_config.h"
    LfRfidSceneNum,
} LfRfidScene;
#undef ADD_SCENE

extern const SceneManagerHandlers lfrfid_scene_handlers;

#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_enter(void*);
#include "lfrfid_scene_config.h"
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) \
    bool prefix##_scene_##name##_on_event(void* context, SceneManagerEvent event);
#include "lfrfid_scene_config.h"
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_exit(void* context);
#include "lfrfid_scene_config.h"
#undef ADD_SCENE
