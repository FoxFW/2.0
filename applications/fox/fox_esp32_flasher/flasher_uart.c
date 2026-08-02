#include "fox_esp32_flasher.h"

#include <furi_hal_serial_control.h>
#include <furi_hal_gpio.h>
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

static volatile uint32_t g_rx_overrun_count = 0;
static volatile uint32_t g_rx_frame_count   = 0;
static volatile uint32_t g_rx_noise_count   = 0;

void flasher_uart_get_and_reset_rx_errors(uint32_t* overrun, uint32_t* frame, uint32_t* noise) {
    if(overrun) { *overrun = g_rx_overrun_count; g_rx_overrun_count = 0; }
    if(frame)   { *frame   = g_rx_frame_count;   g_rx_frame_count   = 0; }
    if(noise)   { *noise   = g_rx_noise_count;   g_rx_noise_count   = 0; }
}

static void uart_irq_cb(
    FuriHalSerialHandle* handle,
    FuriHalSerialRxEvent event,
    void* context) {
    FlasherApp* app = context;

    if(event & FuriHalSerialRxEventOverrunError) g_rx_overrun_count++;
    if(event & FuriHalSerialRxEventFrameError)   g_rx_frame_count++;
    if(event & FuriHalSerialRxEventNoiseError)   g_rx_noise_count++;

    if(event & FuriHalSerialRxEventData) {
        uint8_t byte = furi_hal_serial_async_rx(handle);

        if(app->uart_rx_stream) {
            furi_stream_buffer_send(app->uart_rx_stream, &byte, 1, 0);
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
            if(n == 0) continue;

            if(app->flash_rx_stream) {
                furi_stream_buffer_send(app->flash_rx_stream, buf, n, 0);
            } else {
                view_terminal_append(app, (char*)buf, n);

                static uint32_t s_last_terminal_ev = 0;
                uint32_t now = furi_get_tick();
                if(now - s_last_terminal_ev >= furi_ms_to_ticks(100)) {
                    s_last_terminal_ev = now;
                    view_dispatcher_send_custom_event(
                        app->view_dispatcher, FlasherEventTerminalUpdate);
                }
            }
        }
    }
    return 0;
}

void flasher_uart_open(FlasherApp* app) {
    app->expansion = furi_record_open(RECORD_EXPANSION);
    expansion_disable(app->expansion);

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

    expansion_enable(app->expansion);
    furi_record_close(RECORD_EXPANSION);
    app->expansion = NULL;
}

void flasher_uart_pause_rx(FlasherApp* app) {
    if(app->serial_handle) {
        furi_hal_serial_async_rx_stop(app->serial_handle);
    }
}

void flasher_uart_resume_rx(FlasherApp* app) {
    if(!app->serial_handle) return;

    if(app->uart_rx_stream) {
        uint8_t discard;
        while(furi_stream_buffer_receive(app->uart_rx_stream, &discard, 1, 0)) {}
    }
    furi_hal_serial_async_rx_start(app->serial_handle, uart_irq_cb, app, false);
}

#define BOOT_RST_HOLD_MS  100
#define BOOT_GPIO0_HOLD_MS 200
#define BOOT_SETTLE_MS    200

uint32_t flasher_uart_enter_bootloader(FlasherApp* app) {
    if(!app->serial_handle) return 0;

    furi_hal_serial_async_rx_stop(app->serial_handle);
    furi_hal_serial_deinit(app->serial_handle);

    furi_hal_gpio_init(&gpio_ext_pc3, GpioModeOutputPushPull, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_write(&gpio_ext_pc3, false);

    furi_hal_gpio_init(&gpio_ext_pb2, GpioModeOutputPushPull, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_write(&gpio_ext_pb2, false);
    furi_delay_ms(BOOT_RST_HOLD_MS);

    furi_hal_gpio_write(&gpio_ext_pb2, true);

    furi_delay_ms(BOOT_GPIO0_HOLD_MS);
    furi_hal_gpio_write(&gpio_ext_pc3, true);

    furi_hal_gpio_init(&gpio_ext_pb2, GpioModeInput, GpioPullNo, GpioSpeedLow);

    furi_hal_serial_init(app->serial_handle, FLASHER_BAUDRATE);

    furi_hal_serial_async_rx_start(app->serial_handle, uart_irq_cb, app, false);

    furi_delay_ms(BOOT_SETTLE_MS);

    uint32_t drained = 0;
    if(app->flash_rx_stream) {
        uint8_t discard;
        while(furi_stream_buffer_receive(app->flash_rx_stream, &discard, 1, 0)) drained++;
    }
    return drained;
}

void flasher_uart_tx(FlasherApp* app, const uint8_t* data, size_t len) {
    if(app->serial_handle) {
        furi_hal_serial_tx(app->serial_handle, data, len);
    }
}

void flasher_uart_set_br(FlasherApp* app, uint32_t baud) {
    if(app->serial_handle) {
        furi_hal_serial_set_br(app->serial_handle, baud);
    }
}

static int32_t prepare_poll_thread_fn(void* context) {
    FlasherApp* app = context;
    while(app->prepare_poll_running) {
        if(flasher_uart_check_bootloader(app)) {
            if(app->prepare_poll_running) {
                view_dispatcher_send_custom_event(
                    app->view_dispatcher, FlasherEventPrepareAutoDetected);
            }
            break;
        }
        for(int i = 0; i < 7 && app->prepare_poll_running; i++) {
            furi_delay_ms(100);
        }
    }
    return 0;
}

void flasher_prepare_poll_start(FlasherApp* app) {
    if(app->prepare_poll_thread) return;
    app->prepare_poll_running = true;
    app->prepare_poll_thread =
        furi_thread_alloc_ex("FlasherPreparePoll", 1024, prepare_poll_thread_fn, app);
    furi_thread_start(app->prepare_poll_thread);
}

void flasher_prepare_poll_stop(FlasherApp* app) {
    if(!app->prepare_poll_thread) return;
    app->prepare_poll_running = false;
    furi_thread_join(app->prepare_poll_thread);
    furi_thread_free(app->prepare_poll_thread);
    app->prepare_poll_thread = NULL;
}

bool flasher_uart_check_bootloader(FlasherApp* app) {
    static const uint8_t sync[] = {
        0xC0,
        0x00, 0x08, 0x24, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x07, 0x07, 0x12, 0x20,
        0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
        0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
        0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
        0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
        0xC0,
    };

    FuriStreamBuffer* probe = furi_stream_buffer_alloc(64, 1);
    app->flash_rx_stream = probe;
    flasher_uart_tx(app, sync, sizeof(sync));

    uint32_t deadline = furi_get_tick() + furi_ms_to_ticks(300);
    bool found = false;
    uint8_t p0 = 0, p1 = 0;
    while(furi_get_tick() < deadline) {
        uint8_t b;
        if(furi_stream_buffer_receive(probe, &b, 1, 50)) {
            if(p0 == 0xC0 && p1 == 0x01 && b == 0x08) { found = true; break; }
            p0 = p1;
            p1 = b;
        }
    }

    flasher_uart_pause_rx(app);
    app->flash_rx_stream = NULL;
    furi_stream_buffer_free(probe);
    flasher_uart_resume_rx(app);
    return found;
}
