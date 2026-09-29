#pragma once

#include <stdint.h>
#include <stddef.h>

#include <furi_hal_gpio.h>

#include <stm32wbxx_ll_spi.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FuriHalSpiBus FuriHalSpiBus;
typedef struct FuriHalSpiBusHandle FuriHalSpiBusHandle;

typedef enum {
    FuriHalSpiBusEventInit,
    FuriHalSpiBusEventDeinit,
    FuriHalSpiBusEventLock,
    FuriHalSpiBusEventUnlock,
    FuriHalSpiBusEventActivate,
    FuriHalSpiBusEventDeactivate,
} FuriHalSpiBusEvent;

typedef void (*FuriHalSpiBusEventCallback)(FuriHalSpiBus* bus, FuriHalSpiBusEvent event);

struct FuriHalSpiBus {
    SPI_TypeDef* spi;
    FuriHalSpiBusEventCallback callback;
    const FuriHalSpiBusHandle* current_handle;
};

typedef enum {
    FuriHalSpiBusHandleEventInit,
    FuriHalSpiBusHandleEventDeinit,
    FuriHalSpiBusHandleEventActivate,
    FuriHalSpiBusHandleEventDeactivate,
} FuriHalSpiBusHandleEvent;

typedef void (*FuriHalSpiBusHandleEventCallback)(
    const FuriHalSpiBusHandle* handle,
    FuriHalSpiBusHandleEvent event);

struct FuriHalSpiBusHandle {
    FuriHalSpiBus* bus;
    FuriHalSpiBusHandleEventCallback callback;
    const GpioPin* miso;
    const GpioPin* mosi;
    const GpioPin* sck;
    const GpioPin* cs;
};

#ifdef __cplusplus
}
#endif
