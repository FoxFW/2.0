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

void esp_at_begin_raw(EspAt* esp_at);
size_t esp_at_read_raw(EspAt* esp_at, uint8_t* buf, size_t len, uint32_t timeout_ms);
void esp_at_end_raw(EspAt* esp_at);
