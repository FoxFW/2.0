#pragma once

#include <stdbool.h>
#include "core_defines.h"

#ifdef __cplusplus
extern "C" {
#endif

void furi_record_init(void);

bool furi_record_exists(const char* name);

void furi_record_create(const char* name, void* data);

bool furi_record_destroy(const char* name);

FURI_RETURNS_NONNULL void* furi_record_open(const char* name);

void furi_record_close(const char* name);

#ifdef __cplusplus
}
#endif
