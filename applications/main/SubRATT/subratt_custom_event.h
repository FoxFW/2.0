#pragma once

typedef enum {

    SubRattCustomEventTypeReserved = 100,

    SubRattCustomEventTypeBackPressed,
    SubRattCustomEventTypeIndexSelected,
    SubRattCustomEventTypeTransmitStarted,
    SubRattCustomEventTypeError,
    SubRattCustomEventTypeTransmitFinished,
    SubRattCustomEventTypeTransmitNotStarted,
    SubRattCustomEventTypeTransmitCustom,
    SubRattCustomEventTypeSaveFile,
    SubRattCustomEventTypeExtraSettings,
    SubRattCustomEventTypeUpdateView,
    SubRattCustomEventTypeChangeStepUp,
    SubRattCustomEventTypeChangeStepDown,
    SubRattCustomEventTypeChangeStepUpMore,
    SubRattCustomEventTypeChangeStepDownMore,

    SubRattCustomEventTypeMenuSelected,
    SubRattCustomEventTypeTextEditDone,
    SubRattCustomEventTypePopupClosed,

    SubRattCustomEventTypeLoadFile,

    SubRattCustomEventTypeLoadSavedKeys,

    SubRattCustomEventTypeKeylogSave,

    SubRattCustomEventTypeKeylogDiscard,
} SubRattCustomEvent;
