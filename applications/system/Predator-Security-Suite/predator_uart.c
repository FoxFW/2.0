#include "predator_uart.h"
#include <furi_hal_serial.h>
#include <furi_hal_resources.h>

#define PREDATOR_UART_RX_BUF_SIZE 2048

struct PredatorUart {
    FuriHalSerialHandle* serial_handle;
    FuriThread* rx_thread;
    FuriStreamBuffer* rx_stream;
    PredatorUartRxCallback rx_callback;
    void* rx_callback_context;
    bool running;
};

static int32_t predator_uart_rx_thread(void* context) {
    // Critical safety check
    if(!context) {
        FURI_LOG_E("PredatorUART", "NULL context in RX thread");
        return -1;
    }
    
    PredatorUart* uart = (PredatorUart*)context;
    uint8_t data[64];
    
    // Verify stream buffer exists before entering loop
    if(!uart->rx_stream) {
        FURI_LOG_E("PredatorUART", "NULL rx_stream in RX thread");
        return -1;
    }
    
    // Main receive loop with thorough null checks
    while(uart && uart->rx_stream && uart->running) {
        // Use shorter timeout to check running flag more frequently
        size_t len = furi_stream_buffer_receive(uart->rx_stream, data, sizeof(data), 50);
        
        // Only call callback if we have valid data and callback is set
        if(len > 0 && uart && uart->rx_callback) {
            // Safety check - capture callback locally in case it changes during execution
            PredatorUartRxCallback callback = uart->rx_callback;
            void* callback_context = uart->rx_callback_context;
            
            // Only call if we have both a valid callback and context
            if(callback) {
                callback(data, len, callback_context);
            }
        }
        
        // Brief pause to prevent CPU hogging
        furi_delay_us(100);
    }
    
    FURI_LOG_I("PredatorUART", "RX thread exiting cleanly");
    return 0;
}

static void predator_uart_on_irq_cb(FuriHalSerialHandle* handle, FuriHalSerialRxEvent event, void* context) {
    // Critical safety checks
    if(!handle || !context) {
        // Cannot log from IRQ context
        return;
    }
    
    PredatorUart* uart = (PredatorUart*)context;
    
    // Handle receive event with null pointer protections
    if(event == FuriHalSerialRxEventData && uart && uart->serial_handle && uart->rx_stream) {
        // Get data safely
        uint8_t data = furi_hal_serial_async_rx(uart->serial_handle);
        
        // Momentum SDK does not expose an ISR variant; use zero-timeout send.
        if(uart->running) {
            furi_stream_buffer_send(uart->rx_stream, &data, 1, 0);
        }
    }
}

PredatorUart* predator_uart_init(
    const GpioPin* tx_pin,
    const GpioPin* rx_pin,
    uint32_t baud_rate,
    PredatorUartRxCallback rx_callback,
    void* context) {
    
    // Input validation to prevent crashes
    if(!tx_pin || !rx_pin) {
        FURI_LOG_E("PredatorUART", "Invalid GPIO pins for UART");
        return NULL;
    }
    
    // Simple check that pins are not NULL (already done above)
    // Skip additional validation as furi_hal_gpio_is_valid is not available
    
    // Allocate with NULL check
    PredatorUart* uart = malloc(sizeof(PredatorUart));
    if(!uart) {
        FURI_LOG_E("PredatorUART", "Failed to allocate memory for UART");
        return NULL;
    }
    
    // Clear structure to prevent undefined behavior
    memset(uart, 0, sizeof(PredatorUart));
    
    // Determine serial ID based on pins with validation
    FuriHalSerialId serial_id;
    if(tx_pin == &gpio_ext_pc0 && rx_pin == &gpio_ext_pc1) {
        serial_id = FuriHalSerialIdUsart;
    } else if(tx_pin == &gpio_ext_pb2 && rx_pin == &gpio_ext_pb3) {
        serial_id = FuriHalSerialIdLpuart;
    } else {
        FURI_LOG_W("PredatorUART", "Unsupported pin configuration, using default");
        serial_id = FuriHalSerialIdUsart; // Default
    }
    
    uart->rx_callback = rx_callback;
    uart->rx_callback_context = context;
    uart->running = true;
    
    // Error handling for stream buffer allocation
    uart->rx_stream = furi_stream_buffer_alloc(PREDATOR_UART_RX_BUF_SIZE, 1);
    if(!uart->rx_stream) {
        FURI_LOG_E("PredatorUART", "Failed to allocate stream buffer");
        free(uart);
        return NULL;
    }
    
    // Handle serial acquisition failure
    uart->serial_handle = furi_hal_serial_control_acquire(serial_id);
    if(!uart->serial_handle) {
        FURI_LOG_E("PredatorUART", "Failed to acquire serial port");
        furi_stream_buffer_free(uart->rx_stream);
        free(uart);
        return NULL;
    }
    
    // Initialize with error handling
    furi_hal_serial_init(uart->serial_handle, baud_rate);
    
    // Start RX
    furi_hal_serial_async_rx_start(uart->serial_handle, predator_uart_on_irq_cb, uart, false);
    
    // Thread allocation with error checking
    uart->rx_thread = furi_thread_alloc_ex("PredatorUartRx", 1024, predator_uart_rx_thread, uart);
    if(!uart->rx_thread) {
        FURI_LOG_E("PredatorUART", "Failed to allocate rx thread");
        furi_hal_serial_async_rx_stop(uart->serial_handle);
        furi_hal_serial_deinit(uart->serial_handle);
        furi_hal_serial_control_release(uart->serial_handle);
        furi_stream_buffer_free(uart->rx_stream);
        free(uart);
        return NULL;
    }
    
    // Start thread (Momentum SDK: furi_thread_start returns void)
    furi_thread_start(uart->rx_thread);
    
    FURI_LOG_I("PredatorUART", "UART initialized successfully");
    return uart;
}

void predator_uart_deinit(PredatorUart* uart) {
    // Safety check - return if NULL
    if (!uart) return;
    
    // First mark thread as not running to prevent callbacks during cleanup
    uart->running = false;
    
    // Safety checks for each component
    if (uart->rx_thread) {
        // Allow thread time to exit cleanly
        furi_delay_ms(10);
        furi_thread_join(uart->rx_thread);
        furi_thread_free(uart->rx_thread);
        uart->rx_thread = NULL;
    }
    
    // Safely clean up serial components
    if (uart->serial_handle) {
        furi_hal_serial_async_rx_stop(uart->serial_handle);
        furi_hal_serial_deinit(uart->serial_handle);
        furi_hal_serial_control_release(uart->serial_handle);
        uart->serial_handle = NULL;
    }
    
    // Free stream buffer if it exists
    if (uart->rx_stream) {
        furi_stream_buffer_free(uart->rx_stream);
        uart->rx_stream = NULL;
    }
    
    free(uart);
}

void predator_uart_tx(PredatorUart* uart, uint8_t* data, size_t len) {
    // Safety check - return if NULL or invalid parameters
    if (!uart || !data || len == 0) return;
    
    // Check if serial handle is valid
    if (!uart->serial_handle) {
        FURI_LOG_E("PredatorUART", "Attempted TX on invalid serial handle");
        return;
    }
    
    // Send data with error handling
    furi_hal_serial_tx(uart->serial_handle, data, len);
}

void predator_uart_set_br(PredatorUart* uart, uint32_t baud) {
    // Safety check - return if NULL
    if (!uart) return;
    
    // Check if serial handle is valid
    if (!uart->serial_handle) {
        FURI_LOG_E("PredatorUART", "Attempted to set baud rate on invalid serial handle");
        return;
    }
    
    furi_hal_serial_set_br(uart->serial_handle, baud);
}

void predator_uart_set_rx_callback(PredatorUart* uart, PredatorUartRxCallback callback, void* context) {
    // Safety check - return if NULL
    if (!uart) return;
    
    // First disable running to prevent thread from using callback during change
    bool was_running = uart->running;
    uart->running = false;
    
    // Wait a moment to ensure thread sees the change
    if (was_running) {
        furi_delay_ms(5);
    }
    
    // Update callback info
    uart->rx_callback = callback;
    uart->rx_callback_context = context;
    
    // Restore running state
    uart->running = was_running;
}
