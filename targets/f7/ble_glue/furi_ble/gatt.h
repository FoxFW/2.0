#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <ble/core/auto/ble_types.h>

typedef bool (
    *cbBleGattCharacteristicData)(const void* context, const uint8_t** data, uint16_t* data_len);

typedef enum {
    FlipperGattCharacteristicDataFixed,
    FlipperGattCharacteristicDataCallback,
} BleGattCharacteristicDataType;

typedef struct {
    Char_Desc_Uuid_t uuid;
    struct {
        cbBleGattCharacteristicData fn;
        const void* context;
    } data_callback;
    uint8_t uuid_type;
    uint8_t max_length;
    uint8_t security_permissions;
    uint8_t access_permissions;
    uint8_t gatt_evt_mask;
    uint8_t is_variable;
} BleGattCharacteristicDescriptorParams;

typedef struct {
    const char* name;
    BleGattCharacteristicDescriptorParams* descriptor_params;
    union {
        struct {
            const uint8_t* ptr;
            uint16_t length;
        } fixed;
        struct {
            cbBleGattCharacteristicData fn;
            const void* context;
        } callback;
    } data;
    Char_UUID_t uuid;

    BleGattCharacteristicDataType data_prop_type : 2;
    uint8_t is_variable                          : 2;
    uint8_t uuid_type                            : 2;
    uint8_t char_properties;
    uint8_t security_permissions;
    uint8_t gatt_evt_mask;
} BleGattCharacteristicParams;

_Static_assert(
    sizeof(BleGattCharacteristicParams) == 36,
    "BleGattCharacteristicParams size must be 36 bytes");

typedef struct {
    const BleGattCharacteristicParams* characteristic;
    uint16_t handle;
    uint16_t descriptor_handle;
} BleGattCharacteristicInstance;

void ble_gatt_characteristic_init(
    uint16_t svc_handle,
    const BleGattCharacteristicParams* char_descriptor,
    BleGattCharacteristicInstance* char_instance);

void ble_gatt_characteristic_delete(
    uint16_t svc_handle,
    BleGattCharacteristicInstance* char_instance);

bool ble_gatt_characteristic_update(
    uint16_t svc_handle,
    BleGattCharacteristicInstance* char_instance,
    const void* source);

bool ble_gatt_service_add(
    uint8_t Service_UUID_Type,
    const Service_UUID_t* Service_UUID,
    uint8_t Service_Type,
    uint8_t Max_Attribute_Records,
    uint16_t* Service_Handle);

bool ble_gatt_service_delete(uint16_t svc_handle);

#ifdef __cplusplus
}
#endif
