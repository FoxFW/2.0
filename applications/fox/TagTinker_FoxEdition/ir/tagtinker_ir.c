#include "tagtinker_ir.h"

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_bus.h>
#include <furi_hal_resources.h>
#include <furi_hal_gpio.h>
#include <furi_hal_cortex.h>

#include <stm32wbxx_ll_tim.h>

#define CARRIER_TIM       TIM1
#define CARRIER_ARR       (51 - 1)
#define CARRIER_CCR       25

#define PP4_BURST_CYCLES  2581
static const uint32_t pp4_gap_cycles[4] = {
    3871,
    15483,
    7741,
    11612,
};

static bool ir_initialized = false;
static volatile bool ir_stop_requested = false;

static inline void carrier_on(void) {
    uint32_t ccmr2 = CARRIER_TIM->CCMR2;
    ccmr2 &= ~(TIM_CCMR2_OC3M);
    ccmr2 |= (TIM_CCMR2_OC3M_2 | TIM_CCMR2_OC3M_1 | TIM_CCMR2_OC3M_0);
    CARRIER_TIM->CCMR2 = ccmr2;
}

static inline void carrier_off(void) {
    uint32_t ccmr2 = CARRIER_TIM->CCMR2;
    ccmr2 &= ~(TIM_CCMR2_OC3M);
    ccmr2 |= TIM_CCMR2_OC3M_2;
    CARRIER_TIM->CCMR2 = ccmr2;
}

static inline void delay_cycles(uint32_t cycles) {
    uint32_t start = DWT->CYCCNT;
    while((DWT->CYCCNT - start) < cycles) {
    }
}

static void send_frame_pp4(const uint8_t* data, size_t len) {
    for(size_t byte_idx = 0; byte_idx < len; byte_idx++) {
        uint8_t current_byte = data[byte_idx];
        for(int sym = 0; sym < 4; sym++) {
            uint8_t symbol = current_byte & 0x03;
            current_byte >>= 2;

            carrier_on();
            delay_cycles(PP4_BURST_CYCLES);

            carrier_off();
            delay_cycles(pp4_gap_cycles[symbol]);
        }
    }

    carrier_on();
    delay_cycles(PP4_BURST_CYCLES);
    carrier_off();
}

void tagtinker_ir_init(void) {
    if(ir_initialized) return;

    if(furi_hal_bus_is_enabled(FuriHalBusTIM1)) {
        furi_hal_bus_disable(FuriHalBusTIM1);
    }
    furi_hal_bus_enable(FuriHalBusTIM1);

    furi_hal_gpio_init_ex(
        &gpio_infrared_tx,
        GpioModeAltFunctionPushPull,
        GpioPullNo,
        GpioSpeedVeryHigh,
        GpioAltFn1TIM1);

    LL_TIM_SetPrescaler(CARRIER_TIM, 0);
    LL_TIM_SetAutoReload(CARRIER_TIM, CARRIER_ARR);
    LL_TIM_SetCounter(CARRIER_TIM, 0);

    LL_TIM_OC_SetMode(CARRIER_TIM, LL_TIM_CHANNEL_CH3, LL_TIM_OCMODE_PWM2);
    LL_TIM_OC_SetCompareCH3(CARRIER_TIM, CARRIER_CCR);
    LL_TIM_OC_EnablePreload(CARRIER_TIM, LL_TIM_CHANNEL_CH3);

    LL_TIM_CC_EnableChannel(CARRIER_TIM, LL_TIM_CHANNEL_CH3N);
    LL_TIM_EnableAllOutputs(CARRIER_TIM);

    carrier_off();
    LL_TIM_EnableCounter(CARRIER_TIM);
    LL_TIM_GenerateEvent_UPDATE(CARRIER_TIM);

    ir_stop_requested = false;
    ir_initialized = true;
}

void tagtinker_ir_deinit(void) {
    if(!ir_initialized) return;

    tagtinker_ir_stop();
    carrier_off();

    LL_TIM_DisableAllOutputs(CARRIER_TIM);
    LL_TIM_CC_DisableChannel(CARRIER_TIM, LL_TIM_CHANNEL_CH3N);
    LL_TIM_DisableCounter(CARRIER_TIM);

    if(furi_hal_bus_is_enabled(FuriHalBusTIM1)) {
        furi_hal_bus_disable(FuriHalBusTIM1);
    }

    furi_hal_gpio_init(&gpio_infrared_tx, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    ir_initialized = false;
}

bool tagtinker_ir_transmit(const uint8_t* data, size_t len, uint16_t repeats_raw, uint8_t delay) {
    if(!ir_initialized) return false;
    if(!data || len == 0 || len > 255) return false;

    ir_stop_requested = false;
    uint32_t repeats = repeats_raw & 0x7FFF;

    for(uint32_t rep = 0; rep <= repeats; rep++) {
        if(ir_stop_requested) {
            carrier_off();
            return false;
        }

        FURI_CRITICAL_ENTER();
        send_frame_pp4(data, len);
        FURI_CRITICAL_EXIT();

        if(rep < repeats) {

            if(delay > 0) {
                uint32_t gap_cycles = (uint32_t)delay * 32000U;
                delay_cycles(gap_cycles);
            }

            if((rep % 5U) == 4U) furi_delay_ms(1);
        }
    }

    return true;
}

void tagtinker_ir_stop(void) {
    ir_stop_requested = true;
    carrier_off();
}
