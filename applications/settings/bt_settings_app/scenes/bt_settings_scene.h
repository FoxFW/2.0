#pragma once

#include <gui/scene_manager.h>

#define ADD_SCENE(prefix, name, id) BtSettingsAppScene##id,
typedef enum {
#include "bt_settings_scene_config.h"
    BtSettingsAppSceneNum,
} BtSettingsAppScene;
#undef ADD_SCENE

extern const SceneManagerHandlers bt_settings_scene_handlers;

#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_enter(void*);
#include "bt_settings_scene_config.h"
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) \
    bool prefix##_scene_##name##_on_event(void* context, SceneManagerEvent event);
#include "bt_settings_scene_config.h"
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_exit(void* context);
#include "bt_settings_scene_config.h"
#undef ADD_SCENE
