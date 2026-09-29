#pragma once

#include <furi.h>
#include <furi_hal.h>

#define TPMS_KEY_FILE_VERSION 1
#define TPMS_KEY_FILE_TYPE "Flipper Tire Pressure Monitoring System Key File"

typedef enum {
    TPMSRxKeyStateIDLE,
    TPMSRxKeyStateBack,
    TPMSRxKeyStateStart,
    TPMSRxKeyStateAddKey,
} TPMSRxKeyState;

typedef enum {
    TPMSHopperStateOFF,
    TPMSHopperStateRunnig,
    TPMSHopperStatePause,
    TPMSHopperStateRSSITimeOut,
} TPMSHopperState;

typedef enum {
    TPMSLockOff,
    TPMSLockOn,
} TPMSLock;

typedef enum {
    TPMSViewVariableItemList,
    TPMSViewSubmenu,
    TPMSViewReceiver,
    TPMSViewReceiverInfo,
    TPMSViewWidget,
    TPMSViewNumberInput,
    TPMSViewByteInput,
    TPMSViewBoxList,
} TPMSView;

typedef enum {
    TPMSFieldPressure = 0,
    TPMSFieldTemperature,
    TPMSFieldId,
    TPMSFieldBattery,
    TPMSFieldCount,
} TPMSField;

typedef enum {
    TPMSTxRxStateIDLE,
    TPMSTxRxStateRx,
    TPMSTxRxStateTx,
    TPMSTxRxStateSleep,
} TPMSTxRxState;

typedef enum {
    TPMSRelearnOff,
    TPMSRelearnOn,
} TPMSRelearn;

typedef enum {
    TPMSRelearnTypeCommon,

} TPMSRelearnType;
