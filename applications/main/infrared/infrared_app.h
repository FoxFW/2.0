#pragma once

typedef struct InfraredApp InfraredApp;

#include <storage/storage.h>
#include <furi_hal_infrared.h>

#define INFRARED_SETTINGS_PATH    EXT_PATH("infrared/.infrared.settings")
#define INFRARED_SETTINGS_VERSION (1)
#define INFRARED_SETTINGS_MAGIC   (0x1F)

typedef struct {
    FuriHalInfraredTxPin tx_pin;
    bool otg_enabled;
} InfraredSettings;
