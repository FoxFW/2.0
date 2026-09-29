#pragma once

typedef enum {

    TPMSCustomEventStartId = 100,

    TPMSCustomEventSceneSettingLock,

    TPMSCustomEventViewReceiverOK,
    TPMSCustomEventViewReceiverConfig,
    TPMSCustomEventViewReceiverBack,
    TPMSCustomEventViewReceiverOffDisplay,
    TPMSCustomEventViewReceiverUnlock,
    TPMSCustomEventViewReceiverRetrigger,

    TPMSCustomEventTpmsEditPressure,
    TPMSCustomEventTpmsEditTemperature,
    TPMSCustomEventTpmsEditId,
    TPMSCustomEventTpmsToggleBattery,
    TPMSCustomEventNumberInputDone,
    TPMSCustomEventByteInputDone,
} TPMSCustomEvent;
