#pragma once

#include <gui/scene_manager.h>

#define ADD_SCENE(prefix, name, id) GpioScene##id,
typedef enum {
#include "gpio_scene_config.h"
    GpioSceneNum,
} GpioScene;
#undef ADD_SCENE

extern const SceneManagerHandlers gpio_scene_handlers;

#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_enter(void*);
#include "gpio_scene_config.h"
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) \
    bool prefix##_scene_##name##_on_event(void* context, SceneManagerEvent event);
#include "gpio_scene_config.h"
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_exit(void* context);
#include "gpio_scene_config.h"
#undef ADD_SCENE
