#pragma once

#include <stdint.h>
#include <core/common_defines.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BleEventNotAck,
    BleEventAckFlowEnable,
    BleEventAckFlowDisable,
} BleEventAckStatus;

typedef enum {
    BleEventFlowDisable,
    BleEventFlowEnable,
} BleEventFlowStatus;

typedef BleEventAckStatus (*BleSvcEventHandlerCb)(void* event, void* context);

typedef struct GapEventHandler GapSvcEventHandler;

void ble_event_dispatcher_init(void);

void ble_event_dispatcher_reset(void);

BleEventFlowStatus ble_event_dispatcher_process_event(void* payload);

BleEventFlowStatus ble_event_app_notification(void* pckt);

FURI_WARN_UNUSED GapSvcEventHandler*
    ble_event_dispatcher_register_svc_handler(BleSvcEventHandlerCb handler, void* context);

void ble_event_dispatcher_unregister_svc_handler(GapSvcEventHandler* handler);

#ifdef __cplusplus
}
#endif
