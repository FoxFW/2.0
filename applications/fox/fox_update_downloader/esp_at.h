#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <furi_hal_serial_types.h>

#define ESP_AT_LINE_MAX 6200

typedef struct {
    char line[ESP_AT_LINE_MAX];
} EspAtMsg;

typedef struct EspAt EspAt;

EspAt* esp_at_alloc(FuriHalSerialId serial_id, uint32_t baud_rate);
void esp_at_free(EspAt* esp_at);

void esp_at_send(EspAt* esp_at, const char* command);

bool esp_at_receive(EspAt* esp_at, EspAtMsg* msg, uint32_t timeout_ms);

void esp_at_set_baud(EspAt* esp_at, uint32_t baud_rate);
void esp_at_flush_rx(EspAt* esp_at);

void esp_at_begin_raw(EspAt* esp_at);
size_t esp_at_read_raw(EspAt* esp_at, uint8_t* buf, size_t len, uint32_t timeout_ms);
void esp_at_end_raw(EspAt* esp_at);

// Arms a trigger so the RX worker thread itself flips into raw mode the
// instant it recognises a complete line starting with `line_prefix` -
// synchronously, in the same loop iteration, with no cross-thread latency.
// This must be used instead of calling esp_at_begin_raw() after waiting for
// the line via esp_at_receive(): that pattern leaves a race window where
// bytes belonging to the raw stream (sent immediately after the marker line
// by the peer) can be consumed as text by the worker thread before raw mode
// is enabled, silently corrupting the start of the binary payload.
void esp_at_arm_raw_trigger(EspAt* esp_at, const char* line_prefix);
void esp_at_disarm_raw_trigger(EspAt* esp_at);
