#pragma once

#include <gui/scene_manager.h>

// Use legacy config list which matches function names (many files end with _new)
extern const SceneManagerHandlers predator_scene_handlers;

// Generate scene on_enter handlers declaration
#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_enter(void*);
#include "predator_scene_config.h"
#undef ADD_SCENE

// Generate scene on_event handlers declaration
#define ADD_SCENE(prefix, name, id) \
    bool prefix##_scene_##name##_on_event(void* context, SceneManagerEvent event);
#include "predator_scene_config.h"
#undef ADD_SCENE

// Generate scene on_exit handlers declaration
#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_exit(void* context);
#include "predator_scene_config.h"
#undef ADD_SCENE

// Generate a scene ID enum that matches the order in predator_scene_config.h
// This ensures scene IDs always align with the handlers arrays
#define ADD_SCENE(prefix, name, id) PredatorScene##id,
typedef enum {
#include "predator_scene_config.h"
    PredatorSceneCountEnumGuard
} PredatorScene;
#undef ADD_SCENE
