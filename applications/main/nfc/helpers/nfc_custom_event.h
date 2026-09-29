#pragma once

typedef enum {

    NfcCustomEventReserved = 100,

    NfcCustomEventDictAttackComplete,
    NfcCustomEventDictAttackSkip,
    NfcCustomEventDictAttackDataUpdate,

    NfcCustomEventCardDetected,
    NfcCustomEventCardLost,

    NfcCustomEventViewExit,
    NfcCustomEventRetry,
    NfcCustomEventWorkerExit,
    NfcCustomEventWorkerUpdate,
    NfcCustomEventWrongCard,
    NfcCustomEventTimerExpired,
    NfcCustomEventByteInputDone,
    NfcCustomEventTextInputDone,
    NfcCustomEventDictAttackDone,

    NfcCustomEventRpcLoadFile,
    NfcCustomEventRpcExit,
    NfcCustomEventRpcSessionClose,

    NfcCustomEventPollerSuccess,
    NfcCustomEventPollerIncomplete,
    NfcCustomEventPollerFailure,

    NfcCustomEventListenerUpdate,

    NfcCustomEventEmulationTimeExpired,

    NfcCustomEventFuzzerExit,
} NfcCustomEvent;
