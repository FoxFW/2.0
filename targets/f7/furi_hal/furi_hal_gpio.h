#pragma once
#include "stdbool.h"
#include <stm32wbxx_ll_gpio.h>
#include <stm32wbxx_ll_system.h>
#include <stm32wbxx_ll_exti.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GPIO_NUMBER (16U)

typedef void (*GpioExtiCallback)(void* ctx);

typedef struct {
    GpioExtiCallback callback;
    void* context;
} GpioInterrupt;

typedef enum {
    GpioModeInput,
    GpioModeOutputPushPull,
    GpioModeOutputOpenDrain,
    GpioModeAltFunctionPushPull,
    GpioModeAltFunctionOpenDrain,
    GpioModeAnalog,
    GpioModeInterruptRise,
    GpioModeInterruptFall,
    GpioModeInterruptRiseFall,
    GpioModeEventRise,
    GpioModeEventFall,
    GpioModeEventRiseFall,
} GpioMode;

typedef enum {
    GpioPullNo,
    GpioPullUp,
    GpioPullDown,
} GpioPull;

typedef enum {
    GpioSpeedLow,
    GpioSpeedMedium,
    GpioSpeedHigh,
    GpioSpeedVeryHigh,
} GpioSpeed;

typedef enum {
    GpioAltFn0MCO = 0,
    GpioAltFn0LSCO = 0,
    GpioAltFn0JTMS_SWDIO = 0,
    GpioAltFn0JTCK_SWCLK = 0,
    GpioAltFn0JTDI = 0,
    GpioAltFn0RTC_OUT = 0,
    GpioAltFn0JTD_TRACE = 0,
    GpioAltFn0NJTRST = 0,
    GpioAltFn0RTC_REFIN = 0,
    GpioAltFn0TRACED0 = 0,
    GpioAltFn0TRACED1 = 0,
    GpioAltFn0TRACED2 = 0,
    GpioAltFn0TRACED3 = 0,
    GpioAltFn0TRIG_INOUT = 0,
    GpioAltFn0TRACECK = 0,
    GpioAltFn0SYS = 0,

    GpioAltFn1TIM1 = 1,
    GpioAltFn1TIM2 = 1,
    GpioAltFn1LPTIM1 = 1,

    GpioAltFn2TIM2 = 2,
    GpioAltFn2TIM1 = 2,

    GpioAltFn3SAI1 = 3,
    GpioAltFn3SPI2 = 3,
    GpioAltFn3TIM1 = 3,

    GpioAltFn4I2C1 = 4,
    GpioAltFn4I2C3 = 4,

    GpioAltFn5SPI1 = 5,
    GpioAltFn5SPI2 = 5,

    GpioAltFn6MCO = 6,
    GpioAltFn6LSCO = 6,
    GpioAltFn6RF_DTB0 = 6,
    GpioAltFn6RF_DTB1 = 6,
    GpioAltFn6RF_DTB2 = 6,
    GpioAltFn6RF_DTB3 = 6,
    GpioAltFn6RF_DTB4 = 6,
    GpioAltFn6RF_DTB5 = 6,
    GpioAltFn6RF_DTB6 = 6,
    GpioAltFn6RF_DTB7 = 6,
    GpioAltFn6RF_DTB8 = 6,
    GpioAltFn6RF_DTB9 = 6,
    GpioAltFn6RF_DTB10 = 6,
    GpioAltFn6RF_DTB11 = 6,
    GpioAltFn6RF_DTB12 = 6,
    GpioAltFn6RF_DTB13 = 6,
    GpioAltFn6RF_DTB14 = 6,
    GpioAltFn6RF_DTB15 = 6,
    GpioAltFn6RF_DTB16 = 6,
    GpioAltFn6RF_DTB17 = 6,
    GpioAltFn6RF_DTB18 = 6,
    GpioAltFn6RF_MISO = 6,
    GpioAltFn6RF_MOSI = 6,
    GpioAltFn6RF_SCK = 6,
    GpioAltFn6RF_NSS = 6,

    GpioAltFn7USART1 = 7,

    GpioAltFn8LPUART1 = 8,
    GpioAltFn8IR = 8,

    GpioAltFn9TSC = 9,

    GpioAltFn10QUADSPI = 10,
    GpioAltFn10USB = 10,

    GpioAltFn11LCD = 11,

    GpioAltFn12COMP1 = 12,
    GpioAltFn12COMP2 = 12,
    GpioAltFn12TIM1 = 12,

    GpioAltFn13SAI1 = 13,

    GpioAltFn14TIM2 = 14,
    GpioAltFn14TIM16 = 14,
    GpioAltFn14TIM17 = 14,
    GpioAltFn14LPTIM2 = 14,

    GpioAltFn15EVENTOUT = 15,

    GpioAltFnUnused = 16,
} GpioAltFn;

typedef struct {
    GPIO_TypeDef* port;
    uint16_t pin;
} GpioPin;

void furi_hal_gpio_init_simple(const GpioPin* gpio, const GpioMode mode);

void furi_hal_gpio_init(
    const GpioPin* gpio,
    const GpioMode mode,
    const GpioPull pull,
    const GpioSpeed speed);

void furi_hal_gpio_init_ex(
    const GpioPin* gpio,
    const GpioMode mode,
    const GpioPull pull,
    const GpioSpeed speed,
    const GpioAltFn alt_fn);

void furi_hal_gpio_add_int_callback(const GpioPin* gpio, GpioExtiCallback cb, void* ctx);

void furi_hal_gpio_enable_int_callback(const GpioPin* gpio);

void furi_hal_gpio_disable_int_callback(const GpioPin* gpio);

void furi_hal_gpio_remove_int_callback(const GpioPin* gpio);

static inline void furi_hal_gpio_write(const GpioPin* gpio, const bool state) {

    if(state == true) {
        gpio->port->BSRR = gpio->pin;
    } else {
        gpio->port->BSRR = (uint32_t)gpio->pin << GPIO_NUMBER;
    }
}

static inline void
    furi_hal_gpio_write_port_pin(GPIO_TypeDef* port, uint16_t pin, const bool state) {

    if(state == true) {
        port->BSRR = pin;
    } else {
        port->BSRR = pin << GPIO_NUMBER;
    }
}

static inline bool furi_hal_gpio_read(const GpioPin* gpio) {
    if((gpio->port->IDR & gpio->pin) != 0x00U) {
        return true;
    } else {
        return false;
    }
}

static inline bool furi_hal_gpio_read_port_pin(GPIO_TypeDef* port, uint16_t pin) {
    if((port->IDR & pin) != 0x00U) {
        return true;
    } else {
        return false;
    }
}

#ifdef __cplusplus
}
#endif
