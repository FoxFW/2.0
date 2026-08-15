// SPDX-License-Identifier: GPL-3.0-or-later
// Based on FlipDeFlock by ReconGrunt (https://github.com/ReconGrunt/FlipDeFlock),
// adapted for Fox ESP32 Firmware. See LICENSE and README.md.
#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view_port.h>
#include <notification/notification.h>

#include "wifi/esp_at.h"
#include "helpers/flock_db.h"
#include "helpers/flock_ble.h"
#include "gpio_remap_compat.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FOXDEFLOCK_MAX_HITS 32

typedef enum {
    FoxDeFlockSourceWifi,
    FoxDeFlockSourceBle,
} FoxDeFlockSource;

typedef struct {
    bool used;
    uint8_t mac[6];
    bool have_ssid;
    char ssid[33];
    char ble_name[32];
    FlockConfidence confidence;
    FlockDevClass dev_class;
    FoxDeFlockSource source;
    int8_t rssi;
    uint8_t channel; // 0 = unknown (BLE hits)
    uint32_t last_seen_tick;
    uint32_t first_seen_tick;
    uint16_t sightings;
} FoxDeFlockHit;

typedef enum {
    FoxDeFlockStateEsp32Check,
    FoxDeFlockStateEsp32NotFound,
    FoxDeFlockStatePinSelect,
    FoxDeFlockStateScanning,
    FoxDeFlockStatePaused,
    FoxDeFlockStateDetail,
    FoxDeFlockStateAbout,
} FoxDeFlockAppState;

typedef enum {
    FoxDeFlockModeWifiProbe,
    FoxDeFlockModeWifiBeacon,
    FoxDeFlockModeBleFlock,
} FoxDeFlockScanMode;

typedef struct {
    Gui* gui;
    NotificationApp* notifications;
    ViewPort* view_port;
    FuriMessageQueue* event_queue;
    FuriTimer* timer;

    EspAt* esp_at;
    FuriHalSerialId serial_id;
    size_t pin_option_index; /**< 0 = Usart (13/14), 1 = Lpuart (15/16) */

    FoxDeFlockAppState state;
    bool esp32_probe_ok;
    uint32_t esp32_check_start_tick;
    bool esp32_check_focus_settings;
    FoxDeFlockScanMode scan_mode;
    uint32_t mode_started_tick;

    FoxDeFlockHit hits[FOXDEFLOCK_MAX_HITS];
    uint16_t hit_count;
    uint16_t selected_hit;
    uint16_t list_offset;

    uint32_t wifi_lines_seen;
    uint32_t ble_lines_seen;

    // char[6200] - kept here, not as a local, or FuriTimer's small stack overflows.
    EspAtMsg rx_msg;
} FoxDeFlockApp;

#ifdef __cplusplus
}
#endif
