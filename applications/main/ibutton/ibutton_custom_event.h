#pragma once

typedef enum {

    iButtonCustomEventReserved = 100,

    iButtonCustomEventBack,
    iButtonCustomEventTextEditResult,
    iButtonCustomEventByteEditChanged,
    iButtonCustomEventByteEditResult,
    iButtonCustomEventWorkerEmulated,
    iButtonCustomEventWorkerRead,
    iButtonCustomEventWorkerWriteOK,
    iButtonCustomEventWorkerWriteSameKey,
    iButtonCustomEventWorkerWriteNoDetect,
    iButtonCustomEventWorkerWriteCannotWrite,

    iButtonCustomEventRpcLoadFile,
    iButtonCustomEventRpcExit,
    iButtonCustomEventRpcSessionClose,

    iButtonCustomEventFuzzerExit,
} iButtonCustomEvent;
