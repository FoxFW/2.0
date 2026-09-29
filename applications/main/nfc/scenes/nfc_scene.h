#pragma once

#include <gui/scene_manager.h>

#define ADD_SCENE(prefix, name, id) NfcScene##id,
typedef enum {
#include "nfc_scene_config.h"
    NfcSceneNum,
} NfcScene;
#undef ADD_SCENE

extern const SceneManagerHandlers nfc_scene_handlers;

#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_enter(void*);
#include "nfc_scene_config.h"
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) \
    bool prefix##_scene_##name##_on_event(void* context, SceneManagerEvent event);
#include "nfc_scene_config.h"
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_exit(void* context);
#include "nfc_scene_config.h"
#undef ADD_SCENE
