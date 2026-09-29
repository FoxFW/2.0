#pragma once

typedef enum {

    FuzzerCustomEventViewMainBack = 100,
    FuzzerCustomEventViewMainOk,
    FuzzerCustomEventViewMainPopupErr,

    FuzzerCustomEventViewAttackEnd,

    FuzzerCustomEventViewAttackExit,
    FuzzerCustomEventViewAttackRunAttack,
    FuzzerCustomEventViewAttackPause,
    FuzzerCustomEventViewAttackIdle,
    FuzzerCustomEventViewAttackEmulateCurrent,
    FuzzerCustomEventViewAttackSave,
    FuzzerCustomEventViewAttackNextUid,
    FuzzerCustomEventViewAttackPrevUid,

    FuzzerCustomEventViewFieldEditorBack,
    FuzzerCustomEventViewFieldEditorOk,

    FuzzerCustomEventTextEditResult,

    FuzzerCustomEventPopupClosed,
} FuzzerCustomEvent;
