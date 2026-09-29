#pragma once

#include <stm32wbxx_ll_lptim.h>
#include <stm32wbxx_ll_rcc.h>
#include <stm32wbxx_ll_bus.h>

#include <furi_hal_bus.h>

#define FURI_HAL_IDLE_TIMER_MAX 0xFFFF
#define FURI_HAL_IDLE_TIMER     LPTIM1
#define FURI_HAL_IDLE_TIMER_IRQ LPTIM1_IRQn

static inline void furi_hal_idle_timer_init() {
    furi_hal_bus_enable(FuriHalBusLPTIM1);

    LL_RCC_SetLPTIMClockSource(LL_RCC_LPTIM1_CLKSOURCE_LSE);

    LL_APB1_GRP1_EnableClockSleep(LL_APB1_GRP1_PERIPH_LPTIM1);

    NVIC_SetPriority(
        FURI_HAL_IDLE_TIMER_IRQ, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 15, 0));
    NVIC_EnableIRQ(FURI_HAL_IDLE_TIMER_IRQ);
}

static inline void furi_hal_idle_timer_start(uint32_t count) {
    count--;

    LL_LPTIM_Enable(FURI_HAL_IDLE_TIMER);
    while(!LL_LPTIM_IsEnabled(FURI_HAL_IDLE_TIMER))
        ;

    LL_LPTIM_EnableIT_CMPM(FURI_HAL_IDLE_TIMER);

    LL_LPTIM_SetCompare(FURI_HAL_IDLE_TIMER, count - 3);
    LL_LPTIM_SetAutoReload(FURI_HAL_IDLE_TIMER, count);
    LL_LPTIM_StartCounter(FURI_HAL_IDLE_TIMER, LL_LPTIM_OPERATING_MODE_ONESHOT);
}

static inline void furi_hal_idle_timer_reset() {

    furi_hal_bus_reset(FuriHalBusLPTIM1);

    NVIC_ClearPendingIRQ(FURI_HAL_IDLE_TIMER_IRQ);
}

static inline uint32_t furi_hal_idle_timer_get_cnt() {
    uint32_t counter = LL_LPTIM_GetCounter(FURI_HAL_IDLE_TIMER);
    uint32_t counter_shadow = LL_LPTIM_GetCounter(FURI_HAL_IDLE_TIMER);
    while(counter != counter_shadow) {
        counter = counter_shadow;
        counter_shadow = LL_LPTIM_GetCounter(FURI_HAL_IDLE_TIMER);
    }
    return counter;
}
