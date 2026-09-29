#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <furi_config.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    FuriWaitForever = 0xFFFFFFFFU,
} FuriWait;

typedef enum {
    FuriFlagWaitAny = 0x00000000U,
    FuriFlagWaitAll = 0x00000001U,
    FuriFlagNoClear = 0x00000002U,

    FuriFlagError = 0x80000000U,
    FuriFlagErrorUnknown = 0xFFFFFFFFU,
    FuriFlagErrorTimeout = 0xFFFFFFFEU,
    FuriFlagErrorResource = 0xFFFFFFFDU,
    FuriFlagErrorParameter = 0xFFFFFFFCU,
    FuriFlagErrorISR = 0xFFFFFFFAU,
} FuriFlag;

typedef enum {
    FuriStatusOk = 0,
    FuriStatusError =
        -1,
    FuriStatusErrorTimeout = -2,
    FuriStatusErrorResource = -3,
    FuriStatusErrorParameter = -4,
    FuriStatusErrorNoMemory =
        -5,
    FuriStatusErrorISR =
        -6,
    FuriStatusReserved = 0x7FFFFFFF
} FuriStatus;

typedef enum {
    FuriSignalExit,

    FuriSignalCustom = 100,
} FuriSignal;

#ifdef __cplusplus
}
#endif
