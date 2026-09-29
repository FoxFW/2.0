#pragma once

#include <lib/subghz/devices/devices.h>

typedef enum {
    SubGhzRadioDeviceTypeInternal,
    SubGhzRadioDeviceTypeExternalCC1101,
} SubGhzRadioDeviceType;

const SubGhzDevice* subratt_radio_device_loader_set(
    const SubGhzDevice* current_radio_device,
    SubGhzRadioDeviceType radio_device_type);

void subratt_radio_device_loader_end(const SubGhzDevice* radio_device);
