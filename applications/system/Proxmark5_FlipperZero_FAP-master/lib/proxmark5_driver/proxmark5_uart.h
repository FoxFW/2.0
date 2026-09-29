#ifndef proxmark5_UART_H
#define proxmark5_UART_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

void proxmark5_uart_init(void);
void proxmark5_uart_deinit(void);
bool proxmark5_uart_is_idle(void);
void proxmark5_uart_send_byte(uint8_t data);
void proxmark5_uart_send_data(uint8_t* data, size_t length);
void proxmark5_uart_send_string(const char* str);

#endif // proxmark5_UART_H
