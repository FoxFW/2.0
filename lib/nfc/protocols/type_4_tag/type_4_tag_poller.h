#pragma once

#include "type_4_tag.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Type4TagPoller Type4TagPoller;

typedef enum {
    Type4TagPollerEventTypeRequestMode,
    Type4TagPollerEventTypeReadSuccess,
    Type4TagPollerEventTypeReadFailed,
    Type4TagPollerEventTypeWriteSuccess,
    Type4TagPollerEventTypeWriteFailed,
} Type4TagPollerEventType;

typedef enum {
    Type4TagPollerModeRead,
    Type4TagPollerModeWrite,
} Type4TagPollerMode;

typedef struct {
    Type4TagPollerMode mode;
    const Type4TagData* data;
} Type4TagPollerEventDataRequestMode;

typedef union {
    Type4TagError error;
    Type4TagPollerEventDataRequestMode poller_mode;
} Type4TagPollerEventData;

typedef struct {
    Type4TagPollerEventType type;
    Type4TagPollerEventData* data;
} Type4TagPollerEvent;

#ifdef __cplusplus
}
#endif
