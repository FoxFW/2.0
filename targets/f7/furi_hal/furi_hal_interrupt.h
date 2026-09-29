#pragma once

#include <stm32wbxx_ll_tim.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*FuriHalInterruptISR)(void* context);

typedef enum {

    FuriHalInterruptIdTim1TrgComTim17,
    FuriHalInterruptIdTim1Cc,
    FuriHalInterruptIdTim1UpTim16,

    FuriHalInterruptIdTIM2,

    FuriHalInterruptIdDma1Ch1,
    FuriHalInterruptIdDma1Ch2,
    FuriHalInterruptIdDma1Ch3,
    FuriHalInterruptIdDma1Ch4,
    FuriHalInterruptIdDma1Ch5,
    FuriHalInterruptIdDma1Ch6,
    FuriHalInterruptIdDma1Ch7,

    FuriHalInterruptIdDma2Ch1,
    FuriHalInterruptIdDma2Ch2,
    FuriHalInterruptIdDma2Ch3,
    FuriHalInterruptIdDma2Ch4,
    FuriHalInterruptIdDma2Ch5,
    FuriHalInterruptIdDma2Ch6,
    FuriHalInterruptIdDma2Ch7,

    FuriHalInterruptIdRcc,

    FuriHalInterruptIdCOMP,

    FuriHalInterruptIdRtcAlarm,

    FuriHalInterruptIdHsem,

    FuriHalInterruptIdLpTim1,
    FuriHalInterruptIdLpTim2,

    FuriHalInterruptIdUart1,

    FuriHalInterruptIdLpUart1,

    FuriHalInterruptIdMax,
} FuriHalInterruptId;

typedef enum {
    FuriHalInterruptPriorityLowest =
        -3,
    FuriHalInterruptPriorityLower =
        -2,
    FuriHalInterruptPriorityLow =
        -1,
    FuriHalInterruptPriorityNormal =
        0,
    FuriHalInterruptPriorityHigh =
        1,
    FuriHalInterruptPriorityHigher =
        2,
    FuriHalInterruptPriorityHighest =
        3,

    FuriHalInterruptPriorityKamiSama =
        6,
} FuriHalInterruptPriority;

void furi_hal_interrupt_init(void);

void furi_hal_interrupt_set_isr(FuriHalInterruptId index, FuriHalInterruptISR isr, void* context);

void furi_hal_interrupt_set_isr_ex(
    FuriHalInterruptId index,
    FuriHalInterruptPriority priority,
    FuriHalInterruptISR isr,
    void* context);

const char* furi_hal_interrupt_get_name(uint8_t exception_number);

uint32_t furi_hal_interrupt_get_time_in_isr_total(void);

#ifdef __cplusplus
}
#endif
