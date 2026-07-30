#pragma once

#include <furi.h>
#include <furi_hal.h>
#include "../defines.h"

typedef enum {
    GDRViewVariableItemList,
    GDRViewSubmenu,
    GDRViewWidget,
    GDRViewReceiver,
    GDRViewAbout,
    GDRViewFileBrowser,
    GDRViewTextInput,
#ifdef ENABLE_DUAL_RX_SCENE
    GDRViewDualReceiver,
#endif
} GDRView;

typedef enum {
    GDRCustomEventViewReceiverOK,
    GDRCustomEventViewReceiverConfig,
    GDRCustomEventViewReceiverBack,
    GDRCustomEventViewReceiverDeleteItem,
    GDRCustomEventViewReceiverUnlock,

    GDRCustomEventSceneReceiverUpdate,
    GDRCustomEventReceiverDeferredRxStart,
    GDRCustomEventSceneSettingLock,

    GDRCustomEventReceiverInfoSave,
    GDRCustomEventReceiverInfoSaveConfirm,
    GDRCustomEventReceiverInfoEmulate,
    GDRCustomEventReceiverInfoBruteforceStart,
    GDRCustomEventReceiverInfoBruteforceCancel,
    GDRCustomEventSavedInfoDelete,

    GDRCustomEventSavedInfoEmulate,
    GDRCustomEventEmulateTransmit,
    GDRCustomEventEmulateStop,
    GDRCustomEventEmulateExit,

    GDRCustomEventSubDecodeUpdate,
    GDRCustomEventSubDecodeSave,
    GDRCustomEventSubDecodeBruteforceStart,
    GDRCustomEventPsaBruteforceComplete,

    GDRCustomEventSavedFileSelected,

    GDRCustomEventSceneStay,
    GDRCustomEventSceneExit,

    GDRCustomEventAboutToggleEmulate,
#ifdef ENABLE_DUAL_RX_SCENE

    GDRCustomEventDualReceiverDeferredRxStart,
    GDRCustomEventDualReceiverUpdate,
    GDRCustomEventViewDualReceiverOK,
    GDRCustomEventViewDualReceiverBack,
    GDRCustomEventViewDualReceiverDeleteItem,
    GDRCustomEventViewDualReceiverConfig,
#endif
#ifdef ENABLE_SHIELD_RX_SCENE
    GDRCustomEventShieldReceiverDeferredStart,
    GDRCustomEventShieldReceiverUpdate,
#endif
} GDRCustomEvent;

typedef enum {
    GDRLockOff,
    GDRLockOn,
} GDRLock;

typedef enum {
    GDRTxRxStateIDLE,
    GDRTxRxStateRx,
    GDRTxRxStateTx,
    GDRTxRxStateSleep,
} GDRTxRxState;

typedef enum {
    GDRHopperStateOFF,
    GDRHopperStateRunning,
    GDRHopperStatePause,
    GDRHopperStateRSSITimeOut,
} GDRHopperState;

typedef enum {
    GDRRxKeyStateIDLE,
    GDRRxKeyStateBack,
    GDRRxKeyStateStart,
    GDRRxKeyStateAddKey,
} GDRRxKeyState;
