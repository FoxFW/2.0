#pragma once

#include <gui/scene_manager.h>

#define ADD_SCENE(prefix, name, id) MifareFuzzerScene##id,
typedef enum {
#include "mifare_fuzzer_scene_config.h"
    MifareFuzzerSceneNum,
} MifareFuzzerScene;
#undef ADD_SCENE

extern const SceneManagerHandlers mifare_fuzzer_scene_handlers;

#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_enter(void*);
#include "mifare_fuzzer_scene_config.h"
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) \
    bool prefix##_scene_##name##_on_event(void* context, SceneManagerEvent event);
#include "mifare_fuzzer_scene_config.h"
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_exit(void* context);
#include "mifare_fuzzer_scene_config.h"
#undef ADD_SCENE
