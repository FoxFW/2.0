#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <furi_ble/profile_interface.h>
#include <core/common_defines.h>

#include <services/serial_service.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RECORD_BT "bt"

typedef struct Bt Bt;

typedef enum {
    BtStatusUnavailable,
    BtStatusOff,
    BtStatusAdvertising,
    BtStatusConnected,
} BtStatus;

typedef struct {
    uint8_t rssi;
    uint32_t since;
} BtRssi;

typedef void (*BtStatusChangedCallback)(BtStatus status, void* context);

FURI_WARN_UNUSED FuriHalBleProfileBase* bt_profile_start(
    Bt* bt,
    const FuriHalBleProfileTemplate* profile_template,
    FuriHalBleProfileParams params);

bool bt_profile_restore_default(Bt* bt);

void bt_disconnect(Bt* bt);

void bt_set_status_changed_callback(Bt* bt, BtStatusChangedCallback callback, void* context);

void bt_forget_bonded_devices(Bt* bt);

void bt_keys_storage_set_storage_path(Bt* bt, const char* keys_storage_path);

void bt_keys_storage_set_default_path(Bt* bt);

void bt_set_custom_data_callback(Bt* bt, SerialServiceCustomDataCallback callback, void* context);

bool bt_custom_data_tx(Bt* bt, uint8_t* data, uint16_t size);

bool bt_is_connected(Bt* bt);

#ifdef __cplusplus
}
#endif
