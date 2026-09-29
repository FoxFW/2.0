#pragma once

#include <gui/scene_manager.h>

#define ADD_SCENE(prefix, name, id) ClockSettingsScene##id,
typedef enum {
#include "clock_settings_scene_config.h"
    ClockSettingsSceneNum,
} ClockSettingsScene;
#undef ADD_SCENE

extern const SceneManagerHandlers clock_settings_scene_handlers;

#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_enter(void*);
#include "clock_settings_scene_config.h"
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) \
    bool prefix##_scene_##name##_on_event(void* context, SceneManagerEvent event);
#include "clock_settings_scene_config.h"
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_exit(void* context);
#include "clock_settings_scene_config.h"
#undef ADD_SCENE
