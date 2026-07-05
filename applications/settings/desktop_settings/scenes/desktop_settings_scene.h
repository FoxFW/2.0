#pragma once

#include <gui/scene_manager.h>

// Generate scene IDs dynamically from configuration header - Single Source of Truth
#define ADD_SCENE(prefix, name, id) DesktopSettingsAppScene##id,
typedef enum {
#include "desktop_settings_scene_config.h"
    DesktopSettingsAppSceneNum,
} DesktopSettingsAppScene;
#undef ADD_SCENE

// Explicitly define every distinct view ID referenced across the UI routing dispatcher layers
typedef enum {
    DesktopSettingsAppViewMenu,
    DesktopSettingsAppViewVarItemList,
    DesktopSettingsAppViewIdPopup,
    DesktopSettingsAppViewIdPinInput,
    DesktopSettingsAppViewIdPinSetupHowto,
    DesktopSettingsAppViewIdPinSetupHowto2,
    DesktopSettingsAppViewIdPinError,
    DesktopSettingsAppViewDialogEx,
    DesktopSettingsAppViewTextInput,
} DesktopSettingsAppView;

extern const SceneManagerHandlers desktop_settings_scene_handlers;

// Generate scene on_enter handlers declaration
#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_enter(void*);
#include "desktop_settings_scene_config.h"
#undef ADD_SCENE

// Generate scene on_event handlers declaration
#define ADD_SCENE(prefix, name, id) \
    bool prefix##_scene_##name##_on_event(void* context, SceneManagerEvent event);
#include "desktop_settings_scene_config.h"
#undef ADD_SCENE

// Generate scene on_exit handlers declaration
#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_exit(void* context);
#include "desktop_settings_scene_config.h"
#undef ADD_SCENE