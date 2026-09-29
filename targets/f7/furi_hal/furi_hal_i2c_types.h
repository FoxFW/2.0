#pragma once

#include <stm32wbxx_ll_i2c.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FuriHalI2cBus FuriHalI2cBus;
typedef struct FuriHalI2cBusHandle FuriHalI2cBusHandle;

typedef enum {
    FuriHalI2cBusEventInit,
    FuriHalI2cBusEventDeinit,
    FuriHalI2cBusEventLock,
    FuriHalI2cBusEventUnlock,
    FuriHalI2cBusEventActivate,
    FuriHalI2cBusEventDeactivate,
} FuriHalI2cBusEvent;

typedef void (*FuriHalI2cBusEventCallback)(FuriHalI2cBus* bus, FuriHalI2cBusEvent event);

struct FuriHalI2cBus {
    I2C_TypeDef* i2c;
    const FuriHalI2cBusHandle* current_handle;
    FuriHalI2cBusEventCallback callback;
};

typedef enum {
    FuriHalI2cBusHandleEventActivate,
    FuriHalI2cBusHandleEventDeactivate,
} FuriHalI2cBusHandleEvent;

typedef void (*FuriHalI2cBusHandleEventCallback)(
    const FuriHalI2cBusHandle* handle,
    FuriHalI2cBusHandleEvent event);

struct FuriHalI2cBusHandle {
    FuriHalI2cBus* bus;
    FuriHalI2cBusHandleEventCallback callback;
};

#ifdef __cplusplus
}
#endif
