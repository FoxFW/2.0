#pragma once

#include <furi.h>

#include "desktop_settings.h"
#include "helpers/pin_code.h"

#define RECORD_DESKTOP "desktop"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Desktop Desktop;

typedef struct {
    bool locked;
} DesktopStatus;

bool desktop_api_is_locked(Desktop* instance);

void desktop_api_unlock(Desktop* instance);

FuriPubSub* desktop_api_get_status_pubsub(Desktop* instance);

void desktop_api_get_settings(Desktop* instance, DesktopSettings* settings);

void desktop_api_set_settings(Desktop* instance, const DesktopSettings* settings);

// Set the active PIN through the desktop service.
// This updates both the in-memory PIN state and persists it to /int/.fox_pin.bin,
// so the change takes effect immediately without requiring a reboot.
void desktop_api_set_pin(Desktop* instance, const DesktopPinCode* pin_code);

// Clear the active PIN through the desktop service.
void desktop_api_clear_pin(Desktop* instance);

#ifdef __cplusplus
}
#endif
