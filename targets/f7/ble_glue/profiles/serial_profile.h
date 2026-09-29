#pragma once

#include <furi_ble/profile_interface.h>

#include <services/serial_service.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BLE_PROFILE_SERIAL_PACKET_SIZE_MAX BLE_SVC_SERIAL_DATA_LEN_MAX

typedef enum {
    FuriHalBtSerialRpcStatusNotActive,
    FuriHalBtSerialRpcStatusActive,
} FuriHalBtSerialRpcStatus;

typedef SerialServiceEventCallback FuriHalBtSerialCallback;

extern const FuriHalBleProfileTemplate* const ble_profile_serial;

bool ble_profile_serial_tx(FuriHalBleProfileBase* profile, uint8_t* data, uint16_t size);

void ble_profile_serial_set_rpc_active(FuriHalBleProfileBase* profile, bool active);

void ble_profile_serial_notify_buffer_is_empty(FuriHalBleProfileBase* profile);

void ble_profile_serial_set_event_callback(
    FuriHalBleProfileBase* profile,
    uint16_t buff_size,
    FuriHalBtSerialCallback callback,
    void* context);

void ble_profile_serial_set_custom_data_callback(
    FuriHalBleProfileBase* profile,
    SerialServiceCustomDataCallback callback,
    void* context);

bool ble_profile_serial_custom_data_tx(
    FuriHalBleProfileBase* profile,
    uint8_t* data,
    uint16_t size);

#ifdef __cplusplus
}
#endif
