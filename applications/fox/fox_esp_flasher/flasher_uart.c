#include "fox_esp_flasher.h"

#include <furi_hal_serial_control.h>
#include <string.h>

#define TAG         "FlasherUart"
#define UART_CH     FuriHalSerialIdUsart
#define RX_BUF_SIZE 2048U

static const struct {
    const char* label;
} k_pin_options[] = {
    {"13/14 (default)"},
    {"15/16 (alt)"},
};

size_t flasher_pin_option_count(void) {
    return sizeof(k_pin_options) / sizeof(k_pin_options[0]);
}

const char* flasher_pin_option_label(size_t index) {
    if(index >= flasher_pin_option_count()) return "?";
    return k_pin_options[index].label;
}

typedef enum {
    UartEvtStop  = (1 << 0),
    UartEvtRxDone = (1 << 1),
} UartEvtFlags;

static void uart_irq_cb(
    FuriHalSerialHandle* handle,
    FuriHalSerialRxEvent event,
    void* context) {
    FlasherApp* app = context;
    if(event == FuriHalSerialRxEventData) {
        uint8_t byte = furi_hal_serial_async_rx(handle);
        FuriStreamBuffer* dest = app->flash_rx_stream ? app->flash_rx_stream
                                                       : app->uart_rx_stream;
        if(dest) {
            furi_stream_buffer_send(dest, &byte, 1, 0);
        }
        furi_thread_flags_set(furi_thread_get_id(app->uart_rx_thread), UartEvtRxDone);
    }
}

static int32_t uart_rx_worker(void* context) {
    FlasherApp* app = context;
    uint8_t buf[64];

    while(true) {
        uint32_t flags = furi_thread_flags_wait(
            UartEvtStop | UartEvtRxDone, FuriFlagWaitAny, FuriWaitForever);
        if(flags & FuriFlagError) break;
        if(flags & UartEvtStop) break;
        if(flags & UartEvtRxDone) {
            size_t n = furi_stream_buffer_receive(app->uart_rx_stream, buf, sizeof(buf), 0);
            if(n > 0 && !app->flash_rx_stream) {
                view_terminal_append(app, (char*)buf, n);
                view_dispatcher_send_custom_event(
                    app->view_dispatcher, FlasherEventTerminalUpdate);
            }
        }
    }
    return 0;
}

void flasher_uart_open(FlasherApp* app) {
    app->uart_rx_stream = furi_stream_buffer_alloc(RX_BUF_SIZE, 1);

    app->uart_rx_thread = furi_thread_alloc_ex("FlasherUartRx", 1024, uart_rx_worker, app);
    furi_thread_start(app->uart_rx_thread);

    app->serial_handle = furi_hal_serial_control_acquire(UART_CH);
    furi_check(app->serial_handle);
    furi_hal_serial_init(app->serial_handle, FLASHER_BAUDRATE);
    furi_hal_serial_async_rx_start(app->serial_handle, uart_irq_cb, app, false);
}

void flasher_uart_close(FlasherApp* app) {
    if(!app->serial_handle) return;

    furi_hal_serial_async_rx_stop(app->serial_handle);
    furi_hal_serial_deinit(app->serial_handle);
    furi_hal_serial_control_release(app->serial_handle);
    app->serial_handle = NULL;

    furi_thread_flags_set(furi_thread_get_id(app->uart_rx_thread), UartEvtStop);
    furi_thread_join(app->uart_rx_thread);
    furi_thread_free(app->uart_rx_thread);
    app->uart_rx_thread = NULL;

    furi_stream_buffer_free(app->uart_rx_stream);
    app->uart_rx_stream = NULL;
}

void flasher_uart_tx(FlasherApp* app, const uint8_t* data, size_t len) {
    if(app->serial_handle) {
        furi_hal_serial_tx(app->serial_handle, data, len);
    }
}

#define PROBE_CMD     "AT\r\n"
#define PROBE_EXPECT  "Fox ESP32"
#define PROBE_TIMEOUT 3000U   /* ms */

bool flasher_uart_probe(FlasherApp* app) {
    furi_stream_buffer_reset(app->uart_rx_stream);
    flasher_uart_tx(app, (const uint8_t*)PROBE_CMD, strlen(PROBE_CMD));

    uint32_t deadline = furi_get_tick() + furi_ms_to_ticks(PROBE_TIMEOUT);
    char rx_buf[128];
    size_t rx_len = 0;

    while(furi_get_tick() < deadline) {
        uint8_t byte;
        size_t got = furi_stream_buffer_receive(app->uart_rx_stream, &byte, 1, 50);
        if(got) {
            if(rx_len < sizeof(rx_buf) - 1) {
                rx_buf[rx_len++] = (char)byte;
                rx_buf[rx_len] = '\0';
            }
            if(strstr(rx_buf, PROBE_EXPECT)) {
                return true;
            }
        }
    }
    return false;
}
