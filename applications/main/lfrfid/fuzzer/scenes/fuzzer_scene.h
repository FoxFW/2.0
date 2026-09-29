#pragma once

#include <gui/scene_manager.h>

#define ADD_SCENE(prefix, name, id) FuzzerScene##id,
typedef enum {
#include "fuzzer_scene_config.h"
    FuzzerSceneNum,
} FuzzerScene;
#undef ADD_SCENE

extern const SceneManagerHandlers fuzzer_scene_handlers;

#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_enter(void*);
#include "fuzzer_scene_config.h"
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) \
    bool prefix##_scene_##name##_on_event(void* context, SceneManagerEvent event);
#include "fuzzer_scene_config.h"
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_exit(void* context);
#include "fuzzer_scene_config.h"
#undef ADD_SCENE
