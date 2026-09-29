#pragma once

#include <furi_hal_serial_types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RECORD_EXPANSION "expansion"

typedef struct Expansion Expansion;

void expansion_enable(Expansion* instance);

void expansion_disable(Expansion* instance);

bool expansion_is_connected(Expansion* instance);

void expansion_set_listen_serial(Expansion* instance, FuriHalSerialId serial_id);

#ifdef __cplusplus
}
#endif
