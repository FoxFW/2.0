#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <furi_hal_serial_types.h>

/* Named esp_at.* from when this transport spoke real Espressif ESP-AT
   commands. It now speaks the Fox ESP32 Firmware's own, simpler
   line-based protocol instead - see README.md, "Why this project moved
   off ESP-AT", and fox_esp32_firmware's own README for the firmware
   side of that change. Kept as esp_at.* rather than renamed, to keep
   this change to what actually needed to change; a rename to something
   like esp32_link.c would be a reasonable, purely cosmetic follow-up. */

#define ESP_AT_LINE_MAX 128

/* Every response in the new protocol - including what used to be a
   raw-byte +NOTIFY: frame under ESP-AT - is a plain '\n'-terminated
   text line, hex-encoded where it carries binary data. That is what
   let this go from a two-message-type protocol (line vs. raw-byte
   notify) down to just this. */
typedef struct {
    char line[ESP_AT_LINE_MAX];
} EspAtMsg;

typedef struct EspAt EspAt;

/* Returns NULL, rather than crashing, if serial_id can't be claimed -
   for example because it's already owned by another subsystem. On a
   Flipper Zero, Settings -> Expansion Modules can be configured to
   listen on the same UART this app wants; if so, that's the most likely
   reason this returns NULL. See README.md, "Known limitations". */
EspAt* esp_at_alloc(FuriHalSerialId serial_id, uint32_t baud_rate);
void esp_at_free(EspAt* esp_at);

/* Appends \r\n - command should not include it. */
void esp_at_send(EspAt* esp_at, const char* command);

bool esp_at_receive(EspAt* esp_at, EspAtMsg* msg, uint32_t timeout_ms);

/* Raw byte capture, independent of the line parser above - a tap on
   the same ISR feed, not a repackaging of what the parser already
   produced. Always allocated but inactive until
   esp_at_raw_capture_start() is called, so it costs nothing when
   unused. */
void esp_at_raw_capture_start(EspAt* esp_at);
size_t esp_at_raw_capture_read(EspAt* esp_at, uint8_t* out, size_t out_capacity, uint32_t timeout_ms);
