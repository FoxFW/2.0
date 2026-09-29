#pragma once

#include <core/string.h>

#include "../../nfc_app.h"
#include "../../nfc_app_i.h"

#include <lib/flipper_application/flipper_application.h>

typedef void (*NfcProtocolSupportOnEnter)(NfcApp* instance);

typedef bool (*NfcProtocolSupportOnEvent)(NfcApp* instance, SceneManagerEvent event);

typedef struct {
    NfcProtocolSupportOnEnter on_enter;
    NfcProtocolSupportOnEvent on_event;
} NfcProtocolSupportSceneBase;

typedef void (*NfcProtocolSupportOnExit)(NfcApp* instance);

typedef struct {
    NfcProtocolSupportOnEnter on_enter;
    NfcProtocolSupportOnEvent on_event;
    NfcProtocolSupportOnExit on_exit;
} NfcProtocolSupportExtraScene;

typedef struct {
    const uint32_t features;

    uint32_t (*get_features)(NfcApp* instance);

    NfcProtocolSupportSceneBase scene_info;

    NfcProtocolSupportSceneBase scene_more_info;

    NfcProtocolSupportSceneBase scene_read;

    NfcProtocolSupportSceneBase scene_read_menu;

    NfcProtocolSupportSceneBase scene_read_success;

    NfcProtocolSupportSceneBase scene_saved_menu;

    NfcProtocolSupportSceneBase scene_save_name;

    NfcProtocolSupportSceneBase scene_emulate;

    NfcProtocolSupportSceneBase scene_write;

    const NfcProtocolSupportExtraScene* extra_scenes;
    size_t extra_scenes_count;
} NfcProtocolSupportBase;

#define NFC_PROTOCOL_SUPPORT_PLUGIN_APP_ID "NfcProtocolSupportPlugin"

#define NFC_PROTOCOL_SUPPORT_PLUGIN_API_VERSION 2

typedef struct {
    NfcProtocol protocol;
    const NfcProtocolSupportBase* base;
} NfcProtocolSupportPlugin;

#define NFC_PROTOCOL_SUPPORT_PLUGIN(name, protocol)                              \
    static const NfcProtocolSupportPlugin nfc_protocol_support_##name##_desc = { \
        protocol,                                                                \
        &nfc_protocol_support_##name,                                            \
    };                                                                           \
                                                                                 \
    static const FlipperAppPluginDescriptor plugin_descriptor_##name = {         \
        .appid = NFC_PROTOCOL_SUPPORT_PLUGIN_APP_ID,                             \
        .ep_api_version = NFC_PROTOCOL_SUPPORT_PLUGIN_API_VERSION,               \
        .entry_point = &nfc_protocol_support_##name##_desc,                      \
    };                                                                           \
                                                                                 \
    const FlipperAppPluginDescriptor* nfc_##name##_ep(void) {                    \
        return &plugin_descriptor_##name;                                        \
    }
