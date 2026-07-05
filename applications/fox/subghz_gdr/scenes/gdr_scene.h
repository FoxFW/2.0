// scenes/gdr_scene.h
#pragma once

#include <gui/scene_manager.h>
#include "../gdr_app_i.h"

// Generate scene id and total number
#define ADD_SCENE(prefix, name, id) GDRScene##id,
typedef enum {
#include "gdr_scene_config.h"
    GDRSceneNum,
} GDRScene;
#undef ADD_SCENE

extern const SceneManagerHandlers gdr_scene_handlers;

// Generate scene on_enter handlers declaration
#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_enter(void*);
#include "gdr_scene_config.h"
#undef ADD_SCENE

// Generate scene on_event handlers declaration
#define ADD_SCENE(prefix, name, id) \
    bool prefix##_scene_##name##_on_event(void* context, SceneManagerEvent event);
#include "gdr_scene_config.h"
#undef ADD_SCENE

// Generate scene on_exit handlers declaration
#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_exit(void* context);
#include "gdr_scene_config.h"
#undef ADD_SCENE
