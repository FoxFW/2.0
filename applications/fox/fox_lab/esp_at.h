#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <furi_hal_serial_types.h>

/* 512 (was 256) so a single FLPR line the ESP32 sends TO the Flipper
 * always fits - the longest being a base64 FILES/WRITE/CHUNK payload,
 * deliberately chunked to 360 raw bytes (-> 480 b64 chars) so it fits
 * under this budget with room for the "[FLPR/FILES/WRITE/CHUNK]" tag.
 * Costs ~4KB extra static RAM for the 16-deep message queue (esp_at_
 * router.h), trivial on F7's 256KB SRAM. This only bounds lines the
 * ESP32 sends TO the Flipper; lines the Flipper itself sends
 * (esp_at_send(), e.g. FILES/READ/CHUNK replies or SCREEN/FRAME) have no
 * length limit here. */
#define ESP_AT_LINE_MAX 512

typedef struct {
    char line[ESP_AT_LINE_MAX];
} EspAtMsg;

typedef struct EspAt EspAt;

EspAt* esp_at_alloc(FuriHalSerialId serial_id, uint32_t baud_rate);
void esp_at_free(EspAt* esp_at);

void esp_at_send(EspAt* esp_at, const char* command);

bool esp_at_receive(EspAt* esp_at, EspAtMsg* msg, uint32_t timeout_ms);

void esp_at_raw_capture_start(EspAt* esp_at);
size_t esp_at_raw_capture_read(EspAt* esp_at, uint8_t* out, size_t out_capacity, uint32_t timeout_ms);
