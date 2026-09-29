#include <furi_hal.h>
#include "proxmark5_uart.h"

#define PROXMARK5_GPIO_UART &gpio_ext_pb2
#define PROXMARK5_BAUDRATE  2400
#define PROXMARK5_BIT_US    (1000000 / PROXMARK5_BAUDRATE)

// proxmark5 UART initialization, sets up the GPIO pin for output
void proxmark5_uart_init(void) {
    // Initialize PB2 as push-pull output, default high
    furi_hal_gpio_init_simple(PROXMARK5_GPIO_UART, GpioModeOutputOpenDrain);
    furi_hal_gpio_write(PROXMARK5_GPIO_UART, true);
}

/**
 * @brief Deinitialize proxmark5 UART, resets the GPIO pin to input state
 * 
 */
void proxmark5_uart_deinit(void) {
    furi_hal_gpio_init_simple(PROXMARK5_GPIO_UART, GpioModeInput);
}

// Check if the proxmark5 UART line is idle (high) or busy (low)
bool proxmark5_uart_is_idle(void) {
    // If the line is high, it's idle. If it's low, it's currently transmitting.
    return furi_hal_gpio_read(PROXMARK5_GPIO_UART);
}

// proxmark5 UART sends one byte: start bit + 8 data bits + stop bit, LSB first
void proxmark5_uart_send_byte(uint8_t data) {
    // Disable interrupts for the entire byte to prevent FreeRTOS/BLE/USB
    // interrupts from stretching any bit period and causing framing errors.
    FURI_CRITICAL_ENTER();
    // start bit
    furi_hal_gpio_write(PROXMARK5_GPIO_UART, false);
    furi_delay_us(PROXMARK5_BIT_US);
    // data bits
    for(int i = 0; i < 8; i++) {
        furi_hal_gpio_write(PROXMARK5_GPIO_UART, (data >> i) & 0x01);
        furi_delay_us(PROXMARK5_BIT_US);
    }
    // stop bit
    furi_hal_gpio_write(PROXMARK5_GPIO_UART, true);
    furi_delay_us(PROXMARK5_BIT_US);
    FURI_CRITICAL_EXIT();
}

// proxmark5 UART sends multiple bytes of data
void proxmark5_uart_send_data(uint8_t* data, size_t length) {
    for(size_t i = 0; i < length; i++) {
        proxmark5_uart_send_byte(data[i]);
    }
}

// proxmark5 UART sends a null-terminated string, excluding the null terminator
void proxmark5_uart_send_string(const char* str) {
    while(*str) {
        proxmark5_uart_send_byte((uint8_t)(*str));
        str++;
    }
}
