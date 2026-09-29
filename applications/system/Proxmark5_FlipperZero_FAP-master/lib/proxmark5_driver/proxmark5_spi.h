#ifndef proxmark5_SPI_H
#define proxmark5_SPI_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

void proxmark5_spi_init(void);
void proxmark5_spi_deinit(void);
bool proxmark5_spi_send_data(uint8_t* data, size_t length, uint32_t timeout);
bool proxmark5_spi_receive_data(uint8_t* buffer, size_t length, uint32_t timeout);

#endif // proxmark5_SPI_H
