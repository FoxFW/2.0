#pragma once

#include "gps.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    GpsRpcCommandStreamStart,
    GpsRpcCommandStreamStop,
    GpsRpcCommandLocation,
    GpsRpcCommandSendLocation,
} GpsRpcCommand;

typedef void (*GpsRpcSend)(
    GpsRpcCommand command,
    uint8_t frequency,
    const GpsLocation* location,
    void* context);

void gps_set_rpc_bridge(Gps* gps, GpsRpcSend send, void* context);

void gps_on_location(Gps* gps, GpsStatus status, const GpsLocation* location);

#ifdef __cplusplus
}
#endif
