#include "esp_at.h"

#include <furi.h>
#include <furi_hal.h>
#include <expansion/expansion.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define ESP_AT_RX_BUFFER   8192
#define ESP_AT_QUEUE_DEPTH 2

struct EspAt {
    FuriHalSerialHandle* serial;
    bool serial_owned;
    Expansion* expansion;
    FuriStreamBuffer* rx_stream;
    FuriMessageQueue* msg_queue;
    FuriThread* worker;
    volatile bool running;
    volatile bool raw_mode;
};

static void esp_at_rx_callback(FuriHalSerialHandle* handle, FuriHalSerialRxEvent event, void* context) {
    EspAt* esp_at = context;
    if(event == FuriHalSerialRxEventData) {
        uint8_t byte = furi_hal_serial_async_rx(handle);
        furi_stream_buffer_send(esp_at->rx_stream, &byte, 1, 0);
    }
}

static EspAtMsg s_emit_msg;

static void esp_at_emit_line(EspAt* esp_at, const char* text, size_t length) {
    if(length == 0) return;
    memset(&s_emit_msg, 0, sizeof(s_emit_msg));
    size_t copy_len = length < (ESP_AT_LINE_MAX - 1) ? length : (ESP_AT_LINE_MAX - 1);
    memcpy(s_emit_msg.line, text, copy_len);
    s_emit_msg.line[copy_len] = '\0';
    furi_message_queue_put(esp_at->msg_queue, &s_emit_msg, FuriWaitForever);
}

static int32_t esp_at_worker(void* context) {
    EspAt* esp_at = context;

    static char line[ESP_AT_LINE_MAX];
    size_t line_len = 0;

    while(esp_at->running) {
        if(esp_at->raw_mode) {
            furi_delay_ms(2);
            continue;
        }

        uint8_t byte;
        size_t got = furi_stream_buffer_receive(esp_at->rx_stream, &byte, 1, 5);
        if(got == 0) continue;

        if(byte == '\n') {
            size_t n = line_len;
            if(n > 0 && line[n - 1] == '\r') n--;
            esp_at_emit_line(esp_at, line, n);
            line_len = 0;
        } else if(line_len < ESP_AT_LINE_MAX - 1) {
            line[line_len++] = (char)byte;
        } else {
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
    esp_at->msg_queue = furi_message_queue_alloc(ESP_AT_QUEUE_DEPTH, sizeof(EspAtMsg));
    esp_at->running = true;
    esp_at->raw_mode = false;
    esp_at->serial = handle;
    esp_at->expansion = expansion;

    esp_at->serial_owned = !furi_hal_bus_is_enabled(esp_at_bus_for_serial(serial_id));
    if(esp_at->serial_owned) {
        furi_hal_serial_init(esp_at->serial, baud_rate);
    }
    furi_hal_serial_set_br(esp_at->serial, baud_rate);
    furi_hal_serial_async_rx_start(esp_at->serial, esp_at_rx_callback, esp_at, false);

    esp_at->worker = furi_thread_alloc_ex("EspAtWorker", 2048, esp_at_worker, esp_at);
    furi_thread_start(esp_at->worker);

    return esp_at;
}

void esp_at_free(EspAt* esp_at) {
    esp_at->running = false;
    furi_thread_join(esp_at->worker);
    furi_thread_free(esp_at->worker);

    furi_hal_serial_async_rx_stop(esp_at->serial);

    if(esp_at->serial_owned) {
        furi_hal_serial_deinit(esp_at->serial);
    }
    furi_hal_serial_control_release(esp_at->serial);

    expansion_enable(esp_at->expansion);
    furi_record_close(RECORD_EXPANSION);

    furi_stream_buffer_free(esp_at->rx_stream);
    furi_message_queue_free(esp_at->msg_queue);
    free(esp_at);
}

void esp_at_send(EspAt* esp_at, const char* command) {
    furi_hal_serial_tx(esp_at->serial, (const uint8_t*)command, strlen(command));
    furi_hal_serial_tx(esp_at->serial, (const uint8_t*)"\r\n", 2);
}

bool esp_at_receive(EspAt* esp_at, EspAtMsg* msg, uint32_t timeout_ms) {
    return furi_message_queue_get(esp_at->msg_queue, msg, timeout_ms) == FuriStatusOk;
}

void esp_at_set_baud(EspAt* esp_at, uint32_t baud_rate) {
    furi_hal_serial_tx_wait_complete(esp_at->serial);
    furi_hal_serial_set_br(esp_at->serial, baud_rate);
}

void esp_at_begin_raw(EspAt* esp_at) {
    esp_at->raw_mode = true;
    furi_delay_ms(10);
}

size_t esp_at_read_raw(EspAt* esp_at, uint8_t* buf, size_t len, uint32_t timeout_ms) {
    size_t got = 0;
    uint32_t start = furi_get_tick();
    while(got < len) {
        uint32_t elapsed = furi_get_tick() - start;
        if(elapsed >= timeout_ms) break;
        uint32_t remaining = timeout_ms - elapsed;
        size_t chunk =
            furi_stream_buffer_receive(esp_at->rx_stream, buf + got, len - got, remaining);
        if(chunk == 0) break;
        got += chunk;
    }
    return got;
}

void esp_at_end_raw(EspAt* esp_at) {
    esp_at->raw_mode = false;
}
