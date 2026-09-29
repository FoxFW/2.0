#ifndef proxmark5_COM_H
#define proxmark5_COM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

void proxmark5_com_init(void);
void proxmark5_com_deinit(void);
bool proxmark5_com_handshake(void);
bool proxmark5_com_send_spi(uint8_t* data, size_t length);

#endif // proxmark5_COM_H
