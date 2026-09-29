#pragma once

typedef enum {
    NfcProtocolFeatureNone = 0,
    NfcProtocolFeatureEmulateUid = 1UL << 0,
    NfcProtocolFeatureEmulateFull = 1UL << 1,
    NfcProtocolFeatureEditUid = 1UL << 2,
    NfcProtocolFeatureMoreInfo = 1UL << 3,
    NfcProtocolFeatureWrite = 1UL << 4,
} NfcProtocolFeature;

typedef enum {
    NfcProtocolSupportSceneInfo,
    NfcProtocolSupportSceneMoreInfo,
    NfcProtocolSupportSceneRead,
    NfcProtocolSupportSceneReadMenu,
    NfcProtocolSupportSceneReadSuccess,
    NfcProtocolSupportSceneSavedMenu,
    NfcProtocolSupportSceneSaveName,
    NfcProtocolSupportSceneEmulate,
    NfcProtocolSupportSceneWrite,
    NfcProtocolSupportSceneRpc,

    NfcProtocolSupportSceneCount,
} NfcProtocolSupportScene;
