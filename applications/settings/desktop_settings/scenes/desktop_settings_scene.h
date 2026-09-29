#pragma once

#include <gui/scene_manager.h>

#define ADD_SCENE(prefix, name, id) DesktopSettingsAppScene##id,
typedef enum {
#include "desktop_settings_scene_config.h"
    DesktopSettingsAppSceneNum,
} DesktopSettingsAppScene;
#undef ADD_SCENE

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
    DesktopSettingsAppViewWallpaper,
    DesktopSettingsAppViewAlarmEdit,
    DesktopSettingsAppViewMenuStyle,
    DesktopSettingsAppViewUsbMode,
} DesktopSettingsAppView;

extern const SceneManagerHandlers desktop_settings_scene_handlers;

#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_enter(void*);
#include "desktop_settings_scene_config.h"
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) \
    bool prefix##_scene_##name##_on_event(void* context, SceneManagerEvent event);
#include "desktop_settings_scene_config.h"
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_exit(void* context);
#include "desktop_settings_scene_config.h"
#undef ADD_SCENE
