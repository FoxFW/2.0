#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <gap.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FuriHalBleProfileTemplate FuriHalBleProfileTemplate;

typedef struct {

    const FuriHalBleProfileTemplate* config;
} FuriHalBleProfileBase;

typedef void* FuriHalBleProfileParams;

typedef FuriHalBleProfileBase* (*FuriHalBleProfileStart)(FuriHalBleProfileParams profile_params);
typedef void (*FuriHalBleProfileStop)(FuriHalBleProfileBase* profile);
typedef void (*FuriHalBleProfileGetGapConfig)(
    GapConfig* target_config,
    FuriHalBleProfileParams profile_params);

struct FuriHalBleProfileTemplate {

    FuriHalBleProfileStart start;

    FuriHalBleProfileStop stop;

    FuriHalBleProfileGetGapConfig get_gap_config;
};

#ifdef __cplusplus
}
#endif
