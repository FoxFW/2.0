#include <furi_hal.h>
#include <furi_hal_spi.h>

// SPI bus handle for proxmark5 communication
const FuriHalSpiBusHandle* spi_bus = &furi_hal_spi_bus_handle_external;

/**
 * @brief Initialize the SPI bus for proxmark5 communication
 * 
 */
void proxmark5_spi_init(void) {
    furi_hal_spi_bus_handle_init(spi_bus);
}

/**
 * @brief Deinitialize the SPI bus
 * 
 */
void proxmark5_spi_deinit(void) {
    furi_hal_spi_bus_handle_deinit(spi_bus);
}

/**
 * @brief Send data over SPI to proxmark5, will release the bus after transmission
 * 
 * @param data Pointer to the data to send
 * @param length Number of bytes to send
 * @param timeout Timeout in milliseconds
 */
bool proxmark5_spi_send_data(uint8_t* data, size_t length, uint32_t timeout) {
    furi_hal_spi_acquire(spi_bus);
    bool ret = furi_hal_spi_bus_tx(spi_bus, data, length, timeout);
    furi_hal_spi_release(spi_bus);
    return ret;
}

/**
 * @brief Receive data over SPI from proxmark5, will release the bus after reception
 * 
 * @param buffer Pointer to the buffer to store received data
 * @param length Number of bytes to receive
 * @param timeout Timeout in milliseconds
 */
bool proxmark5_spi_receive_data(uint8_t* buffer, size_t length, uint32_t timeout) {
    furi_hal_spi_acquire(spi_bus);
    // bool ret = furi_hal_spi_bus_rx(spi_bus, buffer, length, timeout);
    bool ret = furi_hal_spi_bus_trx(spi_bus, NULL, buffer, length, timeout);
    furi_hal_spi_release(spi_bus);
    return ret;
}
