#pragma once

#include <gui/scene_manager.h>

/* Scene ids. Order must match the handler arrays in mfkey_collector_scene.c.
 * Enum constant names are kept identical to the NFC app's so the copied scene
 * bodies (which navigate with scene_manager_next_scene(..., NfcSceneMfClassic...))
 * compile and behave unchanged. */
typedef enum {
    NfcSceneMfClassicDetectReader,
    NfcSceneMfClassicMfkeyNoncesInfo,
    NfcSceneMfClassicMfkeyComplete,
    NfcSceneNum,
} NfcScene;

extern const SceneManagerHandlers mfkey_collector_scene_handlers;
