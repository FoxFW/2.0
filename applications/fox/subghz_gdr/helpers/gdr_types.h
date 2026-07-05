// helpers/gdr_types.h
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
    // Custom events for views
    GDRCustomEventViewReceiverOK,
    GDRCustomEventViewReceiverConfig,
    GDRCustomEventViewReceiverBack,
    GDRCustomEventViewReceiverDeleteItem,
    GDRCustomEventViewReceiverUnlock,
    // Custom events for scenes
    GDRCustomEventSceneReceiverUpdate,
    GDRCustomEventReceiverDeferredRxStart,
    GDRCustomEventSceneSettingLock,
    // File management
    GDRCustomEventReceiverInfoSave,
    GDRCustomEventReceiverInfoSaveConfirm,
    GDRCustomEventReceiverInfoEmulate,
    GDRCustomEventReceiverInfoBruteforceStart,
    GDRCustomEventReceiverInfoBruteforceCancel,
    GDRCustomEventSavedInfoDelete,
    // Emulator
    GDRCustomEventSavedInfoEmulate,
    GDRCustomEventEmulateTransmit,
    GDRCustomEventEmulateStop,
    GDRCustomEventEmulateExit,
    // Sub decode
    GDRCustomEventSubDecodeUpdate,
    GDRCustomEventSubDecodeSave,
    GDRCustomEventSubDecodeBruteforceStart,
    GDRCustomEventPsaBruteforceComplete,
    // File Browser
    GDRCustomEventSavedFileSelected,
    // Need saving confirmation
    GDRCustomEventSceneStay,
    GDRCustomEventSceneExit,
    // About scene
    GDRCustomEventAboutToggleEmulate,
#ifdef ENABLE_DUAL_RX_SCENE
    // Dual RX scene
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
