#include "esp_at.h"

#include <furi.h>
#include <furi_hal.h>
#include <expansion/expansion.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define ESP_AT_RX_BUFFER          2048
#define ESP_AT_RAW_CAPTURE_BUFFER 4096
#define ESP_AT_QUEUE_DEPTH        16

struct EspAt {
    FuriHalSerialHandle* serial;
    bool serial_owned;
    Expansion* expansion;
    FuriStreamBuffer* rx_stream;
    FuriStreamBuffer* raw_capture_stream;
    volatile bool raw_capture_enabled;
    FuriMessageQueue* msg_queue;
    FuriThread* worker;
    volatile bool running;
};

static void esp_at_rx_callback(FuriHalSerialHandle* handle, FuriHalSerialRxEvent event, void* context) {
    EspAt* esp_at = context;
    if(event == FuriHalSerialRxEventData) {
        uint8_t byte = furi_hal_serial_async_rx(handle);
        furi_stream_buffer_send(esp_at->rx_stream, &byte, 1, 0);
        if(esp_at->raw_capture_enabled) {
            furi_stream_buffer_send(esp_at->raw_capture_stream, &byte, 1, 0);
        }
    }
}

static void esp_at_emit_line(EspAt* esp_at, const char* text, size_t length) {
    if(length == 0) return;
    EspAtMsg msg;
    memset(&msg, 0, sizeof(msg));
    size_t copy_len = length < (ESP_AT_LINE_MAX - 1) ? length : (ESP_AT_LINE_MAX - 1);
    memcpy(msg.line, text, copy_len);
    msg.line[copy_len] = '\0';
    furi_message_queue_put(esp_at->msg_queue, &msg, 0);
}

/* The Fox ESP32 Firmware's protocol is entirely line-based - every
   response, including NOTIFY:<hex>, ends in a plain '\n' with no
   embedded raw bytes. That is a deliberate design choice on the
   firmware side (see fox_esp32_firmware's README), specifically so this
   side never needs the raw-byte, declared-length parsing ESP-AT's
   +NOTIFY: frames required - a genuine simplification versus the
   version of this file that used to talk to real ESP-AT, not just
   fewer lines of code. */
static int32_t esp_at_worker(void* context) {
    EspAt* esp_at = context;

    char line[ESP_AT_LINE_MAX];
    size_t line_len = 0;

    while(esp_at->running) {
        uint8_t byte;
        size_t got = furi_stream_buffer_receive(esp_at->rx_stream, &byte, 1, 100);
        if(got == 0) continue;

        if(byte == '\n') {
            size_t n = line_len;
            if(n > 0 && line[n - 1] == '\r') n--;
            esp_at_emit_line(esp_at, line, n);
            line_len = 0;
        } else if(line_len < ESP_AT_LINE_MAX - 1) {
            line[line_len++] = (char)byte;
        } else {
            /* Line longer than the buffer: flush what's been collected
               so far rather than silently drop it, then keep going. */
            esp_at_emit_line(esp_at, line, line_len);
            line_len = 0;
            line[line_len++] = (char)byte;
        }
    }

    return 0;
}

static FuriHalBus esp_at_bus_for_serial(FuriHalSerialId serial_id) {
    return serial_id == FuriHalSerialIdUsart ? FuriHalBusUSART1 : FuriHalBusLPUART1;
}

EspAt* esp_at_alloc(FuriHalSerialId serial_id, uint32_t baud_rate) {
    /* Flipper's own Expansion Modules service is enabled by default at
       boot, listening on USART (13/14) unless reconfigured, and its own
       header is explicit that any app wanting real serial access must
       call expansion_disable() before furi_hal_serial_control_acquire().
       expansion_disable()/expansion_enable() are both no-ops if the
       service wasn't enabled on this serial_id in the first place (e.g.
       LPUART, which isn't the default listen port), so this is safe to
       do unconditionally. */
    Expansion* expansion = furi_record_open(RECORD_EXPANSION);
    expansion_disable(expansion);

    FuriHalSerialHandle* handle = furi_hal_serial_control_acquire(serial_id);
    if(handle == NULL) {
        expansion_enable(expansion);
        furi_record_close(RECORD_EXPANSION);
        return NULL;
    }

    EspAt* esp_at = malloc(sizeof(EspAt));
    esp_at->rx_stream = furi_stream_buffer_alloc(ESP_AT_RX_BUFFER, 1);
    esp_at->raw_capture_stream = furi_stream_buffer_alloc(ESP_AT_RAW_CAPTURE_BUFFER, 1);
    esp_at->raw_capture_enabled = false;
    esp_at->msg_queue = furi_message_queue_alloc(ESP_AT_QUEUE_DEPTH, sizeof(EspAtMsg));
    esp_at->running = true;
    esp_at->serial = handle;
    esp_at->expansion = expansion;

    esp_at->serial_owned = !furi_hal_bus_is_enabled(esp_at_bus_for_serial(serial_id));
    if(esp_at->serial_owned) {
        furi_hal_serial_init(esp_at->serial, baud_rate);
    }
    furi_hal_serial_set_br(esp_at->serial, baud_rate);
    furi_hal_serial_async_rx_start(esp_at->serial, esp_at_rx_callback, esp_at, false);

    esp_at->worker = furi_thread_alloc_ex("EspAtWorker", 1536, esp_at_worker, esp_at);
    furi_thread_start(esp_at->worker);

    return esp_at;
}

void esp_at_free(EspAt* esp_at) {
    esp_at->running = false;
    furi_thread_join(esp_at->worker);
    furi_thread_free(esp_at->worker);

    /* Stopping the receive interrupt explicitly, before deinit/release,
       isn't optional: flipperdevices/flipperzero-firmware PR #4246 fixed
       a real crash in Flipper's own Expansion service caused by tearing
       down a serial handle without stopping async RX first, and Fox
       ESP32 Detector (a companion app to this one) hit the same class of
       bug - reported as a NULL pointer dereference - from freeing its
       receive buffer before this call existed. */
    furi_hal_serial_async_rx_stop(esp_at->serial);

    if(esp_at->serial_owned) {
        furi_hal_serial_deinit(esp_at->serial);
    }
    furi_hal_serial_control_release(esp_at->serial);

    /* Per expansion.h: expansion_enable() MUST be called right after
       furi_hal_serial_control_release(), to restore the user's own
       Expansion Modules setting now that this app is done with the UART. */
    expansion_enable(esp_at->expansion);
    furi_record_close(RECORD_EXPANSION);

    furi_stream_buffer_free(esp_at->rx_stream);
    furi_stream_buffer_free(esp_at->raw_capture_stream);
    furi_message_queue_free(esp_at->msg_queue);
    free(esp_at);
}

void esp_at_raw_capture_start(EspAt* esp_at) {
    uint8_t discard;
    while(furi_stream_buffer_receive(esp_at->raw_capture_stream, &discard, 1, 0) > 0) {
    }
    esp_at->raw_capture_enabled = true;
}

size_t esp_at_raw_capture_read(EspAt* esp_at, uint8_t* out, size_t out_capacity, uint32_t timeout_ms) {
    return furi_stream_buffer_receive(esp_at->raw_capture_stream, out, out_capacity, timeout_ms);
}

void esp_at_send(EspAt* esp_at, const char* command) {
    furi_hal_serial_tx(esp_at->serial, (const uint8_t*)command, strlen(command));
    furi_hal_serial_tx(esp_at->serial, (const uint8_t*)"\r\n", 2);
}

bool esp_at_receive(EspAt* esp_at, EspAtMsg* msg, uint32_t timeout_ms) {
    return furi_message_queue_get(esp_at->msg_queue, msg, timeout_ms) == FuriStatusOk;
}
