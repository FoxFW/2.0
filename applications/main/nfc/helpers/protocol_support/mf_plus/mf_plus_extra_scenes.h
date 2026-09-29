#pragma once

#include "../nfc_protocol_support_base.h"

typedef enum {
    MfPlusExtraSceneDictAttack,
    MfPlusExtraSceneShowKeys,
    MfPlusExtraSceneMoreInfo,
    MfPlusExtraSceneIso4Info,
    MfPlusExtraSceneVersion,
    MfPlusExtraSceneUpdateInitial,

    MfPlusExtraSceneNum,
} MfPlusExtraScene;

extern const NfcProtocolSupportExtraScene mf_plus_extra_scenes[MfPlusExtraSceneNum];
